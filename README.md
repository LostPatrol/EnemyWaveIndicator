<!-- Public product overview and reproducible source-build instructions. -->
# Normal Wave Indicator

Early **0.6.0 alpha** for Deep Rock Galactic, Windows / Steam. Host-local only.

**Pre-release audit: blocked for mod.io subscription-only distribution.** The current Pak requires native capture and initialization. No equivalent pure-Blueprint source attribution has been established; this build is not a release candidate for that installation target. See the [feasibility audit](docs/MODIO-FEASIBILITY.md).

Shows actual normal-wave enemy spawn regions with animated spheres, per-frame world labels, straight-line distance in meters, and offscreen edge indicators. Open **H → Mod Hub → Normal Wave Indicator** to change warning text, visibility duration, sphere size, RGB intensity and gentle text pulsing. Click **Apply and save**. Settings survive game restarts.

The native component attributes successful enemy creation to an audited normal-wave queue path. It does not infer waves from nearby enemies or log timestamps, and never creates enemies. Points from the same wave within 8 m of a region's first actual point share a marker. Up to 8 regions can be displayed; three markers is a common grouping result, not a fixed count.

## Compatibility and installation

This is a **native DLL + Pak** mod. A mod.io Pak subscription alone is insufficient. Install Mod Hub separately; DRGlib is not required by our settings page. The supported loader is UE4SSL.JavaScript stable 0.31.0 as bundled in the tested MintCat 0.5.5 environment, not arbitrary upstream UE4SS builds.

| Component | Tested SHA-256 |
| --- | --- |
| FSD-Win64-Shipping.exe (Steam build 24903151) | `9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13` |
| UE4SSL.dll | `D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7` |

The installer refuses different executable/runtime hashes. Game or loader updates require a new compatibility audit. Do not replace either component with files from another installation.

Release archives, when available, contain `Install.ps1`, `main.dll`, our presentation Pak and a hash manifest. Exit the game, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -GameRoot 'D:\Steam\steamapps\common\Deep Rock Galactic'
```

Launch normally through Steam. The diagnostic startup script is optional. MintCat reintegration can remove independently installed files; reinstall the matching archive afterward if needed. The installer retires this project's legacy NormalWaveDiagnostic and NormalWaveNativeProbe loaders with backups, removing the old one-second HUD and its polling.

Settings are stored in the game's `FSD/Saved/SaveGames/NormalWaveIndicator_v1.sav`, independently of player progress. Text distance is measured from the controlled pawn, not the camera or walkable path. Duration extends from the latest spawn event in that region; changing duration affects subsequent events.

## Build from source

Required: Windows, Git, Visual Studio C++ x64 tools and Windows SDK, .NET Framework 4.8 SDK, Unreal Engine **4.27.2**. No Rust toolchain is needed. MinHook 1.3.4 source is included under its own license.

Run the following in PowerShell from this repository. Use a process-scoped execution-policy override if needed; changing the system policy is unnecessary.

```powershell
.\scripts\Prepare-ModHubDevkit.ps1
$editorBuild = .\scripts\Build-EditorAuthoring.ps1
$verification = .\scripts\Test-PresentationAssets.ps1 -AuthoringBuild $editorBuild
$cook = .\scripts\Cook-PresentationAssets.ps1 -VerificationDirectory $verification
$nativeBuild = .\scripts\Build-NativeProbe.ps1
.\scripts\Test-SpawnAttribution.ps1
.\scripts\Install-NativeProbe.ps1 -BuildDirectory $nativeBuild -PresentationCook $cook -GameRoot 'D:\Steam\steamapps\common\Deep Rock Galactic'
```

The authoring module generates our Blueprints and material using stock engine nodes. Mod Hub's pinned interface devkit is fetched for editor references only. It is neither committed nor included in our Pak. The original mini-MULE pulse curves are loaded from the installed game at runtime; no game assets are distributed here. Generated build evidence goes to the Git-ignored `agent/codex` directory.

Native tests cover actual MinHook trampolines and exactly-once original forwarding, mixed-source queue attribution, swap removal, failures, bounded storage, thread rejection and expiry. Editor tests execute serialized Blueprint graphs, including edge placement, pooling and configuration. They do not substitute for real H-menu interaction, GPU profiling or game compatibility tests.

For developer alpha packaging, run `scripts/Prepare-Release.ps1 -BuildDirectory $nativeBuild -PresentationCook $cook -Target NativeAlpha`. It checks validation results and recorded input hashes and marks the archive as requiring manual native installation. `-Target Modio` deliberately fails before producing an archive while the subscription-only runtime is unimplemented. Packaging success is not gameplay or release approval.

## Alpha limitations

- Host-local display only; client synchronization is not implemented.
- Startup waits for a stable game world and resource preparation. Very early spawns can be missed.
- Bootstrap tracks up to eight world identities and approximately 46 minutes of transition monitoring per process. Existing frame rendering continues afterward. Restart the game for longer test sessions.
- Exact first-visible-frame latency and heavy-load CPU/GPU percentiles are not yet measured. The observer records successful spawn inputs, not a guaranteed terrain-surface projection.
- Unknown queue changes that preserve the same count and indistinguishable keys cannot all be detected. Unsupported source paths fail attribution conservatively.
- No mod.io approval or Verified status is claimed. See [release preparation](docs/RELEASE.md).

The [Sandbox Utilities investigation](docs/SANDBOX-UTILITIES-RESEARCH.md) explains which content-only techniques can be reused, why its spawn callback does not capture the game's natural waves, and the proposed game-side event needed for an exact subscription-only version. That interface is not implemented in the current game.

## Credits and license

Original project source: MIT, LostPatrol. MinHook: BSD-style license in `third_party/MinHook/LICENSE.txt`. Mod Hub interface references: [trumank/drg-mods](https://github.com/trumank/drg-mods). Deep Rock Galactic and its assets belong to Ghost Ship Games / their respective owners. This project is an unofficial mod.
