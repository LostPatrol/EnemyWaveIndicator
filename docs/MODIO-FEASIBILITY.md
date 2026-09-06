<!-- Evidence, scope and acceptance criteria for the subscription-only release decision. -->
# mod.io subscription-only feasibility audit — 2026-09-07

**Decision: the current 0.6.0 architecture cannot satisfy subscription-only installation. No verified equivalent pure-Pak replacement has been found for its exact normal-wave attribution. Do not release it under that claim.** This is a conclusion about the current implementation and audited interfaces, not proof that every possible future technique is impossible.

The required behavior remains: observe genuine normal-wave spawns, preserve their successful spawn positions and source identity, show smooth host-local markers, and require no player-installed DLL, UE4SSL, installer or PowerShell. Developer-side C++/PowerShell cooking is compatible with this goal; player-side native installation is not.

Follow-up investigation: [Sandbox Utilities source analysis and solution paths](SANDBOX-UTILITIES-RESEARCH.md) verifies its reflected APIs, Custom Difficulty 2's separate bytecode splicing, and the current executable's empty pool-spawn callback. A concrete [game-owned event proposal](NORMAL-WAVE-EVENT-PROPOSAL.md) describes the missing source-preserving interface and the subsequent content-only conversion. Neither document represents an implemented subscription release.

## Runtime dependency audit

| Layer | Current implementation | Subscription-only assessment |
| --- | --- | --- |
| Source and successful creation | `GameCapture.cpp` observes native normal-wave call context, queue append, successful `UWorld::SpawnActor` return, and swap removal. | Essential native code; packing the DLL into a ZIP/Pak does not execute or register it. |
| Event delivery | `AutomaticPresentation.cpp` binds the generated `NwiPoll` no-op event to a native function and writes `NativePoint/Serial/Visible`. | Without the DLL, these values do not receive gameplay events. |
| Initialization | DLL bootstrap creates the controller. The current 16-entry Pak contains no `InitCave` or `InitSpacerig`. | Pak mounting alone does not start the controller. Adding initialization alone leaves capture missing. |
| Rendering and settings | Generated stock-engine Blueprints, UMG, material and SaveGame; Mod Hub interfaces. | Reusable in a future pure-Pak mod. Mod Hub provides settings integration, not missing normal-wave provenance. |
| Authoring | `NwiAuthoring` is an editor-only C++ module. | May remain developer-side. It is not the runtime DLL and must not be shipped to players. |

DRG's documented native Blueprint loading uses `InitCave` and `InitSpacerig` actors. “Native spawning” here means the game starts a Blueprint; it is not a native C++ DLL loader. See the [Blueprint guide, loading and initialization sections](https://drg-modding.github.io/docs/guides/blueprint-modding-guide.html). The [FSD template](https://github.com/DRG-Modding/FSD-Template) supplies reflected declarations for authoring; adding a dummy C++ declaration does not add a new engine implementation to the installed game.

## Why the obvious replacements are insufficient

The public template was checked at commit `15fef14b2c20ce25d11b60346f0b7f8db500d7e3` (2024-07-05). It is older than the installed game, so it is supporting evidence rather than a current complete SDK. The installed executable was independently re-hashed and its native registration strings/callback body checked read-only. SHA-256: `9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13`.

