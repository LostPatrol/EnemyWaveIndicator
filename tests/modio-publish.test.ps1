# Verify mod.io preflight, activation and retry behavior using synthetic archives and HTTP substitutes.
# All fixtures remain inside a unique project-owned directory; no real token or API is used.
#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
# Publication entrypoint and isolated fixture directory used by every case.
$root = Split-Path $PSScriptRoot -Parent
$publisher = Join-Path $root 'scripts\Publish-ModioRelease.ps1'
$fixture = Join-Path $root ('agent\codex\modio-publish-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
# Global test state is shared with HTTP functions invoked from the publisher's child script scope.
# HTTP calls must remain zero throughout the local preflight cases.
$global:EwiModioPublishHttpCalls = 0
$script:cases = 0
# A null session makes all HTTP forbidden during local preflight tests.
$global:EwiModioPublishMock = $null

# Model only the authenticated publication endpoints needed by the core lifecycle tests.
function Invoke-RestMethod {
    [CmdletBinding()]
    param([string]$Method, [string]$Uri, [hashtable]$Headers, [hashtable]$Form, [hashtable]$Body, [string]$ContentType, [int]$TimeoutSec, [int]$MaximumRedirection)
    $global:EwiModioPublishHttpCalls++
    if (!$global:EwiModioPublishMock) { throw 'Unexpected HTTP request in a local-only publication test.' }
    $address = [Uri]$Uri
    $request = $Method + ' ' + ($address.PathAndQuery -replace '^/v1', '')
    $expectedHost = if ($request -match '^GET /(?:me|games\?name_id=drg)$') { 'api.mod.io' } else { 'g-7.modapi.io' }
    if ($address.Host -cne $expectedHost -or $Headers.Authorization -cne ('Bearer ' + $env:MODIO_TOKEN)) { throw 'Unexpected API host or authentication header.' }
    if (!$PSBoundParameters.ContainsKey('MaximumRedirection') -or $MaximumRedirection -ne 0) { throw 'Authenticated API requests must disable redirects.' }
    $global:EwiModioPublishMock.Calls.Add($request)
    switch -Regex ($request) {
        '^GET /me$' { return @{id=42} }
        '^GET /games\?name_id=drg$' {
            $platforms = if ($global:EwiModioPublishMock.HasWindows) { @(@{platform='windows';moderated=$global:EwiModioPublishMock.Moderated}) } else { $null }
            return @{data=@(@{id=7;name_id='drg';platforms=$platforms})}
        }
        '^GET /games/7/mods\?name_id=enemy-wave-indicator$' { return @{data=@(@{id=9;game_id=7;name_id='enemy-wave-indicator';submitted_by=@{id=42}})} }
        '^GET /games/7/mods/9/files\?version=1\.1\.1&_limit=100$' {
            return @{data=@(if ($global:EwiModioPublishMock.Exists) { New-MockModfile })}
        }
        '^POST /games/7/mods/9/files$' {
            if ($Form.active -cne 'false' -or $Form.version -cne '1.1.1' -or $Form.filehash -ine $global:EwiModioPublishMock.MD5 -or $Form.filedata.FullName -cne $global:EwiModioPublishMock.Archive -or $Form.changelog -notmatch '<h1') { throw 'Incorrect inactive upload form.' }
            if ($global:EwiModioPublishMock.HasWindows) {
                if ($Form.'platforms[]' -cne 'windows') { throw 'A per-platform game upload omitted the Windows platform.' }
            } elseif ($Form.ContainsKey('platforms[]')) { throw 'A legacy game upload must omit the unsupported platforms field.' }
            if ($global:EwiModioPublishMock.UploadTimeout) { throw 'Synthetic upload timeout after request dispatch.' }
            if ($global:EwiModioPublishMock.RejectUpload) {
                $response = [Net.Http.HttpResponseMessage]::new([Net.HttpStatusCode]::Unauthorized)
                $failure = [Microsoft.PowerShell.Commands.HttpResponseException]::new('Synthetic upload rejection.', $response)
                $record = [Management.Automation.ErrorRecord]::new($failure, 'SyntheticUnauthorized', [Management.Automation.ErrorCategory]::AuthenticationError, $null)
                $record.ErrorDetails = [Management.Automation.ErrorDetails]::new('{"error":{"error_ref":11001}}')
                $PSCmdlet.ThrowTerminatingError($record)
            }
            $global:EwiModioPublishMock.Exists = $true
            $global:EwiModioPublishMock.Changelog = $Form.changelog
            return New-MockModfile
        }
        '^GET /games/7/mods/9/files/555$' {
            if (!$global:EwiModioPublishMock.Exists) { throw 'Requested a nonexistent mock modfile.' }
            $global:EwiModioPublishMock.ScanReads++
            return New-MockModfile
        }
        '^GET /games/7/mods/9$' {
            if ($Headers.'X-Modio-Platform' -cne 'windows') { throw 'Current file was checked without the Windows platform header.' }
            return @{modfile=@{id=$global:EwiModioPublishMock.LiveId};platforms=@(if ($global:EwiModioPublishMock.HasWindows) { @{platform='windows';modfile_live=$global:EwiModioPublishMock.LiveId} })}
        }
        '^PUT /games/7/mods/9/files/555$' {
            if (!$global:EwiModioPublishMock.ScanReads -or $Body.active -cne 'true') { throw 'Activation occurred before scan verification or used the wrong body.' }
            $global:EwiModioPublishMock.LiveId = 555
            return New-MockModfile
        }
        default { throw 'Unexpected API operation in the publication test.' }
    }
}

# Copy fixture bytes to emulate a CDN download and reject any bearer header on that call.
function Invoke-WebRequest {
    [CmdletBinding()]
    param([string]$Uri, [string]$OutFile, [hashtable]$Headers, [int]$TimeoutSec)
    $global:EwiModioPublishHttpCalls++
    if (!$global:EwiModioPublishMock) { throw 'Unexpected HTTP request in a local-only publication test.' }
    if ($Headers.Authorization -or $Uri -cne 'https://example.invalid/release.zip' -or $global:EwiModioPublishMock.LiveId -ne 555) { throw 'Invalid CDN verification request or leaked authentication.' }
    $global:EwiModioPublishMock.Calls.Add('DOWNLOAD release.zip')
    Copy-Item -LiteralPath $global:EwiModioPublishMock.Archive -Destination $OutFile -Force
}

# Provide matching immutable bytes and completed scan metadata for an uploaded file.
function New-MockModfile {
    return @{id=555;version='1.1.1';filesize=$global:EwiModioPublishMock.Bytes;filehash=@{md5=$global:EwiModioPublishMock.MD5};changelog=$global:EwiModioPublishMock.Changelog;virus_status=1;virus_positive=0;platforms=@(if ($global:EwiModioPublishMock.HasWindows) { @{platform='windows';status=$global:EwiModioPublishMock.PlatformStatus} });download=@{binary_url='https://example.invalid/release.zip'}}
}

# Start a fresh server model while keeping every request inside the test process.
function New-MockSession([string]$Archive) {
    $global:EwiModioPublishMock = @{Archive=$Archive;Bytes=(Get-Item -LiteralPath $Archive).Length;MD5=(Get-FileHash -LiteralPath $Archive -Algorithm MD5).Hash;Exists=$false;LiveId=0;ScanReads=0;Changelog='';HasWindows=$false;Moderated=$false;PlatformStatus=1;UploadTimeout=$false;RejectUpload=$false;Calls=[Collections.Generic.List[string]]::new()}
}

# Create a tiny ZIP with the same three root entries as the player distribution.
function New-TestArchive([string]$Path, [switch]$MissingLicense, [switch]$EmptyDll) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew)
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in @('main.dll', 'EnemyWaveIndicator_P.pak', 'LICENSES.txt')) {
            if ($MissingLicense -and $name -eq 'LICENSES.txt') { continue }
            $entry = $zip.CreateEntry($name)
            $entryStream = $entry.Open()
            try {
                if (!$EmptyDll -or $name -ne 'main.dll') {
                    $bytes = [Text.Encoding]::UTF8.GetBytes('Synthetic publication fixture: ' + $name)
                    $entryStream.Write($bytes, 0, $bytes.Length)
                }
            } finally { $entryStream.Dispose() }
        }
    } finally { $zip.Dispose(); $stream.Dispose() }
}

