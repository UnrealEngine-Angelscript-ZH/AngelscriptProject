# Angelscript 插件新一轮技术债刷新与分流计划

## 背景与目标

### 背景

`Archives/Plan_TechnicalDebt.md` 已完成上一轮高优先级技术债收口，但当前仓库的债务入口已经再次漂移：

1. 本计划创建前，`Documents/Plans/Plan_OpportunityIndex.md` 与目录扫描都指向 `43` 份活跃 `Plan_*.md`；本计划加入后必须同步提升为 `44` 份，避免索引继续滞后于当前状态。
2. `Documents/Guides/TestCatalog.md` 仍以 `275/275 PASS` 作为已编目基线，而 `Documents/Guides/TechnicalDebtInventory.md` 已记录 `443` 个 full-suite 测试与 `7` 个当前已知失败，同时 `Plan_KnownTestFailureFixes.md` 已经开始承接这组失败项。
3. 代码结构本身出现了新一轮“不是立即修一个函数就能结束”的整理债务：`Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h` 中的 `FAngelscriptEngine` 既是 `USTRUCT()`，又承载生命周期、编译、热重载、world context、debug server、coverage 等多类职责；`ClassGenerator/AngelscriptClassGenerator.h` 也同时管理类、枚举、委托、reload 传播与 full/soft reload 路径。
4. 插件边界也在继续变模糊：`AngelscriptEditor` 有 `Public/Private` 分层，而 `AngelscriptRuntime` 仍按功能目录直铺；`AngelscriptRuntime/Tests/` 已经有 7 个运行时内部 C++ 测试文件，但 `AngelscriptTest/` 同时承担主题化集成、学习型 trace、示例与 scenario 测试，缺少统一 owner map。
5. 相对 Hazelight 参考源（通过 `AgentConfig.ini` 的 `References.HazelightAngelscriptEngineRoot` 指向），当前仓库仍存在结构与能力差距：Hazelight 有独立 `AngelscriptLoader`、`AngelscriptGAS`、`AngelscriptEnhancedInput` 插件与 `Script-Examples/`，当前仓库则把 GAS / EnhancedInput 直接内收进 `AngelscriptRuntime`，且插件级示例脚本远少于参考源。
6. 测试覆盖的弱点也已经足够明确：`AngelscriptRuntime/StaticJIT/` 与 GAS 路径几乎没有专项测试，`Subsystem/AngelscriptSubsystemScenarioTests.cpp` 目前只验证“当前分支仍不支持脚本生成”，`Component/` / `Delegate/` / `Editor/` 主题也明显偏薄。

这意味着当前真正缺的不是再开一份“大而全总路线图”，而是一份**只负责刷新 live debt、厘清 owner、把新债务分流到正确 sibling plan** 的执行计划。

### 目标

1. 重新冻结当前仍然有效的技术债事实口径，覆盖命名、边界、架构、Hazelight 对齐与测试覆盖五类问题。
2. 把每一项债务明确归类为：**已关闭**、**本计划继续承接**、**已有 sibling plan 承接**、**需要新建 sibling plan**、**受引擎侧能力限制阻塞**、**接受为结构差异 / 非当前对齐目标（accepted divergence）**。
3. 让 `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md` 与相关 sibling plan 的状态重新一致，避免下一轮继续重复盘点同一问题。
4. 为下一批真正要实施的工作输出一份最多 5 项的明确起点，而不是继续保留模糊 backlog。

## 范围与边界

- **范围内**
  - `Documents/Plans/` 中与技术债 owner、状态、优先级直接相关的 Plan 文档。
  - `Documents/Guides/TechnicalDebtInventory.md` 与 `Documents/Guides/TestCatalog.md` 的口径同步。
  - `Plugins/Angelscript/Source/AngelscriptRuntime/`、`Plugins/Angelscript/Source/AngelscriptEditor/`、`Plugins/Angelscript/Source/AngelscriptTest/` 的命名/边界/覆盖审计锚点。
  - `AgentConfig.ini` 中 `References.HazelightAngelscriptEngineRoot` 指向的 Hazelight 参考源，用于对齐能力边界和示例/模块拆分差异。
