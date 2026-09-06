# Launch DRG with buffered file logs; per-line blocking flush is opt-in for exceptional legacy diagnostics.
[CmdletBinding()]
param([switch]$Preview, [switch]$SynchronousLogging)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'FSD-Win64-Shipping', 'FSD' -ErrorAction SilentlyContinue) {
    throw 'Exit Deep Rock Galactic before starting the native log test.'
}
$steamExe = (Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -Name SteamExe).SteamExe
if (!(Test-Path -LiteralPath $steamExe -PathType Leaf)) { throw 'Steam executable not found.' }
# Keep a collectable file log without making the game thread wait for the writer after each line.
# The native 0.5.x observer reads engine events/queue state and does not depend on log delivery.
# These arguments apply to this launch. They do not edit saved Steam launch options.
$launchArgs = @('-applaunch', '548430', '-NOLOGTOMEMORY')
if ($SynchronousLogging) { $launchArgs += '-FORCELOGFLUSH' }
if ($Preview) {
    [pscustomobject]@{ SteamExe = $steamExe; Arguments = ($launchArgs -join ' '); SynchronousLogging = [bool]$SynchronousLogging; Preview = $true }
    return
}
Start-Process -FilePath $steamExe -ArgumentList $launchArgs -WorkingDirectory (Split-Path -Parent $steamExe) -WindowStyle Hidden
Write-Output ('Steam launch requested. Synchronous per-line logging: ' + [bool]$SynchronousLogging)
