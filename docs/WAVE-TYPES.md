<!-- Bilingual player reference for every wave type available in Enemy Wave Indicator 0.9.4. -->

# Wave Types / 虫潮类型

Enemy Wave Indicator 0.9.4 provides separate switches and labels for one natural-wave source and 46 stock scripted wave types: 35 wave controllers, three direct machine-event sources, and eight mission-warning or anomaly sources. Every type is enabled by default except IDs **1, 6, 32, 34, and 46**. After changing a setting in Mod Hub, select **Apply and save**. The settings UI follows the game language in English or Simplified Chinese, while editable marker defaults remain English.

Enemy Wave Indicator 0.9.4 为自然潮和 46 种游戏自带的脚本虫潮类型提供独立开关与提示文字，其中包括 35 种虫潮控制器、3 种直接刷怪的机械事件，以及 8 种任务警告或异变来源。除 ID **1、6、32、34、46** 外，其余类型默认全部开启；在 Mod Hub 中修改设置后，请点击**应用并保存**。设置界面会随游戏语言显示英文或简体中文，但可编辑的标记默认内容始终保留英文。

## Main settings / 主要设置

| ID | Setting / 设置项 | Game source / 游戏来源 | Notes / 备注 |
|---:|---|---|---|
| 0 | Natural wave<br>自然潮 | `Natural` | Regular unannounced enemy pressure generated during a mission.<br>任务过程中自然生成、不被任务中心播报的虫潮。 |
| 1 | Drillevator<br>深掘钻梯 | `EWC_DeepScan_Drillevator` | Waves attacking the team during the Drillevator descent in Deep Scan.<br>深层采掘任务中，深掘钻梯向下钻进时出现的虫潮。 |
| 2 | Egg hunt ambush<br>虫蛋伏击 | `EWC_EggHunt_Ambush` | An ambush triggered when an Alien Egg is removed in Egg Hunt.<br>虫蛋收集任务中挖开虫蛋时触发的虫潮。 |
| 3 | Extraction<br>常规撤离 | `EWC_EndMission` | Enemy pressure during the extraction phase of standard missions.<br>常规任务呼叫空降舱后，撤离阶段出现的虫潮。 |
| 4 | Motherlode extraction<br>定点提取撤离 | `EWC_EndMission_MotherLode` | The extraction wave after the Minehead launches in Point Extraction.<br>定点提取任务中矿井平台升空后、等待空降仓期间的虫潮。 |
| 6 | Escort: drilling<br>执勤护送：钻进 | `EWC_Escort_DigPhase` | Waves attacking Doretta while the Drilldozer travels and drills forward.<br>执勤护送任务中掘进机移动、向前钻进时袭击朵蕾塔的虫潮。 |
| 7 | Escort: heartstone defense<br>执勤护送：心石防守 | `EWC_Escort_EndDefense` | Waves during the Ommoran Heartstone drilling and defense sequence.<br>执勤护送任务钻取奥魔兰心石阶段的虫潮。 |
| 8 | Escort: extraction<br>执勤护送：撤离 | `EWC_Escort_EndMission` | The extraction wave after the Escort Duty objective is complete.<br>执勤护送任务完成主要目标后、撤离阶段出现的虫潮。 |
| 9 | Escort: refueling<br>执勤护送：补充燃料 | `EWC_Escort_Refueling` | Waves while the stopped Drilldozer is being refueled.<br>掘进机停止前进、收集油页岩时出现的虫潮。 |
| 10 | Excavation: mining<br>重型采掘：挖掘 | `EWC_Excavation_ExcavationPhase` | Waves triggered while excavating Resinite Masses in Heavy Extraction.<br>重型采掘任务中挖开古脂矿块时触发的虫潮。 |
| 11 | Excavation: launch<br>重型采掘：升空 | `EWC_Excavation_LaunchPhase` | Waves associated with launching an excavated Resinite Mass by Lift Rockets.<br>古脂矿块火箭升空发射时出现的虫潮。 |
| 12 | Announced / generic swarm<br>播报潮／通用潮 | `EWC_Generic` | A generic scripted-wave controller used by announced swarms and other triggers.<br>游戏播报潮及其他部分触发器共用的通用脚本虫潮控制器。 |
| 14 | Industrial Sabotage: power station<br>设施破坏：发电站 | `EWC_OverloadShieldGenerator_Facility` | Bug waves while Hack-C disables a Caretaker shield power station.<br>设施破坏任务中保护骇入仓入侵发电站时出现的虫潮。 |
| 15 | Plague meteor defense<br>噬岩体陨石防守 | `EWC_PlagueMeteorDefence` | Waves attacking the Rock Crackers while they break a Lithophage meteor.<br>碎岩器处理噬岩体陨石期间的防守虫潮。 |
| 16 | Refinery: constant pressure<br>就地精炼：恒压潮 | `EWC_PumpSequence_ConstantPresure_Refinery` | The continuous enemy pressure active during the refining sequence.<br>就地精炼开始炼油后持续生成的恒压潮。 |
| 17 | Refinery: pumping swarm<br>就地精炼：炼油虫潮 | `EWC_PumpSequence_Wave_Refinery` | Distinct swarm spikes during the active pumping sequence.<br>就地精炼炼油过程中出现的播报潮。 |
| 18 | Refinery: broken pipe<br>就地精炼：管道故障 | `EWC_Refinery_BokenPipe_LocalWave` | Local waves around damaged pipelines that need repair.<br>管道故障后，在需要维修的管道附近出现的局部虫潮。 |
| 19 | Refinery: extraction<br>就地精炼：撤离 | `EWC_Refinery_End` | The extraction wave after refining is complete and the cargo rocket launches.<br>精炼完成、货运火箭升空后进入撤离阶段时出现的虫潮。 |
| 20 | Special swarm: grunts<br>特殊虫潮：战士 | `EWC_SW_Grunts` | A special swarm dominated by Glyphid Grunts.<br>以战士三兄弟（战士，刀锋，护卫）为主的特殊虫潮。 |
| 21 | Special swarm: mactera<br>特殊虫潮：异虫蝇 | `EWC_SW_Macteras` | A special swarm dominated by flying Mactera enemies.<br>以异虫蝇类敌人为主的特殊虫潮。 |
| 22 | Special swarm: rockpox<br>特殊虫潮：岩痘 | `EWC_SW_Plague_RockpoxInfectedEnemies` | A special swarm composed mainly of Rockpox-infected enemies.<br>以岩痘敌人为主的特殊虫潮。 |
| 23 | Special swarm: praetorians<br>特殊虫潮：禁卫 | `EWC_SW_Pretorians` | A special swarm featuring many Glyphid Praetorians.<br>包含大量异虫禁卫的特殊虫潮。 |
| 24 | Special swarm: swarmers<br>特殊虫潮：蜂拥 | `EWC_SW_Swarmers` | A special swarm dominated by small Glyphid Swarmers.<br>以蜂拥异虫为主的特殊虫潮。 |
| 25 | Salvage: mini-MULE ambush<br>搜救行动：矿骡伏击 | `EWC_Salvage_Ambush` | A localized ambush tied to recovering a broken mini-M.U.L.E.<br>搜救行动中首次接近损坏的迷你矿骡时触发的虫潮。 |
| 26 | Salvage: defense<br>搜救行动：据点防守 | `EWC_Salvage_Defend` | Waves during the Uplink and Fuel Cell defense stages.<br>搜救行动中防守定位装置和燃料电池时出现的虫潮。 |
| 27 | Salvage: extraction<br>搜救行动：撤离 | `EWC_Salvage_End` | The final enemy pressure after the Drop Pod has been prepared for departure.<br>搜救行动的燃料电池防守完成后，等待空降仓充能至最终撤离阶段出现的虫潮。 |
| 28 | Industrial Sabotage: drones<br>设施破坏：无人机 | `EWC_ShieledGenerator_DronePresure_Facility` | Triggered once when the Hacking Pod is called at each power station in Industrial Sabotage. Each trigger produces two successive waves, both composed of Patrol Bots and Shredders.<br>设施破坏任务中，在每个发电站呼叫骇入仓时触发一次，每次先后生成两波敌人，每波敌人组合为巡逻无人机和粉碎者 |
| 29 | Dreadnought wave<br>无畏异虫潮 | `EWC_Spiders_Boss` | A rare announced wave outside Elimination missions; it spawns one Dreadnought, one Hiveguard, or an Arbalest/Lacerator pair.<br>非消灭任务中的稀有播报潮；生成 1 只无畏异虫或1 只巢主无畏异虫，或一对双子无畏异虫。 |
| 30 | Motherlode wave<br>定点提取压力潮 | `EWC_Spiders_Motherlode` | Recurring pressure waves during Point Extraction.<br>定点提取任务中随时间反复出现、频率逐渐加快的压力潮。 |
| 32 | Core Stone event<br>核心岩事件 | `EWC_CoreRift` | Corespawn emerging from rifts after a Core Stone event begins.<br>核心岩事件启动后，从裂隙中出现的吗喽。 |
| 33 | Rival communications event<br>强敌科技通讯事件 | `EWC_BombEvent` | Enemy waves during the Rival Communications Router event.<br>关闭强敌科技通讯天线事件期间出现的虫潮。 |
| 34 | Core Corruption warning<br>核心侵扰警告 | `EWC_CoreCorruption`, `BP_CoreCorruption_Crystal` | Corespawn generated slowly and continuously before the enhanced Core Stone is triggered, then rapidly and continuously after it is triggered.<br>核心侵扰警告中，触发增强核心岩之前缓慢持续生成的吗喽，和触发增强核心岩之后快速持续生成的吗喽。 |
| 35 | Hacking defense<br>骇入防守 | `EWC_HackBuilding` | Waves attacking Hack-C during hacking objectives such as a Data Deposit.<br>强敌科技数据存储站等骇入目标中，保护骇入仓时出现的防守虫潮。 |
| 36 | Tritilyte Deposit<br>三提石矿藏 | `BP_ExplosiveBarrelsEvent` | Enemy waves generated by the Tritilyte Deposit event.<br>三提石矿藏事件生成的虫潮。 |
| 37 | Ebonite Mutation<br>矿化爆发（事件） | `BP_RockEnemiesEvent` | Ebonite Glyphid waves generated by the Ebonite Mutation event.<br>矿化爆发事件生成的矿化异虫潮。 |
| 38 | Kursite Infection<br>氪石感染 | `BP_AmberEvent` | Infected Glyphid waves generated by the Kursite Infection event.<br>氪石感染事件生成的感染异虫潮。 |
| 39 | Swarmageddon<br>蜂拥浩劫 | `BP_Swarmageddon` | Glyphid Swarmer groups generated by the Swarmageddon warning.<br>蜂拥浩劫警告生成的蜂拥异虫群。 |
| 40 | Exploder Infestation<br>自爆群袭 | `BP_ExploderInfestation` | Glyphid Exploder groups generated by the Exploder Infestation warning.<br>自爆群袭警告生成的自爆异虫群。 |
| 41 | Scrab Nesting Grounds<br>掠痕集居 | `BP_ScrabNestingGrounds` | Flying Scrab groups generated by the Scrab Nesting Grounds warning.<br>掠痕集居警告生成的掠痕飞虫群。 |
| 42 | Blood Sugar<br>凝血化糖 | `BP_BloodSugar` | Small groups of Glyphid Swarmers or Grunts generated by the Blood Sugar anomaly.<br>凝血化糖异变生成的小股蜂拥或战士异虫。 |
| 43 | Rival Presence<br>强敌环伺 | `BP_RivalIncursionWarning` | Groups of Patrol Bots, Shredders, and other robots generated by the Rival Presence warning.<br>强敌环伺警告生成的巡逻机器人、粉碎者等组合。 |
| 44 | Ebonite Outbreak<br>矿化爆发（警告） | `BP_RockInfestation` | Ebonite Glyphid groups generated by the Ebonite Outbreak warning.<br>矿化爆发警告生成的矿化虫群。 |
| 45 | Lithophage Outbreak<br>噬岩体爆发 | `BP_PlagueWarning` | Rockpox-infected Glyphid groups that appear at or near a Contagion Spike after players approach and begin dealing with it.<br>噬岩体爆发警告中，玩家接近并处理感染尖刺后，尖刺中心或附近出现的岩痘虫群。 |
| 46 | Haunted Cave<br>幽魂不散 | `BP_GhostMutator` | The Unknown Horror's spawn position.<br>未知恐惧的生成点。 |

