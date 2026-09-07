<!-- Stable stock source IDs, UI labels, native provenance and scope limits for 0.9.0. -->
# 0.9.0 虫潮类型

当前游戏的35种具体 EnemyWaveController，加自然潮，共36项。每项有独立显示开关和文字；外观设置共用，默认只启用自然潮。矿骡伏击选 **Salvage: mini-MULE ambush**；古脂矿体相关阶段选 **Excavation: mining / launch**；常规播报潮选 **Announced / generic swarm**。修改后点击Apply and save。

| ID | 设置名称 | 游戏控制器 |
|---|---|---|
| 0 | Natural wave | Natural |
| 1 | Drillevator | EWC_DeepScan_Drillevator |
| 2 | Egg hunt ambush | EWC_EggHunt_Ambush |
| 3 | Extraction | EWC_EndMission |
| 4 | Motherlode extraction | EWC_EndMission_MotherLode |
| 5 | Tutorial extraction | EWC_EndMission_Tutorial |
| 6 | Escort: drilling | EWC_Escort_DigPhase |
| 7 | Escort: heartstone defense | EWC_Escort_EndDefense |
| 8 | Escort: extraction | EWC_Escort_EndMission |
| 9 | Escort: refueling | EWC_Escort_Refueling |
| 10 | Excavation: mining | EWC_Excavation_ExcavationPhase |
| 11 | Excavation: launch | EWC_Excavation_LaunchPhase |
| 12 | Announced / generic swarm | EWC_Generic |
| 13 | Oktoberfest beer ambush | EWC_OktoberFest_BeerAmbush |
| 14 | Industrial sabotage: power station | EWC_OverloadShieldGenerator_Facility |
| 15 | Plague meteor defense | EWC_PlagueMeteorDefence |
| 16 | Refinery: constant pressure | EWC_PumpSequence_ConstantPresure_Refinery |
| 17 | Refinery: pumping swarm | EWC_PumpSequence_Wave_Refinery |
| 18 | Refinery: broken pipe | EWC_Refinery_BokenPipe_LocalWave |
| 19 | Refinery: extraction | EWC_Refinery_End |
| 20 | Special swarm: grunts | EWC_SW_Grunts |
| 21 | Special swarm: mactera | EWC_SW_Macteras |
| 22 | Special swarm: rockpox | EWC_SW_Plague_RockpoxInfectedEnemies |
| 23 | Special swarm: praetorians | EWC_SW_Pretorians |
| 24 | Special swarm: swarmers | EWC_SW_Swarmers |
| 25 | Salvage: mini-MULE ambush | EWC_Salvage_Ambush |
| 26 | Salvage: defense | EWC_Salvage_Defend |
| 27 | Salvage: extraction | EWC_Salvage_End |
| 28 | Industrial sabotage: drones | EWC_ShieledGenerator_DronePresure_Facility |
| 29 | Dreadnought wave | EWC_Spiders_Boss |
| 30 | Motherlode wave | EWC_Spiders_Motherlode |
| 31 | Tutorial grunts | EWC_TutorialGrunts |
| 32 | Core stone event | EWC_CoreRift |
| 33 | Rival communications event | EWC_BombEvent |
| 34 | Core corruption warning | EWC_CoreCorruption |
| 35 | Hacking defense | EWC_HackBuilding |

## 归因证据和限制

目录来自当前安装Pak中的全部EWC资产，EWC_Base只作基类，不单列开关。专项潮和精炼撤离等也逐一核对了自己的生成调用。审计记录见[wave-controller-audit.json](wave-controller-audit.json)；实际生成调用的首个WorldContextObject操作数为EX_Self。DLL据发起对象的完整类路径识别，按同步请求保存类型和唯一编号，跟随异步队列与swap removal到成功创建；不会选取“当前活动潮列表的第一项”。不同来源、不同请求不会因为空间相近而合并。

自然来源仍要求原有六个调度返回地址。新增观察当前EXE的五个生成接口（FromPool 0x19db030、AtLocationWithCallback 0x19dab90、GroupDescriptor 0x19dbf00、SpreadOut 0x19dc1b0、WithCallbackSpreadOut 0x19dc470），原函数原样调用一次。共享批次接口0x19db3d0第五参数是游戏从位置数组中选出的中心，因此多中心不用“距敌人最近的点”猜测。36个类型和ABI通过离线夹具不等于35种任务都已实机验收。

同一个EWC_Generic会被多种游戏触发器复用，因此该项表示**同一代码类型**，不能仅凭它的类名进一步断言触发原因一定是定时播报。它的独立请求仍分开显示。非EWC的独立Boss召唤、直接SpawnActor、机器事件自有生成组件和新增Mod控制器尚未接入本目录；不能把36项宣称为任意敌人来源。自然潮仍排除这些来源，即使Custom Difficulty替换了召唤物。

自然潮排除小型和环境生物；明确启用的脚本潮可显示注册的小型敌人（包含蜂拥专项潮），环境生物及Hoarder/Huuli名称仍排除。房主开启某类型后才输出该类型区域；客机可在收到的类型中再用自己的开关和文字筛选。RegionType与位置/规模等一起复制，双机传输尚待实测。

0.9.0使用新NativeAbi=0x90000，必须配套更新DLL和Pak；沿用v2设置文件，旧自然设置保留，新增字段默认关闭。八区域上限、基础DifficultyRating权重和无提前预测的限制继续存在。用户已确认0.8.0初始化修复后自然潮显示正常；新增类型请分别实测，不用手动生成普通战士代替来源触发。
