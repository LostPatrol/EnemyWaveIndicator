# Snapshot runtime, native game and native probe logs before the next launch; extract JS events.
[CmdletBinding()]
param(
    [string]$GameRoot = 'D:\Steam\steamapps\common\Deep Rock Galactic'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceLog = Join-Path $GameRoot 'FSD\Binaries\Win64\ue4ss\UE4SS.log'
if (!(Test-Path -LiteralPath $sourceLog)) { throw 'UE4SS.log not found.' }
$outputDir = Join-Path $projectRoot ('agent\codex\playtest-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $outputDir | Out-Null
$snapshot = Join-Path $outputDir 'UE4SS.log'
Copy-Item -LiteralPath $sourceLog -Destination $snapshot
# Preserve the native category log alongside script events before another launch rotates it.
$nativeLog = Join-Path $GameRoot 'FSD\Saved\Logs\FSD.log'
if (Test-Path -LiteralPath $nativeLog) { Copy-Item -LiteralPath $nativeLog -Destination (Join-Path $outputDir 'FSD.log') }
# Probe logs use unique run names; preserve them verbatim, separate from synthetic build evidence.
$probeRoot = Join-Path $GameRoot 'FSD\Binaries\Win64\ue4ss\mods\NormalWaveIndicator'
if (!(Test-Path -LiteralPath $probeRoot)) { $probeRoot = Join-Path $GameRoot 'FSD\Binaries\Win64\ue4ss\mods\NormalWaveNativeProbe' }
if (Test-Path -LiteralPath $probeRoot) {
    $probeLogs = @(Get-ChildItem -LiteralPath $probeRoot -Filter 'probe-*.jsonl' -File)
    if ($probeLogs.Count -gt 0) {
        $probeSnapshot = Join-Path $outputDir 'native-probe'
        New-Item -ItemType Directory -Path $probeSnapshot | Out-Null
        $probeLogs | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $probeSnapshot }
    }
}
$events = @()
$malformed = 0
foreach ($line in Get-Content -LiteralPath $snapshot) {
    $index = $line.IndexOf('[NWDiag] ')
    if ($index -lt 0) { continue }
    try { $events += $line.Substring($index + 9) | ConvertFrom-Json }
    catch { $malformed++ }
}
@($events | ForEach-Object { $_ | ConvertTo-Json -Depth 12 -Compress }) | Set-Content -LiteralPath (Join-Path $outputDir 'events.jsonl') -Encoding utf8
$counts = $events | Group-Object kind | Sort-Object Name | ForEach-Object { '- ' + $_.Name + ': ' + $_.Count }
@(
    '<!-- Diagnostic event counts; these do not establish successful Normal Wave identification. -->'
    '# Playtest log snapshot'
    ''
    ('Captured: ' + (Get-Date -Format o))
    ('Parsed events: ' + $events.Count)
    ('Malformed lines: ' + $malformed)
    ''
    $counts
    ''
    'This is a diagnostic log summary, not evidence that Normal Wave detection works.'
) | Set-Content -LiteralPath (Join-Path $outputDir 'summary.md') -Encoding utf8
if ($events.Count -eq 0 -and !(Test-Path -LiteralPath (Join-Path $outputDir 'native-probe'))) { Write-Warning 'No native or legacy diagnostic records found.' }
Write-Output $outputDir
