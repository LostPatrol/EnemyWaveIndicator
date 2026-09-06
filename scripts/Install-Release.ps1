# Install only this mod's manifest-verified payload; preserve the loader, other mods and rollback copies.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$GameRoot)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'FSD-Win64-Shipping','FSD' -ErrorAction SilentlyContinue) { throw 'Exit Deep Rock Galactic first.' }
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach ($name in @('main.dll','NormalWaveIndicator_P.pak')) {
    if ((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash -ne $manifest.Files.$name) { throw "Payload hash mismatch: $name" }
}
$game = [IO.Path]::GetFullPath($GameRoot).TrimEnd('\')
$runtime = Join-Path $game 'FSD\Binaries\Win64\ue4ss'
if ((Get-FileHash -LiteralPath (Join-Path $game 'FSD\Binaries\Win64\FSD-Win64-Shipping.exe')).Hash -ne $manifest.GameSHA256) { throw 'Unsupported game executable. Use a compatible release.' }
if ((Get-FileHash -LiteralPath (Join-Path $runtime 'UE4SSL.dll')).Hash -ne $manifest.RuntimeSHA256) { throw 'Unsupported or missing UE4SSL runtime. Do not overwrite your loader.' }
$mods = [IO.Path]::GetFullPath((Join-Path $runtime 'mods')).TrimEnd('\') + '\'
$paks = Join-Path $game 'FSD\Content\Paks'
$backup = Join-Path $game ('NWI-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force $backup | Out-Null
$oldPaths = @()
foreach ($name in @('NormalWaveIndicator','NormalWaveNativeProbe','NormalWaveDiagnostic')) {
    $path = [IO.Path]::GetFullPath((Join-Path $mods $name))
    if (!$path.StartsWith($mods, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid mod path.' }
    if (Test-Path -LiteralPath $path) {
        if ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing a mod directory junction.' }
        Copy-Item -LiteralPath $path -Destination (Join-Path $backup $name) -Recurse
        $oldPaths += $path
    }
}
foreach ($name in @('NormalWavePresentation_P.pak','NormalWaveIndicator_P.pak')) {
    $path = Join-Path $paks $name
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $backup; $oldPaths += $path }
}
# All backups finish before mutations; on any failure restore those exact own-mod paths.
$destination = Join-Path $mods 'NormalWaveIndicator'
$pak = Join-Path $paks 'NormalWaveIndicator_P.pak'
try {
    foreach ($path in $oldPaths) { Remove-Item -LiteralPath $path -Recurse -Force }
    New-Item -ItemType Directory -Force $destination | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'main.dll') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MinHook-LICENSE.txt') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'NormalWaveIndicator_P.pak') -Destination $pak
    if ((Get-FileHash -LiteralPath (Join-Path $destination 'main.dll')).Hash -ne $manifest.Files.'main.dll' -or (Get-FileHash -LiteralPath $pak).Hash -ne $manifest.Files.'NormalWaveIndicator_P.pak') { throw 'Installed file verification failed.' }
} catch {
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
    if (Test-Path -LiteralPath $pak) { Remove-Item -LiteralPath $pak -Force }
    foreach ($path in $oldPaths) { Copy-Item -LiteralPath (Join-Path $backup (Split-Path $path -Leaf)) -Destination $path -Recurse -Force }
    throw
}
Write-Output "Installed Normal Wave Indicator $($manifest.Version). Backup: $backup"
Write-Output 'Launch through Steam, then H -> Mod Hub -> Normal Wave Indicator. New H-menu integration still needs gameplay validation.'
