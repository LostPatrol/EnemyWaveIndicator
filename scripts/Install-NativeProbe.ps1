# Deploy the verified visual bootstrap and its own cooked pak; never replace UE4SSL or game assets.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$PresentationCook,
    [string]$GameRoot = 'D:\Steam\steamapps\common\Deep Rock Galactic'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process -Name 'FSD-Win64-Shipping','FSD' -ErrorAction SilentlyContinue) {
    throw 'Close Deep Rock Galactic before installing the native probe.'
}
$verification = Get-Content -LiteralPath (Join-Path $BuildDirectory 'verification.json') -Raw | ConvertFrom-Json
$source = Join-Path $BuildDirectory 'main.dll'
if (!$verification.OfflinePassed -or (Get-FileHash -LiteralPath $source).Hash -ne $verification.SHA256) {
    throw 'Missing or mismatched successful offline verification.'
}
if ((Get-FileHash -LiteralPath (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\main.cpp')).Hash -ne $verification.SourceSHA256) {
    throw 'Probe source changed after this build; rebuild and retest first.'
}
if (!$verification.DispatchOfflinePassed -or !$verification.CaptureOfflinePassed -or $verification.ProbeVersion -ne '0.6.0' -or
    (Get-FileHash -LiteralPath (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\DispatchProbe.h')).Hash -ne $verification.HeaderSHA256) {
    throw 'Dispatch core changed or has no matching successful tests; rebuild first.'
}
if ((Get-FileHash -LiteralPath (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\EngineThreadIdentity.h')).Hash -ne $verification.EngineHeaderSHA256) {
    throw 'Engine identity reader changed after verification; rebuild first.'
}
if (!$verification.PresentationOfflinePassed -or
    (Get-FileHash -LiteralPath (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\PresentationBootstrap.h')).Hash -ne $verification.PresentationHeaderSHA256) {
    throw 'Presentation bootstrap has no matching successful tests.'
}
$cooked = Get-Content -LiteralPath (Join-Path $PresentationCook 'verification.json') -Raw | ConvertFrom-Json
if (!$verification.SourceFiles) { throw 'Missing complete native source and dependency manifest.' }
foreach ($file in $verification.SourceFiles) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Native input changed: $($file.Path)" }
}
if ((Get-FileHash -LiteralPath (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\ActiveWorld.h')).Hash -ne $verification.WorldHeaderSHA256) {
    throw 'Active-world reader changed after verification; rebuild first.'
}
$pakSource = Join-Path $PresentationCook 'NormalWavePresentation-assets-only.pak'
if (!$cooked.Success -or !$cooked.AssetsOnly -or $cooked.ContainsGameAssetCopies -or $cooked.Files.Count -ne 16 -or
    (Get-FileHash -LiteralPath $pakSource).Hash -ne $cooked.Pak.Hash) { throw 'Expected a verified sixteen-file configurable presentation pak.' }
if (!$cooked.InlineMaterialShaders -or
    (Get-FileHash -LiteralPath (Join-Path $projectRoot 'engine\FSD\Config\DefaultGame.ini')).Hash -ne $cooked.PackagingConfig.Hash) {
    throw 'Missing matching inline material shader verification; recook first.'
}
$assetVerification = Get-Content -LiteralPath (Join-Path $cooked.Verification 'verification.json') -Raw | ConvertFrom-Json
if (!$assetVerification.Success -or !$assetVerification.Validation.visual_no_controller_test -or
    !$assetVerification.Validation.red_material_test -or !$assetVerification.Validation.automatic_pool_test -or $assetVerification.Validation.edge_cases -ne 1452) {
    throw 'Missing matching red material, edge-placement or lifecycle tests.'
}
foreach ($asset in $assetVerification.Assets) {
    if ((Get-FileHash -LiteralPath $asset.Path).Hash -ne $asset.Hash) { throw 'Presentation assets changed after verification.' }
}
$gameExe = Join-Path $GameRoot 'FSD\Binaries\Win64\FSD-Win64-Shipping.exe'
if ((Get-FileHash -LiteralPath $gameExe).Hash -ne '9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13') {
    throw 'Game executable differs from the audited engine thread identity. Re-audit before installing.'
}
$runtimeRoot = Join-Path $GameRoot 'FSD\Binaries\Win64\ue4ss'
$runtime = Join-Path $runtimeRoot 'UE4SSL.dll'
# The installed 0.31.0 loader was audited for these versioned C lifecycle entrypoints.
$expectedRuntime = 'D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7'
if ((Get-FileHash -LiteralPath $runtime).Hash -ne $expectedRuntime) {
    throw 'UE4SSL differs from the audited runtime. Recheck its ABI before deployment.'
}
# Use the same transactional installer as the public archive after stronger source-build checks.
$release = & (Join-Path $PSScriptRoot 'Prepare-Release.ps1') -BuildDirectory $BuildDirectory -PresentationCook $PresentationCook
& (Join-Path $release 'Install.ps1') -GameRoot $GameRoot | Out-Host
[pscustomobject]@{
    Installed = Join-Path $runtimeRoot 'mods\NormalWaveIndicator'
    Release = $release; BuildDirectory = $BuildDirectory; PresentationCook = $PresentationCook
    SHA256 = $verification.SHA256; PakSHA256 = $cooked.Pak.Hash
    InGameVerified = $false; DeployedAt = (Get-Date -Format o)
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $release 'deployment.json') -Encoding utf8
Write-Output $release
