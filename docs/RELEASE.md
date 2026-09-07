<!-- DLL/Pak delivery, migration and moderation instructions; no publication is claimed. -->
# 发布与安装准备 — 0.9.0 测试候选

当前路线是 MintCat 自动安装 DLL+Pak。**task1 尚未全部实现，当前包仅供测试，不应以完整发行版发布。** 源码不再提供旧版手工 PowerShell 安装器。

0.9.0新增35种脚本虫潮，加自然潮共36项独立开关和文字配置，见[类型目录](WAVE-TYPES.md)。默认仅自然潮开启，测试矿骡伏击需启用 **Salvage: mini-MULE ambush** 并Apply。新NativeAbi=0x90000，必须同时更新DLL和Pak。原v2配置继续使用，新增类型默认关闭。

## 本次本机测试操作

1. 关闭 DRG。以下是安装/迁移流程；本机实际已安装版本以最新部署报告及 MEMORY.md 为准。
2. 用资源管理器把旧 `D:\Steam\steamapps\common\Deep Rock Galactic\FSD\Content\Paks\NormalWaveIndicator_P.pak` 和 `FSD\Binaries\Win64\ue4ss\mods\NormalWaveIndicator\main.dll` 备份到游戏目录之外，并从原位置移走，避免同名蓝图和旧 DLL 混用。不要移动其他 Mod、加载器或存档。
3. 在 MintCat 中使用“添加本地文件”导入新的 ZIP，保留/启用 Mod Hub，执行应用/集成。压缩包包含 `main.dll`、`NormalWaveIndicator_P.pak`、`LICENSES.txt`；不要导入旁边的 manifest，也不要再同时启用 0.7.1 纯 Pak 候选。
4. 手动启动游戏。0.8.0 初始化修复版的 DLL 会在启动后至少30秒、世界稳定至少5秒时加载并执行对应 Init 入口；入口负责房主检查及控制器去重，直接本机部署也能启动。进入空间站后等待约40秒，再重新打开 Mod Hub 验证页面和默认自然潮提示，然后执行 [TASK1-ACCEPTANCE.md](TASK1-ACCEPTANCE.md) 的排除与联机测试。此步骤由用户操作，不自动启动游戏。
5. 联机测试时房主与客机均安装匹配的 Pak；可都通过同一 ZIP 安装。未安装 Pak 的客机没有自定义蓝图、HUD和材质，不能显示同款效果。安装行为与双机显示尚未实测。
6. 如果出现问题，关闭游戏，在 MintCat 禁用新候选并重新集成，再恢复步骤 2 的两份旧文件。不要把两个版本同时启用。

代码目前核对游戏 EXE SHA256 `9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13` 和已审计的 UE4SSL 运行时 SHA256 `D1AC7156B8C8C16E46CE5CE06667457274816358329C5641CE1D2F80B53B4EB7`。这不等于兼容任意 MintCat/UE4SSL 版本；不匹配应重新审计，不能关闭校验强行加载。

## mod.io 发布时的操作

完成剩余来源类型与实机验收后：

1. 登录 [DRG 的 mod.io 页面](https://mod.io/g/drg)，创建 Mod 条目（Add mod/添加 Mod）。填写名称、摘要、完整说明、封面与实机截图。
2. 在说明中明确：DLL+Pak、推荐 MintCat 导入链接后应用、房主运行 DLL、显示端需要匹配 Pak、Mod Hub 依赖、实际支持的来源类型和构建兼容范围。不要写原生菜单订阅自动运行 DLL，也不要承诺全部敌人、提前五秒准确预测或未经验证的联机效果。
3. 在文件/版本页面上传经验证的 ZIP，填写版本号、Windows 平台与更新日志。保留 `LICENSES.txt`。不要上传整个构建目录、第三方游戏资产、报告中的私有过程数据或凭据。
4. 为该条目添加 Mod Hub 内容依赖；MintCat/UE4SSL 是安装与运行环境要求，在描述中写清楚。
5. 使用真实发布链接在 MintCat 验证首次安装、更新、禁用、重新集成、切图以及房主/客机行为。由你决定何时公开；本次没有创建条目、上传文件或发布 GitHub 二进制 Release。
6. 等待 DRG 社区/开发方审核分类；需要调整时按官方说明申请复核。

以上页面按钮名称可能随 mod.io 界面变化；上传前再核对当前页面。打包命令见根目录 README。

## Verified / Approved / Sandbox

根据 [DRG 官方 FAQ](https://www.deeprockgalactic.com/modding-support-faq)（2026-09-07 查询），新提交的 Mod **默认 Sandbox**，审核人员随后根据内容调整分类；不是作者自行决定。官方把 Verified 限定为不会显著改变其他玩家体验的本地音效或生活质量类功能。

本 Mod 提供游戏未直接展示的刷怪中心信息，并可向客机同步；未来若加入提前提示，影响还会增加。因此**不能承诺 Verified，也不能保证 Approved**。初始按 Sandbox 预期，最终由审核决定。是否含 DLL 并非在这份 FAQ 中列出的唯一分类依据。详细规则入口为 [官方分类指南](https://mod.io/g/drg/r/mod-guidelines-and-status-categories)；该网页当前需要 JavaScript，未将无法读取的细则当作已验证事实。
