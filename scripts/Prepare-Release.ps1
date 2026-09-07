# Compatibility entrypoint for the sole supported DLL + Pak MintCat distribution.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory,[Parameter(Mandatory)][string]$PresentationCook)
& (Join-Path $PSScriptRoot 'Prepare-ModioRelease.ps1') -BuildDirectory $BuildDirectory -PresentationCook $PresentationCook