- **范围外**
  - 直接实现 GAS mixin、Enhanced Input mixin、Loader 模块、StaticJIT 测试、网络复制能力等真实代码改动。
  - 重开 `Archives/Plan_TechnicalDebt.md` 已明确关闭的 helper 命名、弃用 API、低风险 bind 收口等旧债务。
  - 用一次性大改名或大拆文件替代“先冻结 debt inventory + owner map”的整理工作。
  - 把 Hazelight 引擎分支能力直接误记为当前插件内可独立完成的普通待办。
- **执行边界**
  - 本计划优先产出“债务矩阵 + owner 分流 + 入口同步”，不把整理任务膨胀成新的总实现计划。
  - 所有路径说明都使用仓库相对路径，涉及 Hazelight 参考源时统一写成 `References.HazelightAngelscriptEngineRoot/<relative-path>`，不写死本机绝对路径。

## 影响范围

本次刷新涉及以下操作（按需组合）：

- **基线校准**：修正计划数量、已知失败、测试基线等文档口径漂移。
- **债务分级**：把条目标注为关闭 / 活跃 / sibling owner / 阻塞 / 待拆新计划。
- **边界澄清**：把命名债、模块边界债、引擎侧约束、测试 owner map 写清楚。
- **Hazelight 对照**：把引擎侧能力差距与插件侧可行动项分开记录。
- **计划分流**：把已存在的 sibling plan 重新挂接回机会索引与技术债入口文档。

### 按目录分组的文件清单

Documents/（9 个）：
- `Documents/Plans/Plan_OpportunityIndex.md` — 基线校准 + 计划分级
- `Documents/Guides/TechnicalDebtInventory.md` — 基线校准 + 计划分级
- `Documents/Guides/TestCatalog.md` — 基线校准 + 计划分级
- `Documents/Plans/Plan_HazelightCapabilityGap.md` — Hazelight 对照 + 计划分流
- `Documents/Plans/Plan_TestCoverageExpansion.md` — 计划分流
- `Documents/Plans/Plan_KnownTestFailureFixes.md` — 计划分流
- `Documents/Plans/Plan_FullDeGlobalization.md` — 边界澄清 + 计划分流
- `Documents/Plans/Plan_HazelightBindModuleMigration.md` — 计划分流
- `Documents/Plans/Plan_AngelscriptResearchRoadmap.md` — 边界澄清 + 计划分流

Runtime / Editor / Test 审计锚点（11 个）：
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h` — 命名债 + 架构边界
- `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.h` — 架构边界
- `Plugins/Angelscript/Source/AngelscriptRuntime/Tests/` — 测试 owner map 澄清
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/` — 测试覆盖缺口
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptGAS*.{h,cpp}` — Hazelight / 测试 gap 审计
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_UEnhancedInputComponent.cpp` — Hazelight / 测试 gap 审计
- `Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/EditorSubsystemLibrary.h` — Hazelight 注解差异
- `Plugins/Angelscript/Source/AngelscriptTest/Subsystem/AngelscriptSubsystemScenarioTests.cpp` — Unsupported feature 负向测试口径
- `Plugins/Angelscript/Source/AngelscriptTest/Component/AngelscriptComponentScenarioTests.cpp` — 弱覆盖审计
- `Plugins/Angelscript/Source/AngelscriptTest/Editor/AngelscriptSourceNavigationTests.cpp` — Editor 弱覆盖审计
- `Script/` — 插件级示例与测试脚本边界

