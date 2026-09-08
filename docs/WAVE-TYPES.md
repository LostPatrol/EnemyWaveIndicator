<!-- Bilingual player reference for every wave type available in Enemy Wave Indicator 0.9.0. -->
# Wave Types / 虫潮类型

Enemy Wave Indicator 0.9.0 provides separate switches and labels for one natural-wave source and 35 stock scripted wave controllers. Only **Natural wave** is enabled by default; after changing a setting in Mod Hub, select **Apply and save**.

Enemy Wave Indicator 0.9.0 为自然潮和 35 种游戏自带的脚本虫潮控制器提供独立开关与提示文字。默认只启用 **Natural wave**；在 Mod Hub 中修改设置后，请点击 **Apply and save**。

| ID | Setting / 设置项 | Game controller / 游戏控制器 | Notes / 备注 |
|---:|---|---|---|
| 0 | Natural wave<br>自然潮 | `Natural` | Regular unannounced enemy pressure generated during a mission.<br>任务过程中自然生成、通常不会由任务控制中心播报的小规模敌潮。 |
| 1 | Drillevator<br>钻井升降机 | `EWC_DeepScan_Drillevator` | Waves attacking the team during the Drillevator descent in Deep Scan.<br>深度扫描任务中，钻井升降机向下钻进时出现的敌潮。 |
| 2 | Egg hunt ambush<br>寻蛋伏击 | `EWC_EggHunt_Ambush` | An ambush triggered when an Alien Egg is removed in Egg Hunt.<br>寻蛋任务中取出异虫蛋时触发的伏击潮。 |
| 3 | Extraction<br>常规撤离 | `EWC_EndMission` | Enemy pressure during the extraction phase of standard missions.<br>常规任务呼叫逃生舱后，撤离阶段出现的敌潮。 |
| 4 | Motherlode extraction<br>定点提取撤离 | `EWC_EndMission_MotherLode` | The extraction wave after the Minehead launches in Point Extraction.<br>定点提取任务中矿井平台升空后、等待逃生舱期间的敌潮。 |
| 5 | Tutorial extraction<br>教程撤离 | `EWC_EndMission_Tutorial` | The extraction wave used by the introductory tutorial mission.<br>新手教程撤离阶段使用的敌潮。 |
| 6 | Escort: drilling<br>护送：钻进 | `EWC_Escort_DigPhase` | Waves attacking Doretta while the Drilldozer travels and drills forward.<br>护送任务中钻地机移动、向前钻进时袭击朵蕾塔的敌潮。 |
| 7 | Escort: heartstone defense<br>护送：心石防守 | `EWC_Escort_EndDefense` | Waves during the Ommoran Heartstone drilling and defense sequence.<br>钻取奥莫兰心石并保护朵蕾塔时出现的阶段敌潮。 |
| 8 | Escort: extraction<br>护送：撤离 | `EWC_Escort_EndMission` | The extraction wave after the Escort Duty objective is complete.<br>护送任务完成主要目标后、撤离阶段出现的敌潮。 |
| 9 | Escort: refueling<br>护送：补充燃料 | `EWC_Escort_Refueling` | Waves while the stopped Drilldozer is being refueled.<br>钻地机停止前进、队伍收集油页岩并补充燃料时出现的敌潮。 |
| 10 | Excavation: mining<br>重型提取：挖掘 | `EWC_Excavation_ExcavationPhase` | Waves triggered while excavating Resinite Masses in Heavy Extraction.<br>重型提取任务中挖出树脂矿块时触发的敌潮。 |
| 11 | Excavation: launch<br>重型提取：升空 | `EWC_Excavation_LaunchPhase` | Waves associated with launching an excavated Resinite Mass by Lift Rockets.<br>为挖出的树脂矿块安装升空火箭并将其发射时出现的敌潮。 |
| 12 | Announced / generic swarm<br>播报潮／通用潮 | `EWC_Generic` | A generic scripted-wave controller used by announced swarms and other triggers.<br>游戏播报潮及其他部分触发器共用的通用脚本虫潮控制器。 |
| 13 | Oktoberfest beer ambush<br>啤酒节伏击 | `EWC_OktoberFest_BeerAmbush` | A beer-related ambush used by the seasonal Oktoberfest event.<br>啤酒节季节活动中与啤酒目标相关的伏击潮。 |
| 14 | Industrial Sabotage: power station<br>工业破坏：能源站 | `EWC_OverloadShieldGenerator_Facility` | Bug waves while Hack-C disables a Caretaker shield power station.<br>工业破坏任务中 Hack-C 入侵并关闭看守者护盾能源站时出现的异虫潮。 |
| 15 | Plague meteor defense<br>瘟疫陨石防守 | `EWC_PlagueMeteorDefence` | Waves attacking the Rock Crackers while they break a Lithophage meteor.<br>碎岩器处理疫岩陨石期间来袭的防守敌潮。 |
| 16 | Refinery: constant pressure<br>现场精炼：持续压力 | `EWC_PumpSequence_ConstantPresure_Refinery` | The continuous enemy pressure active during the refining sequence.<br>现场精炼开始泵送后持续生成的敌人压力潮。 |
| 17 | Refinery: pumping swarm<br>现场精炼：泵送虫潮 | `EWC_PumpSequence_Wave_Refinery` | Distinct swarm spikes during the active pumping sequence.<br>现场精炼泵送过程中出现的集中虫潮。 |
| 18 | Refinery: broken pipe<br>现场精炼：管道破裂 | `EWC_Refinery_BokenPipe_LocalWave` | Local waves around damaged pipelines that need repair.<br>管道故障后，在需要维修的破损管线附近出现的局部敌潮。 |
| 19 | Refinery: extraction<br>现场精炼：撤离 | `EWC_Refinery_End` | The extraction wave after refining is complete and the cargo rocket launches.<br>精炼完成、货运火箭升空后进入撤离阶段时出现的敌潮。 |
| 20 | Special swarm: grunts<br>特殊虫潮：战士 | `EWC_SW_Grunts` | A special swarm dominated by Glyphid Grunts.<br>以异虫战士为主的特殊虫潮。 |
| 21 | Special swarm: mactera<br>特殊虫潮：异蜂 | `EWC_SW_Macteras` | A special swarm dominated by flying Mactera enemies.<br>以飞行异蜂类敌人为主的特殊虫潮。 |
| 22 | Special swarm: rockpox<br>特殊虫潮：疫岩感染体 | `EWC_SW_Plague_RockpoxInfectedEnemies` | A special swarm composed mainly of Rockpox-infected enemies.<br>以疫岩感染敌人为主的特殊虫潮。 |
| 23 | Special swarm: praetorians<br>特殊虫潮：禁卫 | `EWC_SW_Pretorians` | A special swarm featuring many Glyphid Praetorians.<br>包含大量异虫禁卫的特殊虫潮。 |
| 24 | Special swarm: swarmers<br>特殊虫潮：蜂拥异虫 | `EWC_SW_Swarmers` | A special swarm dominated by small Glyphid Swarmers.<br>以小型蜂拥异虫为主的特殊虫潮。 |
| 25 | Salvage: mini-MULE ambush<br>搜救：迷你矿骡伏击 | `EWC_Salvage_Ambush` | A localized ambush tied to recovering a broken mini-M.U.L.E.<br>搜救任务中寻找并维修损坏迷你矿骡时触发的局部伏击潮。 |
| 26 | Salvage: defense<br>搜救：据点防守 | `EWC_Salvage_Defend` | Waves during the Uplink and Fuel Cell defense stages.<br>搜救任务中防守通讯上行链路和燃料电池时出现的敌潮。 |
| 27 | Salvage: extraction<br>搜救：撤离 | `EWC_Salvage_End` | The final enemy pressure after the Drop Pod has been prepared for departure.<br>搜救任务完成逃生舱启动准备后，最终撤离阶段出现的敌潮。 |
| 28 | Industrial Sabotage: drones<br>工业破坏：无人机 | `EWC_ShieledGenerator_DronePresure_Facility` | Patrol Bot and Shredder pressure during the power-station sequence.<br>工业破坏能源站阶段由巡逻机器人、粉碎者等构成的机械压力潮。 |
| 29 | Dreadnought wave<br>无畏异虫战斗潮 | `EWC_Spiders_Boss` | Dreadnought-related enemy spawns during an Elimination encounter.<br>消灭任务中与无畏异虫战斗相关的敌人生成。 |
| 30 | Motherlode wave<br>定点提取压力潮 | `EWC_Spiders_Motherlode` | Recurring pressure waves during Point Extraction.<br>定点提取任务中随时间反复出现、频率逐渐加快的压力潮。 |
| 31 | Tutorial grunts<br>教程战士潮 | `EWC_TutorialGrunts` | Controlled Glyphid Grunt waves used by the introductory tutorial.<br>新手教程中用于战斗教学的异虫战士潮。 |
| 32 | Core Stone event<br>核心石事件 | `EWC_CoreRift` | Corespawn waves emerging from rifts during a Core Stone event.<br>核心石事件启动后，从裂隙中出现的核心衍生物敌潮。 |
| 33 | Rival communications event<br>劲敌通讯事件 | `EWC_BombEvent` | Enemy waves during the Rival Communications Router event.<br>关闭劲敌通讯路由器事件期间出现的敌潮。 |
| 34 | Core Corruption warning<br>核心腐化警告 | `EWC_CoreCorruption` | Corespawn pressure produced by the Core Corruption mission warning.<br>带有核心腐化警告的任务中，由强化核心石持续产生的核心衍生物敌潮。 |
| 35 | Hacking defense<br>骇入防守 | `EWC_HackBuilding` | Waves attacking Hack-C during hacking objectives such as a Data Deposit.<br>数据存储站等骇入目标中，保护 Hack-C 时出现的防守敌潮。 |

[wave-controller-audit.json](wave-controller-audit.json) contains the machine-readable stock-controller audit behind this list / 该文件包含本清单对应的游戏控制器审计数据。