- [`EnemySpawnManager.OnEnemySpawned`](https://github.com/DRG-Modding/FSD-Template/blob/15fef14b2c20ce25d11b60346f0b7f8db500d7e3/Source/FSD/Public/EnemySpawnedSignatureDelegate.h) is a multicast notification carrying Pawn and descriptor. A Pawn can supply a location at callback time, but the signature carries no normal-wave source or wave ID. Callback-time location also needs validation against the original successful spawn transform.
- [`EnemyWaveManager.OnEnemySpawned`](https://github.com/DRG-Modding/FSD-Template/blob/15fef14b2c20ce25d11b60346f0b7f8db500d7e3/Source/FSD/Public/EnemyWaveManager.h) is a callable native handler taking a Pawn, not a Blueprint-assignable wave event. The installed thunk at RVA `0x1a7fa50` calls native code at `0x169b970`; declaring a same-named Blueprint event does not intercept existing native calls.
- `NormalWavesEnabled`, `AreNormalWavesBlocked` and `ActiveScriptedWaves` describe state. They do not prove which source produced an individual enemy when sources overlap. Filtering on them can misclassify enemies or suppress valid simultaneous normal spawns.
- [`SpawnQueueItem`](https://github.com/DRG-Modding/FSD-Template/blob/15fef14b2c20ce25d11b60346f0b7f8db500d7e3/Source/FSD/Public/SpawnQueueItem.h) exposes class, descriptor and a completion delegate in that template. The DLL uses additional native layout and call-stack information. Frame snapshots miss entries appended and removed between observations and cannot reproduce synchronous provenance tracking. Replacing completion delegates would also mutate gameplay behavior and is not an established read-only substitute.
- `C_SpawnNormalWave` is a command to create a wave, not an observation event. Calling it, disabling the built-in scheduler or replacing enemy-spawn logic changes gameplay and is outside this indicator's scope.

Consequently generic enemy events, proximity scans, countdown rollover and time windows must not be presented as equivalent implementations. No new gameplay hooks, scheduler replacement or approximate fallback were introduced by this audit.

## Pre-release findings

| Priority | Finding | Disposition |
| --- | --- | --- |
| Blocker | Runtime DLL/UE4SSL installation remains necessary. | Subscription-only build blocked explicitly by packaging target. |
| Blocker | No established pure-Pak source attribution or native Blueprint entry assets. | Requires a validated replacement before conversion; no nonfunctional Pak published. |
| High | Direct `Prepare-Release.ps1` previously accepted fewer checks than source installation, including no overall Blueprint-success check or recorded-input freshness checks. | Packaging now requires complete native/Blueprint pass flags and checks recorded source, asset, cooked-file and packaging-config hashes. |
| High | 0.6.0 real Mod Hub discovery, save/restart and visual performance acceptance remain incomplete. | Offline tests cannot close these items. |
| High | Long-session bootstrap is bounded to eight observed world identities and about 46 minutes of transition monitoring. | Existing alpha limitation; formal long-session acceptance remains open. |
| Medium | Native compatibility is tied to exact executable/loader hashes; host-local only. | Retain explicit compatibility limits; no broader version or client-support claim. |

Packaging manifests now say `DistributionTarget: NativeAlpha`, `ModioSubscriptionOnly: false`, `ReleaseReady: false` and list external requirements. `-Target Modio` rejects before reading inputs or producing files. This is release preparation, not an implementation of subscription-only gameplay.

## Conditions for reconsidering the release

1. Establish an existing game/reflected interface or content-only interception that supplies unambiguous normal-wave provenance and successful spawn coordinates without replacing the scheduler. Prove behavior for overlapping normal/event waves, failed creation and queue reordering.
2. Implement game-loaded initialization and lifecycle handling for station, cave, travel and long sessions, with no DLL or external process. Reuse the existing presentation/settings where compatible.
3. Cook an original-assets-only Pak and test on an isolated game installation without Mint/MintCat hooks, UE4SSL or this project's loose DLL/Pak. Test native mod.io subscription, enable/disable, restart and dependency installation. A locally injected test environment cannot establish this acceptance.
4. Complete host gameplay, event-wave exclusions, Mod Hub, GPU/CPU and latency checks. Confirm category and dependency settings against the current moderation requirements.

The [official FAQ](https://www.deeprockgalactic.com/modding-support-faq) describes in-game mod downloads and approval categories. The [approval checklist page](https://mod.io/g/drg/r/approval-process-and-checklist-for-upload) returned no readable policy body during this audit; no claim about current DLL moderation permission or Verified eligibility is made. No upload, moderator message, game launch or live-install change was performed.
