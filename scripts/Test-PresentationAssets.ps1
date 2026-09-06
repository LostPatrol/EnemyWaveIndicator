# Install the developer-only helper into the authoring project, generate, then cold-validate assets.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$AuthoringBuild, [string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$environment = & (Join-Path $PSScriptRoot 'Test-RenderingEnvironment.ps1') -EngineRoot $EngineRoot
if (!$environment.CompatibleEditors.Count) { throw 'UE4.27.2 editor is missing.' }
if (!$EngineRoot) { $EngineRoot = $environment.CompatibleEditors[0].Root }
$buildInfo = Get-Content -LiteralPath ($AuthoringBuild + '.json') -Raw | ConvertFrom-Json
if ($buildInfo.ExitCode -ne 0) { throw 'The supplied editor module did not build successfully.' }
if (!$buildInfo.SourceFiles) { throw 'Rebuild the authoring module to record its source hashes.' }
foreach ($file in $buildInfo.SourceFiles) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw "Authoring source changed after build: $($file.Path)" }
}
$destination = Join-Path $projectRoot 'engine\FSD\Plugins\NwiAuthoring'
foreach ($folder in @('Binaries', 'Source')) {
    $sourceFolder = Join-Path $AuthoringBuild $folder
    foreach ($file in Get-ChildItem -LiteralPath $sourceFolder -Recurse -File) {
        $relative = $file.FullName.Substring($AuthoringBuild.TrimEnd('\').Length + 1)
        $target = Join-Path $destination $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    }
}
Copy-Item -LiteralPath (Join-Path $AuthoringBuild 'NwiAuthoring.uplugin') -Destination (Join-Path $destination 'NwiAuthoring.uplugin') -Force
$evidence = Join-Path $projectRoot ('agent\codex\presentation-check-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$project = Join-Path $projectRoot 'engine\FSD\FSD.uproject'
$entry = Join-Path $projectRoot 'engine\FSD\Content\Python\commandlet_entry.py'
$common = @($project, '-run=pythonscript', "-script=$entry", '-unattended', '-nullrhi', '-nosplash', '-nosound', '-nocrashreports')
# Separate processes are intentional: validation must load the serialized assets from disk.
& $editor @common -NwiAuthorAssets "-abslog=$evidence\generate.log" *> (Join-Path $evidence 'generate-console.txt')
$generateCode = $LASTEXITCODE
$generateLog = Get-Content -LiteralPath (Join-Path $evidence 'generate.log') -Raw
if ($generateCode -ne 0 -or $generateLog -notmatch 'NWI_AUTHORING_SUCCESS' -or $generateLog -match 'Log\w+: Error:') {
    throw "Asset generation failed. See $evidence"
}
$resultFile = Join-Path $evidence 'validation.json'
& $editor @common -NwiValidateAssets "-NwiValidationResult=$resultFile" "-abslog=$evidence\validate.log" *> (Join-Path $evidence 'validate-console.txt')
$validationCode = $LASTEXITCODE
if ($validationCode -ne 0 -or !(Test-Path -LiteralPath $resultFile)) { throw "Validation process failed. See $evidence" }
$result = Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
$validationLog = Get-Content -LiteralPath (Join-Path $evidence 'validate.log') -Raw
if (!$result.success -or $validationLog -match 'Log\w+: Error:') { throw "Blueprint runtime validation failed. See $evidence" }
$assets = @(Get-ChildItem (Join-Path $projectRoot 'engine\FSD\Content\NormalWaveIndicator') -Filter '*.uasset' -File | Get-FileHash)
[pscustomobject]@{ Success = $true; AuthoringBuild = $AuthoringBuild; SourceFiles = @(Get-ChildItem (Join-Path $projectRoot 'engine\Authoring\NwiAuthoring') -Recurse -File | Get-FileHash); Assets = $assets; Validation = $result; GameDeployed = $false } |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'verification.json') -Encoding utf8
Write-Output $evidence
