<!-- Source-backed investigation of exact normal-wave attribution and content-only distribution. -->
# Sandbox Utilities 与订阅即用方案研究

研究日期：2026-09-07。目标仍是**准确识别游戏原有的自然普通波次，记录成功生成时的输入位置，玩家只需通过游戏内 mod.io 订阅和启用**。

本轮找到了可以借鉴的蓝图调用、初始化、联机和打包技术，也检查了打包后蓝图字节码插入。但没有找到能在当前游戏中、只靠 Pak、保持上述语义的完整检测入口。**没有完成去 DLL 改造，也没有制作或发布声称达标的订阅包。** 以下区分已核实事实与需要外部条件的方案；这不是对所有未来技术的不可能性证明。

## 1. 实际检查范围

- 作者公开仓库 [trumank/drg-mods](https://github.com/trumank/drg-mods)，固定到 `d6ff9fac56abf15937a01824332aa970232768f6`，提交时间 2024-03-02。仓库明确链接 Sandbox Utilities 的 mod.io 页面。
- 只读解析 SandboxUtilities、Common、CustomDifficulty2、DRGLib 共 **443 个源资产**，提取 **16,387 个图节点成员引用**；另检查同仓库 EventLog 的 14 个资产。
- 解析器读取包表和 tagged properties，**没有执行蓝图、没有完整解释执行连线或 Kismet 字节码**。34,364 个导出中 34,342 个完成 tagged-property 读取；22 个导入元数据导出未解析，未知属性保留 opaque。不能把这个统计说成全量蓝图行为验证。
- 交叉检查当前安装的游戏 EXE，而非仅依赖旧 SDK。SHA-256：`9B005BB6E1072F3CD98FCFAA75698316DC47B808D83A99DDF96DE529D00BAC13`。
- mod.io 页面只返回需要 JavaScript 的占位内容；**未核实当前下载版本与该源码提交逐字节一致，未核实当前依赖列表或审核分类**。本报告对 Sandbox 的实现结论限于上述公开源码。
- 没有启动游戏、修改已安装游戏文件、发送外部消息或上传 Mod。静态分析不能替代无加载器环境下的订阅实测。

## 2. Sandbox Utilities 的底层功能如何实现

它借助游戏已经编译进 EXE、并保留了 Unreal 反射信息的类、函数和属性。开发项目里的 dummy 声明让编辑器能够编译这些引用；实际运行时调用的是游戏自己的实现。一个接口原本未在普通编辑器菜单中显示，不代表游戏运行时没有它；但编写一个新的 dummy 函数也不会凭空给游戏添加该函数。[DRG 蓝图指南的 dummy 方法](https://drg-modding.github.io/docs/guides/blueprint-modding-guide.html)

下面列出从序列化节点中实际读出的引用，路径均相对于源码 `Content/_AssemblyStorm/SandboxUtilities/`：

| 功能 / 资产 | 实际接口 | 对本项目的意义 |
| --- | --- | --- |
| `MenusPage.uasset`，调试和控制台窗口 | 游戏蓝图 `ShowCheatMenu`，`WindowManager.OpenWindowFromClass`，锻造/晋升控制台 | 调用游戏现成的窗口能力，不是安装本机调试器。 |
| `SandboxUtilities.uasset`、`Creative/FreeCam.uasset`，地形编辑 | `DeepCSGWorld.CarveWithMeshUsingTransform` | 地形修改本身已有原生接口；不能由此推导出任意 C++ 函数都可 Hook。 |
| `ReplicatedActor.uasset`，生命、弹药和状态 | `Heal`、`SetCanTakeDamage`、`Resupply`、`ChangeState` 等 | 可借鉴将需要房主执行的操作放入自己的复制 Actor。 |
| `MissionPage.uasset`，触发波次 | `FSDGameMode.GetWaveManager` → `EnemyWaveManager.TriggerWave` | 主动命令，不是自然波次发生通知。 |
| `SandboxUtilities.uasset`，查看/修改管理器 | `ScriptedWaves`、`ActiveScriptedWaves`、`SpawnRarityModifiers` 等反射属性 | 可访问比常规 UI 更多的数据，但这些字段不能给每次普通刷怪补出来源。 |
| `SU_ProxySpawnModifiedEnemy.uasset`，刷怪并暂停 AI | `SpawnEnemiesAtLocationWithCallback`、`GetFSDAIController`、`PauseLogic` | 可为**自己提交的生成请求**接收回调。 |

这些资产可从[固定提交的 Sandbox 源目录](https://github.com/trumank/drg-mods/tree/d6ff9fac56abf15937a01824332aa970232768f6/Content/_AssemblyStorm/SandboxUtilities)复核。`SU_ProxySpawnModifiedEnemy` 不是监听所有敌人的原生 Hook；把它的回调绑定方式复制到本项目，仍然收不到游戏自然普通波次的专属回调。

打包代码也支持这一判断：Sandbox 的 `PackageJob` 只收集 SandboxUtilities、Common、StateManager 的 cooked 内容，`providers: &[]`；构建工具生成 Pak，再将 Pak 放入 ZIP。Rust 和开发用 FSD C++ 工程属于作者工具链，不是玩家端依赖。[打包定义](https://github.com/trumank/drg-mods/blob/d6ff9fac56abf15937a01824332aa970232768f6/src/main.rs#L161)

## 3. 真正可以参考的蓝图 Hook，以及它的边界

同仓库 **Custom Difficulty 2** 的 `cd2::splice_pls_base` 确实会修改游戏蓝图：开发机提取 `Landscape/PLS_Base` 的 cooked 资产，在已有 `SetSeed` 调用后插入自己的等待/通知逻辑，重定位跳转，再写出替换资产。该 provider 挂在 Custom Difficulty 2 的打包任务上，**不在 Sandbox 的任务上**。[实现](https://github.com/trumank/drg-mods/blob/d6ff9fac56abf15937a01824332aa970232768f6/src/main.rs#L451)

这证明 Pak 能承载开发阶段改好的蓝图程序。对有蓝图调用边界的目标，可以插入前后通知；代价是游戏版本兼容性和相同资源覆盖冲突。但本项目要求的自然波次来源边界在 EXE 的直接 C++ 调用链中，不在 `PLS_Base` 的图里。修改地图初始化图不能自动改写这段机器码。

因此应借鉴其**定位实际可插入边界、构建期处理资产**的方法，不能把“能插入蓝图函数”推广成“能拦截任意原生调用”。

## 4. 当前游戏中，来源信息在哪里丢失

下面的地址都是上述 EXE 的 RVA，不能移植到其他版本后继续直接使用。

```text
EnemyWaveManager 原生 Tick 0x16af180
  → 自然普通波次例程 0x16ab690
  → 无额外排除项的池生成包装 0x19db3a0
  → 池选择 0x19db030
  → 批量分发 0x19dbcd0：构造空 completion delegate
  → 共享生成例程 0x19db3d0
  → SpawnEnemy 入队 0x16571c0
  ... 队列可以延后处理 ...
  → UWorld::SpawnActor 0x37e89a0
  → 非空返回后：单项回调 → 通用 OnEnemySpawned → RemoveAtSwap
```

关键证据及含义：

1. `0x19dbd40–0x19dbd5e` 构造空弱对象和空函数名；`0x19dbe57–0x19dbe71` 将该空 completion delegate 交给共享生成例程。**自然池生成不会提供 Sandbox 那种可绑定到自己的完成回调。**
2. `0x19db3a0` 包装的空参数是一个数组，不是回调。当前 `SpawnEnemiesFromPool` thunk `0x1bdc420` 比旧公开头文件多读取一个对象指针数组；原生实现用它参与描述符排除。不能把旧签名当作当前完整接口，也不能把这个额外参数误判为隐藏回调。
3. 成功创建后，游戏在 `0x16615a4` 广播通用事件，然后才交换删除队列项。广播期间队列项尚在，但事件没有当前队列下标、自然来源或波次 ID；队列中“空回调”也不是自然来源的唯一特征。
4. `EnemyWaveManager.OnEnemySpawned` 的当前原生处理体只操作 AI Blackboard 的 `IsAlerted` 状态；这是 callable handler，不能根据名字当成自然波次专属广播。`IsAlerted` 本身在其他代码中也被使用。
5. 同仓库的 EventLog 通过 `EnemySpawnManager.OnEnemySpawned`、伤害和死亡委托建立自己的事件记录。没有发现它提供读取引擎原始日志流的蓝图桥接；“日志 Mod”并不意味着能订阅 EXE 的所有日志输出。

自然池包装和公开的池生成 UFunction 最终汇入同一池实现，随后丢弃专属来源并进入相同队列结构。只观察最后的 Pawn/Descriptor/位置，无法从这些字段唯一倒推出原来的调用者。此结论针对**这些已检查的观测字段**；没有声称已穷举游戏所有可能的副作用或隐藏接口。

“每帧比较队列 + 波次计时器”也未证明等价：Tick 会处理脚本波次和普通波次的调度，同次 Tick 内可能走两个分支；队列还可能在两次快照间入队、成功生成、交换删除。提高到每帧轮询仍不等于同步来源标记。改成检查所有敌人 `BeginPlay` 可以看到成功对象，却同样缺少来源，且对象此时位置还需与原始生成输入变换核对。

## 5. 能保持原目标的具体方案

**推荐架构：在游戏的原生来源边界保留波次标记，并由游戏向蓝图广播成功事件；本 Mod 只发布订阅 Pak。** 具体接口和处理顺序见 [原生事件提案](NORMAL-WAVE-EVENT-PROPOSAL.md)。这条架构能满足目标，但当前缺少游戏端实现，不能由我们只在 Mod 里新增 dummy 声明完成。

| 层 | 所需实现 | 当前状态 |
| --- | --- | --- |
| 游戏原生代码 | 自然调度分支分配 WaveId；每个成功入队项携带该 ID；成功 SpawnActor 后广播原始输入位置、Pawn、Descriptor 和 ID；交换删除保留对应关系。 | 需要游戏开发方提供，或发现已有等价入口；本轮未找到。 |
| Pak 事件接收 | `InitCave` 初始化房主接收器，绑定上述委托；换图解绑；以当前 World + WaveId 分组。 | 有清晰实现路径，但接口落地前不能声称有效。 |
| Pak 显示 | 把当前 `NwiPoll` 原生赋值改为蓝图事件输入；移植已有最多 8 组、8 米分区、成功位置、TTL、距离和闪烁逻辑。 | 显示/设置资产可复用；不必引入 Sandbox 作为运行依赖。 |
| 空间站和设置 | `InitSpacerig` 初始化设置/Mod Hub 页面；不依赖 DLL 的 World 轮询引导。 | 使用游戏已有加载机制。 |
| 发布 | Cook 仅自有资产，ZIP 内为 Pak；开发者仍可使用 C++/PowerShell 构建。 | 接口和无加载器实测通过后才解除 Modio 打包阻断。 |

这里的关键是把必要的原生能力放到**所有玩家已有的游戏实现**中，和 Sandbox 使用地形接口的方式一致。若把同样的桥做成另一个需要玩家安装的 DLL，只是转移依赖，并未满足目标。mod.io 能传输文件，也不代表 DRG 的内容加载流程会注册新的原生模块。

已准备可以交给游戏开发方审阅的最小接口说明，但没有替用户发送。开发方是否接受、何时更新均未知。

## 6. 如果必须完全由我们独立完成

当前可考虑的纯蓝图路径是**自己生成且标记波次**：暂停内置自然调度，建立自己的控制器，使用带回调的逐描述符生成接口，以控制器的 WaveId 归属成功对象。其回调模式可以参考 Sandbox。

它能准确知道“自己生成的虫属于自己的哪一波”，但有两个尚未解决的关键问题：

- 必须重现原游戏的倒计时、暂停/恢复、随机调用顺序、敌人池选择、难度修正、玩家分组、导航点选择、数量和队列行为。公开池接口本身不提供完成回调，因此不能只包一层函数就宣称全部等价。
- 内置调度被暂停、替换后，这成为改变刷怪逻辑的 Mod。即使宏观频率接近，也不能称为准确识别原游戏的自然潮。与其他波次 Mod、任务事件的兼容性也需重新验收。

**本轮未实施这项重大架构变更。** 这不是一个已经攻克且待打包的等价方案；只有用户接受改变刷怪机制和相应验收标准后，才适合独立立项。扫描敌人、时间窗、按 `IsAlerted` 分类也不能作为本任务的默认降级实现。

## 7. 验收方法与剩余工作

接口候选出现后，先以当前 DLL 作为**仅开发测试环境的对照采集器**，比较逐事件的 World、WaveId 分组、成功原始位置和唯一 SpawnId，再制作纯 Pak。参考采集器须在已验证调用链和兼容版本范围内使用，不能视为对所有未来版本的绝对真值。

必须覆盖：自然/虫蛋/脚本波次同帧重叠；相同描述符和位置的不同来源；生成失败；数量上限拒绝；队列延迟和交换删除；重入；关卡切换；启停/重启。要求误归属、漏记成功目标和重复记录为零，并确认无额外 RNG 消耗、刷怪次数和参数不变。真实游戏表现另外测首显延迟、帧耗和 Mod Hub。

最终在没有 Mint/MintCat 注入、UE4SSL、松散 DLL 的独立环境中，通过游戏内 mod.io 完成订阅、启用、进任务和重启测试；不能拿当前带加载器的安装验证“订阅即用”。当前发布阻断保持生效。

## 8. 本地复核资料

所有过程资料位于 `agent/codex/sandbox-research-20260907/`，不将第三方源资产、游戏二进制或反汇编文件提交到公开仓库：

- `source-version.json`、`upstream/`：固定源码提交。
- `read-assets.cjs`、`survey.cjs`、`coverage.cjs`：只读解析器和统计过程。
- `survey.json`、`parser-coverage.json`、`event-log-refs.json`：实际成员引用及完整解析限制。
- `pool-dispatch.txt`、`pool-reflected-thunk.txt`、`pool-implementation.txt`、`success-order.txt`：上述原生参数和执行顺序。
- `pool-wrapper-callers.jsonl`、`pool-callback-callers.jsonl`、`enqueue-callers.jsonl`：直接调用点扫描。`pool-callback-callers` 是调查早期文件名，实际目标 `0x19db030` 的额外参数已确认是数组。直接调用扫描不覆盖间接/虚表调用，函数 unwind 分段也不等于完整源码函数边界。

这些是静态研究证据，不是游戏内准确率或订阅验收结果。
