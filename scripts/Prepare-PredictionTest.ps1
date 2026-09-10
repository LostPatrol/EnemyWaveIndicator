# Package the opt-in natural-wave prediction experiment under agent/codex; never publish, install, or write dist.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory, [Parameter(Mandatory)][string]$PresentationCook)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$native = Get-Content -LiteralPath (Join-Path $BuildDirectory 'verification.json') -Raw | ConvertFrom-Json
$cook = Get-Content -LiteralPath (Join-Path $PresentationCook 'verification.json') -Raw | ConvertFrom-Json
if (!$native.PredictionTest -or $native.ProbeVersion -ne '0.9.4-prediction-test.6' -or !$native.OfflinePassed -or !$native.PredictionOfflinePassed) {
    throw 'A verified -PredictionTest native build is required.'
}
if (!$cook.Success -or !$cook.AssetsOnly -or $cook.ContainsGameAssetCopies -or !$cook.PakHashesVerified -or $cook.Files.Count -ne 18) {
    throw 'A verified assets-only presentation cook is required.'
}
foreach ($file in @($native.SourceFiles) + @($cook.Files) + @($cook.Pak, $cook.DependencyAudit, $cook.PackagingConfig)) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Input changed: $($file.Path)" }
}
$output = Join-Path $root ('agent\codex\EnemyWaveIndicator-prediction-test-6-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$dll = Join-Path $BuildDirectory 'main.dll'
$pak = Join-Path $output 'EnemyWaveIndicator_P.pak'
Copy-Item -LiteralPath $dll -Destination $output
Copy-Item -LiteralPath $cook.Pak.Path -Destination $pak
$licenses = (Get-Content -LiteralPath (Join-Path $root 'LICENSE') -Raw) + "`r`n`r`nMinHook:`r`n" + (Get-Content -LiteralPath (Join-Path $root 'third_party\MinHook\LICENSE.txt') -Raw)
$licensePath = Join-Path $output 'LICENSES.txt'
Set-Content -LiteralPath $licensePath -Value $licenses -Encoding utf8
@{
    Version = '0.9.4-prediction-test.6'; Experimental = $true; Published = $false; Installed = $false
    Behavior = 'One stock center-selector call at T-5; reuse that result at the exact natural-wave call only when players remain still and the relevant navigation fingerprint is unchanged; otherwise fall through.'
    Risk = 'Controlled gameplay mutation: selector RNG/state is consumed early; a later rejected lock requires a second selector call.'
    Marker = '[?] PREDICTED NATURAL (~5s)'; PredictionRegionType = 255
    OutputScope = 'agent/codex only'; DistModified = $false
    NativeBuild = $BuildDirectory; PresentationCook = $PresentationCook
    Files = @{'main.dll'=(Get-FileHash $dll).Hash;'EnemyWaveIndicator_P.pak'=(Get-FileHash $pak).Hash;'LICENSES.txt'=(Get-FileHash $licensePath).Hash}
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
$archive = $output + '.zip'
Compress-Archive -LiteralPath (Join-Path $output 'main.dll'), $pak, $licensePath -DestinationPath $archive
Write-Output $output
