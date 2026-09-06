<!-- Developer-only Blueprint authoring project and packaging boundary. -->
# Presentation authoring

Open FSD.uproject with Unreal Engine 4.27.2. The project name FSD is required by the game content mount path. The editor-only NwiAuthoring plugin generates seven Blueprints and one material under Content/NormalWaveIndicator.

- BP_NwiAuto owns a fixed pool of eight sphere/widget pairs and implements Mod Hub's IHubMod interface.
- WBP_NwiSettings implements IHubPageWidget. SG_NwiSettings persists local options.
- WBP_NwiMarker updates projection each frame and formats distance only when rounded meters change. Text emphasis is a bounded 1.5 Hz opacity pulse.
- BP_NwiPulse reuses its material instance, has no collision, overlap, shadow, navigation or replication, and disables Tick while hidden.
- BP_NwiResources asynchronously loads the installed game's mini-MULE Scale/Alpha curves and our own material. It does not spawn a mini-MULE.
- BP_NwiVisualTest is an internal bootstrap/resource base and optional F5 visual diagnostic. It does not run the retired one-second HUD.

Run Prepare-ModHubDevkit.ps1 before authoring. Its three pinned interface assets are local editor references only. Never include Content/_ModHub, Content/NwiValidation, editor binaries, original game assets or shared FSD shader libraries in a release Pak. Cook-PresentationAssets.ps1 allows only the sixteen original runtime files.

Test-PresentationAssets.ps1 generates assets and validates them in a fresh process using isolated UWorld instances. Tests cover 1,452 edge cases, five worlds / 813 world ticks, pooled native calls, resource lifetime, the actual settings button and disk round trip, Mod Hub page return/reuse, actual pawn distance text, and disabling pulsing. H-menu discovery in the installed game and final GPU appearance still require playtesting.