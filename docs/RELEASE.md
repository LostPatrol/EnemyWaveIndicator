<!-- Release procedure for the revised all-spawn content-only candidate; distinguish offline and game acceptance. -->
# Release preparation — 0.7.0 content-only beta

The user explicitly accepts approximate wave classification. The 0.7.0 candidate therefore uses the existing `EnemySpawnManager.OnEnemySpawned` event, records callback-time locations, and attaches optional active-wave context. The source-preserving game API in earlier research is **not required for this revised scope**.

## What to publish

Build with the README's developer commands and `Prepare-Release.ps1 -Target Modio`, or call `Prepare-ModioRelease.ps1` directly. The ZIP must contain exactly one `NormalWaveIndicator_P.pak`, with these nine asset pairs inside:

- BP_NwiAuto, BP_NwiResources, BP_NwiPulse, WBP_NwiMarker
- SG_NwiSettings, WBP_NwiSettings, M_NwiRedPulse
- InitCave, InitSpacerig

No DLL, native loader, install script, FSD stub module, authoring plugin, test asset, original game asset or copied Mod Hub interface belongs in the upload. Dependencies resolve to stock Engine/FSD implementations, owned Blueprints and the separately subscribed Mod Hub interface. Cooking uses inline material shaders.

The packager requires cold Blueprint capture/display/settings checks, source/asset/cooked/config hashes, an exact entry list and a cooked-import audit. It marks the architecture as content-only but keeps `ModioSubscriptionTested=false` and `ReleaseReady=false`. Passing these checks does not grant moderation approval or prove that every game spawn path is covered.

## Required acceptance before public release

1. Use a backed-up clean game environment with the old Normal Wave Indicator DLL/Pak disabled and no Mint/MintCat injection or UE4SSL. Do not uninstall unrelated mods or erase saves. The previous alpha installation requires a one-time migration; a new subscriber does not.
2. Upload the candidate privately only with the owner's authorization. Declare Mod Hub as a content dependency. Subscribe through the game, enable, restart and enter a host mission. Verify both native entry actors initialize exactly one controller. Then verify disable/re-enable and another mission/world.
3. Check normal, scripted, egg ambush, extraction/defence and overlapping spawns. Points must correspond to newly notified enemy locations; context may be uncertain. Missing notifications from a particular game path must be reported, not hidden behind an “all actors” claim.
4. Check the eight-region capacity policy, 8 m merge boundary, stationary origins, configured expiry, distance, blink, Mod Hub H discovery and save/restart. Confirm long sessions/travel without the old native bootstrap's time/world limits.
5. Measure frame time under dense spawns. The fixed pool bounds rendering objects, but the per-enemy Blueprint work still needs actual-game profiling. Check the current mod.io category with moderation; no Verified/Approved status is claimed here.

The automation does not launch the game or upload the archive. Offline fixtures use the real delegate shape but dummy FSD implementations; native game compatibility and mod.io installation remain separate acceptance steps.

## Historical native alpha

`Prepare-Release.ps1 -Target NativeAlpha` remains for compatible **0.6.0** native/presentation records. It rejects the 0.7.0 18-entry content cook. Its installer/runtime requirements do not apply to a new 0.7.0 content-only subscriber. Do not publish a native-alpha ZIP under the new subscription-only description.