# Verify a semantic refusal and prove it occurred before any HTTP request.
function Assert-Refused([hashtable]$Arguments, [string]$Pattern) {
    $message = ''
    try { & $publisher @Arguments | Out-Null } catch { $message = $_.Exception.Message }
    if (!$message -or $message -notmatch $Pattern) { throw "Expected refusal matching '$Pattern'; received '$message'." }
    if ($global:EwiModioPublishHttpCalls -ne 0) { throw 'A local refusal attempted HTTP.' }
    $script:cases++
}

New-Item -ItemType Directory -Path $fixture | Out-Null
# Remove the real credential from this test's environment and restore it without displaying it.
$savedToken = [Environment]::GetEnvironmentVariable('MODIO_TOKEN', 'Process')
try {
    [Environment]::SetEnvironmentVariable('MODIO_TOKEN', $null, 'Process')
    $archive = Join-Path $fixture 'EnemyWaveIndicator-1.1.1.zip'
    $notes = Join-Path $fixture '1.1.1.md'
    New-TestArchive $archive
    Set-Content -LiteralPath $notes -Encoding utf8 -Value "# Enemy Wave Indicator 1.1.1`n`nSynthetic release notes."
    $arguments = @{
        Version = '1.1.1'; ArchivePath = $archive; ChangelogPath = $notes
        EvidenceDirectory = (Join-Path $fixture 'prepared')
    }
    $prepared = & $publisher @arguments
    if ($prepared.Status -ne 'Prepared' -or $prepared.Version -ne '1.1.1' -or $prepared.ChangelogFormat -ne 'html') { throw 'Local preparation returned incorrect release metadata.' }
    if ($prepared.Bytes -ne (Get-Item -LiteralPath $archive).Length -or $prepared.SHA256 -ne (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -or $prepared.MD5 -ne (Get-FileHash -LiteralPath $archive -Algorithm MD5).Hash) { throw 'Prepared release hashes or size differ from the archive.' }
    if ($global:EwiModioPublishHttpCalls -ne 0) { throw 'Default preparation attempted HTTP.' }
    $script:cases++

    $publishArguments = $arguments.Clone(); $publishArguments.Publish = $true
    Assert-Refused $publishArguments 'token|credential|MODIO_TOKEN'
    $remoteArguments = $arguments.Clone(); $remoteArguments.CheckRemote = $true
    Assert-Refused $remoteArguments 'token|credential|MODIO_TOKEN'

    $missingLicense = Join-Path $fixture 'missing-license.zip'
    New-TestArchive $missingLicense -MissingLicense
    $invalidArguments = $arguments.Clone(); $invalidArguments.ArchivePath = $missingLicense
    Assert-Refused $invalidArguments 'ZIP|archive|entries|LICENSES'

    $emptyDll = Join-Path $fixture 'empty-dll.zip'
    New-TestArchive $emptyDll -EmptyDll
    $invalidArguments = $arguments.Clone(); $invalidArguments.ArchivePath = $emptyDll
    Assert-Refused $invalidArguments 'empty|non.?empty|main\.dll'

    $wrongNotes = Join-Path $fixture 'wrong-version.md'
    Set-Content -LiteralPath $wrongNotes -Encoding utf8 -Value '# Enemy Wave Indicator 1.1.0'
    $invalidArguments = $arguments.Clone(); $invalidArguments.ChangelogPath = $wrongNotes
    Assert-Refused $invalidArguments 'changelog|heading|version'

    $invalidArguments = $arguments.Clone(); $invalidArguments.ExpectedSHA256 = ('0' * 64)
    Assert-Refused $invalidArguments 'SHA256|SHA-256|hash'
    # This credential is deliberately fake and can only reach the functions above.
    $env:MODIO_TOKEN = 'synthetic-publication-test-token'
    New-MockSession $archive
    $remotePublish = $arguments.Clone(); $remotePublish.Publish = $true; $remotePublish.WaitSeconds = 0
    $remotePublish.EvidenceDirectory = Join-Path $fixture 'publish'
    $published = & $publisher @remotePublish
    if ($published.Status -ne 'Published' -or !$published.Uploaded -or !$published.Activated -or !$published.Live -or !$published.DownloadVerified) { throw 'The first mocked publication did not complete the upload/activation/download lifecycle.' }
    $firstWrites = @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^(POST|PUT) ' })
    if (($firstWrites -join ',') -cne 'POST /games/7/mods/9/files,PUT /games/7/mods/9/files/555' -or $global:EwiModioPublishMock.Calls[-1] -cne 'DOWNLOAD release.zip') { throw 'Unexpected publication write order or missing final download verification.' }
    $stateFile = Join-Path $remotePublish.EvidenceDirectory 'publication.json'
    $state = Get-Content -LiteralPath $stateFile -Raw
    if (($state | ConvertFrom-Json).FileId -ne 555 -or $state.Contains($env:MODIO_TOKEN)) { throw 'Publication state omitted the resumable file ID or stored a credential.' }
    $script:cases++

    $global:EwiModioPublishMock.Calls.Clear()
    $reused = & $publisher @remotePublish
    if (!$reused.DownloadVerified -or $reused.Uploaded -or $reused.Activated -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^(POST|PUT) ' }).Count -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^GET .*/files\?' }).Count) { throw 'Retry did not reuse the saved file ID without writes.' }
    $script:cases++

    $global:EwiModioPublishMock.Calls.Clear()
    $remoteCheck = $arguments.Clone(); $remoteCheck.CheckRemote = $true; $remoteCheck.WaitSeconds = 0
    $remoteCheck.EvidenceDirectory = Join-Path $fixture 'remote-check'
    $checked = & $publisher @remoteCheck
    if (!$checked.Live -or !$checked.DownloadVerified -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^(POST|PUT) ' }).Count) { throw 'CheckRemote did not verify the live file without mutation.' }
    $script:cases++

    New-MockSession $archive
    $global:EwiModioPublishMock.UploadTimeout = $true
    $uncertain = $remotePublish.Clone(); $uncertain.EvidenceDirectory = Join-Path $fixture 'uncertain-upload'
    $message = ''
    try { & $publisher @uncertain | Out-Null } catch { $message = $_.Exception.Message }
    $uploadState = Get-Content -LiteralPath (Join-Path $uncertain.EvidenceDirectory 'publication.json') -Raw | ConvertFrom-Json
    if (!$message -or $uploadState.Stage -cne 'Uploading' -or $uploadState.FileId -ne 0) { throw 'An uncertain upload did not preserve its retry warning state.' }
    $message = ''
    try { & $publisher @uncertain | Out-Null } catch { $message = $_.Exception.Message }
    if ($message -notmatch 'unknown|second upload|retry' -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^POST ' }).Count -ne 1 -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^PUT ' }).Count) { throw 'An uncertain upload was blindly duplicated or activated.' }
    $script:cases++

    New-MockSession $archive
    $global:EwiModioPublishMock.HasWindows = $true
    $global:EwiModioPublishMock.Moderated = $true
    $global:EwiModioPublishMock.PlatformStatus = 0
    $pending = $remotePublish.Clone(); $pending.EvidenceDirectory = Join-Path $fixture 'pending-platform'
    $pendingResult = & $publisher @pending
    if ($pendingResult.Status -cne 'PendingPlatformApproval' -or $pendingResult.Activated -or $pendingResult.Live -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^PUT |^DOWNLOAD ' }).Count) { throw 'A moderated pending file was activated or downloaded before approval.' }
    $script:cases++

    New-MockSession $archive
    $global:EwiModioPublishMock.RejectUpload = $true
    $rejected = $remotePublish.Clone(); $rejected.EvidenceDirectory = Join-Path $fixture 'rejected-upload'
    $failure = $null
    try { & $publisher @rejected | Out-Null } catch { $failure = $_.Exception }
    $rejectedState = Get-Content -LiteralPath (Join-Path $rejected.EvidenceDirectory 'publication.json') -Raw | ConvertFrom-Json
    if (!$failure -or $failure.Data['HttpStatus'] -ne 401 -or $failure.Data['ErrorRef'] -ne '11001' -or $rejectedState.Stage -cne 'Rejected' -or $rejectedState.FileId -ne 0) { throw 'A definite HTTP rejection was not recorded distinctly from an unknown upload.' }
    $global:EwiModioPublishMock.RejectUpload = $false
    $retried = & $publisher @rejected
    if (!$retried.DownloadVerified -or @($global:EwiModioPublishMock.Calls | Where-Object { $_ -match '^POST ' }).Count -ne 2) { throw 'A definitely rejected upload could not be retried safely.' }
    $script:cases++
    Write-Output "PASS: $script:cases publication cases; all HTTP used local substitutes. Evidence: $fixture"
} finally {
    [Environment]::SetEnvironmentVariable('MODIO_TOKEN', $savedToken, 'Process')
    Remove-Variable -Name EwiModioPublishMock, EwiModioPublishHttpCalls -Scope Global -ErrorAction SilentlyContinue
}
