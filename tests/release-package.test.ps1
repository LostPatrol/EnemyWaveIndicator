# Exercise the real packager in an isolated fixture, including rejection before archive creation.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $root ('agent\codex\release-package-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
foreach ($relative in @('scripts','docs','engine\FSD\Config','third_party\MinHook','native','cook','assets')) {
    New-Item -ItemType Directory -Force (Join-Path $testRoot $relative) | Out-Null
}
Copy-Item (Join-Path $root 'scripts\Prepare-Release.ps1') (Join-Path $testRoot 'scripts\Prepare-Release.ps1')
foreach ($relative in @('README.md','LICENSE','scripts\Install-Release.ps1','docs\RELEASE.md','docs\MODIO-FEASIBILITY.md','third_party\MinHook\LICENSE.txt')) {
    Copy-Item (Join-Path $root $relative) (Join-Path $testRoot $relative)
}
$config = Join-Path $testRoot 'engine\FSD\Config\DefaultGame.ini'
Copy-Item (Join-Path $root 'engine\FSD\Config\DefaultGame.ini') $config
$nativeDir = Join-Path $testRoot 'native'; $cookDir = Join-Path $testRoot 'cook'; $assetDir = Join-Path $testRoot 'assets'
$source = Join-Path $nativeDir 'source.cpp'; $dll = Join-Path $nativeDir 'main.dll'
$asset = Join-Path $assetDir 'fixture.uasset'; $pak = Join-Path $cookDir 'NormalWavePresentation-assets-only.pak'
Set-Content $source '// Fixture native source.'; Set-Content $dll 'fixture-dll'
Set-Content $asset 'fixture-asset'; Set-Content $pak 'fixture-pak'
$files = @(1..16 | ForEach-Object {
    $path = Join-Path $cookDir ("cooked-$_.bin"); Set-Content $path "fixture-$_"; Get-FileHash $path
})
$native = @{
    OfflinePassed=$true; DispatchOfflinePassed=$true; CaptureOfflinePassed=$true; PresentationOfflinePassed=$true
    ProbeVersion='0.6.0'; SourceFiles=@(Get-FileHash $source); SHA256=(Get-FileHash $dll).Hash
}
$assets = @{
    Success=$true; Assets=@(Get-FileHash $asset)
    Validation=@{success=$true;settings_test=$true;automatic_pool_test=$true;red_material_test=$true;visual_no_controller_test=$true;async_resource_tests=$true;edge_cases=1452}
}
$cook = @{
    Success=$true; AssetsOnly=$true; ContainsGameAssetCopies=$false; InlineMaterialShaders=$true
    Files=$files; Verification=$assetDir; Pak=Get-FileHash $pak; PackagingConfig=Get-FileHash $config
}
# Save independent validation records, just as the build/cook scripts do.
function Save-Fixture {
    $native | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $nativeDir 'verification.json')
    $assets | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $assetDir 'verification.json')
    $cook | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $cookDir 'verification.json')
}
function Assert-Rejected([string]$Pattern, [string]$Target = 'NativeAlpha') {
    $outputRoot = Join-Path $testRoot 'agent\codex'
    $before = @(Get-ChildItem $outputRoot -ErrorAction SilentlyContinue).Count
    $message = ''
    try { & (Join-Path $testRoot 'scripts\Prepare-Release.ps1') -BuildDirectory $nativeDir -PresentationCook $cookDir -Target $Target | Out-Null }
    catch { $message = $_.Exception.Message }
    if ($message -notmatch $Pattern) { throw "Expected rejection '$Pattern', got '$message'." }
    if (@(Get-ChildItem $outputRoot -ErrorAction SilentlyContinue).Count -ne $before) { throw 'Rejected input produced release output.' }
}
# This must fail even before verification records exist, leaving no output tree.
Assert-Rejected 'subscription-only release is blocked' 'Modio'
Save-Fixture
$assets.Success = $false; Save-Fixture; Assert-Rejected 'Missing complete release validation'; $assets.Success = $true
$assets.Validation.success = $false; Save-Fixture; Assert-Rejected 'Missing complete release validation'; $assets.Validation.success = $true
$native.CaptureOfflinePassed = $false; Save-Fixture; Assert-Rejected 'Missing complete release validation'; $native.CaptureOfflinePassed = $true
$native.ProbeVersion = '0.5.1'; Save-Fixture; Assert-Rejected 'Missing complete release validation'; $native.ProbeVersion = '0.6.0'
$cook.ContainsGameAssetCopies = $true; Save-Fixture; Assert-Rejected 'Missing complete release validation'; $cook.ContainsGameAssetCopies = $false
$savedSources = $native.SourceFiles; $native.SourceFiles = @(); Save-Fixture; Assert-Rejected 'Missing release input hashes'; $native.SourceFiles = $savedSources
Save-Fixture
foreach ($path in @($source,$asset,$files[0].Path,$config,$dll,$pak)) {
    $original = [IO.File]::ReadAllBytes($path)
    try {
        Add-Content $path 'changed-after-validation'
        Assert-Rejected 'Release input changed|Packaging configuration changed|Build payload changed'
    } finally { [IO.File]::WriteAllBytes($path, $original) }
}
$release = & (Join-Path $testRoot 'scripts\Prepare-Release.ps1') -BuildDirectory $nativeDir -PresentationCook $cookDir
$manifest = Get-Content (Join-Path $release 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.DistributionTarget -ne 'NativeAlpha' -or $manifest.ModioSubscriptionOnly -ne $false -or
    $manifest.ReleaseReady -ne $false -or $manifest.Requires.Count -ne 3) { throw 'Misleading distribution manifest.' }
# Read the actual archive to check payload hashes, required documentation and absence of fixture/source leaks.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($release + '.zip')
try {
    $names = @($zip.Entries.FullName | ForEach-Object { $_.Replace('\','/') })
    $expected = @('main.dll','NormalWaveIndicator_P.pak','Install.ps1','README.md','LICENSE','MinHook-LICENSE.txt','manifest.json','docs/RELEASE.md','docs/MODIO-FEASIBILITY.md')
    if (@(Compare-Object ($names | Where-Object { !$_.EndsWith('/') } | Sort-Object) ($expected | Sort-Object)).Count) { throw 'Unexpected archive contents.' }
    foreach ($name in @('main.dll','NormalWaveIndicator_P.pak')) {
        $entry = $zip.Entries | Where-Object FullName -eq $name
        $stream = $entry.Open(); $hasher = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','') }
        finally { $stream.Dispose(); $hasher.Dispose() }
        if ($hash -ne $manifest.Files.$name) { throw "Archive payload differs: $name" }
    }
} finally { $zip.Dispose() }
Write-Output "PASS: 13 early rejection cases; native archive contents/hashes and truthful distribution metadata. Evidence: $testRoot"
