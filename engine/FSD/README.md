<!-- Native handoff and editor-only Blueprint build boundary. -->
# Presentation authoring — 0.9.1

Open FSD.uproject in UE4.27.2. FSD is the required content mount name, not a shipped native module. The editor-only NwiAuthoring plugin emits nine production assets: Auto, Resources, Pulse, Marker, Settings page, Settings SaveGame, Red Material, InitCave and InitSpacerig.

Auto is an always-relevant replicated actor created by host initializers. The native DLL binds only Auto.NwiPoll (zero arguments), validates NativeAbi=0x90000, and writes owned region fields. Clients skip native polling and render replicated regions. Clock expiry uses server world time; display preferences and visual pools remain local. Each pulse owns one reusable MID and has no collision, gameplay navigation or replication.

Settings expose 36 independent wave toggles and labels in a compact three-section layout. All sources default on except IDs 1, 6, 32 and 34. Text defaults to yellow/red 2 Hz alternation; sphere appearance remains independently editable. The local v3 SaveGame intentionally leaves the old v2 file untouched so the 0.9.1 defaults take effect. The preview checks draft values without saving. Native source, filtering and measured timing are described in the repository README.

Prepare-ModHubDevkit.ps1 installs pinned development interface references. Never package _ModHub, NwiValidation, editor binaries or original game assets. The cook admits exactly 18 owned files and verifies their Pak hashes. The obsolete FSD reflection stub and approximate content-capture graph have been removed. BP_NwiVisualTest remains an unshipped resource/display regression fixture, not Auto's parent.

Test-PresentationAssets.ps1 generates assets and cold-loads them in another editor process. It tests 1,452 HUD edge cases, actual saved-graph settings/preview/save/reload, material opacity, pool reuse, expiry, distance formatting, native ABI, replicated property metadata and initializer ownership. These tests do not establish gameplay source coverage, MintCat installation, actual network traffic or GPU appearance.
