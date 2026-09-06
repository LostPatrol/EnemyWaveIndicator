# Exercise the actual release installer against a private fake game tree; never launch or touch the real game.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $root ('agent\codex\release-installer-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$payload = Join-Path $testRoot 'payload'; $game = Join-Path $testRoot 'fake-game'
$runtime = Join-Path $game 'FSD\Binaries\Win64\ue4ss'; $mods = Join-Path $runtime 'mods'; $paks = Join-Path $game 'FSD\Content\Paks'
foreach ($path in @($payload,$paks,(Join-Path $mods 'NormalWaveDiagnostic\js'),(Join-Path $mods 'NormalWaveNativeProbe'),(Join-Path $mods 'UnrelatedMod'))) { New-Item -ItemType Directory -Force $path | Out-Null }
Copy-Item -LiteralPath (Join-Path $root 'scripts\Install-Release.ps1') -Destination (Join-Path $payload 'Install.ps1')
Set-Content (Join-Path $payload 'main.dll') 'test-native'; Set-Content (Join-Path $payload 'NormalWaveIndicator_P.pak') 'test-pak'; Set-Content (Join-Path $payload 'MinHook-LICENSE.txt') 'test-license'
Set-Content (Join-Path $game 'FSD\Binaries\Win64\FSD-Win64-Shipping.exe') 'test-game'
Set-Content (Join-Path $runtime 'UE4SSL.dll') 'test-loader'
Set-Content (Join-Path $mods 'NormalWaveDiagnostic\js\main.js') 'legacy-js'
Set-Content (Join-Path $mods 'NormalWaveNativeProbe\main.dll') 'legacy-native'
Set-Content (Join-Path $mods 'UnrelatedMod\keep.txt') 'keep'
Set-Content (Join-Path $paks 'NormalWavePresentation_P.pak') 'legacy-pak'
$manifest = @{
    Version='fixture'; GameSHA256=(Get-FileHash (Join-Path $game 'FSD\Binaries\Win64\FSD-Win64-Shipping.exe')).Hash
    RuntimeSHA256=(Get-FileHash (Join-Path $runtime 'UE4SSL.dll')).Hash
    Files=@{'main.dll'=(Get-FileHash (Join-Path $payload 'main.dll')).Hash;'NormalWaveIndicator_P.pak'=(Get-FileHash (Join-Path $payload 'NormalWaveIndicator_P.pak')).Hash}
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $payload 'manifest.json')
& (Join-Path $payload 'Install.ps1') -GameRoot $game | Out-Null
if ((Get-Content (Join-Path $mods 'UnrelatedMod\keep.txt')) -ne 'keep' -or (Test-Path (Join-Path $mods 'NormalWaveNativeProbe')) -or (Test-Path (Join-Path $mods 'NormalWaveDiagnostic')) -or (Test-Path (Join-Path $paks 'NormalWavePresentation_P.pak'))) { throw 'Legacy retirement or unrelated-file preservation failed.' }
if ((Get-FileHash (Join-Path $mods 'NormalWaveIndicator\main.dll')).Hash -ne $manifest.Files.'main.dll') { throw 'New DLL differs.' }
$backups = @(Get-ChildItem (Join-Path $game 'NWI-backups') -Directory)
if ($backups.Count -ne 1 -or (Get-Content (Join-Path $backups[0].FullName 'NormalWaveDiagnostic\js\main.js')) -ne 'legacy-js') { throw 'Rollback backup differs.' }
Set-Content (Join-Path $payload 'main.dll') 'tampered'
$rejected = $false
try { & (Join-Path $payload 'Install.ps1') -GameRoot $game | Out-Null } catch { $rejected = $_.Exception.Message -match 'Payload hash mismatch' }
if (!$rejected -or (Get-FileHash (Join-Path $mods 'NormalWaveIndicator\main.dll')).Hash -ne $manifest.Files.'main.dll') { throw 'Tampered payload was not rejected before mutation.' }
Write-Output 'PASS: exact new payload, legacy retirement, rollback backups, unrelated mod preserved, tampering rejected before mutation.'
