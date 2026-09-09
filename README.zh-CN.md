<!-- 面向玩家的中文项目介绍、安装教程、设置说明与故障排查。 -->
# Enemy Wave Indicator（敌潮指示器）

[English](README.md) | [简体中文](README.zh-CN.md)

![Enemy Wave Indicator 封面](docs/media/cover-new.png)

Enemy Wave Indicator 会用发光球体、可自定义文字和距离信息，标出检测到的敌人生成区域。目前支持《Deep Rock Galactic》的自然潮，以及 35 种游戏自带的脚本虫潮。

> 本项目目前是 Windows 公开测试版，适配 Steam 版 DRG 1.40（已测试游戏版本 build 24903151）。

## 功能

- 支持自然潮和 35 种脚本虫潮，每种类型都有独立的开关和提示文字。
- Mod Hub 侧栏的注册名称固定为 `Enemy Wave Indicator`；打开后的设置内容会随游戏语言自动显示英文或简体中文，两种语言下的可编辑标记默认内容均保留英文。
- 可设置球体颜色、透明度、大小和显示时长。
- 可设置两种交替文字颜色及闪烁速度。
- 标记大小会根据成功生成敌人的数量和基础难度权重变化。
- 最多同时显示 8 个生成区域。
- 所有玩家安装匹配版本内容时，可由房主向客机同步标记。
- 本 Mod 只观察刷怪，不会改变敌人、伤害、奖励或任务进度。

0.9.1 默认开启全部虫潮播报，只有**深掘钻梯**、**执勤护送：钻进**、**核心岩事件**和**核心侵扰警告**默认关闭；警示文字默认以黄—红两色闪烁，球体 RGB 通道使用 0–1 的标准化范围。在 Mod Hub 中修改任意选项后，请点击**应用并保存**。全部类型见[虫潮类型清单](docs/WAVE-TYPES.md)。

![自然潮标记](docs/media/normal_wave_1.png)

## 安装方法

0.9.1 发布后，请从 [GitHub Releases](https://github.com/LostPatrol/EnemyWaveIndicator/releases) 下载对应版本。压缩包包含 `main.dll`、`EnemyWaveIndicator_P.pak` 和再分发依赖的许可证。

### 方式一：使用 MintCat（推荐）

1. 安装 [MintCat](https://github.com/iris-cat-dev/mintcat)，并关闭游戏。
2. 在 mod.io 订阅 [Mod Hub](https://mod.io/g/drg/m/mod-hub)。
3. 在 MintCat 中把下载的 Enemy Wave Indicator Release ZIP 作为本地 Mod 导入。
4. 启用 Enemy Wave Indicator 和 Mod Hub，点击 **Apply Changes / Save Changes**，等待安装完成。
5. 启动游戏并进入空间站。本 Mod 会在正式游戏 World 与 Mod Hub 控制器就绪后立即初始化；随后打开 Mod Hub，选择需要显示的虫潮类型，再点击**应用并保存**。

MintCat 会安装 DLL，并把它管理的 Pak 内容合并到游戏中。使用 MintCat 后，不要再手动复制同一份 Pak 或 DLL。

### 方式二：手动安装

1. 关闭游戏，并禁用 MintCat 中已安装的本 Mod，避免重复加载。
2. 通过游戏内 Mod 菜单订阅并启用 [Mod Hub](https://mod.io/g/drg/m/mod-hub)。
3. 下载兼容的 [UE4SSL 0.31.0 运行时](https://yuri-oss-hz.oss-cn-hangzhou.aliyuncs.com/releases/ue4ssl/windows/stable/0.31.0/UE4SSL.zip)。
4. 在 Steam 中打开 **Deep Rock Galactic → 管理 → 浏览本地文件**，进入 `FSD\Binaries\Win64`。
5. 把运行时按原目录结构解压到 `Win64`。完成后，`dwmapi.dll` 和 `ue4ss` 文件夹都应直接位于 `Win64` 下。
6. 打开 Enemy Wave Indicator 的 Release ZIP，复制：
   - `main.dll` 到 `FSD\Binaries\Win64\ue4ss\mods\EnemyWaveIndicator\main.dll`
   - `EnemyWaveIndicator_P.pak` 到 `FSD\Content\Paks\EnemyWaveIndicator_P.pak`
7. 启动游戏并进入空间站，待空间站完成加载后即可通过 Mod Hub 配置本 Mod。

DLL 和 Pak 必须来自同一个版本。手动更新时需要同时替换这两个文件。如果其他加载器已经安装了自己的 `dwmapi.dll`，请先确认兼容性再决定是否覆盖。

## 游戏截图

| 寻蛋任务伏击 | 矿骡修复防守 |
|---|---|
| ![寻蛋任务伏击标记](docs/media/egg_ambush.png) | ![矿骡修复防守标记](docs/media/salvage_defense_1.png) |

![古脂矿体标记](docs/media/excavation_1.png)

## 常见问题

- **Mod Hub 中没有出现本 Mod：**确认 Mod Hub 已启用，并确认空间站已经完成加载。当前 0.9.1 包没有固定启动延时，且 Mod Hub 注册接口使用与 0.9.0 已验证版本一致的稳定静态元数据。若压缩包的 SHA-256 与发布说明不同，请关闭游戏后在 MintCat 中重新导入。
- **四种默认关闭的事件没有显示：**请在 Mod Hub 中开启对应类型，再点击**应用并保存**。
- **完全没有标记：**确认 `main.dll` 和 `EnemyWaveIndicator_P.pak` 来自同一版本。房主需要安装 DLL，客机需要匹配的 Pak 内容才能显示自定义标记。
- **MintCat 提示重复或松散 Pak：**先确认 MintCat 已管理本 Mod，再删除手动安装的 `EnemyWaveIndicator_P.pak`，然后重新应用更改。
- **从很早的手动安装版本升级：**先移走或备份旧 DLL 和 Pak，再安装当前版本，避免同时加载两份。

反馈问题时，请附上任务类型、触发问题的虫潮或事件、你是房主还是客机、安装方式，以及 Mod DLL 目录旁生成的 `probe-*.jsonl` 文件。

## 当前限制

- 标记会在受支持敌人开始生成时出现，不提供提前数秒的虫潮预测。
- 新增 Mod 自定义控制器、部分直接生成的 Boss，以及少数事件自有的刷怪路径可能无法识别。
- 联机同步和中途加入尚未完成完整双机验收。房主需要原生 DLL，参与玩家需要匹配版本的内容。
- Pak 内资源根目录、Pak 文件名、手动安装 DLL 目录和当前设置存档槽均已统一为 `EnemyWaveIndicator`。内部重命名前的设置不会导入，也不会被删除。

## 许可证

项目源码使用 MIT 许可证。发布包中包含 MinHook 的许可证。《Deep Rock Galactic》及其相关资产归 Ghost Ship Games 和各自权利人所有。本项目是非官方社区 Mod。