Hazelight 参考源（5 组）：
- `AgentConfig.ini -> References.HazelightAngelscriptEngineRoot/Engine/Plugins/Angelscript/Source/AngelscriptLoader/` — Loader 模块对照
- `AgentConfig.ini -> References.HazelightAngelscriptEngineRoot/Engine/Plugins/AngelscriptGAS/` — GAS 插件对照
- `AgentConfig.ini -> References.HazelightAngelscriptEngineRoot/Engine/Plugins/AngelscriptEnhancedInput/` — Enhanced Input 插件对照
- `AgentConfig.ini -> References.HazelightAngelscriptEngineRoot/Engine/Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/EditorSubsystemLibrary.h` — 注解差异对照
- `AgentConfig.ini -> References.HazelightAngelscriptEngineRoot/Script-Examples/` — 示例与工作流对照

## 当前事实状态快照

1. **Plan 数量需要随本计划落地同步重算**：本计划创建前根目录已有 `43` 份活跃 `Plan_*.md`；本计划加入后应按 `44` 份同步回索引与后续引用文档。
2. **测试基线存在双口径**：`TestCatalog.md` 仍记录 `275/275 PASS` 的已编目基线，而 `TechnicalDebtInventory.md` 已进一步记录 live inventory、`443` suite 和 `7` 个已知失败；`Plan_KnownTestFailureFixes.md` 也已经开始承接这组失败。
3. **Runtime 核心类型同时承担过多职责**：`Core/AngelscriptEngine.h` 中 `FAngelscriptEngine` 是 `USTRUCT()`，但同时暴露创建模式、全局入口、world context、compile stage、bind、hot reload、coverage/debug 相关状态和 API。
4. **ClassGenerator 仍是大块职责集合**：`ClassGenerator/AngelscriptClassGenerator.h` 同时处理 class / enum / delegate reload、dependency propagation、soft/full reload、metadata 传播与对象重建。
5. **Public/Internal 边界不一致**：`AngelscriptEditor` 已采用 `Public/Private` 目录，而 `AngelscriptRuntime` 仍按功能分目录暴露；这使“哪些头是插件 API、哪些只是内部实现”难以从目录层一眼判断。
6. **测试 owner map 不清晰**：`AngelscriptRuntime/Tests/` 已有 7 个内部 C++ 测试文件，而 `AngelscriptTest/` 同时承载主题化 scenario、integration、learning trace 与 examples；当前缺少一份把三层测试边界写清楚的 debt owner map。
7. **Unsupported feature 与薄覆盖被混写**：`Subsystem/AngelscriptSubsystemScenarioTests.cpp` 当前明确断言 script subsystem 生成仍不支持；而 `Component/` / `Delegate/` / `Editor/` 主题则是“有功能但覆盖不足”，两类问题不应继续写成同一类 backlog。
8. **Hazelight 对照暴露出结构与示例差距**：参考源存在独立 `AngelscriptLoader`、`AngelscriptGAS`、`AngelscriptEnhancedInput` 插件，以及 `Script-Examples/EditorExamples`、`GASExamples`、`EnhancedInputExamples`；当前仓库仅有 `Script/Example_Actor.as` 与 `Script/Tests/`，且 `Plugins/Angelscript/Source/` 只包含 `AngelscriptRuntime`、`AngelscriptEditor`、`AngelscriptTest` 三模块。
9. **Hazelight 与当前插件存在明确注解差异**：`AngelscriptEditor/Public/FunctionLibraries/EditorSubsystemLibrary.h` 当前用 `UFUNCTION(BlueprintCallable)`，而 Hazelight 对照文件使用 `UFUNCTION(ScriptCallable)`；这是可落证据的 parity debt，而不是泛化“Editor 能力还差一点”。
10. **GAS / Enhanced Input 仍偏“集成存在、示例与 mixin 不完整”**：当前仓库已将 `AngelscriptGASActor`、`AngelscriptGASCharacter`、`Bind_AngelscriptGASLibrary.cpp` 等能力并入 runtime，也有 `Bind_UEnhancedInputComponent.cpp`；但相对 Hazelight 的独立插件、示例和 mixin 体系，当前更像“能力在、结构与验证不足”。

## 债务分级与执行策略

