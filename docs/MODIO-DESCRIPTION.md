<!-- Ready-to-paste mod.io listing draft; no unpublished URL or unverified acceptance claim. -->
# Enemy Wave Indicator — 0.9.0 Public Beta / 公开测试版

## Summary

Marks enemy wave spawn locations with glowing spheres, customizable text and distance indicators. Supports natural swarms and 35 scripted wave types, with individual toggles and labels plus shared appearance settings in Mod Hub.

## Description — English, ready to paste

**Enemy Wave Indicator** helps you see where enemy waves are spawning. It adds glowing spheres and floating text with distance readouts at detected spawn locations, making it easier to spot threats and prepare your position.

The current public beta supports natural waves and 35 stock scripted wave types, including mini-MULE ambushes, egg-hunt ambushes, escort stages, refinery waves and extraction waves. Each type has its own enable toggle and custom label. **Only natural waves are enabled by default**—open Mod Hub to enable the other types you want.

Customize sphere color, opacity, size and display duration, plus text colors and flashing frequency. Appearance settings are shared across wave types. Sphere size also scales with the number and base difficulty weights of successfully spawned enemies. Up to eight spawn areas can be displayed at once. Settings are saved between sessions.

**Installation:** Subscribe to this mod and [Mod Hub](https://mod.io/g/drg/m/mod-hub), add both to MintCat and apply changes. MintCat installs the included DLL and Pak. Subscription alone does not install the native DLL. Without MintCat, enable the subscribed Pak through the in-game mod menu and install the compatible UE4SSL runtime and bundled DLL manually; follow the [installation guide](https://github.com/LostPatrol/NormalWaveIndicator/blob/main/docs/RELEASE.md). After starting the game, allow about 40 seconds in the Space Rig, then open Mod Hub and click **Apply and save** after changing settings. For mini-MULE ambushes, enable **Salvage: mini-MULE ambush**.

**Compatibility and multiplayer:** The current build targets Steam Windows game version 1.40 (tested build 24903151) with UE4SSL 0.31.0. The host needs the native DLL, and clients need matching mod content to display markers. Multiplayer synchronization and late joining are still undergoing validation; do not assume this is a client-only mod that works in an unmodded host's lobby.

**Scope:** This mod observes spawning; it does not change enemy generation, damage, rewards or progression. Markers appear as supported enemies spawn, rather than predicting waves several seconds in advance. Independent boss summons, some event-specific spawning paths and new mod-defined wave controllers are not fully covered. This is a public beta; please report missing markers or incorrect wave labels with the mission type and reproduction steps.

[Supported wave types](https://github.com/LostPatrol/NormalWaveIndicator/blob/main/docs/WAVE-TYPES.md) · [Roadmap](https://github.com/LostPatrol/NormalWaveIndicator/issues/1) · [Source and feedback](https://github.com/LostPatrol/NormalWaveIndicator)

## 中文说明参考

为虫潮生成位置添加光球与HUD文字提示。支持自然潮以及当前游戏35种明确的虫潮控制器，共36种类型。每类可独立启用并设置提示文字，默认仅显示自然潮。

球体大小会随成功生成敌人的数量和基础权重变化。Mod Hub可设置球体颜色、透明度、大小、显示时长，以及文字的两种闪烁颜色与频率。自然潮排除小型单位及环境生物。

Marks wave spawning locations with spheres and HUD labels. Includes natural waves and 35 stock scripted wave-controller types, with separate toggles and labels. Only natural waves are enabled by default. Appearance settings are shared across types.

## 安装 / Installation

**推荐MintCat0.5.5：**订阅本Mod和Mod Hub，在MintCat“添加Mod→mod.io订阅”中加入，或在“在线”粘贴本页面链接；启用后点击保存更改。MintCat会自动安装DLL和Pak，无需玩家运行脚本。只订阅而不导入/应用，不会完成DLL安装。

Recommended: add this mod and Mod Hub to MintCat, then Apply Changes. The ZIP includes both the native DLL and Pak. Subscription alone does not install the native DLL.

**不用MintCat：**原生菜单订阅并启用本Mod和Mod Hub，退出游戏后安装兼容的UE4SSL 0.31.0运行时，再把本包main.dll放到 `FSD/Binaries/Win64/ue4ss/mods/NormalWaveIndicator/main.dll`。[完整手动安装与兼容说明](https://github.com/LostPatrol/NormalWaveIndicator/blob/main/docs/RELEASE.md)。请勿混用不匹配的Pak、DLL或其他UE4SS版本。

启动后在空间站等待约40秒，再打开Mod Hub。启用新增虫潮类型后点击Apply and save；矿骡伏击对应Salvage: mini-MULE ambush。

## 当前状态 / Current status

此前主要功能经作者实机测试反馈正常；随后收到文字缺失反馈，已增加HUD恢复修复并通过自动测试，修复后实机确认仍待完成。MintCat本地ZIP导入与安装文件校验已通过。原生订阅加手动DLL安装的干净环境验证，以及双机/晚加入测试尚待完成。

支持Steam Windows 1.40（本机ProjectVersion=1.40.150580.0，build24903151）及已核对的UE4SSL 0.31.0运行时。不会改动游戏刷怪、伤害或奖励，不提供提前预测。最多同时显示8个生成区域。复用Generic控制器的不同触发原因共用同一类型。

独立Boss召唤、非虫潮控制器的事件自有生成路径及新增Mod控制器尚未全部覆盖。客机显示需要安装匹配Pak，房主需要完整DLL安装；当前不承诺已完成双机验收。

[完整类型清单](https://github.com/LostPatrol/NormalWaveIndicator/blob/main/docs/WAVE-TYPES.md) · [后续TODO](https://github.com/LostPatrol/NormalWaveIndicator/issues/1) · [开源代码与反馈](https://github.com/LostPatrol/NormalWaveIndicator)

非官方Mod，与Ghost Ship Games无隶属关系。原始代码MIT许可，压缩包内包含所用MinHook许可。
