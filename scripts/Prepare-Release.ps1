# Assemble only original runtime payloads and licenses after native and Blueprint verification.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory,[Parameter(Mandatory)][string]$PresentationCook)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$native = Get-Content -LiteralPath (Join-Path $BuildDirectory 'verification.json') -Raw | ConvertFrom-Json
$cook = Get-Content -LiteralPath (Join-Path $PresentationCook 'verification.json') -Raw | ConvertFrom-Json
$assets = Get-Content -LiteralPath (Join-Path $cook.Verification 'verification.json') -Raw | ConvertFrom-Json
if (!$native.OfflinePassed -or !$cook.Success -or !$assets.Validation.settings_test -or !$cook.InlineMaterialShaders -or $cook.Files.Count -ne 16) { throw 'Missing complete release validation.' }
$dll = Join-Path $BuildDirectory 'main.dll'; $pak = Join-Path $PresentationCook 'NormalWavePresentation-assets-only.pak'
if ((Get-FileHash -LiteralPath $dll).Hash -ne $native.SHA256 -or (Get-FileHash -LiteralPath $pak).Hash -ne $cook.Pak.Hash) { throw 'Build payload changed.' }
$output = Join-Path $root ('agent\codex\release-' + $native.ProbeVersion + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force $output | Out-Null
Copy-Item -LiteralPath $dll -Destination $output
Copy-Item -LiteralPath $pak -Destination (Join-Path $output 'NormalWaveIndicator_P.pak')
Copy-Item -LiteralPath (Join-Path $root 'scripts\Install-Release.ps1') -Destination (Join-Path $output 'Install.ps1')
Copy-Item -LiteralPath (Join-Path $root 'README.md'),(Join-Path $root 'LICENSE') -Destination $output
New-Item -ItemType Directory -Path (Join-Path $output 'docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'docs\RELEASE.md') -Destination (Join-Path $output 'docs\RELEASE.md')
Copy-Item -LiteralPath (Join-Path $root 'third_party\MinHook\LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
@{
    Version=$native.ProbeVersion; Status='alpha; new Mod Hub UI requires in-game testing'
    GameSHA256='9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13'
    RuntimeSHA256='D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7'
    Files=@{'main.dll'=$native.SHA256; 'NormalWaveIndicator_P.pak'=$cook.Pak.Hash}
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
Compress-Archive -Path (Join-Path $output '*') -DestinationPath ($output + '.zip')
Write-Output $output
