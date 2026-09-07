<!-- Current native test baseline; task1's all-source and prediction work is explicitly incomplete. -->
# Enemy Wave Indicator — 0.9.0 test candidate

DRG enemy-wave spawn markers using **DLL + Pak**, installed through MintCat. This is an unpublished test candidate, not completion of every requirement in `task1-refine.md`.

Previously named Normal Wave Indicator. The public name and Mod Hub page now use Enemy Wave Indicator; existing asset paths, DLL identifiers and save slots retain their original names for compatibility. Upload package: `dist/EnemyWaveIndicator-0.9.0.zip`. [Publication cover](docs/media/enemy-wave-indicator-cover-v1.png).

## Implemented

- Natural provenance requires six exact native call-chain returns, including the natural scheduler. Ownership follows each accepted queue request through swap removal and successful actor creation; ambient/boss/event requests are not relabeled because a natural wave is active.
- Natural waves require registered regular enemies; explicitly enabled scripted wave types also admit registered small enemies. Critters and unknown/unregistered actors are excluded; small membership overrides regular membership for natural waves. Hoarder/Huuli names are additionally excluded. Source attribution does not depend on the summoned enemy's descriptor, so changing a summon to a grunt does not make that summon natural.
- Native generation scopes identify all 35 concrete stock wave-controller classes; each queued entry keeps its own source type and request identity. The shared batch helper supplies the actual selected center, including multi-center requests. Successful requests retain this center; unavailable centers fall back to the queued origin. Eight regions are supported. Distinct known centers and wave identities stay separate. Overflow is counted and dropped.
- Sphere size uses the sum of successful enemies' **base descriptor `DifficultyRating`**, normalized to 200 and cube-root scaled (0.4–4 multiplier). It grows with count and cost. It does not claim to reproduce all runtime difficulty modifiers or precise combat threat.
- Mod Hub: 36 independent wave-type enable/text pairs, natural only by default, global size, duration, sphere RGB and opacity (default 0.4), text color A/B and frequency (default red/white, two complete cycles/second). Draft RGBA swatch and text previews update before Apply; Apply persists `NormalWaveIndicator_v2.sav`. Preview is a color swatch, not a 3D sphere/GPU preview. Old v1 preferences are left intact and not imported.
- A host-created, always relevant Blueprint actor replicates source type, region points, size, visibility and expiry. Clients with the matching Pak draw their own HUD/spheres and use synchronized server time. **Network transport and late-join behavior still need real two-peer testing.** Clients without the Pak cannot display these custom assets. The DLL is required on the host.
- Diagnostic logs measure actual selected-center→spawn, queue→spawn and spawn→native handoff intervals. They do not represent a rendered GPU frame or a prediction.

## Planned work

The 35 concrete stock EWC types and natural waves have independent toggles/text and request provenance; see [the complete type catalog](docs/WAVE-TYPES.md). Non-EWC boss/direct summons, machine-event spawning components and new Mod-defined controller classes are not covered by this catalog. Generic controllers reused by several triggers remain one code type. This is not universal enemy-source coverage or completed task1.

One-to-five-second exact position prediction is not established: the current game chooses a player, RNG and navigation location when triggering the wave. No early game function invocation, RNG consumption, enemy delay or spawning change is used. See [test instructions and acceptance gaps](docs/TASK1-ACCEPTANCE.md).

Future work is tracked in [TODO issue #1](https://github.com/LostPatrol/NormalWaveIndicator/issues/1). Following broad positive gameplay feedback, the author reported missing text with spheres still visible. The September 8 HUD fix restores active widgets detached during HUD cleanup and places them above the default viewport layer. A real Slate attachment regression passes; gameplay confirmation of this fix is pending. Online mod.io delivery and clean native subscription/manual-loader installation remain to be tested.

## Player installation

Close DRG, import the generated ZIP into MintCat using its local-file import, enable Mod Hub and apply/integrate. New players do not run PowerShell. Host and participating clients need matching content. This candidate is pinned to the audited game build and UE4SSL runtime; incompatible binaries do not install observation hooks.

The old 0.6.0 manually installed DLL/Pak must be backed up and disabled before importing the new package. Updating through MintCat does not prove it removed a separately installed loose Pak. Detailed migration, testing and mod.io submission steps are in [RELEASE.md](docs/RELEASE.md).

## Developer build

Windows PowerShell, Visual Studio C++ x64/Windows SDK/.NET Framework 4.8 SDK, UE **4.27.2**, and Node.js are required. Python used inside UE is its bundled project/editor interpreter; no global Python environment is needed.

```powershell
.\scripts\Prepare-ModHubDevkit.ps1
$native = .\scripts\Build-NativeProbe.ps1
$editor = .\scripts\Build-EditorAuthoring.ps1
$check = .\scripts\Test-PresentationAssets.ps1 -AuthoringBuild $editor
$cook = .\scripts\Cook-PresentationAssets.ps1 -VerificationDirectory $check
.\scripts\Prepare-Release.ps1 -BuildDirectory $native -PresentationCook $cook
.\scripts\Test-SpawnAttribution.ps1
.\tests\modio-package.test.ps1
```

The Pak contains nine owned asset pairs. There is no runtime FSD stub, authoring module, validation asset or copied game asset. The ZIP includes `main.dll`, `NormalWaveIndicator_P.pak`, and `LICENSES.txt`. The original build manifest remains `ReleaseReady=false` for full stable-release acceptance. Public beta preparation and measured local MintCat import results are documented in [RELEASE.md](docs/RELEASE.md).

## License and references

Original source: MIT, LostPatrol. MinHook's license is included in distributed ZIPs. Mod Hub interface references come from [trumank/drg-mods](https://github.com/trumank/drg-mods). DRG and its assets belong to Ghost Ship Games and their respective owners. This is an unofficial Mod. Historical architecture investigations in `docs` are evidence, not descriptions of current implementation.
