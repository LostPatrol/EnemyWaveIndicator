<!-- Explain notification coverage, stock counting buckets and the limits of static boss-path evidence. -->
# 显示触发、小型单位过滤与 Boss 召唤审计

审计日期：2026-09-07。源码候选从 0.7.0 更新为 **0.7.1 beta**；未启动游戏、未替换本机旧 0.6.0 安装、未上传 mod.io。

## 何时显示

0.7.0 监听 `EnemySpawnManager.OnEnemySpawned(enemy, descriptor)`，每条有效 Pawn 通知都可以触发显示，**没有最低数量、单位生成速率或“正式虫潮”门槛**，也不要求任务控制中心播报。不是全世界所有 Actor 创建都会触发。

0.7.1 在这条通知链前增加过滤：如果 Pawn 位于当前刷怪管理器的 `ActiveSwarmerEnemies` 或 `ActiveCritters`，立即忽略。其余有效通知仍可由单只敌人触发。过滤不写入采样、计数或区域，因此小型单位不会创建区域、延长普通区域的寿命或占用八个显示名额。

收到符合条件的通知后，复制该时刻的位置。同一上下文标签、距离既有区域首点不超过 8 米、区域尚未到期，则保留首点并续时；否则创建区域，满八个后替换最早到期项。资源和房主本地控制器就绪后由显示 Tick 绘制，默认保持 8 秒。不是预测生成前的位置，也不追踪敌人后续移动。

活动脚本潮控制器只决定近似上下文文字，**不决定是否允许显示**。没有上下文的 Boss 召唤、零散生成也可能得到 `Unknown / possible natural wave`，这不是自然潮的确认。当前没有为召唤物新增精确事件归因。

## 为什么按实际计数名单过滤

本机 Steam build 24903151，EXE SHA256：`9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13`。只读检查当前 EXE 和 Pak，并用公开 FSD 模板辅助解释反射名称；结论不只依赖旧模板。

- 当前 `CanSpawn` 的反射入口 `0x1a7b480` 调用 `0x1631fc0`，按描述符 `EnemySignificance` 选择小型／普通／环境生物上限，Critical 则直接允许。
- 当前 `RegisterSpawnedEnemy` 的反射入口 `0x1a7bc30` 调用 `0x164fe40`。它按 Pawn 的 Gameplay Tags 检查管理器的 SwarmerTag、RegularTag、CritterTag，并分别加入实际存活名单；它本身没有广播 `OnEnemySpawned`。
- 小型名单位于管理器 `+0x100`，数量 `+0x108`；普通名单 `+0xf0`，数量 `+0xf8`。这些地址仅用于本次静态核对，**Mod 不读取固定内存地址**，实际蓝图只使用同名反射属性。
- 游戏的 `EnemyHealthComponent` 注册路径在 `0x1630640` 调用上述注册函数。通用队列成功生成后的 `OnEnemySpawned` 广播在 `0x16615a4`，位于 `UWorld::SpawnActor`、初始化和调用方回调之后。
- 当前资产中，`ENE_Spider_Swarmer`、`ENE_Jelly_Swarmer`、`ENE_Shredder_Base` 的 Gameplay Tags 都包含 `Enemy.Type.Swarmer`。各自子类沿用该分类，包括普通／冰原／辐射蜂拥异虫及两类小水母的子类；虫卵和虫穴描述符引用这些蜂拥异虫类。
- **育母体不是小水母。** `ENE_JellyBreeder_Base` 和岩痘育母体的实际标签为 `Enemy.Type.Regular`，应保留。它们的描述符没有显式覆盖 `EnemySignificance`，因此不能简单用描述符枚举或名字包含 Jelly 来排除，否则可能误伤大单位。

因此，本次读取游戏已经建立的计数名单，不维护易漏变体的名称黑名单。它适用于按原版方式完成注册的单位；其他 Mod 禁用注册、改动标签、或在回调前移出名单的情况仍需兼容性验证。未知／未登记单位不会被本过滤器自动判成小型单位。