| 分类 | 当前事项 | 执行策略 |
| --- | --- | --- |
| **立即校准**（高影响 / 低风险） | Plan 数量、测试基线、已知失败、owner map 漂移 | 先修正文档与分流关系，不进入代码实现 |
| **先审计再分流**（高影响 / 中风险） | 命名债、Public/Internal 边界、大文件/混合职责 | 先冻结 debt matrix，再决定是否拆 sibling plan |
| **Hazelight parity 分层处理**（高影响 / 高不确定） | Loader、GAS、EnhancedInput、示例脚本、Editor 注解差异 | 先区分引擎侧 / 插件侧 / 工作流侧，再决定 owner |
| **覆盖缺口补齐**（高影响 / 高价值） | StaticJIT、GAS、Networking、Editor、Subsystem、Component、Delegate、GC | 先回写到测试主计划 / 已知失败计划，不在本计划里直接开测实现 |
| **不再重复打开**（已关闭） | 旧 helper 命名迁移、弃用 API 清理、上一轮低风险 bind 收口 | 只有新证据时才允许重新入表 |

## 分阶段执行计划

### Phase 0：重新冻结 debt baseline 与 owner map

> 目标：先把当前“哪些是事实、哪些是过期口径、哪些已经由其他计划承接”冻结下来。

- [ ] **P0.1** 校准 Plan / 测试 / 已知失败三套基线数字
  - 统一核对 `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md` 中的活跃 Plan 数、已编目测试数、live suite 状态与已知失败项，明确哪些数字是“编目基线”，哪些是“当前源码/回归事实”。
  - 本项不做任何功能修复，只修正口径漂移，避免后续计划继续引用过期的 active-plan 数或“仍保留 4 个失败”的旧说法。
  - 完成后应得到一份最小事实表：活跃 Plan 数、归档 Plan 数、当前已知失败 owner、测试基线解释。
- [ ] **P0.1** 📦 Git 提交：`[Docs/Debt] Chore: reconcile plan counts and test baseline facts`

- [ ] **P0.2** 产出新一轮技术债 live matrix
  - 以命名 / 边界 / 架构 / Hazelight parity / 测试覆盖五类为列，给每个债务条目至少记录：证据路径、当前状态、owner、下一步动作。
  - 对像 `FAngelscriptEngine`、`AngelscriptClassGenerator`、`Subsystem` negative tests、`EditorSubsystemLibrary` 注解差异、`StaticJIT` 零覆盖这类已经有明确锚点的问题，直接进入矩阵，不再靠口头说明维持。
  - 这份矩阵应成为后续 sibling plan 的唯一入口表，避免把同一问题在 `ResearchRoadmap`、`HazelightCapabilityGap`、`TestCoverageExpansion` 里各写一遍。
- [ ] **P0.2** 📦 Git 提交：`[Docs/Debt] Feat: add refreshed technical debt matrix with evidence and owners`

- [ ] **P0.3** 给每个 debt item 固定 owner 结论
  - 每一项都必须落到六选一：已关闭、继续留在本计划、转交现有 sibling plan、需要新增 sibling plan、明确标记为引擎侧阻塞/非插件工作、接受为结构差异 / 非当前对齐目标。
  - 不允许再保留“可能需要后续研究”“之后再看看”的模糊状态；即使暂时不做，也要写清楚是谁负责、为什么现在不做。
  - 本项完成后，本计划应只保留真正属于“刷新与分流”的工作，不继续膨胀成实现主线。
- [ ] **P0.3** 📦 Git 提交：`[Docs/Debt] Refactor: classify debt items by owner and execution path`

### Phase 1：命名、目录与边界债务收口

> 目标：把“命名看起来不顺”“目录职责说不清”的问题从主观印象变成可执行的 debt slices。

