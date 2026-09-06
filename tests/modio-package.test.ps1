# Regression-test the content-only packager's refusal gates and its actual one-Pak upload ZIP.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $root ('agent\codex\modio-package-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
foreach ($dir in @('scripts','cook','assets','source')) { New-Item -ItemType Directory -Force (Join-Path $testRoot $dir) | Out-Null }
Copy-Item (Join-Path $root 'scripts\Prepare-ModioRelease.ps1') (Join-Path $testRoot 'scripts')
$cookDir=Join-Path $testRoot 'cook'; $assetsDir=Join-Path $testRoot 'assets'
$source=Join-Path $testRoot 'source\fixture.cpp'; $asset=Join-Path $assetsDir 'fixture.uasset'; $config=Join-Path $testRoot 'config.ini'
Set-Content $source '// Test fixture'; Set-Content $asset 'fixture'; Set-Content $config 'fixture config'
$names=@('BP_NwiAuto','BP_NwiResources','BP_NwiPulse','WBP_NwiMarker','SG_NwiSettings','WBP_NwiSettings','M_NwiRedPulse','InitCave','InitSpacerig')
$files=@($names | ForEach-Object { foreach($extension in @('uasset','uexp')) { $p=Join-Path $cookDir "$_.${extension}"; Set-Content $p 'fixture cooked bytes'; Get-FileHash $p } })
$pak=Join-Path $cookDir 'NormalWavePresentation-assets-only.pak'; Set-Content $pak 'fixture Pak payload'
$audit=Join-Path $cookDir 'dependency-audit.json'; @{passed=$true;contentOnly=$true;assets=9} | ConvertTo-Json | Set-Content $audit
$assets=@{Success=$true;SourceFiles=@(Get-FileHash $source);Assets=@(Get-FileHash $asset);Validation=@{success=$true;capture_test=$true;content_only=$true;automatic_pool_test=$true;settings_test=$true;async_resource_tests=$true;red_material_test=$true;edge_cases=1452}}
$cook=@{Success=$true;ContentOnly=$true;Version='0.7.0';SeparateNativeBootstrapRequired=$false;AssetsOnly=$true;ContainsGameAssetCopies=$false;InlineMaterialShaders=$true;PakHashesVerified=$true;Files=$files;Verification=$assetsDir;Pak=Get-FileHash $pak;DependencyAudit=Get-FileHash $audit;PackagingConfig=Get-FileHash $config}
function Save-Fixture { $assets | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $assetsDir 'verification.json'); $cook | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $cookDir 'verification.json') }
$script=Join-Path $testRoot 'scripts\Prepare-ModioRelease.ps1'
$rejections=0
function Assert-Rejected([string]$Pattern) {
    $before=@(Get-ChildItem (Join-Path $testRoot 'agent\codex') -ErrorAction SilentlyContinue).Count
    $message=''; try { & $script -PresentationCook $cookDir | Out-Null } catch { $message=$_.Exception.Message }
    if($message -notmatch $Pattern) { throw "Expected $Pattern, got $message" }
    if(@(Get-ChildItem (Join-Path $testRoot 'agent\codex') -ErrorAction SilentlyContinue).Count -ne $before) { throw 'Rejected input wrote release files.' }
    $script:rejections++
}
Assert-Rejected 'Missing content-only cook verification'
Save-Fixture
foreach($flag in @('Success','ContentOnly','AssetsOnly','InlineMaterialShaders','PakHashesVerified')) { $cook[$flag]=$false; Save-Fixture; Assert-Rejected 'Missing complete content-only'; $cook[$flag]=$true }
$cook.SeparateNativeBootstrapRequired=$true; Save-Fixture; Assert-Rejected 'Missing complete content-only'; $cook.SeparateNativeBootstrapRequired=$false
$cook.ContainsGameAssetCopies=$true; Save-Fixture; Assert-Rejected 'Missing complete content-only'; $cook.ContainsGameAssetCopies=$false
$assets.Validation.capture_test=$false; Save-Fixture; Assert-Rejected 'Missing content-only Blueprint'; $assets.Validation.capture_test=$true
$assets.Validation.content_only=$false; Save-Fixture; Assert-Rejected 'Missing content-only Blueprint'; $assets.Validation.content_only=$true
$oldSources=$assets.SourceFiles; $assets.SourceFiles=@(); Save-Fixture; Assert-Rejected 'Missing content-only input hashes'; $assets.SourceFiles=$oldSources
Save-Fixture
foreach($p in @($source,$asset,$files[0].Path,$config,$pak,$audit)) {
    $old=[IO.File]::ReadAllBytes($p)
    try { Add-Content $p 'tamper'; Assert-Rejected 'Content-only input changed' } finally { [IO.File]::WriteAllBytes($p,$old) }
}
$release=& $script -PresentationCook $cookDir
$m=Get-Content (Join-Path $release 'manifest.json') -Raw | ConvertFrom-Json
if(!$m.ModioSubscriptionOnly -or $m.RuntimeDllRequired -or $m.ModioSubscriptionTested -or $m.ReleaseReady) { throw 'Untruthful distribution metadata.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($release+'.zip')
try {
    if($zip.Entries.Count -ne 1 -or $zip.Entries[0].FullName -ne 'NormalWaveIndicator_P.pak') { throw 'Upload ZIP must contain exactly one Pak.' }
    $stream=$zip.Entries[0].Open(); $sha=[Security.Cryptography.SHA256]::Create()
    try { $hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') } finally { $stream.Dispose(); $sha.Dispose() }
    if($hash -ne $m.Files.'NormalWaveIndicator_P.pak') { throw 'Archive payload differs from manifest.' }
} finally { $zip.Dispose() }
Write-Output "PASS: $rejections rejection cases and a one-Pak ZIP with accurate untested-subscription metadata. Evidence: $testRoot"