## 特殊召唤物

下表是**当前安装版本资产与原生调用链的静态预期**，不是游戏实测记录。所有“会”均以监听已绑定、实际收到通知、房主显示条件满足为前提。

| 生成物 | 0.7.0 候选 | 0.7.1 候选 | 核对到的路径 |
| --- | --- | --- | --- |
| 巢主无畏异虫召唤的哨卫 | 会 | 保留 | `AIC_Spider_Boss_Heavy` 调用 `SpawnEnemiesAtLocationWithCallback`，描述符为 `ED_Spider_Tank_HeavySpawn`；其标签为 MiniBoss，不是 Swarmer |
| 看守者召唤的粉碎者 | 会 | **过滤** | `BP_CaretakerShredderAction` 调用 `SpawnEnemiesAtLocation`，传入 `ED_Shredder`；粉碎者属于小型计数组 |
| 看守者召唤的巡逻机器人 | 会 | 保留 | `ENE_FacilityCaretaker` 调用 `SpawnEnemiesAtLocation`，传入 `ED_PatrolBot_Caretaker`；不是小型计数组 |
| 看守者触手及钻地触手末端 | 不覆盖所查路径 | 不覆盖所查路径 | `BP_FacilityTentacleManager` 使用 `BeginDeferredActorSpawnFromClass` / `FinishSpawningActor`；钻地触手还使用直接放置接口，没有经过上述通用成功通知 |

三个通用接口调用已在相应 Ubergraph 的序列化脚本区域定位到调用操作码，且紧随的对象常量分别是上述三个描述符，超过“名称表里出现相关名字”的证据强度。原生 `SpawnEnemiesAtLocation` 经 `0x19dab00` 转入 `0x19dab90`；WithCallback 入口直接进入 `0x19dab90`；其队列提交点 `0x19daf1d` 调用 `EnemySpawnManager::SpawnEnemy`（`0x16571c0`），最终进入通用成功广播。

触手管理器脚本中的直接创建调用也已定位。`PlaceActors` 的实现 `0x160d160` 直接调用 `UWorld::SpawnActor`（调用点 `0x160d392`），与通用队列通知不同。单位被登记到存活名单，并不等于它发出了我们监听的成功通知。

曾检查到巢主 Pawn 自身的 `PlaceActorsWithCallback`，但那属于其他特殊攻击逻辑，不能据此判断哨卫不显示；实际哨卫召唤应跟踪其 **AI 控制器**。同样，没有将名称带 UNUSED 的旧看守者 DroneAction 当成当前巡逻机器人路径。

这里没有通过替换 Boss 蓝图或扩大 Actor 扫描来新增覆盖；用户本轮要求的小型单位过滤不需要 DLL，候选 ZIP 仍然只有一个自有 Pak。

## 验证与未解决项

保存并冷加载生成蓝图，通过与游戏同形的委托夹具，验证了小型／环境生物拒绝、100 次密集小型通知不新增或续时标记、正常通知仍合并与过期、事件上下文、八槽上限及绑定清理。原有显示、资源和设置验证也通过。打包要求新增 `small_enemy_filter_test` 结果，仍需 18 条 Pak 内容哈希和依赖审计。

**尚未验证：真实游戏里各变体的注册时序、这些 Boss 战的实际标记表现、其他 Mod 改动生成路径后的行为，以及干净 mod.io 订阅安装。** 不宣称“只显示正式虫潮”或“所有 Boss 召唤都覆盖”。验收步骤见 [发布流程](RELEASE.md)。公开参考的 [FSD 模板](https://github.com/trumank/drg-mods/tree/d6ff9fac56abf15937a01824332aa970232768f6/Source/FSD) 和 [描述符名称表](https://github.com/trumank/drg-custom-difficulties/blob/master/DATA.md) 仅用于辅助对照；本次本地资产与反汇编过程存放于 `agent/codex/spawn-filter-20260907`，不分发游戏资产。
