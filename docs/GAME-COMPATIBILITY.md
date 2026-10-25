<!-- Binding discovery design and the maintained entry points for checking and adapting game updates. -->
# Game compatibility

The native observer uses one reviewed structural profile rather than fixed game RVAs. It still requires the original normal-wave scheduler provenance, exact scripted source class, successful spawn/request association and actual batch center. Settings, filtering, presentation and replication do not derive their identity from guessed active controllers or newly created enemies.

## Where the implementation lives

| File | Responsibility |
|---|---|
| `mods/EnemyWaveNativeProbe/GameBindingProfile.json` | Reviewed instruction spans, address-operand masks, call relationships, queue ABI windows and known release identities. `baselineRva` is audit evidence, not a runtime lookup. |
| `scripts/Generate-GameBindings.cjs` | Validate the profile and generate the deterministic native table; optionally extract a separate candidate from reviewed locations. |
| `mods/EnemyWaveNativeProbe/GameBindingResolver.h` | Read mapped PE metadata, find unique instruction shapes, check full-span hashes, calls, queue windows and the thread initialization/import relationship. Shared by production and offline tools. |
| `mods/EnemyWaveNativeProbe/GameBindings.h` | Discover once per process and retain the resolved image and diagnostic state. |
| `mods/EnemyWaveNativeProbe/EngineThreadIdentity.h` | Read the resolved engine thread scalars; no fixed thread-global address. |
| `mods/EnemyWaveNativeProbe/AutomaticPresentation.cpp` | Revalidate the resolved spans before installing, then construct the unchanged capture contract from actual discovered entries and returns. |
| `mods/EnemyWaveNativeProbe/GameCapture.cpp` | Original hook forwarding, exact provenance and queue association. It compares discovery-time entry snapshots before MinHook installation. |

For a normal code move, no production profile edit should be needed if all reviewed instruction shapes and relationships remain compatible. A new full-file SHA is reported separately from structural compatibility. Unknown hashes do not hide individual missing/ambiguous candidates, and a structural match is not gameplay or release acceptance.

## What discovery verifies

Short masked anchors narrow the search. Each candidate must also match the complete normalized span and be unique. Function entries come from valid PE unwind records; the thread-store anchor is scanned separately and tied to the named `GetCurrentThreadId` import and readable non-code scalar storage.

Only RIP-relative address operands and calls/branches leaving an audited span are normalized. Internal branch distances, opcodes, registers, stack/field offsets and ordinary immediates remain significant. Actual call targets and return addresses establish the source chain; separate windows verify the queue fields consumed by the observer. A normalized hash is an instruction-shape check, not proof that every external callee retains its semantics.

Discovery is cached, but installation rechecks the complete resolved spans and relationships without scanning again. Entry snapshots remain those captured at discovery. If another mod changes them meanwhile, initialization rejects the changed code. `binding_matched` describes initial discovery; `auto_fault=4` indicates that pre-installation revalidation failed. Existing `hook_status=100` still indicates an entry mismatch at the final installation guard.

Discovery does not promise compatibility with changed parameters, layouts, control flow, inlining or arbitrary patches from other mods. Review those changes rather than enlarging masks to make a mismatch disappear. The UE4SSL runtime ABI remains separately pinned and checked.

## Maintained commands

Run from the project root using the existing Node, PowerShell 7, MSVC x64 and UE4.27 environment. None of these inspection commands starts the game.

```powershell
node scripts/Generate-GameBindings.cjs --check
node scripts/Test-GameCompatibility.cjs '<game.exe>' 'agent/codex/<run>/compatibility.json'
node scripts/Test-GameBindingDiscovery.cjs '<game.exe>' 'agent/codex/<run>/discovery'
```

The compatibility command compiles or reuses a source-addressed native inspector. Its JSON includes the full-file identity, known-baseline flag, all candidate counts, resolved entries/returns, profile digest and a specific failure reason. Add `--require-known` when validating an explicitly reviewed release baseline; the default permits structurally compatible unknown hashes while labeling them unknown. Failure diagnostics are saved before the CLI exits unsuccessfully.

The discovery test copies matching real instruction spans into two independently relocated, non-executable PE-shaped fixtures. It also changes a call target, a queue field and uniqueness to ensure these incompatible cases are rejected. It does not turn the fixture into evidence of another supported Steam build.

## When instruction structure changes

Use the existing Node/iced-x86 audit workflow to establish new entry/span boundaries, parameter and return ABI, both enqueue branches, actual source-chain calls, successful creation and swap-removal layout. Inspect the affected stock Blueprint routes when a source path or reflected contract changes. Keep the existing source/request/center semantics.

The following command can regenerate mechanical masks and hashes **after the new locations and offsets have been reviewed**:

```powershell
node scripts/Generate-GameBindings.cjs --refresh '<game.exe>' --profile '<reviewed-locations.json>' --decoder '<existing iced-x86 module>' --output 'agent/codex/<run>/candidate-profile.json'
node scripts/Generate-GameBindings.cjs --profile 'agent/codex/<run>/candidate-profile.json' --output 'agent/codex/<run>/candidate.generated.h'
```

The input uses the production schema with reviewed `baselineRva`, span sizes and call/window offsets. The extractor does not find those locations, audit changed semantics or install a decoder. It writes separately and clears `knownBuilds`; it does not approve the candidate. Compare decoded instructions and generated changes before adopting it. Update core field accessors if an actually changed layout requires that change; do not merely rewrite a window to accept incompatible reads.

After adoption, register the reviewed build identity, generate the production header and run the real-image/discovery checks. The regular native build checks header freshness and runs resolver ambiguity, mutation, import and snapshot tests alongside the existing hook and attribution tests. Packaging requires that resolver validation succeeded and reads baseline metadata from the profile.

## Build and acceptance

Use the paths returned by these existing stages:

```powershell
$native = & ./scripts/Build-NativeProbe.ps1
$authoring = & ./scripts/Build-EditorAuthoring.ps1
$assets = & ./scripts/Test-PresentationAssets.ps1 -AuthoringBuild $authoring
$cook = & ./scripts/Cook-PresentationAssets.ps1 -VerificationDirectory $assets
& ./scripts/Prepare-ModioRelease.ps1 -BuildDirectory $native -PresentationCook $cook
```

Native and authoring builds may run independently; the later stages depend on their actual verified outputs. Preserve the SaveGame class/slot and NativeAbi unless a separately designed migration requires otherwise. Verify final ZIP payload hashes. Local packaging does not install, launch or publish the mod.

The normal native build keeps a bounded `probe-latest.jsonl` with lifecycle and changed startup/fault status; missing log access does not block initialization. Use `Build-NativeProbe.ps1 -Diagnostics` only when full five-second snapshots and event timing are needed. The normal build skips prediction tests and excludes their interfaces/statistics. `-PredictionTest` retains the separate archived experiment and implicitly enables diagnostics; its old bindings remain outside production compatibility support. The experimental label is generated only with the editor command-line flag `-EwiPredictionTest`.

User-led gameplay should confirm `binding_matched=true`, valid engine identity, successful hook/Auto binding, one natural wave and one enabled scripted wave with text/spheres, then consecutive missions. Delayed viewport discovery must continue retrying after its initial fast attempts. Multiplayer requires matching peers and separate acceptance. Keep static, synthetic, installed, gameplay and public-download evidence distinct.
