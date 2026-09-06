# Assemble a verified native alpha; reject subscription-only packaging until its runtime exists.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$PresentationCook,
    [ValidateSet('NativeAlpha','Modio')][string]$Target = 'NativeAlpha'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# A cooked presentation Pak does not replace native provenance capture or bootstrap.
if ($Target -eq 'Modio') {
    throw 'mod.io subscription-only release is blocked: native capture/UE4SSL and bootstrap are still required. See docs/MODIO-FEASIBILITY.md.'
}
$native = Get-Content -LiteralPath (Join-Path $BuildDirectory 'verification.json') -Raw | ConvertFrom-Json
$cook = Get-Content -LiteralPath (Join-Path $PresentationCook 'verification.json') -Raw | ConvertFrom-Json
$assets = Get-Content -LiteralPath (Join-Path $cook.Verification 'verification.json') -Raw | ConvertFrom-Json
if ($native.ProbeVersion -ne '0.6.0' -or !$native.OfflinePassed -or !$native.DispatchOfflinePassed -or !$native.CaptureOfflinePassed -or !$native.PresentationOfflinePassed -or
    !$cook.Success -or !$cook.AssetsOnly -or $cook.ContainsGameAssetCopies -or !$cook.InlineMaterialShaders -or $cook.Files.Count -ne 16 -or
    !$assets.Success -or !$assets.Validation.success -or !$assets.Validation.settings_test -or !$assets.Validation.automatic_pool_test -or
    !$assets.Validation.red_material_test -or !$assets.Validation.visual_no_controller_test -or !$assets.Validation.async_resource_tests -or
    $assets.Validation.edge_cases -ne 1452) { throw 'Missing complete release validation.' }
# Direct packaging must enforce the same input freshness as source installation.
if (!$native.SourceFiles -or !$assets.Assets -or !$cook.PackagingConfig.Hash) { throw 'Missing release input hashes.' }
foreach ($file in @($native.SourceFiles) + @($assets.Assets) + @($cook.Files)) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Release input changed: $($file.Path)" }
}
if ((Get-FileHash -LiteralPath (Join-Path $root 'engine\FSD\Config\DefaultGame.ini')).Hash -ne $cook.PackagingConfig.Hash) {
    throw 'Packaging configuration changed; recook first.'
}
$dll = Join-Path $BuildDirectory 'main.dll'; $pak = Join-Path $PresentationCook 'NormalWavePresentation-assets-only.pak'
if ((Get-FileHash -LiteralPath $dll).Hash -ne $native.SHA256 -or (Get-FileHash -LiteralPath $pak).Hash -ne $cook.Pak.Hash) { throw 'Build payload changed.' }
$output = Join-Path $root ('agent\codex\release-' + $native.ProbeVersion + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force $output | Out-Null
Copy-Item -LiteralPath $dll -Destination $output
Copy-Item -LiteralPath $pak -Destination (Join-Path $output 'NormalWaveIndicator_P.pak')
Copy-Item -LiteralPath (Join-Path $root 'scripts\Install-Release.ps1') -Destination (Join-Path $output 'Install.ps1')
Copy-Item -LiteralPath (Join-Path $root 'README.md'),(Join-Path $root 'LICENSE') -Destination $output
New-Item -ItemType Directory -Path (Join-Path $output 'docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'docs\RELEASE.md') -Destination (Join-Path $output 'docs\RELEASE.md')
Copy-Item -LiteralPath (Join-Path $root 'docs\MODIO-FEASIBILITY.md') -Destination (Join-Path $output 'docs\MODIO-FEASIBILITY.md')
Copy-Item -LiteralPath (Join-Path $root 'third_party\MinHook\LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
@{
    Version=$native.ProbeVersion; Status='alpha; new Mod Hub UI requires in-game testing'
    DistributionTarget='NativeAlpha'; ModioSubscriptionOnly=$false; ReleaseReady=$false
    Requires=@('UE4SSL.JavaScript stable 0.31.0 (audited hash)', 'manual native installation', 'Mod Hub for settings')
    GameSHA256='9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13'
    RuntimeSHA256='D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7'
    Files=@{'main.dll'=$native.SHA256; 'NormalWaveIndicator_P.pak'=$cook.Pak.Hash}
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
Compress-Archive -Path (Join-Path $output '*') -DestinationPath ($output + '.zip')
Write-Output $output
