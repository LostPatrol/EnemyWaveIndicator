# Prepare or publish this project's existing mod.io listing without browser automation.
#Requires -Version 7.0
[CmdletBinding(DefaultParameterSetName = 'Prepare')]
param(
    [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version,
    [string]$ArchivePath,
    [string]$ChangelogPath,
    [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedSHA256,
    [string]$TokenPath,
    [string]$EvidenceDirectory,
    [ValidatePattern('^https://(?:api\.mod\.io|[ug]-\d+\.modapi\.io)/v1/?$')]
    [string]$ApiBase = 'https://api.mod.io/v1',
    [ValidateRange(0, 3600)][int]$WaitSeconds = 60,
    [Parameter(Mandatory, ParameterSetName = 'Publish')][switch]$Publish,
    [Parameter(Mandatory, ParameterSetName = 'Check')][switch]$CheckRemote
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent # All default inputs and evidence belong to this project.
$gameSlug = 'drg' # Existing Deep Rock Galactic listing.
$modSlug = 'enemy-wave-indicator' # Never create a second mod or update another listing.
$profileUrl = "https://mod.io/g/$gameSlug/m/$modSlug" # Intended public destination.
$ApiBase = $ApiBase.TrimEnd('/')
if (!$ArchivePath) { $ArchivePath = Join-Path $root "dist\EnemyWaveIndicator-$Version.zip" }
if (!$ChangelogPath) { $ChangelogPath = Join-Path $root "docs\changelog\$Version.md" }
if (!$EvidenceDirectory) { $EvidenceDirectory = Join-Path $root "agent\codex\modio-publish-$Version" }
$archive = Get-Item -LiteralPath $ArchivePath
$notesFile = Get-Item -LiteralPath $ChangelogPath
if ($archive.Extension -ine '.zip') { throw 'The release archive must be a ZIP.' }
$notes = [IO.File]::ReadAllText($notesFile.FullName)
if ($notes -notmatch ('(?m)^# Enemy Wave Indicator ' + [regex]::Escape($Version) + '\s*$')) {
    throw 'The changelog heading does not match the requested version.'
}
$html = (ConvertFrom-Markdown -InputObject $notes).Html
$sha256 = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash
$md5 = (Get-FileHash -LiteralPath $archive.FullName -Algorithm MD5).Hash.ToLowerInvariant()
if ($ExpectedSHA256 -and $sha256 -ine $ExpectedSHA256) { throw 'Release archive SHA256 does not match ExpectedSHA256.' }
$zip = [IO.Compression.ZipFile]::OpenRead($archive.FullName)
try {
    $expected = @('main.dll', 'EnemyWaveIndicator_P.pak', 'LICENSES.txt')
    if (@(Compare-Object $expected @($zip.Entries.FullName)).Count -or @($zip.Entries | Where-Object Length -eq 0).Count) {
        throw 'ZIP must contain only nonempty main.dll, EnemyWaveIndicator_P.pak and LICENSES.txt at its root.'
    }
} finally { $zip.Dispose() }
$result = [ordered]@{
    Status='Prepared'; Version=$Version; ArchivePath=$archive.FullName; Bytes=$archive.Length
    SHA256=$sha256; MD5=$md5; ChangelogPath=$notesFile.FullName; ChangelogFormat='html'
    ProfileUrl=$profileUrl; Uploaded=$false; Activated=$false; Live=$false; DownloadVerified=$false
}
if (!$Publish -and !$CheckRemote) { return [pscustomobject]$result }

$accessToken = if ($TokenPath) { [IO.File]::ReadAllText((Resolve-Path -LiteralPath $TokenPath).Path).Trim() } else { $env:MODIO_TOKEN }
if ([string]::IsNullOrWhiteSpace($accessToken)) { throw 'Provide MODIO_TOKEN or -TokenPath with a read+write access token.' }
$headers = @{ Authorization="Bearer $accessToken"; Accept='application/json'; 'User-Agent'='EnemyWaveIndicator-Publisher' }

# Keep credentials in headers and report only status/error reference, never request objects.
function Invoke-ModioRequest([string]$Method, [string]$Path, [hashtable]$Form, [hashtable]$Body, [switch]$Windows) {
    $requestHeaders = $headers.Clone()
    if ($Windows) { $requestHeaders['X-Modio-Platform'] = 'windows' }
    $arguments = @{Method=$Method; Uri=($ApiBase + $Path); Headers=$requestHeaders; TimeoutSec=120; ErrorAction='Stop'; Verbose=$false; Debug=$false}
    if ($Form) { $arguments.Form = $Form }
    if ($Body) { $arguments.Body = $Body; $arguments.ContentType = 'application/x-www-form-urlencoded; charset=utf-8' }
    try { Invoke-RestMethod @arguments }
    catch {
        $status = if ($_.Exception.Response) { [int]$_.Exception.Response.StatusCode } else { 'network error' }
        $reference = ''
        try { $reference = [string](($_.ErrorDetails.Message | ConvertFrom-Json).error.error_ref) } catch { }
        throw "mod.io $Method failed ($status; error_ref=$reference). Check account permissions/token or connection; no automatic write retry was sent."
    }
}

# Require the exact bytes/version before activating a newly created or reused file.
function Assert-Modfile($File) {
    if (!$File.id -or $File.version -cne $Version -or $File.filehash.md5 -ine $md5 -or [long]$File.filesize -ne $archive.Length) {
        throw 'Remote modfile version, size or MD5 differs from the selected release archive.'
    }
}

# Compare the authored wording while allowing the service to sanitize HTML formatting.
function Get-ChangelogText([string]$Value) {
    $withoutComments = [regex]::Replace($Value, '<!--[\s\S]*?-->', '')
    $plain = [Net.WebUtility]::HtmlDecode([regex]::Replace($withoutComments, '<[^>]+>', ' '))
    [regex]::Replace($plain, '\s+', ' ').Trim()
}

# Save only resumable publication metadata, never tokens or signed download URLs.
function Save-PublicationState([string]$Stage, [long]$FileId) {
    New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
    [ordered]@{Version=$Version; GameId=$game.id; ModId=$mod.id; MD5=$md5; SHA256=$sha256; Stage=$Stage; FileId=$FileId} |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
}

$me = Invoke-ModioRequest GET '/me'
$games = Invoke-ModioRequest GET ("/games?name_id=$gameSlug")
$gameMatches = @($games.data | Where-Object name_id -CEQ $gameSlug)
if ($gameMatches.Count -ne 1) { throw 'Could not uniquely resolve the Deep Rock Galactic game ID.' }
$game = $gameMatches[0]
$gamePlatform = @($game.platforms | Where-Object { $_.platform -ieq 'windows' })
$platformModerated = $gamePlatform.Count -gt 0 -and $gamePlatform[0].moderated # Reviewed platforms cannot go live before approval.
$mods = Invoke-ModioRequest GET ("/games/$($game.id)/mods?name_id=$modSlug")
$modMatches = @($mods.data | Where-Object { $_.name_id -ceq $modSlug -and $_.game_id -eq $game.id })
if ($modMatches.Count -ne 1) { throw 'Could not uniquely resolve the existing Enemy Wave Indicator mod.' }
$mod = $modMatches[0]
if (!$me.id) { throw 'The access token did not identify an authenticated user.' }
if ($Publish -and $mod.submitted_by.id -ne $me.id) {
    $team = Invoke-ModioRequest GET ("/games/$($game.id)/mods/$($mod.id)/team?_limit=100")
    $authorized = @($team.data | Where-Object { $_.user.id -eq $me.id -and $_.invite_pending -eq 0 -and $_.level -in 4,8 })
    if (!$authorized.Count) { throw 'This account is not the mod owner or an accepted team manager/administrator.' }
}
$result.GameId = $game.id; $result.ModId = $mod.id
$basePath = "/games/$($game.id)/mods/$($mod.id)"
$statePath = Join-Path $EvidenceDirectory 'publication.json'
$state = if (Test-Path -LiteralPath $statePath) { Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json } else { $null }
if ($state -and ($state.Version -cne $Version -or $state.MD5 -ine $md5 -or $state.GameId -ne $game.id -or $state.ModId -ne $mod.id)) {
    throw 'Existing publication evidence describes a different release; inspect it or select another EvidenceDirectory.'
}
$file = $null
if ($state.FileId) {
    $file = Invoke-ModioRequest GET ("$basePath/files/$($state.FileId)")
} else {
    $files = Invoke-ModioRequest GET ("$basePath/files?version=$([Uri]::EscapeDataString($Version))&_limit=100")
    $sameVersion = @($files.data | Where-Object version -CEQ $Version)
    $sameBytes = @($sameVersion | Where-Object { $_.filehash.md5 -ieq $md5 -and [long]$_.filesize -eq $archive.Length })
    if ($sameBytes.Count) { $file = $sameBytes | Sort-Object id -Descending | Select-Object -First 1 }
    elseif ($sameVersion.Count) { throw 'This version already exists with different bytes; do not overwrite or silently upload a duplicate.' }
}
if (!$file -and !$Publish) { $result.Status='ReadyToUpload'; return [pscustomobject]$result }
if (!$file) {
    if ($state.Stage -eq 'Uploading') { throw 'Previous upload outcome is unknown. Inspect mod.io or use -CheckRemote before retrying; a second upload was not sent.' }
    Save-PublicationState 'Uploading' 0
    $form = @{filedata=$archive; version=$Version; changelog=$html; filehash=$md5; active='false'; 'platforms[]'='windows'}
    $file = Invoke-ModioRequest POST "$basePath/files" -Form $form
    if (!$file.id) { throw 'Upload response did not include a file ID; its outcome is unknown. Inspect mod.io before retrying.' }
    Save-PublicationState 'Uploaded' $file.id
    $result.Uploaded = $true
}
Assert-Modfile $file
$result.FileId = $file.id
$result.ChangelogMatches = (Get-ChangelogText $file.changelog) -ceq (Get-ChangelogText $html)
if ($Publish -and !$result.ChangelogMatches) {
    $file = Invoke-ModioRequest PUT ("$basePath/files/$($file.id)") -Body @{changelog=$html}
    Assert-Modfile $file
    $result.ChangelogMatches = (Get-ChangelogText $file.changelog) -ceq (Get-ChangelogText $html)
    if (!$result.ChangelogMatches) { throw 'Remote changelog wording differs from the authored release notes.' }
}
if ($Publish) { Save-PublicationState 'Uploaded' $file.id }
$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds) # Waiting bounds polling, not the validity of a pending upload.
do {
    $file = Invoke-ModioRequest GET ("$basePath/files/$($file.id)")
    Assert-Modfile $file
    $result.VirusStatus = $file.virus_status; $result.VirusPositive = $file.virus_positive
    if ($file.virus_positive -eq 1) { throw 'mod.io flagged the file as malicious. It was not activated by this run.' }
    $platform = @($file.platforms | Where-Object { $_.platform -ieq 'windows' })
    if (@($platform | Where-Object status -eq 2).Count) { throw 'The Windows file was denied by mod.io; it was not activated by this run.' }
    $scanComplete = $null -ne $file.virus_status -and $file.virus_status -eq 1
    $liveMod = Invoke-ModioRequest GET $basePath -Windows
    $livePlatform = @($liveMod.platforms | Where-Object { $_.platform -ieq 'windows' })
    $result.Live = $liveMod.modfile.id -eq $file.id -and (!$livePlatform.Count -or $livePlatform[0].modfile_live -eq $file.id)
    $platformReady = !$platformModerated -or @($platform | Where-Object status -eq 1).Count -gt 0
    if ($scanComplete -and $platformReady -and $Publish -and !$result.Live -and !$result.Activated) {
        $file = Invoke-ModioRequest PUT ("$basePath/files/$($file.id)") -Body @{active='true'}
        Assert-Modfile $file
        $result.Activated = $true
        Save-PublicationState 'Activated' $file.id
        $liveMod = Invoke-ModioRequest GET $basePath -Windows
        $livePlatform = @($liveMod.platforms | Where-Object { $_.platform -ieq 'windows' })
        $result.Live = $liveMod.modfile.id -eq $file.id -and (!$livePlatform.Count -or $livePlatform[0].modfile_live -eq $file.id)
    }
    if ($result.Live -and $scanComplete) { break }
    if (!$Publish -or [DateTime]::UtcNow -ge $deadline) { break }
    Start-Sleep -Seconds ([Math]::Min(5, [Math]::Max(1, [int]($deadline - [DateTime]::UtcNow).TotalSeconds)))
} while ($true)
if ($file.virus_positive -eq 2) {
    Write-Warning 'mod.io marked the archive as potentially harmful (for example, native code). This is not the same as a confirmed malicious detection; review its scan result.'
}
if ($result.Live -and $scanComplete) {
    $downloadUrl = [Uri]$file.download.binary_url
    if (!$downloadUrl.IsAbsoluteUri -or $downloadUrl.Scheme -ne 'https') { throw 'The live modfile has no HTTPS download URL.' }
    New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
    $downloadPath = Join-Path $EvidenceDirectory "download-$($file.id).zip"
    # Signed CDN URLs must never receive the account's bearer token.
    try { Invoke-WebRequest -Uri $downloadUrl.AbsoluteUri -OutFile $downloadPath -TimeoutSec 120 -Verbose:$false -Debug:$false -ErrorAction Stop | Out-Null }
    catch { throw 'The live modfile could not be downloaded for verification; publication may already be live. Re-run -CheckRemote.' }
    if ((Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash -ine $sha256 -or
        (Get-FileHash -LiteralPath $downloadPath -Algorithm MD5).Hash -ine $md5) { throw 'Downloaded release bytes differ from the local archive.' }
    $result.DownloadVerified = $true
    $result.Status = if ($result.ChangelogMatches) { 'Published' } else { 'ChangelogMismatch' }
} else {
    $result.Status = if ($file.virus_status -in 3,4,5) { 'ScanUnavailable' } elseif (!$scanComplete) { 'PendingScan' } elseif ($Publish) { 'PendingPlatformApproval' } else { 'ReadyToActivate' }
}
if ($Publish) { Save-PublicationState $result.Status $file.id }
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'verification.json') -Encoding utf8
[pscustomobject]$result