## Less useful settings / 较少使用的设置

These controllers are supported, but they are limited to tutorial or seasonal content and are rarely useful during normal play.<br>这些控制器仍受支持，但仅用于教程或季节活动，在常规游玩中很少用到。

| ID | Setting / 设置项 | Game source / 游戏来源 | Notes / 备注 |
|---:|---|---|---|
| 5 | Tutorial extraction<br>教程撤离 | `EWC_EndMission_Tutorial` | The extraction wave used by the introductory tutorial mission.<br>新手教程撤离阶段使用的虫潮。 |
| 13 | Oktoberfest beer ambush<br>啤酒节伏击 | `EWC_OktoberFest_BeerAmbush` | A beer-related ambush used by the seasonal Oktoberfest event.<br>啤酒节季节活动中与啤酒目标相关的伏击潮。 |
| 31 | Tutorial grunts<br>教程战士潮 | `EWC_TutorialGrunts` | Controlled Glyphid Grunt waves used by the introductory tutorial.<br>新手教程中用于战斗教学的异虫战士潮。 |

[wave-controller-audit.json](wave-controller-audit.json), [event-wave-audit.json](event-wave-audit.json), and [mission-modifier-wave-audit.json](mission-modifier-wave-audit.json) contain the machine-readable stock-source audits behind this list / 这三个文件包含本清单对应的游戏控制器、机械事件和任务词条来源审计数据。

The Core Stone setting also covers `BP_RiftCrystal`; Core Corruption also covers `BP_CoreCorruption_Crystal`. <br>“核心岩事件”还覆盖 `BP_RiftCrystal`，“核心侵扰警告”还覆盖 `BP_CoreCorruption_Crystal`。
