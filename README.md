<!-- Player-facing scope and developer build instructions for the filtered content-only spawn-area beta. -->
# Normal Wave Indicator — 0.7.1 beta

A Deep Rock Galactic **content-only** spawn-area indicator. Version 0.7.1 observes the game's enemy-spawn notifications, excluding its registered small-enemy and critter buckets. It does not modify enemy spawning or attempt exact natural-wave identification.

The intended player workflow is to subscribe and enable the Mod and its Mod Hub content dependency through the game's native mod.io support. **The candidate is not published, and clean mod.io subscription/gameplay acceptance is still pending.** No runtime DLL, UE4SSL, PowerShell or external installer is included in the new upload ZIP.

## Behavior

- Host-local red pulse spheres and screen/edge labels mark the enemy's location at its successful-spawn callback. The marker stays at that origin instead of following the enemy.
- A single eligible notification can trigger a marker; there is no minimum wave size or Mission Control announcement requirement. Ambient spawns and some boss summons therefore qualify too.
- Pawns in `EnemySpawnManager.ActiveSwarmerEnemies` or `ActiveCritters` are ignored before recording, merging or extending marker lifetimes. This follows the game's live counting buckets: swarmers, small naedocytes and hostile shredders are excluded when registered there. Large breeders remain eligible. Descriptor significance alone is not used, because it can differ from the Pawn's counting category. Unregistered or other-mod reclassified units remain a compatibility limitation.
- Spawns within 8 metres of an active region's first point and with the same context label extend that region. At most eight regions are visible; a new distant region replaces the one with the earliest expiry when capacity is full.
- When the wave manager exposes an active scripted controller, its readable class name is shown as `Event context: ...`. Multiple controllers are marked `Mixed events (first): ...`.
- Otherwise the label says `Unknown / possible natural wave`. This is a guess, not proof of natural-wave provenance. Mission-specific events that are not in that active-controller list can receive this fallback too.
- Mod Hub settings retain custom warning text, visibility duration (1–30 seconds), sphere size/colour, distance and optional text pulsing. Settings use `NormalWaveIndicator_v1.sav`.

This observes **the spawn manager's notifications**, not every possible actor creation in the game. Already-existing enemies, spawns before binding, and enemies created by paths or other mods that bypass that notification may not be marked. Spawn callback positions are not pre-spawn predictions or guaranteed terrain-surface projections. Context from overlapping events can be misleading; the UI labels it as context intentionally. Client synchronization is not implemented.

## Developer build

Required: Windows, Git, Visual Studio C++ x64 tools and Windows SDK, .NET Framework 4.8 SDK, Unreal Engine **4.27.2**, and Node.js for the cooked-import audit. The scripts use `node` on PATH or the Codex-bundled Node under `%LOCALAPPDATA%\hermes\node`.

```powershell
.\scripts\Prepare-ModHubDevkit.ps1
$build = .\scripts\Build-EditorAuthoring.ps1
$check = .\scripts\Test-PresentationAssets.ps1 -AuthoringBuild $build
$cook = .\scripts\Cook-PresentationAssets.ps1 -VerificationDirectory $check
.\scripts\Prepare-Release.ps1 -PresentationCook $cook -Target Modio
.\tests\modio-package.test.ps1
```

The authoring plugin includes minimal `/Script/FSD` reflection stubs so the editor can compile calls to existing game APIs. They are development fixtures, not a new runtime bridge. The build creates editor binaries only; the packaged Mod contains **nine owned asset pairs / 18 entries** and never the stub DLL or any `NwiAuthoring` implementation. Mod Hub's pinned interface assets are development references and are excluded from the Pak. Original pulse curves are resolved from the installed game; game assets are not redistributed.

The generated upload ZIP contains exactly `NormalWaveIndicator_P.pak`. The adjacent manifest records hashes, validation evidence and the fact that real native-subscription acceptance is still pending. Upload the ZIP, not the build folder or native-alpha archive. See [release checks](docs/RELEASE.md).

## Validation and migration

Cold editor tests execute saved Blueprint graphs and the FSD-shaped multicast delegate fixture. They cover event capture, exclusion of small enemies/critters, a 100-notification filtered burst that cannot create or extend regions, grouping, context/fallback, lifetime and initialization, along with the existing pooled display/settings checks. These tests do not run the real FSD game implementation and cannot substitute for a clean native mod.io test or GPU profiling. See the [spawn and boss-summon audit](docs/SPAWN-FILTER-AUDIT.md) for current-game static evidence and coverage limits.

Users of the previous **0.6.0 DLL alpha** must remove/disable that old Mod loader and loose presentation Pak before testing this version. Do not mix the old native producer with these changed Blueprint assets. The new subscription Mod does not automatically remove files previously installed outside mod.io. Back up the old installation for rollback; do not remove loaders or other mods indiscriminately.

The prior [exact-attribution audit](docs/MODIO-FEASIBILITY.md) and [Sandbox Utilities investigation](docs/SANDBOX-UTILITIES-RESEARCH.md) explain the original limitation. The user explicitly relaxed exact attribution for 0.7.0; this version does not claim to have solved the original missing-provenance problem.

## Credits and license

Original project source: MIT, LostPatrol. Historical native code includes MinHook under its own license in `third_party/MinHook/LICENSE.txt`. Mod Hub references: [trumank/drg-mods](https://github.com/trumank/drg-mods). DRG's native Blueprint initialization and dummy method are documented in the [modding handbook](https://drg-modding.github.io/docs/guides/blueprint-modding-guide.html). Deep Rock Galactic and its assets belong to Ghost Ship Games / their respective owners. This is an unofficial Mod.
