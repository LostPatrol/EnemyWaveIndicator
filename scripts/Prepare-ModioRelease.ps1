# Package a verified all-spawn content-only beta; the upload ZIP contains exactly one Pak.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$PresentationCook)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$record = Join-Path $PresentationCook 'verification.json'
if (!(Test-Path -LiteralPath $record)) { throw 'Missing content-only cook verification.' }
$cook = Get-Content -LiteralPath $record -Raw | ConvertFrom-Json
if (!$cook.Success -or !$cook.ContentOnly -or $cook.Version -ne '0.7.0' -or $cook.SeparateNativeBootstrapRequired -ne $false -or
    !$cook.AssetsOnly -or $cook.ContainsGameAssetCopies -or !$cook.InlineMaterialShaders -or !$cook.PakHashesVerified -or $cook.Files.Count -ne 18) {
    throw 'Missing complete content-only cook validation.'
}
$assets = Get-Content -LiteralPath (Join-Path $cook.Verification 'verification.json') -Raw | ConvertFrom-Json
if (!$assets.Success -or !$assets.Validation.success -or !$assets.Validation.capture_test -or !$assets.Validation.content_only -or
    !$assets.Validation.automatic_pool_test -or !$assets.Validation.settings_test -or !$assets.Validation.async_resource_tests -or
    !$assets.Validation.red_material_test -or $assets.Validation.edge_cases -ne 1452) { throw 'Missing content-only Blueprint validation.' }
if (!$assets.SourceFiles -or !$assets.Assets -or !$cook.DependencyAudit.Hash -or !$cook.PackagingConfig.Hash) { throw 'Missing content-only input hashes.' }
foreach ($file in @($assets.SourceFiles)+@($assets.Assets)+@($cook.Files)+@($cook.Pak,$cook.DependencyAudit,$cook.PackagingConfig)) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Content-only input changed: $($file.Path)" }
}
$expected = @('BP_NwiAuto','BP_NwiResources','BP_NwiPulse','WBP_NwiMarker','SG_NwiSettings','WBP_NwiSettings','M_NwiRedPulse','InitCave','InitSpacerig') |
    ForEach-Object { "$_.uasset"; "$_.uexp" }
$actual = @($cook.Files | ForEach-Object { Split-Path $_.Path -Leaf })
if (@(Compare-Object ($expected | Sort-Object) ($actual | Sort-Object)).Count) { throw 'Unexpected content-only package entries.' }
$audit = Get-Content -LiteralPath $cook.DependencyAudit.Path -Raw | ConvertFrom-Json
if (!$audit.passed -or !$audit.contentOnly -or $audit.assets -ne 9) { throw 'Invalid cooked dependency audit.' }
$output = Join-Path $root ('agent\codex\modio-0.7.0-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$pak = Join-Path $output 'NormalWaveIndicator_P.pak'
Copy-Item -LiteralPath $cook.Pak.Path -Destination $pak
# Metadata stays beside the upload ZIP; no DLL, scripts, authoring stubs or installer are included.
@{
    Version='0.7.0'; DistributionTarget='Modio'; Status='content-only beta candidate'
    ModioSubscriptionOnly=$true; ModioSubscriptionTested=$false; ReleaseReady=$false
    Capture='EnemySpawnManager.OnEnemySpawned; callback-time positions; approximate active-wave context'
    RuntimeDllRequired=$false; HostOnly=$true; MaximumRegions=8; MergeRadiusMeters=8
    Requires=@('Deep Rock Galactic native mod support','Mod Hub content dependency')
    Files=@{'NormalWaveIndicator_P.pak'=(Get-FileHash -LiteralPath $pak).Hash}
    Verification=$cook.Verification; Cook=$PresentationCook
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
Compress-Archive -LiteralPath $pak -DestinationPath ($output + '.zip')
Write-Output $output
