# Verify category setup/restore using a project-owned fake game configuration, never the real game.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $projectRoot ('agent\codex\native-config-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$ini = Join-Path $fixture 'FSD\Saved\Config\WindowsNoEditor\Engine.ini'
New-Item -ItemType Directory -Path (Split-Path -Parent $ini) -Force | Out-Null
$before = "[Core.System]`r`nPaths=KeepMe`r`n[Other]`r`nValue=42`r`n"
[IO.File]::WriteAllText($ini, $before)
$hash = (Get-FileHash $ini).Hash
$script = Join-Path $projectRoot 'scripts\Set-NativeWaveLogging.ps1'
$result = & $script -GameRoot $fixture
$after = Get-Content $ini -Raw
if (!$after.Contains('Paths=KeepMe') -or !$after.Contains('Value=42') -or !$after.Contains('FSDLog_Spawning=Log')) { throw 'Configuration setup failed.' }
& $script -GameRoot $fixture | Out-Null
if ((Get-Content $ini -Raw) -ne $after) { throw 'Setup is not idempotent.' }
[IO.File]::AppendAllText($ini, '; Later edit')
$rejected = $false
try { & $script -GameRoot $fixture -RestoreState $result.RestoreState | Out-Null }
catch { $rejected = $_.Exception.Message -like '*changed since setup*' }
if (!$rejected) { throw 'Restore overwrote later edits.' }
[IO.File]::WriteAllText($ini, $after)
& $script -GameRoot $fixture -RestoreState $result.RestoreState | Out-Null
if ((Get-FileHash $ini).Hash -ne $hash) { throw 'Restoration was not byte-exact.' }
'PASS: category setup, existing settings preservation, idempotence, later-edit rejection, exact restore.'
