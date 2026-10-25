# Build or reuse the read-only native resolver inspector; emit only its absolute executable path.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
# Every maintained compiler input participates in the content-addressed build directory.
$relativeInputs = @('tests/game-bindings-inspect.cpp', 'mods/EnemyWaveNativeProbe/GameBindingResolver.h', 'mods/EnemyWaveNativeProbe/GameBindingProfile.generated.h', 'mods/EnemyWaveNativeProbe/GameBindingProfile.json', 'scripts/Build-GameBindingInspector.ps1')
$fingerprintLines = foreach ($relative in $relativeInputs) {
    $inputFile = Join-Path $projectRoot $relative
    $inputHash = (Get-FileHash -LiteralPath $inputFile -Algorithm SHA256).Hash
    "$relative $inputHash"
}
$fingerprint = $fingerprintLines -join "`n"
$hasher = [Security.Cryptography.SHA256]::Create()
try { $inputSha = [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($fingerprint))).Replace('-', '') } finally { $hasher.Dispose() }
$output = Join-Path $projectRoot ('agent\codex\game-binding-inspector-' + $inputSha.Substring(0, 16))
New-Item -ItemType Directory -Force -Path $output | Out-Null
$checkLog = Join-Path $output 'profile-check.log'
& node (Join-Path $PSScriptRoot 'Generate-GameBindings.cjs') --check *> $checkLog
if ($LASTEXITCODE -ne 0) { throw "Binding profile check failed. See $checkLog" }
$executable = Join-Path $output 'game-bindings-inspect.exe'
$fingerprintFile = Join-Path $output 'inputs.sha256.txt'
if ((Test-Path -LiteralPath $executable) -and (Test-Path -LiteralPath $fingerprintFile) -and
    (Get-Content -LiteralPath $fingerprintFile -Raw).TrimEnd("`r", "`n") -ceq $fingerprint) {
    Write-Output $executable
    return
}
$environmentLog = Join-Path $output 'compiler-environment.log'
$buildLog = Join-Path $output 'build.log'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Install the existing project prerequisite: MSVC x64 and Windows SDK.' }
try {
    & (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation *> $environmentLog
} catch { throw "MSVC environment setup failed. See $environmentLog" }
Push-Location $output
try {
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 (Join-Path $projectRoot 'tests\game-bindings-inspect.cpp') /Fe:game-bindings-inspect.exe *> $buildLog
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $executable)) { throw "Inspector compilation failed. See $buildLog" }
    Set-Content -LiteralPath $fingerprintFile -Value $fingerprint -Encoding utf8
} finally { Pop-Location }
Write-Output $executable
