<!-- Field recommendations grounded in the installed game, current mod implementation and official rules. -->
# Enemy Wave Indicator mod.io 表单

| 字段 | 当前建议 | 理由 |
|---|---|---|
| Game Version | 只勾1.40 | 本机原版Pak中FSD/Config/DefaultGame.ini的ProjectVersion为1.40.150580.0；Steam build24903151。旧版本未测试。 |
| Approval Category | 保持锁定状态 | 由平台/审核方分配，不能自己宣称Verified或Approved。 |
| Type | Gameplay、Visual、QoL | 虫潮信息提示影响战斗决策，使用可视化HUD；其他类型无需为凑标签而选。不要勾Auto-Verified，本包包含原生DLL与运行逻辑，不是仅替换声音或贴图。 |
| Requested Category | 建议[AimedForApproved] | 属于增加战斗信息的功能且不改奖励/进度，可申请Approved；这是作者申请方向，不保证最终审核结果，也不改变默认分类。 |
| Required | 现阶段RequiredByAll | 当前设计向客户端同步自定义Actor，显示需要匹配Pak；未验证没有本Mod的客户端也能完整兼容，不承诺Optional。以后混合安装联机实测通过后再评估。 |
| Visibility | 准备阶段Private；正式发布Public | 当前仍待复测文字修复及线上下载流程。Public可公开搜索；截图说明Private仅对团队、已有订阅者和有预览权限者可访问。Private不是任意链接持有者可见。 |

Approval和Required含义依据：[GSG modding FAQ](https://www.deeprockgalactic.com/modding-support-faq)及[GSG Season 02公告](https://store.steampowered.com/news/posts/?appids=548430&enddate=1651580639&feed=steam_community_announcements)。FAQ说明新提交Mod默认Sandbox，再由审核调整；Optional用于只需房主安装即可工作的Mod，RequiredByAll用于需要参与者安装的Mod。Requested Category及Type的具体勾选是结合本Mod代码范围作出的建议，不是平台已批准结果。

创建后再上传dist/EnemyWaveIndicator-0.9.0.zip，并在依赖管理添加Mod Hub。RequiredByAll表示玩家之间的安装要求，不是依赖Mod Hub的开关。当前只准备文案，没有操作mod.io表单或提交发布。
