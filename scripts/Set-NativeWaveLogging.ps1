# Enable only the game's spawning log category; retain an exact backup and hash-guarded restore record.
[CmdletBinding()]
param(
    [string]$GameRoot = 'D:\Steam\steamapps\common\Deep Rock Galactic',
    [string]$RestoreState
)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'FSD-Win64-Shipping' -ErrorAction SilentlyContinue) { throw 'Close Deep Rock Galactic first.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$ini = [IO.Path]::GetFullPath((Join-Path $GameRoot 'FSD\Saved\Config\WindowsNoEditor\Engine.ini'))
if (!(Test-Path -LiteralPath $ini)) { throw 'Existing Engine.ini not found; no change made.' }
if ($RestoreState) {
    $state = Get-Content -LiteralPath $RestoreState -Raw | ConvertFrom-Json
    if ($state.EngineIni -ne $ini) { throw 'Restore record is for a different game configuration.' }
    if ((Get-FileHash -LiteralPath $ini).Hash -ne $state.InstalledHash) { throw 'Engine.ini changed since setup; refusing to overwrite later edits.' }
    if ((Get-FileHash -LiteralPath $state.Backup).Hash -ne $state.OriginalHash) { throw 'Backup hash mismatch.' }
    Copy-Item -LiteralPath $state.Backup -Destination $ini -Force
    Write-Output ('Restored exact prior configuration: ' + $ini)
    return
}
$original = [IO.File]::ReadAllText($ini)
$keyPattern = '(?im)^\s*FSDLog_Spawning\s*=\s*[^\r\n]*'
if ([regex]::Matches($original, $keyPattern).Count -gt 1) { throw 'Multiple spawning-log overrides found; no change made.' }
if ($original -match '(?im)^\s*FSDLog_Spawning\s*=\s*Log\s*$') { Write-Output 'FSDLog_Spawning is already set to Log.'; return }
$updated = if ($original -match $keyPattern) {
    [regex]::Replace($original, $keyPattern, 'FSDLog_Spawning=Log')
} else {
    $original.TrimEnd() + "`r`n`r`n; NormalWaveDiagnostic: expose existing native wave log messages only.`r`n[Core.Log]`r`nFSDLog_Spawning=Log`r`n"
}
$artifactRoot = Join-Path $projectRoot 'agent\codex'
New-Item -ItemType Directory -Path $artifactRoot -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$backup = Join-Path $artifactRoot ('Engine-before-native-log-' + $stamp + '.ini')
$record = Join-Path $artifactRoot ('native-log-settings-' + $stamp + '.json')
$originalHash = (Get-FileHash -LiteralPath $ini).Hash
Copy-Item -LiteralPath $ini -Destination $backup
[IO.File]::WriteAllText($ini, $updated, [Text.UTF8Encoding]::new($false))
$state = [pscustomobject]@{ EngineIni = $ini; Backup = $backup; OriginalHash = $originalHash; InstalledHash = (Get-FileHash -LiteralPath $ini).Hash }
$state | ConvertTo-Json | Set-Content -LiteralPath $record -Encoding utf8
[pscustomobject]@{ EngineIni = $ini; Backup = $backup; RestoreState = $record; Category = 'FSDLog_Spawning=Log' }
