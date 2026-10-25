<!-- Publish verified DLL/Pak archives to the existing mod.io listing using its official API. -->
# mod.io publishing

`scripts/Publish-ModioRelease.ps1` publishes file versions to [Enemy Wave Indicator](https://mod.io/g/drg/m/enemy-wave-indicator). It requires PowerShell 7 and uses its built-in Markdown converter and HTTP commands; no extra module is needed.

## Inputs and credentials

Build and verify the release ZIP first with the normal [build workflow](GAME-COMPATIBILITY.md). By default the publisher reads `dist/EnemyWaveIndicator-<version>.zip` and `docs/changelog/<version>.md`. The ZIP must contain the nonempty root entries `main.dll`, `EnemyWaveIndicator_P.pak` and `LICENSES.txt`. The notes heading must match the version. Notes are converted to HTML for mod.io without changing the source file.

Obtain an account access token from [mod.io Access](https://mod.io/me/access) with `read+write` scope. The account must own the mod or be an accepted team manager/administrator. A read-only API key cannot publish. The script reads `MODIO_TOKEN` from its process environment or a local file supplied with `-TokenPath`; it does not accept a token value on the command line or record it in evidence. A convenient ignored local path is `agent/codex/modio-token.txt`. Keep credentials out of source and terminal history.

## Commands

Run from the project root. The default command only prepares locally and requires no token:

```powershell
./scripts/Publish-ModioRelease.ps1 -Version 1.1.1
```

Check the authenticated remote state without uploading or activating a file:

```powershell
./scripts/Publish-ModioRelease.ps1 -Version 1.1.1 -CheckRemote `
    -TokenPath ./agent/codex/modio-token.txt
```

Publish the already verified ZIP. `-ExpectedSHA256` can pin it to the hash verified for the GitHub release:

```powershell
./scripts/Publish-ModioRelease.ps1 -Version 1.1.1 -Publish `
    -TokenPath ./agent/codex/modio-token.txt `
    -ExpectedSHA256 397289FDF61B364D46962570417B4A63AC76E974F1BB4B41251D0F1DA8FBB044
```

`-ArchivePath` and `-ChangelogPath` override the default input locations. `-ApiBase` supports the official `https://api.mod.io/v1` endpoint or the account/game-specific `https://u-<id>.modapi.io/v1` / `https://g-<id>.modapi.io/v1` path shown in the Access page.

## Publication and recovery

The publisher resolves the exact game/mod slugs and account, then checks existing files for the same version and MD5. Matching bytes are reused. A conflicting existing version stops with an error. The API's `filehash` field uses MD5; independent local/CDN verification also uses SHA256.

New files are uploaded as inactive with Windows targeting and the authored changelog. After verifying metadata and scan status, the publisher activates the file. For moderated Windows submissions it waits for platform approval. It verifies both the current modfile and any Windows `modfile_live` reference, then downloads and compares the actual archive. The bearer token is not sent to the download CDN.

Default polling lasts up to 60 seconds; `-WaitSeconds` can extend it. `Published` means the current Windows file and downloaded bytes were verified. `PendingScan`, `ScanUnavailable`, `PendingPlatformApproval` and `ReadyToActivate` report unfinished states; they must not be described as a completed publication. A malicious scan result or denied Windows submission stops activation. The separate potentially-harmful flag produces a warning rather than being silently treated as a clean scan.

Publication state and verification summaries are stored under `agent/codex/modio-publish-<version>/`. Re-running `-Publish` reuses the recorded file ID and can activate a file after approval. A write request is never automatically retried. If an upload times out before returning its ID, use `-CheckRemote` or inspect the listing before retrying; an unresolved upload intent prevents a second blind upload. `-EvidenceDirectory` selects another local evidence directory when needed.

## Maintenance

The publisher is separate from packaging and does not rebuild game assets. Its tests use HTTP substitutes, so they can run without a token and without contacting mod.io:

```powershell
./tests/modio-publish.test.ps1
```

API references: [authentication](https://docs.mod.io/restapi/introduction), [add modfile](https://docs.mod.io/restapi/docs/add-modfile), [edit modfile](https://docs.mod.io/restapi/docs/edit-modfile), [platforms](https://docs.mod.io/restapi/platforms).