- [ ] **P1.1** 冻结命名债的判定规则与首批 live 项
  - 先明确哪些属于真实命名债：例如 `FAngelscriptEngine` 这种 `USTRUCT()` + 全局/生命周期重负载类型、`Learning/` 与 `Template/` 这类用途不够自解释的测试目录、`Bind_*.cpp` / `FunctionCallers_*` 这类“文件名与职责语义割裂”的文件族。
  - 同时把“只是历史命名但不值得现在批量改”的项排除出去，避免下一轮又把半个仓库改名当成 P0 工作。
  - 本项结果应生成一份“命名债候选 -> 是否要改 -> 归谁改”的清单，而不是直接进入 rename。
- [ ] **P1.1** 📦 Git 提交：`[Docs/Naming] Feat: freeze naming-debt criteria and first-pass inventory`

- [ ] **P1.2** 明确 Runtime / Editor / Test / Script 的目录边界
  - 把 `AngelscriptRuntime/Tests/`、`AngelscriptTest/`、`AngelscriptEditor/Private/Tests/`、`Script/` 各自承接什么层级写成明确规则，并与 `Plugins/Angelscript/AGENTS.md` 现有命名约束对齐。
  - 像 `Subsystem` 这种“当前只验证 unsupported”主题，要和 `Component` / `Delegate` 这种“能力存在但覆盖不足”的主题分开归类，防止负向测试和弱覆盖被继续混写。
  - 如果 `Learning/`、`Template/` 等目录命名继续保留，也要在本项说明它们为什么保留、承接什么责任。
- [ ] **P1.2** 📦 Git 提交：`[Docs/TestBoundary] Feat: define directory and ownership boundaries across runtime and test layers`

- [ ] **P1.3** 把命名/目录债映射到现有或新 sibling plan
  - 若某项只是文档口径或少量文件归属问题，可继续留在本计划；若某项已经演变成跨 10+ 文件的大规模 rename 或目录迁移，则必须拆到新 sibling plan。
  - 新 plan 的候选可以包括“测试目录命名规范化”“示例脚本目录与命名统一”等，但只有在矩阵确认存在稳定 owner 后才允许新建。
  - 本项目标是避免命名债再次回流成“顺手改一点”的无边界工作。
- [ ] **P1.3** 📦 Git 提交：`[Docs/Naming] Chore: route naming and directory debts to stable plan owners`

### Phase 2：架构与模块职责债务分流

> 目标：把真正需要架构切片的 debt 与普通命名/文档问题分开，防止继续混写。

- [ ] **P2.1** 固定 Runtime / ClassGenerator / Editor 的大职责热点
  - 以 `Core/AngelscriptEngine.h`、`ClassGenerator/AngelscriptClassGenerator.h`、以及必要的 `AngelscriptEditor` 主模块入口为锚点，记录哪些问题是“大文件 + 混合职责”，哪些只是 API 名称不好。
  - 像全局状态、reload 协调、generated code 管理、Editor 菜单/工具混合这类热点，要明确它们是“架构债”而不是“下一次顺手重构”。
  - 输出结果要足够细到可以判断：应该指向 `Plan_FullDeGlobalization.md`、`Plan_HazelightBindModuleMigration.md`、新的大文件拆分计划，还是只保留在 debt matrix。
- [ ] **P2.1** 📦 Git 提交：`[Docs/Architecture] Feat: freeze mixed-responsibility hotspots and refactor candidates`

- [ ] **P2.2** 区分插件内可做的架构债与生成/第三方边界债
  - `FunctionCallers_*`、`ThirdParty/`、`StaticJIT/` 这类路径要单独判断：哪些是生成产物治理问题，哪些是第三方/引擎本体问题，哪些才是插件架构需要承担的维护债。
  - 这一步的目标不是立即拆 `FunctionCallers_Full.cpp`，而是把“应改生成策略”“应改模块结构”“只是外部依赖边界”的 owner 先定下来。
  - 若某类问题更适合由 `Build/Tooling` 或 `AS2.38` 路线承接，就从本计划移出并标记 owner。
