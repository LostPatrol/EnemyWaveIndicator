<!-- Public-beta upload steps and two distinct installation routes, with measured acceptance boundaries. -->
# 0.9.0 公开测试版：发布与安装

用户此前反馈主要功能正常，随后报告只有光球、没有文字。2026-09-08修复包补上文字控件脱离视口后的恢复，并提高默认显示层级；真实Slate视口回归通过，实机文字恢复仍待确认。发布前先复测此问题。当前仍定位为**已有36类虫潮功能的公开测试版**，不要称为完整task1或稳定版。后续计划统一在[TODO issue #1](https://github.com/LostPatrol/NormalWaveIndicator/issues/1)，发布说明草稿见[MODIO-DESCRIPTION.md](MODIO-DESCRIPTION.md)。

## 作者：在 mod.io 发布

1. 登录[DRG mod.io](https://mod.io/g/drg)，使用添加Mod入口创建条目。名称建议 `Normal Wave Indicator`，摘要说明虫潮生成位置、HUD/光球提示和36类开关。首次版本写 `0.9.0`，说明中明确“公开测试版”。
2. 填写描述，上传一张封面和实际游戏截图；可直接使用本仓库的发布说明草稿。添加[Mod Hub](https://mod.io/g/drg/m/mod-hub)作为内容依赖；MintCat与UE4SSL作为安装/运行要求写入说明。不要把开发用接口包设为依赖。
3. 在文件管理入口上传 `dist/NormalWaveIndicator-0.9.0.zip`，选择Windows，填写版本号及更新说明，并将该文件设为当前可下载版本。ZIP根目录必须是 `main.dll`、`NormalWaveIndicator_P.pak`、`LICENSES.txt`；不上传manifest、游戏文件或整个工作目录。
4. 初次可使用Hidden供本人检查条目和文件，再切换Public提供测试。是否需要平台审核取决于DRG规则；Hidden也有访问限制，不能保证任意测试者拿链接就能下载。[mod.io状态和可见性说明](https://docs.mod.io/restapi/status-and-visibility)。
5. 发布后，订阅自己的条目，在MintCat“添加Mod→mod.io订阅”选择它，或“在线”粘贴页面URL，然后保存更改。先禁用本地测试条目，避免两份同资产/DLL同时加载。验证真实线上首次下载、更新及禁用流程。
6. 再验证下方的非MintCat路线。它需要一个没有MintCat合并包参与的安装环境；不要仅凭本机已有加载器就宣称干净环境订阅测试通过。

截至2026-09-07，本轮只创建了GitHub TODO issue；没有代为创建mod.io条目、上传或公开文件。原构建manifest里的ReleaseReady=false表示完整稳定发行验收未完成，不阻止明确标注范围的公开测试计划。

当前本地0.9.0文字修复包（2026-09-08）SHA256：`13714FB15529CD826D8A03354A6C5DF8E090D6FC3AE73D7FA19FCE94D5E136E2`。尚未发布，版本号沿用0.9.0；旧本地ZIP已备份。

## 玩家路线 A：订阅后由 MintCat 安装

1. 在mod.io订阅本Mod及Mod Hub，关闭游戏。
2. MintCat登录相同mod.io账号，点“添加Mod→mod.io订阅”，选择本Mod加入列表。也可以在“在线”粘贴本Mod的mod.io页面链接；订阅本身不会自动把未导入的条目加入当前配置。
3. 确认本Mod和Mod Hub开启，点击左上角保存更改，等到“安装完成”。MintCat会安装ZIP中的DLL，并把Pak内容合并进它管理的Pak。玩家不用手动复制文件或运行脚本。
4. 启动DRG，进入空间站等待约40秒，再打开Mod Hub。默认只开自然潮；其他类型需要勾选并Apply and save。矿骡伏击对应 `Show Salvage: mini-MULE ambush`。
5. 更新时在MintCat更新该条目并保存更改；不要同时再导入第二份本地包。

此路线与Enemy Wave Timer作者说明中的“粘贴mod.io URL并Apply Changes自动安装DLL”相同。[Enemy Wave Timer](https://mod.io/g/drg/m/enemy-wave-timer)、[MintCat源码](https://github.com/iris-cat-dev/mintcat)。

### 当前本地 ZIP 已验证

2026-09-07 22:59，MintCat0.5.5从 `E:\DRGModDev\dist\NormalWaveIndicator-0.9.0.zip` 导入、启用并集成成功。界面显示名称已改为NormalWaveIndicator。内部安装路径仍取本地文件原名称：

- DLL：`FSD\Binaries\Win64\ue4ss\mods\NormalWaveIndicator-0.9.0.zip\main.dll`。
- 内容：合并在 `FSD\Content\Paks\FSD-WindowsNoEditor_Mods.pak`，没有额外的 `NormalWaveIndicator_P.pak`。
- DLL与原测试包hash一致，合并包内本Mod18项资产逐项hash一致；Mod Hub接口也存在。禁用并应用后确认0个本Mod DLL/资产；重新启用并应用后再次确认DLL与18项资产一致，最终保持启用。

原直装文件已移到 `agent/codex/publish-mintcat-20260907-2253/before-import` 保留。图片中的警告正是原来手动放在Paks目录的独立Pak触发的，并非Mod本身损坏。以后由MintCat管理时，不要再复制同一Pak回游戏目录。导入后未自动启动游戏；线上mod.io链接流程尚待条目发布后测试。

2026-09-08 00:03，沿用同一本地条目重新应用文字修复ZIP，MintCat显示“安装完成”。游戏合并Pak的18项资产与新Cook逐项一致，DLL保持原hash，只有一个本Mod DLL目录，v2设置存档hash未改变。备份与安装核对记录在 `agent/codex/hud-recovery-20260908-0003`。此次未启动游戏，文字修复的实机效果仍待复测。

## 玩家路线 B：原生订阅 + 手动 DLL 安装（不用 MintCat）

这条路线需要**原生菜单启用Pak，以及外部加载器加载DLL**。仅点订阅不会自动执行DLL。当前DLL兼容经过审计的UE4SSL 0.31.0，不是任意版本的标准UE4SS。手动文件安装可按以下步骤完成，无需运行MintCat：

1. 在游戏原生Mod菜单订阅、下载并启用本Mod及Mod Hub，然后完全退出游戏。原生订阅负责本Mod的Pak内容；不要同时保留MintCat合并的同一份内容。
2. 在Steam“管理→浏览本地文件”打开DRG目录，进入 `FSD\Binaries\Win64`。
3. 下载[兼容运行时 UE4SSL 0.31.0](https://yuri-oss-hz.oss-cn-hangzhou.aliyuncs.com/releases/ue4ssl/windows/stable/0.31.0/UE4SSL.zip)，保持目录结构解压到上述Win64目录。它包含 `dwmapi.dll` 和 `ue4ss` 文件夹，不是把所有文件平铺。此下载来自MintCat官方更新清单，本轮重新下载校验；不必安装MintCat程序。
4. 从本Mod的mod.io文件页面手动下载**与订阅版本一致**的ZIP。取其中 `main.dll`，放到 `FSD\Binaries\Win64\ue4ss\mods\NormalWaveIndicator\main.dll`，缺少目录则创建。Pak已由原生菜单启用，不要额外复制一份到Paks目录。
5. 启动游戏，在空间站等约40秒，再打开Mod Hub确认版本与设置；做一次自然潮测试。更新Mod时，订阅Pak更新后还须手动同步替换DLL。禁用时退出游戏，将本Mod的DLL移到游戏目录外，再在原生菜单禁用内容；不要移走其他Mod共用的加载器。

最终应有：

```text
FSD/Binaries/Win64/
  dwmapi.dll
  ue4ss/
    UE4SSL.dll
    mods/
      UE4SSL.JavaScript/main.dll
      UE4SSL.JavaScript.Framework/js/main.js
      NormalWaveIndicator/main.dll
```

如果目录已有不同的dwmapi.dll或不同运行时，先确认其归属及兼容性，不要覆盖其他加载器。当前支持Steam Windows游戏build24903151；游戏或加载器更新后，需要重新核对兼容范围。

手动路线证据边界：运行时下载包、目录结构和DLL兼容hash已核对；此前本机直装DLL+Pak已由用户测试主要功能。**尚未完成新mod.io条目在干净原生订阅环境中的端到端验证**，因此发布说明需保留此状态，不能把它写成实测通过。

### 兼容校验值

- 游戏EXE：`9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13`。
- UE4SSL.dll：`D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7`。
- 本Mod main.dll：`F4BB94269AD83761D8C3A8EEE976E49B5A6938C8D4CA2A508064F3872F3CFFCC`。
- 本Mod Pak（2026-09-08文字修复）：`7F3DBB260F30B8ADEE61292592AF0638CF71CEE5D4647007B944FD6865435F30`。
- 运行时ZIP MD5：`B715F195B448481BBF28FAF53EAC8AE8`（官方清单提供值，下载复核一致）。

## 联机与审核分类

显示端需要同版Pak；房主需要DLL。代码有同步支持，但双机、晚加入和切图尚未完整实测，不把它们作为0.9.0已验收功能宣传。客户端无Pak不能显示自定义HUD/光球。

[DRG官方FAQ](https://www.deeprockgalactic.com/modding-support-faq)说明新Mod默认Sandbox，再由审核人员调整分类。刷怪中心提示会增加游戏信息，不能保证Verified或Approved。公开测试版可以先按Sandbox预期准备，最终以实际审核为准。
