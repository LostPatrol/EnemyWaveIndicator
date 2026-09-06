# Inspect tools for a cooked UE4.27 presentation layer; this script installs or launches nothing.
[CmdletBinding()]
param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = $null
if (Test-Path -LiteralPath $vswhere) {
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$candidates = @($EngineRoot, 'E:\EpicGames\UE_4.27', 'E:\Epic Games\UE_4.27', 'C:\Program Files\Epic Games\UE_4.27')
# Read only engine installation locations from Epic's manifests and engine registration.
$manifestRoot = Join-Path $env:ProgramData 'Epic\EpicGamesLauncher\Data\Manifests'
if (Test-Path -LiteralPath $manifestRoot) {
    foreach ($file in Get-ChildItem -LiteralPath $manifestRoot -Filter '*.item' -File) {
        $manifest = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($manifest.AppName -eq 'UE_4.27') { $candidates += $manifest.InstallLocation }
    }
}
$registration = Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\4.27' -ErrorAction SilentlyContinue
if ($registration) { $candidates += $registration.InstalledDirectory }
$engines = @()
foreach ($candidate in @($candidates | Where-Object { $_ } | Select-Object -Unique)) {
    $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
    $editor = Join-Path $candidate 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
    if ((Test-Path -LiteralPath $versionFile) -and (Test-Path -LiteralPath $editor)) {
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        if ($version.MajorVersion -eq 4 -and $version.MinorVersion -eq 27 -and $version.PatchVersion -eq 2) {
            $engines += [pscustomobject]@{ Root = $candidate; Editor = $editor; Version = '4.27.2' }
        }
    }
}
# UE's editor-module build also queries NETFXSDK; the modern .NET runtime is not that SDK.
$netFxSdks = @()
foreach ($registryRoot in @('HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\NETFXSDK', 'HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\NETFXSDK')) {
    if (Test-Path $registryRoot) {
        foreach ($key in Get-ChildItem $registryRoot) {
            $entry = Get-ItemProperty $key.PSPath
            if ($entry.KitsInstallationFolder -and (Test-Path -LiteralPath $entry.KitsInstallationFolder)) {
                $netFxSdks += [pscustomobject]@{ Version = $key.PSChildName; Root = $entry.KitsInstallationFolder }
            }
        }
    }
}
[pscustomobject]@{
    VisualStudioCpp = $vsPath
    CompatibleEditors = $engines
    ReadyForBlueprintAuthoringAndCooking = [bool]($engines.Count -gt 0)
    MissingForSelectedRoute = @($(if (!$engines.Count) { 'Unreal Engine 4.27.2 editor with Windows support' }))
    NetFrameworkSdks = $netFxSdks
    MissingForEditorAuthoringHelper = @($(if (!$netFxSdks.Count) { '.NET Framework 4.8 SDK (NETFXSDK); optional matching targeting pack' }))
    EditorAuthoringHelperBuildVerified = $null # Prerequisite inspection alone is not a compilation test; see build reports.
    NativeSpawnAttributionVerified = $false
    FramePresentationVerified = $false
    Note = 'Editor availability does not prove correct spawn attribution, thread safety or acceptable CPU/GPU cost.'
}
