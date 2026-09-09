# Verify native/Pak matching, stale evidence rejection and every file in the MintCat ZIP.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$testRoot=Join-Path $root ('agent\codex\mintcat-package-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
foreach($dir in @('scripts','cook','assets','source','native','third_party\MinHook')) { New-Item -ItemType Directory -Force (Join-Path $testRoot $dir) | Out-Null }
Copy-Item (Join-Path $root 'scripts\Prepare-ModioRelease.ps1') (Join-Path $testRoot 'scripts')
Copy-Item (Join-Path $root 'LICENSE') $testRoot
$catalogRelative='engine\Authoring\NwiAuthoring\Source\NwiAuthoring\Public\NwiWaveTypes.h'
New-Item -ItemType Directory -Force (Split-Path (Join-Path $testRoot $catalogRelative)) | Out-Null
Copy-Item (Join-Path $root $catalogRelative) (Join-Path $testRoot $catalogRelative)
Copy-Item (Join-Path $root 'third_party\MinHook\LICENSE.txt') (Join-Path $testRoot 'third_party\MinHook')
$cookDir=Join-Path $testRoot 'cook';$assetsDir=Join-Path $testRoot 'assets';$nativeDir=Join-Path $testRoot 'native'
$source=Join-Path $testRoot 'source\fixture.cpp';$asset=Join-Path $assetsDir 'fixture.uasset';$config=Join-Path $testRoot 'config.ini';$dll=Join-Path $nativeDir 'main.dll'
Set-Content $source '// Synthetic build fixture';Set-Content $asset 'fixture asset';Set-Content $config '; Fixture packaging config';Set-Content $dll 'fixture native binary'
$names=@('BP_NwiAuto','BP_NwiResources','BP_NwiPulse','WBP_NwiMarker','SG_NwiSettings','WBP_NwiSettings','M_NwiRedPulse','InitCave','InitSpacerig')
$files=@($names | ForEach-Object {foreach($extension in @('uasset','uexp')) {$p=Join-Path $cookDir "$_.${extension}";Set-Content $p 'fixture cooked bytes';Get-FileHash $p}})
$pak=Join-Path $cookDir 'EnemyWavePresentation-assets-only.pak';Set-Content $pak 'fixture Pak payload'
$audit=Join-Path $cookDir 'dependency-audit.json';@{passed=$true;contentOnly=$false;nativeAbi=590848;assets=9} | ConvertTo-Json | Set-Content $audit
$native=@{ProbeVersion='0.9.4';OfflinePassed=$true;CaptureOfflinePassed=$true;DispatchOfflinePassed=$true;PresentationOfflinePassed=$true;SourceFiles=@(Get-FileHash $source);SHA256=(Get-FileHash $dll).Hash}
$assets=@{Success=$true;SourceFiles=@(Get-FileHash $source);Assets=@(Get-FileHash $asset);Validation=@{success=$true;native_contract_test=$true;replication_metadata_test=$true;content_only=$false;automatic_pool_test=$true;settings_test=$true;async_resource_tests=$true;red_material_test=$true;edge_cases=1452}}
$cook=@{Success=$true;ContentOnly=$false;RuntimeDllRequired=$true;Version='0.9.4';AssetsOnly=$true;ContainsGameAssetCopies=$false;InlineMaterialShaders=$true;PakHashesVerified=$true;Files=$files;Verification=$assetsDir;Pak=Get-FileHash $pak;DependencyAudit=Get-FileHash $audit;PackagingConfig=Get-FileHash $config}
function Save-Fixture {$native | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $nativeDir 'verification.json');$assets | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $assetsDir 'verification.json');$cook | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $cookDir 'verification.json')}
$script=Join-Path $testRoot 'scripts\Prepare-ModioRelease.ps1';$rejections=0
function Assert-Rejected([string]$Pattern) {
    $before=@(Get-ChildItem (Join-Path $testRoot 'agent\codex') -ErrorAction SilentlyContinue).Count
    $message='';try {& $script -BuildDirectory $nativeDir -PresentationCook $cookDir | Out-Null} catch {$message=$_.Exception.Message}
    if($message -notmatch $Pattern){throw "Expected $Pattern, got $message"}
    if(@(Get-ChildItem (Join-Path $testRoot 'agent\codex') -ErrorAction SilentlyContinue).Count -ne $before){throw 'Rejected input wrote release files.'}
    $script:rejections++
}
Save-Fixture
foreach($flag in @('OfflinePassed','CaptureOfflinePassed','DispatchOfflinePassed','PresentationOfflinePassed')) {$native[$flag]=$false;Save-Fixture;Assert-Rejected 'Missing native validation';$native[$flag]=$true}
$native.ProbeVersion='0.6.0';Save-Fixture;Assert-Rejected 'Missing native validation';$native.ProbeVersion='0.9.4'
foreach($flag in @('Success','RuntimeDllRequired','AssetsOnly','InlineMaterialShaders','PakHashesVerified')) {$cook[$flag]=$false;Save-Fixture;Assert-Rejected 'Missing native-Pak';$cook[$flag]=$true}
$cook.ContentOnly=$true;Save-Fixture;Assert-Rejected 'Missing native-Pak';$cook.ContentOnly=$false
$cook.ContainsGameAssetCopies=$true;Save-Fixture;Assert-Rejected 'Missing native-Pak';$cook.ContainsGameAssetCopies=$false
foreach($flag in @('native_contract_test','replication_metadata_test','settings_test','automatic_pool_test')) {$assets.Validation[$flag]=$false;Save-Fixture;Assert-Rejected 'Missing native Blueprint';$assets.Validation[$flag]=$true}
$assets.Validation.content_only=$true;Save-Fixture;Assert-Rejected 'Missing native Blueprint';$assets.Validation.content_only=$false
$originalSources=$native.SourceFiles;$native.SourceFiles=@();Save-Fixture;Assert-Rejected 'Missing input hashes';$native.SourceFiles=$originalSources
Save-Fixture
foreach($p in @($source,$asset,$files[0].Path,$config,$pak,$audit,$dll)) {
    $original=[IO.File]::ReadAllBytes($p)
    try {Add-Content $p 'tamper';Assert-Rejected 'Input changed|Native DLL changed'} finally {[IO.File]::WriteAllBytes($p,$original)}
}
$release=& $script -BuildDirectory $nativeDir -PresentationCook $cookDir
$m=Get-Content (Join-Path $release 'manifest.json') -Raw | ConvertFrom-Json
if($m.ModioSubscriptionOnly -or !$m.RuntimeDllRequired -or $m.MintCatInstallTested -or $m.NetworkTested -or $m.ReleaseReady){throw 'Untruthful acceptance metadata.'}
if($m.ArchiveName -ne 'EnemyWaveIndicator-0.9.4.zip'){throw 'Unexpected public archive name.'}
if(!$m.Localization.Automatic -or @($m.Localization.Languages).Count -ne 2 -or 'en' -notin $m.Localization.Languages -or 'zh-CN' -notin $m.Localization.Languages -or $m.Localization.MarkerDefaults -notmatch 'English'){throw 'Missing bilingual localization metadata.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($release+'.zip')
try {
    if(@(Compare-Object @('EnemyWaveIndicator_P.pak','LICENSES.txt','main.dll') @($zip.Entries.FullName)).Count){throw 'Unexpected ZIP entries.'}
    foreach($entry in $zip.Entries) {
        $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
        try {$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')} finally {$stream.Dispose();$sha.Dispose()}
        if($hash -ne $m.Files.($entry.FullName)){throw 'ZIP payload differs from manifest.'}
    }
} finally {$zip.Dispose()}
$publicArchive=Join-Path $testRoot 'dist\EnemyWaveIndicator-0.9.4.zip'
if(!(Test-Path -LiteralPath $publicArchive)){throw 'Missing public EnemyWaveIndicator archive.'}
if((Get-FileHash -LiteralPath $publicArchive).Hash -ne (Get-FileHash -LiteralPath ($release+'.zip')).Hash){throw 'Public archive differs from staged archive.'}
$archives=@(Get-ChildItem -LiteralPath (Join-Path $testRoot 'dist') -Filter '*.zip' -File)
if($archives.Count -ne 1 -or $archives[0].Name -ne 'EnemyWaveIndicator-0.9.4.zip'){throw 'Unexpected public archive set.'}
Write-Output "PASS: $rejections refusal cases and all three ZIP hashes. Evidence: $testRoot"
