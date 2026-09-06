# Compile only the developer-side UE asset generator, recording the actual UAT exit code.
[CmdletBinding()]
param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$environment = & (Join-Path $PSScriptRoot 'Test-RenderingEnvironment.ps1') -EngineRoot $EngineRoot
if (!$environment.CompatibleEditors.Count) { throw 'UE4.27.2 editor is missing.' }
if (!$environment.NetFrameworkSdks.Count) { throw 'Missing .NET Framework SDK (NETFXSDK). Install the .NET Framework 4.8 SDK in Visual Studio Installer.' }
if (!$EngineRoot) { $EngineRoot = $environment.CompatibleEditors[0].Root }
$buildRoot = Join-Path $projectRoot ('agent\codex\nwi-authoring-build-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$plugin = Join-Path $projectRoot 'engine\Authoring\NwiAuthoring\NwiAuthoring.uplugin'
$uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$logPath = $buildRoot + '.log'
& $uat BuildPlugin "-Plugin=$plugin" "-Package=$buildRoot" -TargetPlatforms=Win64 -VS2022 *> $logPath
$buildExitCode = $LASTEXITCODE
[pscustomobject]@{ ExitCode = $buildExitCode; Log = $logPath; Output = $buildRoot; DeployedToGame = $false } |
    ConvertTo-Json | Set-Content -LiteralPath ($buildRoot + '.json') -Encoding utf8
if ($buildExitCode -ne 0) { throw "Editor helper build failed with exit code $buildExitCode. See $logPath" }
Write-Output $buildRoot
