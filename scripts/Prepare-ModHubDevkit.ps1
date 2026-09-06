# Fetch pinned editor-only interface references. They are excluded from Git and from the shipped Pak.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root 'engine\FSD\Content\_ModHub'
$revision = 'd6ff9fac56abf15937a01824332aa970232768f6'
$hashes = @{
    IHub = 'C1AB428E26A86336B62D6CAB88DF85BA361863BE688D1E41FB3D8169FF75E069'
    IHubMod = 'F5BDEEB6A9D90D06BCE78CAED4E6F156101D22191C885B5D2965980CD776E7ED'
    IHubPageWidget = '77409B31C25966FEC0CC0E4A334AFC4EB172BB40542BAC3D7436ADA133911A41'
}
New-Item -ItemType Directory -Force $destination | Out-Null
foreach ($name in $hashes.Keys) {
    $path = Join-Path $destination ($name + '.uasset')
    if (Test-Path -LiteralPath $path) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$name]) { throw "Existing interface differs: $path" }
        continue
    }
    Invoke-WebRequest "https://raw.githubusercontent.com/trumank/drg-mods/$revision/Content/_ModHub/$name.uasset" -OutFile $path
    if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$name]) { throw "Downloaded interface hash mismatch: $name" }
}
Write-Output $destination