- [ ] **P2.2** 📦 Git 提交：`[Docs/Architecture] Refactor: classify generated and third-party boundary debts`

- [ ] **P2.3** 固定后续架构债的承接顺序
  - 把 `Plan_FullDeGlobalization.md`、`Plan_HazelightBindModuleMigration.md`、`Plan_AngelscriptResearchRoadmap.md` 与本计划的关系写清楚：哪些是本计划的直接下游，哪些只是背景参考。
  - 对尚无 owner 的架构项，最多只允许留下 1–2 个新增 sibling plan 候选；不能在本计划里继续生成一个新的“大而全架构 backlog”。
  - 完成后，本计划中的架构问题都应能回答“下一步去哪做、为什么不在这里做”。
- [ ] **P2.3** 📦 Git 提交：`[Docs/Architecture] Chore: align technical-debt routing with architecture sibling plans`

### Phase 3：Hazelight parity 与未实现能力债务分层

> 目标：把“相对 Hazelight 还差什么”从模糊大词拆成引擎侧限制、插件侧功能差距、工作流/示例差距三层。

- [ ] **P3.1** 用本地参考源刷新 parity matrix
  - 统一以 `AgentConfig.ini` 的 `References.HazelightAngelscriptEngineRoot` 为根，核对 `AngelscriptLoader`、`AngelscriptGAS`、`AngelscriptEnhancedInput`、`Script-Examples/`、`EditorSubsystemLibrary.h` 等对照锚点。
  - 对每个差距项都同时写出 Hazelight 侧证据路径和当前插件侧证据路径，避免“感觉少了某个系统”的空泛差距描述。
  - 本项结果应与现有 `Plan_HazelightCapabilityGap.md` 互补：前者偏新一轮 debt routing，后者保留能力 gap 本体盘点。
- [ ] **P3.1** 📦 Git 提交：`[Docs/Hazelight] Feat: refresh parity matrix with local reference evidence`

- [ ] **P3.2** 把 parity debt 切成三层 owner
  - 第一层是**引擎侧差距**：例如 Hazelight 的 engine fork / loader / UHT 级能力，不能伪装成普通插件 TODO。
  - 第二层是**插件侧功能/结构差距**：例如缺少 mixin、缺少 script examples、缺少明确的插件 API surface；但像 GAS / EnhancedInput 是否必须拆成独立插件，应先判断为“插件内可行动项”还是“accepted divergence”，不能默认记成 debt。
  - 第三层是**工作流与示例差距**：例如 `Script-Examples/EditorExamples`、GASExamples、EnhancedInputExamples 缺失，以及 `ScriptCallable` / `BlueprintCallable` 的注解偏差。
  - 只有明确分层后，后续计划才能避免把 loader 模块、示例脚本、engine patch 需求混成一个任务。
- [ ] **P3.2** 📦 Git 提交：`[Docs/Hazelight] Refactor: split parity debts into engine plugin and workflow layers`

- [ ] **P3.3** 固定 parity debt 的 sibling plan 归属
  - 与 `Plan_HazelightCapabilityGap.md`、`Plan_HazelightBindModuleMigration.md` 对齐，决定哪些项继续留在能力 gap 计划、哪些要补新 plan、哪些只作为 blocked-by-engine 记录保留。
  - 若需要新建计划，优先限制在单一主题，例如“Hazelight 示例与工作流对齐”“GAS / EnhancedInput mixin parity”，不要再回到“全量 Hazelight 对齐”这种不可执行范围。
  - 本项完成后，用户应能一眼看出：Hazelight 差距里哪些能在当前插件内做、哪些只能记录为架构约束。
- [ ] **P3.3** 📦 Git 提交：`[Docs/Hazelight] Chore: route parity debts to focused plan owners`

### Phase 4：测试覆盖、negative tests 与已知失败债务对齐

> 目标：把“没有覆盖”“当前明确不支持”“当前已知失败待修”三种测试债彻底拆开。

