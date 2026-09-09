# Cook verified presentation assets and package only our own files; this never deploys to the game.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$VerificationDirectory, [string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$verified = Get-Content -LiteralPath (Join-Path $VerificationDirectory 'verification.json') -Raw | ConvertFrom-Json
if (!$verified.Success -or !$verified.Validation.success) { throw 'Successful runtime asset verification is required.' }
foreach ($asset in $verified.Assets) {
    if ((Get-FileHash -LiteralPath $asset.Path).Hash -ne $asset.Hash) { throw "Asset changed since verification: $($asset.Path)" }
}
$environment = & (Join-Path $PSScriptRoot 'Test-RenderingEnvironment.ps1') -EngineRoot $EngineRoot
if (!$environment.CompatibleEditors.Count) { throw 'UE4.27.2 editor is missing.' }
if (!$EngineRoot) { $EngineRoot = $environment.CompatibleEditors[0].Root }
$evidence = Join-Path $projectRoot ('agent\codex\presentation-cook-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$project = Join-Path $projectRoot 'engine\FSD\FSD.uproject'
$content = Join-Path $projectRoot 'engine\FSD\Content\EnemyWaveIndicator'
$output = Join-Path $evidence 'Cooked'
& $editor $project -run=Cook -TargetPlatform=WindowsNoEditor "-CookDir=$content" "-OutputDir=$output" -NoDefaultMaps -NoGameAlwaysCook -unversioned -unattended -nullrhi -nosplash -nosound -nocrashreports "-abslog=$evidence\cook.log" *> (Join-Path $evidence 'cook-console.txt')
$cookCode = $LASTEXITCODE
$cookLog = Get-Content -LiteralPath (Join-Path $evidence 'cook.log') -Raw
if ($cookCode -ne 0 -or $cookLog -match 'Log\w+: Error:') { throw "Cook failed. See $evidence" }
$cookedContent = Join-Path $output 'FSD\Content\EnemyWaveIndicator'
if (!(Test-Path -LiteralPath $cookedContent)) { throw "Expected cooked output not found: $cookedContent" }
# A standalone mod must not depend on a replacement FSD shared shader library.
if (Get-ChildItem -LiteralPath (Join-Path $output 'FSD\Content') -Filter 'ShaderArchive-FSD-*.ushaderbytecode' -File) {
    throw 'Custom material shaders were externalized; disable bShareMaterialShaderCode and recook.'
}
$files = @(Get-ChildItem -LiteralPath $cookedContent -File | Where-Object { $_.BaseName -ne 'BP_NwiVisualTest' })
$allowed = @('BP_NwiPulse.uasset', 'BP_NwiPulse.uexp', 'WBP_NwiMarker.uasset', 'WBP_NwiMarker.uexp', 'BP_NwiResources.uasset', 'BP_NwiResources.uexp', 'BP_NwiAuto.uasset', 'BP_NwiAuto.uexp', 'M_NwiRedPulse.uasset', 'M_NwiRedPulse.uexp')
$allowed += @('InitCave.uasset','InitCave.uexp','InitSpacerig.uasset','InitSpacerig.uexp')
$allowed += @('SG_NwiSettings.uasset','SG_NwiSettings.uexp','WBP_NwiSettings.uasset','WBP_NwiSettings.uexp')
if ($files.Count -ne $allowed.Count) { throw 'Unexpected cooked presentation file count.' }
foreach ($file in $files) { if ($file.Name -notin $allowed) { throw "Unexpected package entry: $($file.Name)" } }
$node = (Get-Command node -ErrorAction SilentlyContinue).Source
if (!$node) { $node = Join-Path $env:LOCALAPPDATA 'hermes\node\node.exe' }
$dependencyLog = Join-Path $evidence 'dependency-audit.json'
& $node (Join-Path $PSScriptRoot 'Test-PakDependencies.cjs') $cookedContent > $dependencyLog
if ($LASTEXITCODE -ne 0) { throw 'Cooked dependency audit failed.' }
$dependencyAudit = Get-Content -LiteralPath $dependencyLog -Raw | ConvertFrom-Json
if (!$dependencyAudit.passed -or $dependencyAudit.assets -ne 9) { throw 'Incomplete dependency audit.' }
$response = Join-Path $evidence 'pak-response.txt'
$lines = @($files | ForEach-Object { '"' + $_.FullName + '" "../../../FSD/Content/EnemyWaveIndicator/' + $_.Name + '"' })
$lines | Set-Content -LiteralPath $response -Encoding utf8
$unrealPak = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealPak.exe'
$pak = Join-Path $evidence 'EnemyWavePresentation-assets-only.pak'
& $unrealPak $pak "-Create=$response" -compress *> (Join-Path $evidence 'pak-create.log')
if ($LASTEXITCODE -ne 0) { throw "Pak creation failed. See $evidence" }
# UE4.27 -Test opens the index only; -Verify also checks every stored entry hash.
& $unrealPak $pak -Verify *> (Join-Path $evidence 'pak-test.log')
if ($LASTEXITCODE -ne 0) { throw "Pak integrity test failed. See $evidence" }
& $unrealPak $pak -List *> (Join-Path $evidence 'pak-list.log')
if ($LASTEXITCODE -ne 0) { throw "Pak listing failed. See $evidence" }
[pscustomobject]@{ Success = $true; Verification = $VerificationDirectory; CookExitCode = $cookCode;
    Pak = Get-FileHash -LiteralPath $pak; Files = @($files | Get-FileHash); AssetsOnly = $true;
    ContainsGameAssetCopies = $false; InlineMaterialShaders = $true;
    PackagingConfig = Get-FileHash -LiteralPath (Join-Path $projectRoot 'engine\FSD\Config\DefaultGame.ini');
    SeparateNativeBootstrapRequired = $false; ContentOnly = $false; RuntimeDllRequired = $true; Version = '0.9.2';
    DependencyAudit = Get-FileHash -LiteralPath $dependencyLog; PakHashesVerified = $true; GameDeployed = $false } |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'verification.json') -Encoding utf8
Write-Output $evidence

