# Package matching tested native/Blueprint artifacts for MintCat; this does not publish or install them.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory, [Parameter(Mandatory)][string]$PresentationCook)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$native=Get-Content -LiteralPath (Join-Path $BuildDirectory 'verification.json') -Raw | ConvertFrom-Json
$cook=Get-Content -LiteralPath (Join-Path $PresentationCook 'verification.json') -Raw | ConvertFrom-Json
if ($native.ProbeVersion -ne '0.9.1' -or !$native.OfflinePassed -or !$native.CaptureOfflinePassed -or !$native.DispatchOfflinePassed -or !$native.PresentationOfflinePassed) { throw 'Missing native validation.' }
if (!$cook.Success -or $cook.ContentOnly -or !$cook.RuntimeDllRequired -or $cook.Version -ne '0.9.1' -or !$cook.AssetsOnly -or $cook.ContainsGameAssetCopies -or !$cook.InlineMaterialShaders -or !$cook.PakHashesVerified -or $cook.Files.Count -ne 18) { throw 'Missing native-Pak cook validation.' }
$assets=Get-Content -LiteralPath (Join-Path $cook.Verification 'verification.json') -Raw | ConvertFrom-Json
if (!$assets.Success -or !$assets.Validation.success -or !$assets.Validation.native_contract_test -or !$assets.Validation.replication_metadata_test -or $assets.Validation.content_only -or !$assets.Validation.automatic_pool_test -or !$assets.Validation.settings_test -or !$assets.Validation.async_resource_tests -or !$assets.Validation.red_material_test -or $assets.Validation.edge_cases -ne 1452) { throw 'Missing native Blueprint validation.' }
if (!$native.SourceFiles -or !$assets.SourceFiles -or !$assets.Assets -or !$cook.DependencyAudit.Hash -or !$cook.PackagingConfig.Hash) { throw 'Missing input hashes.' }
foreach ($file in @($native.SourceFiles)+@($assets.SourceFiles)+@($assets.Assets)+@($cook.Files)+@($cook.Pak,$cook.DependencyAudit,$cook.PackagingConfig)) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Input changed: $($file.Path)" }
}
$dll=Join-Path $BuildDirectory 'main.dll'
if ((Get-FileHash -LiteralPath $dll).Hash -ne $native.SHA256) { throw 'Native DLL changed.' }
$expected=@('BP_NwiAuto','BP_NwiResources','BP_NwiPulse','WBP_NwiMarker','SG_NwiSettings','WBP_NwiSettings','M_NwiRedPulse','InitCave','InitSpacerig') | ForEach-Object { "$_.uasset"; "$_.uexp" }
$actual=@($cook.Files | ForEach-Object { Split-Path $_.Path -Leaf })
if (@(Compare-Object ($expected | Sort-Object) ($actual | Sort-Object)).Count) { throw 'Unexpected package entries.' }
$audit=Get-Content -LiteralPath $cook.DependencyAudit.Path -Raw | ConvertFrom-Json
if (!$audit.passed -or $audit.contentOnly -or $audit.nativeAbi -ne 589824 -or $audit.assets -ne 9) { throw 'Invalid dependency audit.' }
$output=Join-Path $root ('agent\codex\mintcat-0.9.1-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$pak=Join-Path $output 'NormalWaveIndicator_P.pak'
Copy-Item -LiteralPath $dll -Destination $output
Copy-Item -LiteralPath $cook.Pak.Path -Destination $pak
# Include redistributed MinHook's license, even though the loader only consumes the two binary entries.
$licenses=(Get-Content -LiteralPath (Join-Path $root 'LICENSE') -Raw)+"`r`n`r`nMinHook:`r`n"+(Get-Content -LiteralPath (Join-Path $root 'third_party\MinHook\LICENSE.txt') -Raw)
$catalog=Get-Content -LiteralPath (Join-Path $root 'engine\Authoring\NwiAuthoring\Source\NwiAuthoring\Public\NwiWaveTypes.h') -Raw
$sourceNames=@([regex]::Matches($catalog,'\{L"([^"]+)", L"') | ForEach-Object { $_.Groups[1].Value })
if ($sourceNames.Count -ne 36) { throw 'Unexpected wave catalog.' }
$licensePath=Join-Path $output 'LICENSES.txt'; Set-Content -LiteralPath $licensePath -Value $licenses -Encoding utf8
@{
    Version='0.9.1'; DistributionTarget='MintCat'; Status='36 stock wave types test candidate; real-game and network validation pending'
    RuntimeDllRequired=$true; ModioSubscriptionOnly=$false; MintCatInstallTested=$false; NetworkTested=$false; ReleaseReady=$false
    Capture='strict natural scheduler chain; exact initiating scripted-controller class; per-request queue provenance; successful registered regular enemies'
    Position='source center when available; queued spawn origin otherwise'; Weight='successful count times descriptor base DifficultyRating'
    ExcludedBuckets=@('ActiveSwarmerEnemies','ActiveCritters'); UnregisteredExcluded=$true
    SupportedSources=$sourceNames; PerTypeEnableAndText=$true; UnsupportedRequestedFeatures=@('independent non-wave-controller boss/direct summons','multi-second exact prediction')
    Localization=@{Languages=@('en','zh-CN');Automatic=$true;MarkerDefaults='English in every language; user editable'}
    ClientRequiresPak=$true; HostRequiresDll=$true; MaximumRegions=8
    Requires=@('MintCat with UE4SSL.JavaScript stable 0.31.0 audited runtime','Mod Hub','matching 0.9.1 Pak on participating clients')
    GameSHA256='9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13'
    RuntimeSHA256='D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7'
    Files=@{'main.dll'=$native.SHA256;'NormalWaveIndicator_P.pak'=(Get-FileHash $pak).Hash;'LICENSES.txt'=(Get-FileHash $licensePath).Hash}
    Verification=$cook.Verification; Cook=$PresentationCook; NativeBuild=$BuildDirectory
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
Compress-Archive -LiteralPath (Join-Path $output 'main.dll'),$pak,$licensePath -DestinationPath ($output+'.zip')
Write-Output $output