- [ ] **P4.1** 冻结测试债分类规则
  - 把 `StaticJIT`、GAS、Networking、Editor、GC、Delegate、Component、Subsystem 等领域分成四类：**零覆盖**、**弱覆盖**、**negative tests（当前明确不支持/作为能力边界证据存在）**、**known failures（当前应通过但未通过）**。
  - `Subsystem` 当前属于“negative tests 明确存在”，`Component` / `Delegate` / `Editor` 更接近“功能在但覆盖薄”，`StaticJIT` / GAS / Networking 更接近“高价值零覆盖”，而 full-suite 的 `7` 个失败则属于 `Plan_KnownTestFailureFixes.md` 范围；四类必须分别进入不同 owner。
  - 这一步完成后，测试 backlog 不应再把 unsupported feature、薄覆盖和真实失败混写成一类任务。
- [ ] **P4.1** 📦 Git 提交：`[Docs/TestDebt] Feat: classify coverage gaps negative tests and known failures`

- [ ] **P4.2** 让测试 debt 回到现有计划体系
  - 把 zero/weak coverage 条目优先回挂到 `Plan_TestCoverageExpansion.md`；把 `443` suite 里的 7 个已知失败继续交由 `Plan_KnownTestFailureFixes.md`；把 `StaticJIT` 这类已有专门计划的主题优先回挂到 `Plan_StaticJITUnitTests.md`；把测试层级 / 目录 / 命名边界优先回挂到 `Plan_TestSystemNormalization.md` 与 `Plan_TestModuleStandardization.md`。
  - 若现有 `Plan_TestCoverageExpansion.md` 过于泛化，本项应补一张“高优先 coverage debt shortlist”，但不得覆盖掉已经存在的专项或规范化 sibling plans。
  - 同时明确哪些领域暂不应该补测试，例如尚属引擎侧/结构性 blocked 的能力，以及哪些 negative tests 只是能力边界的长期证据。
- [ ] **P4.2** 📦 Git 提交：`[Docs/TestDebt] Chore: route coverage gaps and known failures to existing test plans`

- [ ] **P4.3** 固定下一批测试优先级入口
  - 以业务价值和当前缺口为准，冻结一份最多 5 项的测试 debt shortlist，优先考虑 `StaticJIT`、GAS、Networking、Editor 这类零覆盖或极薄覆盖的高价值区域。
  - 这份 shortlist 既要说明“为什么优先做”，也要说明“为什么不是先做 subsystem script generation”这类仍属 unsupported/blocked 的路线。
  - 最终输出要能直接喂给后续执行者，不必再次扫描整个测试目录。
- [ ] **P4.3** 📦 Git 提交：`[Docs/TestDebt] Feat: freeze next-wave coverage shortlist and rationale`

### Phase 5：索引同步、closeout 口径与下一轮入口

> 目标：把本次刷新结果同步回文档体系，避免它再次只存在于一次整理会话里。

- [ ] **P5.1** 同步 `Plan_OpportunityIndex.md` 与 roadmap 入口
  - 将本计划登记进“缺陷修复与重构”主线，并修正活跃 Plan 数量、状态说明与必要的优先级提示。
  - 若某些原本放在 `新建议 Plan` 的条目已经被现有 plan 覆盖，也在本项顺手收口它们的 owner 说明，避免索引继续保留重复入口。
  - 本项要保证执行者从索引直接能发现这份新计划，而不是只有知道文件名的人才能找到。
- [ ] **P5.1** 📦 Git 提交：`[Docs/Roadmap] Chore: sync technical debt refresh plan into opportunity index`

- [ ] **P5.2** 回写 debt inventory / test docs 的 closeout 说明
  - 把“已编目基线 vs live suite vs known failure owner”这条口径写回 `TechnicalDebtInventory.md`、`TestCatalog.md` 等需要长期引用的文档，避免它们继续只记录旧 closeout 快照。
  - 本项不是要重写整份文档，而是确保本计划新增的 debt owner / blocked-by-engine / current-known-failures 结论有长期可追踪落点。
  - 若某项不适合直接改 Guide，也要在本计划中写出为什么保留为 sibling plan 内部事实。
