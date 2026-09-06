<!-- Proposed game-owned event contract; this is not an existing FSD API or a loadable Mod implementation. -->
# Proposed game API: successful natural normal-wave spawn

Historical proposal for exact source attribution. The user explicitly accepts approximate all-spawn classification in 0.7.0, so the current content-only implementation does **not** depend on this API or game-developer cooperation. See [current release scope](RELEASE.md).

Status: design proposal, 2026-09-07. **Not implemented in the current game or this Mod.** It requires a game update or an independently verified equivalent existing interface. No dummy declaration or packaged editor DLL can install it into the shipping game.

## Use case

Normal Wave Indicator displays the origin of enemies successfully created by the game's automatic normal-wave scheduler. Generic enemy-spawn notifications expose the enemy and descriptor but not its scheduling source or wave identity. Time-window classification cannot safely separate overlapping mission, scripted and normal-wave requests.

Expose a read-only Blueprint multicast event on the game-owned spawn manager. The Mod can then ship content only and use the game's existing Blueprint initialization. All authority and source classification remain in the game; no player-installed native loader is needed.

## Suggested contract

Illustrative UE declaration, **for the game's source tree**, not a buildable addition to this repository:

```cpp
// Fired once per successful automatic normal-wave spawn on the authoritative game thread.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FNaturalNormalEnemySpawned,
    int64, WaveId,
    int64, SpawnId,
    APawn*, Enemy,
    UEnemyDescriptor*, Descriptor,
    FTransform, SpawnInputTransform);

// Owned by the current world's EnemySpawnManager; does not expose queue mutation.
UPROPERTY(BlueprintAssignable)
FNaturalNormalEnemySpawned OnNaturalNormalEnemySpawned;
```

- `WaveId`: positive monotonic ID per manager/world, allocated at the automatic normal-wave scheduler branch. Every accepted queue item for that invocation retains the same ID. Allocation must not consume gameplay RNG.
- `SpawnId`: positive unique ID per accepted tagged queue item in that manager/world. Used for deduplication; never an array index or a reused Pawn address.
- `Enemy`: the non-null successful actor result, with the documented normal enemy Pawn constraint. Do not silently count non-Pawn results as successfully delivered.
- If publication is deferred, retain the object reference using the engine's lifetime rules; a Pawn may already be pending destruction when listeners run. Successful creation still counts. The receiver must use the captured IDs/transform and must not drop that record solely because the Pawn is no longer gameplay-valid.
- `Descriptor`: the exact descriptor associated with that queue item, after any selection rules already used by the game.
- `SpawnInputTransform`: the value actually passed into the successful `SpawnActor` attempt, snapshotted before callbacks or gameplay code can move the object. This intentionally matches the current indicator's input-position semantics, rather than promising an unadjusted actor location after collision handling.
- Source is implicit in this specific event. Scripted, egg/event, direct cheat/manual and arbitrary pool calls must not generate it merely because they share helper functions. A common routine called by a cheat must receive a distinct/empty provenance context.
- Identity is `(manager/world lifetime, WaveId, SpawnId)`. The receiver discards all state on world travel; it must not compare IDs across managers.

## Minimal implementation placement

1. In the natural scheduling branch, create an explicit provenance context and pass it through existing pool selection and enqueue helpers. Keep all original gameplay parameters, RNG calls, order and return values. Do not rely on one global “currently normal” bool that can mislabel nested unrelated requests.
2. Once a request is actually appended, store the context on that exact queue item. Rejected requests have no event. Native-only fields are sufficient; Blueprint exposure of the queue is unnecessary.
3. Let queue moves, reallocation and swap removal move/remove provenance together with the item. If a parallel metadata array is used instead, every mutation must preserve the same permutation; embedding the data is simpler.
4. When `SpawnActor` succeeds, snapshot the result, descriptor, input transform and IDs before invoking existing callbacks. Failed creation must not emit this event. If the game retries an item, emit only for the successful attempt and preserve the intended SpawnId semantics.
5. Preserve existing callback order and exactly-once execution. Publish the snapshot at a documented safe point on the same game thread after existing queue processing is stable. Do not invoke a newly exposed Blueprint callback while holding a queue-element reference across possible reentrant mutations. A brief deferred event buffer is acceptable if it retains the exact captured values and reports world teardown consistently.
6. World teardown cancels undelivered records; the receiver unbinds and clears its presentation. Register listeners before normal scheduling starts for a new mission. If late activation is supported, document that prior events are not replayed, or provide an explicitly bounded snapshot API.

This does not require exposing stack inspection, arbitrary native memory writes, or queue editing to Blueprint. Implementation effort depends on the game's actual source layout; no line-count or schedule commitment is implied.

## Content-only receiver

`InitCave.BeginPlay` obtains the authoritative game mode/spawn manager and binds once per world before capture is needed. The handler feeds an array of region records, grouped by WaveId and the existing 8 m radius, using the immutable spawn input position. Existing region count, TTL, distance, visual and settings behavior remain the acceptance baseline.

`InitSpacerig` handles settings discovery. Replace the current DLL-spawned controller and native-bound `NwiPoll` with these game-started Blueprints and event-driven updates. Preserve EndPlay cleanup, host-only scope and explicit rejection/diagnostic behavior when the required API is absent. Do not silently activate a proximity/timer fallback.

## Acceptance requirements

| Scenario | Required result |
| --- | --- |
| Automatic natural invocation | All and only its successful queue entries emit, sharing one WaveId. |
| Same descriptor/position from different simultaneous sources | Only the natural source emits; position or timestamps never determine source. |
| Failed creation, queue refusal, eventual successful retry | Zero events until success; exactly one success record for the accepted request. |
| Interleaved queue entries, reallocation and swap removal | Source/IDs remain attached to the original request. |
| Callback reentry | Original callbacks retain their order/count; no stale queue references and no inherited false provenance. |
| Pawn destroyed immediately by existing gameplay callbacks | Captured successful origin remains deliverable without dereferencing an invalid Pawn. |
| Map travel, disable and restart | No stale manager binding, duplicate listener or old-world marker. |
| Gameplay parity | No new random draws, spawn retries, changed spawn arguments or scheduler timing decisions. |
| Clean installation | The native game plus subscribed Pak and declared content dependencies suffice; no external loader or script. |

Use the existing hash-matched DLL only as a development comparator, then remove it for the final subscription test. Static source review and editor-only tests cannot establish shipping-game acceptance.
