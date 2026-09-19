<!-- Player-facing overview, installation guide, configuration notes, and troubleshooting. -->
# Enemy Wave Indicator

[English](README.md) | [简体中文](README.zh-CN.md)

![Enemy Wave Indicator cover](docs/media/cover-new.png)

Enemy Wave Indicator marks detected enemy spawn areas with glowing spheres, customizable labels, and distance readouts. It currently supports normal waves and 46 stock scripted waves.

> This is a Windows public beta. The current build targets the Steam version of DRG 1.40 (tested build 24903151).

## Features

- Normal waves plus 46 scripted wave types, each with its own enable switch and label, including direct Tritilyte Deposit, Ebonite Mutation, and Kursite Infection spawns.
- Mod Hub `Enemy Wave Indicator`; includes English and Simplified Chinese.
- Configurable sphere color, opacity, size, and display duration.
- Two alternating text colors with configurable flashing speed.
- Marker size scales with the number and base difficulty weight of spawned enemies.
- Up to eight spawn areas displayed at once.
- Host-to-client marker synchronization when every player has matching mod content.
- Read-only: it only reads game data and does not change wave-spawning logic.

Every broadcast is enabled by default except **Drillevator**, **Escort: drilling**, **Core Stone event**, **Core Corruption warning**, and **Haunted Cave**. See the [complete wave-type list](docs/WAVE-TYPES.md).

![Normal wave markers](docs/media/normal_wave_1.png)

## Installation


### 1. MintCat (recommended)

1. Install [MintCat](https://github.com/iris-cat-dev/mintcat) and close the game.
2. In MintCat, choose **Add mod** → **mod.io subscribe** → **OK**.
3. In the MintCat UI, confirm that Enemy Wave Indicator and Mod Hub are enabled.
4. The first time, start the game from MintCat and check that this mod's settings appear in Mod Hub. Later launches can start from Steam.

MintCat installs the DLL and merges the Pak content it manages. No manual DLL or other extra install steps are required.

### 2. Manual installation

1. Close the game and disable any MintCat installation of this mod to avoid loading it twice.
2. Subscribe to and enable [Mod Hub](https://mod.io/g/drg/m/mod-hub) through DRG's in-game mod menu.
3. Download the compatible [UE4SSL 0.31.0 runtime](https://yuri-oss-hz.oss-cn-hangzhou.aliyuncs.com/releases/ue4ssl/windows/stable/0.31.0/UE4SSL.zip).
4. In Steam, open **Deep Rock Galactic → Manage → Browse local files**, then enter `FSD\Binaries\Win64`.
5. Extract the runtime into `Win64` while preserving its directory structure. `dwmapi.dll` and the `ue4ss` directory should both be directly inside `Win64`.
6. Open the Enemy Wave Indicator release ZIP and copy:
   - `main.dll` to `FSD\Binaries\Win64\ue4ss\mods\EnemyWaveIndicator\main.dll`
   - `EnemyWaveIndicator_P.pak` to `FSD\Content\Paks\EnemyWaveIndicator_P.pak`
7. Start the game, enter the Space Rig, then configure the mod in Mod Hub once the Space Rig finishes loading.

When updating manually, replace both files. If another loader already owns `dwmapi.dll`, verify compatibility before replacing it.

## Screenshots

| Egg Hunt ambush | Salvage defense |
|---|---|
| ![Egg Hunt ambush marker](docs/media/egg_ambush.png) | ![Salvage defense marker](docs/media/salvage_defense_1.png) |
| Excavation | Mod Hub settings |
| ![Excavation marker](docs/media/excavation_1.png) | ![Enemy Wave Indicator Mod Hub settings](docs/media/modhub-new-1.png) |

## Troubleshooting

- **The mod is missing from Mod Hub:** confirm that Mod Hub is enabled and that the Space Rig has finished loading.
- **One of the five default-disabled sources does not appear:** enable its matching type in Mod Hub and select **Apply and save**.
- **Nothing appears:** verify that `main.dll` and `EnemyWaveIndicator_P.pak` are from the same version. The host needs the DLL; clients need matching Pak content to render the custom markers.
- **MintCat reports a duplicate or loose Pak:** remove the manually installed `EnemyWaveIndicator_P.pak` after confirming MintCat manages the mod, then apply changes again.
- **You are updating a very old manual installation:** remove or back up the old DLL and Pak before installing the current pair. Do not load two copies.

When reporting a problem, include the mission type, the wave/event that triggered it, whether you were host or client, your installation method, and any `probe-*.jsonl` file created beside the mod DLL.

## Current limitations

- Markers appear when supported enemies begin spawning. This mod currently cannot predict where a wave will appear.
- New mod-defined controllers, some direct boss summons, and a few event-specific spawning paths may not be identified.

## License

Source code is licensed under MIT. Distributed packages include the MinHook license. Deep Rock Galactic and its assets belong to Ghost Ship Games and their respective owners. This is an unofficial community mod.
