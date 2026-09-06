# Compile and run engine-independent provenance tests; this script never deploys or loads the game.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'MSVC x64 is required.' }
& (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Host
$output = Join-Path $projectRoot ('agent\codex\spawn-attribution-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location $output
try {
    $source = Join-Path $projectRoot 'tests\native-spawn-attribution.cpp'
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 $source /Fe:attribution-test.exe 2>&1 | Tee-Object build.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Attribution tests failed to compile.' }
    & .\attribution-test.exe | Tee-Object tests.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Attribution tests failed.' }
    [pscustomobject]@{
        Description = 'Synthetic bookkeeping only; requires a complete native mutation adapter before use.'
        Passed = $true; InGameVerified = $false; Deployed = $false
        HeaderSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\SpawnAttribution.h')).Hash
        TestSHA256 = (Get-FileHash $source).Hash
        ReferenceOperations = 100000
    } | ConvertTo-Json | Set-Content verification.json -Encoding utf8
} finally { Pop-Location }
Write-Output $output