- [ ] **P5.2** 📦 Git 提交：`[Docs/Debt] Chore: sync refreshed debt ownership and test status explanations`

- [ ] **P5.3** 固定本计划的退出条件与下一轮起点
  - 明确什么情况下本计划可以归档：例如 debt matrix 稳定、owner 全部落地、索引同步完成、下一批 top 5 debt 已给出并有明确下游 plan。
  - 同时输出“下一轮从哪 5 项开始”的清单，避免本计划一结束，下轮又重新做一遍盘点。
  - 若在执行中发现需要新增 sibling plan，应在本项前完成并反向回写 owner；本项不允许带着“待定 owner”归档。
- [ ] **P5.3** 📦 Git 提交：`[Docs/Debt] Chore: define closeout gate and next-iteration starting set`

## 验收标准

1. `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md` 对 active plan 数、测试基线与已知失败 owner 的描述不再互相冲突。
2. 每个新一轮技术债条目都至少具备：证据路径、分类、owner、下一步动作。
3. 命名 / 边界 / 架构 / Hazelight parity / 测试覆盖五类问题被明确拆开，不再把 unsupported feature、引擎侧约束、accepted divergence 和插件内可实现缺口混写。
4. `Plan_HazelightCapabilityGap.md`、`Plan_TestCoverageExpansion.md`、`Plan_KnownTestFailureFixes.md`、`Plan_FullDeGlobalization.md`、`Plan_HazelightBindModuleMigration.md` 与本计划的承接关系清晰且互不重复。
5. 本计划 closeout 时能直接给出下一轮 top 5 起点，而不是再次生成一个模糊 backlog。

## 风险与注意事项

### 风险

1. **和现有 roadmap 重叠**：如果不主动区分本计划与 `Plan_AngelscriptResearchRoadmap.md`、`Plan_HazelightCapabilityGap.md` 的职责，本计划很容易重新变成“大总图”。
   - **缓解**：坚持“只刷新债务事实与 owner，不承担真实实现主线”。

2. **把引擎侧差距误写成插件 TODO**：Hazelight 的 loader / engine fork / UHT 级能力并非都能在纯插件内独立落地。
   - **缓解**：所有 parity debt 都必须同时写 Hazelight 证据和当前插件证据，并先做 engine/plugin/workflow 三层分类。

3. **命名债膨胀成大规模 churn**：像 `FAngelscriptEngine`、`Learning/`、`Template/` 这类命名问题很容易演变成全仓库 rename。
   - **缓解**：先固定判定规则和 owner，再决定是否值得拆成独立 rename plan。

4. **测试债继续混写**：zero coverage、negative tests、known failures 若不拆开，后续测试计划仍会重复走弯路。
   - **缓解**：在 Phase 4 中强制三分法，并明确回挂到现有测试 sibling plan。

### 已知行为变化

1. **`Plan_OpportunityIndex.md` 的数量与优先级提示会被重算**：活跃 Plan 数和技术债主线入口将不再继续沿用旧口径。
   - 影响文件：`Documents/Plans/Plan_OpportunityIndex.md`

2. **技术债文档将不再默认沿用“4 个已知失败”的旧 closeout 结论**：新口径会显式区分历史 closeout、当前 `443` suite 状态与 `Plan_KnownTestFailureFixes.md` 的 owner。
   - 影响文件：`Documents/Guides/TechnicalDebtInventory.md`、`Documents/Guides/TestCatalog.md`

3. **Hazelight 差距的记录方式会从“泛化差距清单”变成“带 owner 的可执行分层”**：引擎侧 blocked、插件内可做项、示例/工作流缺口会被明确拆开。
   - 影响文件：`Documents/Plans/Plan_HazelightCapabilityGap.md`、`Documents/Plans/Plan_TechnicalDebtRefresh.md`
