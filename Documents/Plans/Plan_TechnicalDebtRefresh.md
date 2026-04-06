# Angelscript 插件技术债重排与分流计划

## 背景与目标

### 背景

`Documents/Plans/Archives/Plan_TechnicalDebt.md` 已完成上一轮高优先级技术债收口，仓库随后又新增了一批 sibling plan、测试回归结论和插件工程化议题。**以下数量与状态均以 2026-04-06 的仓库快照为准**。当前真正的问题，不再是“有没有技术债计划”，而是**技术债事实口径、owner 和执行入口再次漂移**：

1. `Documents/Plans/` 根目录当前可见 `49` 份 `Plan_*.md`，其中 `Plan_OpportunityIndex.md` 是索引文档、`Plan_StatusPriorityRoadmap.md` 是状态总览文档，二者都不应计入活跃执行 Plan；`Documents/Plans/Archives/` 下另有 `6` 份已归档 Plan。本次刷新后，活跃执行 Plan 口径统一按 `47` 份计算。
2. 测试文档同时维护三套数字：`Documents/Guides/TestCatalog.md` 仍保留 `275/275 PASS` 的**已编目基线**；`Documents/Guides/TechnicalDebtInventory.md` 记录源码实时定义规模 `324` / `89` 文件；同文档第 17 节又记录最新 full-suite 为 `443 / 436 / 7`。如果不重新冻结定义，后续计划会继续引用不同口径。
3. 运行时债务已经从“零散 patch”演变成少数高密度热点：`Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h` 仍同时承担 engine 生命周期、编译、热重载、world context、debug / coverage 等职责；`Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.h` 继续承接 class / enum / delegate reload、传播、full / soft reload 与对象重建。
4. Bind 层与生成代码层出现了新一轮治理债：`Bind_BlueprintEvent.cpp:805`、`Bind_FName.cpp:83`、`Bind_BlueprintType.cpp:156` 仍保留明确 hack / 注释缺口；`FunctionCallers` 生成文件族已经成为需要独立 owner 的结构性债务，而不是“顺手清一点”的小修。
5. 测试债已经分裂成三种性质完全不同的问题：`Plugins/Angelscript/Source/AngelscriptTest/Subsystem/AngelscriptSubsystemScenarioTests.cpp` 把 subsystem script generation 的编译失败当成当前能力边界；`Documents/Plans/Plan_KnownTestFailureFixes.md` 承接的是当前 `7` 个应通过但未通过的 live failures；`Plan_TestCoverageExpansion.md` 与 `Plan_StaticJITUnitTests.md` 面向的则是 zero / weak coverage。
6. 插件交付债已经足够明确，不能继续埋在笼统 backlog 中：根 `README.md` 仍是 `NULL`，`Plugins/Angelscript/Angelscript.uplugin` 的 `DocsURL` / `MarketplaceURL` / `SupportURL` 仍为空，这些问题更适合由 `Plan_PluginEngineeringHardening.md` 承接，而不是继续混写进泛化技术债。
7. 相对 Hazelight 参考源，当前仓库仍存在 parity debt，但必须先拆层：`Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/EditorSubsystemLibrary.h` 仍是 `BlueprintCallable` 注解；`Plugins/Angelscript/Source/AngelscriptRuntime/FunctionLibraries/AngelscriptActorLibrary.h`、`AngelscriptComponentLibrary.h` 仍保留被注释掉的 `//UFUNCTION(ScriptCallable)` 痕迹；GAS / EnhancedInput 以内收形式存在于 runtime；Loader / 引擎补丁级能力不能被误写成普通插件 TODO。
8. `StaticJIT` / `JIT` 债务目前被低估成“只差单元测试”，但代码和文档已经表明它至少分成四层：`StaticJITConfig.h` 中的跨平台与 Editor gating（`AS_CAN_GENERATE_JIT`、`AS_SKIP_JITTED_CODE`）、`AngelscriptStaticJIT.h` 中 `FJITDatabase` 的全局单例 / shared-state 边界、`PrecompiledData.h/.cpp` 在四阶段编译流中的结构恢复职责、以及 `Plan_AS238JITv2Port.md` 所记录的 V1/V2 接口迁移债。
9. JIT 的代码生成与性能治理也还没有稳定 owner：`AngelscriptStaticJIT.cpp` 负责 `.as.jit.hpp` 生成、唯一符号命名和模块产物组织；`PrecompiledData.cpp` 仍保留一组手工 `TIMER_*` 指标；`Documents/Guides/TestPerformance.md` 已定义启动 / 热重载性能基线，却还没有把 StaticJIT 专项指标、生成产物验证和 Editor `AS_SKIP_JITTED_CODE` 的限制统一收口。

因此，本计划要做的不是再写一份“大而全路线图”，而是把当前 live technical debt **压缩成一份有证据、能分流、能给出下一批执行起点** 的文档入口。

### 目标

1. 重新冻结当前技术债的事实口径，至少覆盖：口径漂移、运行时架构、Bind / 生成治理、JIT / StaticJIT、测试分流、Hazelight parity、观测/性能治理、插件交付八类问题。
2. 为每个 debt item 固定 owner 结论：**已关闭**、**继续留在本计划**、**现有 sibling plan 承接**、**需要新增 sibling plan**、**blocked by engine / external**、**accepted divergence**。
3. 让 `Plan_TechnicalDebtRefresh.md` 成为唯一的 debt routing 入口，避免 `TechnicalDebtInventory.md`、`Plan_OpportunityIndex.md`、各 sibling plan 各自维护一套模糊 backlog。
4. 输出一份最多 `5` 项的下一轮执行起点，保证下一位执行者不需要重新做全仓扫描。

## 范围与边界

- **范围内**
  - `Documents/Plans/` 中与技术债 owner、优先级、路线分流直接相关的计划文档。
  - `Documents/Guides/TechnicalDebtInventory.md` 与 `Documents/Guides/TestCatalog.md` 中的 debt 口径与验证快照。
  - `Documents/Guides/TestPerformance.md` 中与启动 bind / 热重载 / 产物基线相关的性能记录规范。
  - `Plugins/Angelscript/Source/AngelscriptRuntime/`、`Plugins/Angelscript/Source/AngelscriptEditor/`、`Plugins/Angelscript/Source/AngelscriptTest/` 的 debt 锚点文件，用于固定证据与 owner。
  - `README.md`、`Plugins/Angelscript/Angelscript.uplugin` 这类已明确属于插件交付债的入口文件。
- **范围外**
  - 直接实现 import 解析、subsystem 支持、Loader 模块、GAS / EnhancedInput 独立插件化、StaticJIT 新测试、JIT v2 回移或其他真实代码功能改动。
  - 重新打开 `Archives/Plan_TechnicalDebt.md` 已关闭的历史债务，除非出现新证据证明 closeout 失效。
  - 一次性大规模 rename、目录迁移或大文件拆分；这类动作必须先通过 owner 分流形成独立 sibling plan。
  - 任何写死本机绝对路径的说明；涉及 Hazelight 参考源时统一写成 `References.HazelightAngelscriptEngineRoot/<relative-path>`。
- **执行边界**
  - 本计划只负责**刷新事实、固定 owner、给出下一轮入口**，不承担真实功能实现主线。
  - 所有“应该做”的项都必须附证据路径；没有证据的印象式 backlog 不允许进入矩阵。
  - 引擎侧能力、结构差异与插件内普通待办必须分层记录，不能继续混成一类技术债。

## 影响范围

本次重排涉及以下操作（按需组合）：

- **基线校准**：重算 active / archived plan 数量，澄清 `275/275`、`324/89`、`443/436/7` 三套测试口径。
- **债务矩阵刷新**：以八个主题重写 live debt matrix，并为每条记录附证据、owner、下一步动作。
- **owner 分流**：把各条债务稳定挂回现有 sibling plan，或明确标记为需要新增 plan / blocked / divergence。
- **主题拆层**：把 runtime 架构债、Bind / 生成治理债、JIT / StaticJIT 债、测试债、Hazelight parity、观测/性能治理债、插件交付债从一个“总 backlog”里拆开。
- **下一轮入口冻结**：把本轮分析最终压成最多 5 个可直接启动的工作入口。

### 按目录分组的文件清单

Documents / Plans / Guides（16 个）：
- `Documents/Plans/Plan_TechnicalDebtRefresh.md` — 基线校准 + 债务矩阵刷新 + owner 分流
- `Documents/Plans/Plan_OpportunityIndex.md` — active plan 数与 debt 主线入口同步
- `Documents/Guides/TechnicalDebtInventory.md` — 测试口径与 debt snapshot 同步
- `Documents/Guides/TestCatalog.md` — 已编目基线与 live suite 解释同步
- `Documents/Guides/TestPerformance.md` — 性能记录规范与产物基线同步
- `Documents/Plans/Plan_FullDeGlobalization.md` — runtime 架构债承接
- `Documents/Plans/Plan_HazelightBindModuleMigration.md` — Bind parity 层承接
- `Documents/Plans/Plan_BindParallelization.md` — Bind / generated governance 现有承接参考
- `Documents/Plans/Plan_AS238JITv2Port.md` — JIT V2 接口迁移承接
- `Documents/Plans/Plan_KnownTestFailureFixes.md` — live failures 承接
- `Documents/Plans/Plan_TestCoverageExpansion.md` — zero / weak coverage 承接
- `Documents/Plans/Plan_StaticJITUnitTests.md` — StaticJIT 专项测试债承接
- `Documents/Plans/Plan_TestSystemNormalization.md` — 测试分层与边界承接
- `Documents/Plans/Plan_TestModuleStandardization.md` — 测试模块命名与目录债承接
- `Documents/Plans/Plan_HazelightCapabilityGap.md` — parity debt 承接
- `Documents/Plans/Plan_PluginEngineeringHardening.md` — 插件交付债承接

Runtime / Editor / Test 锚点（16 组）：
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h` — runtime 混合职责 / 全局入口热点
- `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.h` — class generator 混合职责热点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.cpp` — 全局状态热点锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp` — local hack 锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FName.cpp` — local hack 锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintType.cpp` — 注释缺口锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionCallers/` — 生成代码治理锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/StaticJITConfig.h` — JIT 平台 / Editor gating 锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/AngelscriptStaticJIT.h` — `FJITDatabase` 与 StaticJIT shared-state 锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/AngelscriptStaticJIT.cpp` — JIT 生成产物与代码生成锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/PrecompiledData.h` — 预编译数据结构与 stage apply 锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/PrecompiledData.cpp` — 预编译数据恢复与手工计时器锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Tests/AngelscriptPrecompiledDataTests.cpp` — StaticJIT 当前窄覆盖锚点
- `Plugins/Angelscript/Source/AngelscriptTest/Subsystem/AngelscriptSubsystemScenarioTests.cpp` — negative tests / unsupported feature 锚点
- `Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/EditorSubsystemLibrary.h` — parity 注解差异锚点
- `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/AngelscriptStateDump.h` — runtime 观测 / JIT dump 锚点

插件交付入口（2 个）：
- `README.md` — 仓库级插件入口缺口
- `Plugins/Angelscript/Angelscript.uplugin` — 对外元数据与支持入口缺口

## 当前事实状态快照

1. **Plan 数量口径已漂移**：截至 `2026-04-06`，`Documents/Plans/` 根目录当前可见 `48` 份 `Plan_*.md`，扣除索引文档 `Plan_OpportunityIndex.md` 后，实际活跃执行 Plan 应按 `47` 份处理；`Archives/` 下另有 `6` 份归档 Plan。`Plan_OpportunityIndex.md` 仍写 `45` 份活跃 Plan。
2. **测试数字必须明确三分**：截至 `2026-04-06` 的文档快照中，`TestCatalog.md` 的 `275/275 PASS` 只代表已编目基线；`TechnicalDebtInventory.md` 同时记录了 `324` 个自动化入口定义、`89` 个文件，以及最新 full-suite `443 / 436 / 7`。
3. **runtime 核心类型仍是混合职责集合**：`FAngelscriptEngine` 仍公开 `Get()`、`TryGetCurrentEngine()`、`TryGetCurrentWorldContextObject()` 等静态入口，同时承载 compile、hot reload、debug、coverage、script root discovery。
4. **ClassGenerator 仍是 reload 总线**：`FAngelscriptClassGenerator` 继续同时管理 class / enum / delegate reload、依赖传播、full / soft reload、对象重建与 finalize。
5. **Bind 层还留有显式 hack / 注释缺口**：`Bind_BlueprintEvent.cpp:805` 写有 `This is a hack!`；`Bind_FName.cpp:83` 写有 `Ugly hack`；`Bind_BlueprintType.cpp:156` 仍注释掉 `CPF_TObjectPtr` 检查。
6. **生成代码已经是独立治理议题**：`FunctionCallers` 目录下的 generated file cluster 不再只是“大文件观感问题”，而是需要 owner、生成策略和验证入口的治理债。
7. **Subsystem 测试当前是能力边界证据，不是普通已知失败**：`AngelscriptSubsystemScenarioTests.cpp` 仍明确断言 world / game-instance subsystem script generation 在当前分支上应编译失败。
8. **Editor parity 有可落证据**：`EditorSubsystemLibrary.h` 仍使用 `UFUNCTION(BlueprintCallable)`，而不是 Hazelight 侧常见的 `ScriptCallable` 形式。
9. **插件交付入口仍未闭环**：根 `README.md` 当前只有 `NULL`；`.uplugin` 的 `DocsURL`、`MarketplaceURL`、`SupportURL` 均为空。
10. **当前活跃 debt routing 已经天然分散**：`Plan_FullDeGlobalization.md`、`Plan_KnownTestFailureFixes.md`、`Plan_TestCoverageExpansion.md`、`Plan_HazelightCapabilityGap.md`、`Plan_PluginEngineeringHardening.md` 都已经存在，但主入口尚未把这些 owner 关系稳定回写。
11. **StaticJIT 当前并不只是测试缺口**：`StaticJITConfig.h` 仅对 `Windows/Linux` 开启 `AS_CAN_GENERATE_JIT`，并在 `WITH_EDITOR` 下定义 `AS_SKIP_JITTED_CODE`；这意味着“Editor 自动化全绿”本身并不能证明真实 jitted 执行链路可用。
12. **JIT 状态属于 engine-owned shared state，而不是普通叶子工具代码**：`Documents/Knowledges/01_02_03_Global_State_Entry_And_Boundaries.md` 已明确把 `StaticJIT` 与 `PrecompiledData` 归到 `FAngelscriptOwnedSharedState`；`AngelscriptStaticJIT.h` 中 `FJITDatabase` 还持有 `Functions`、`FunctionLookups`、`GlobalVarLookups`、`TypeInfoLookups`、`PropertyOffsetLookups` 等全局查找表。
13. **现有 StaticJIT 自动化覆盖仍然极窄**：`AngelscriptPrecompiledDataTests.cpp` 当前只有 `PrecompiledData.EditorOnlyFlagRoundtrip` 与 `ModuleDiff.HighBitFlags` 两个回归；而 `Plan_StaticJITUnitTests.md` 已明确记录 `PrecompiledDataType`、`ApplyToModule_Stage1/2/3()`、`FScriptFunctionNativeForm`、`FJITDatabase` 都没有 direct tests。
14. **JIT 代码生成与性能/产物治理尚未稳定路由**：`AngelscriptStaticJIT.cpp` 负责 `.as.jit.hpp` 文件名、唯一模块名和符号命名；`PrecompiledData.cpp` 仍保留大量 `TIMER_*` 手工计时变量；`TestPerformance.md` 已建立启动/热重载性能规范，但还没有把 StaticJIT 专项指标、生成产物验证和 `AS_SKIP_JITTED_CODE` 限制统一回写成 debt owner 结论。
15. **函数库注解漂移比单个 Editor 文件更广**：`AngelscriptActorLibrary.h`、`AngelscriptComponentLibrary.h` 仍存在被注释掉的 `//UFUNCTION(ScriptCallable)`，说明 `ScriptCallable` / `BlueprintCallable` 的风格漂移并不只发生在 `EditorSubsystemLibrary.h` 一处。

## 债务矩阵与 owner 策略

| 主题 | 证据锚点 | 当前结论 | owner / 下一步 |
| --- | --- | --- | --- |
| **口径漂移** | `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md` | 必须立即校准 | 继续留在本计划，作为 Phase 0 主线 |
| **runtime 架构 / 全局状态** | `AngelscriptEngine.h`、`AngelscriptClassGenerator.h`、`Core/AngelscriptBinds.cpp` | 真实架构债，不能再伪装成“顺手重构” | 现有 `Plan_FullDeGlobalization.md` 承接；大文件治理是否独立立项由本计划判定 |
| **Bind hack / 生成治理** | `Bind_BlueprintEvent.cpp`、`Bind_FName.cpp`、`Bind_BlueprintType.cpp`、`FunctionCallers/` | 本地 hack、Hazelight parity 和 generated governance 已经混在一起 | 先由本计划拆层并暂时持有 local hack / generated governance 的 owner；Hazelight parity 层固定由 `Plan_HazelightBindModuleMigration.md` 承接；若后续新增 `Plan_BindHackCleanup.md` 或独立生成治理 plan，再由本计划完成移交 |
| **JIT / StaticJIT / PrecompiledData** | `StaticJITConfig.h`、`AngelscriptStaticJIT.h/.cpp`、`PrecompiledData.h/.cpp`、`AngelscriptPrecompiledDataTests.cpp` | 这条线同时包含平台 gating、shared-state、V1/V2 接口迁移、deterministic 单元覆盖和生成/产物治理，不应继续只算“StaticJIT 零覆盖” | `Plan_StaticJITUnitTests.md` 承接 deterministic 单元覆盖；`Plan_AS238JITv2Port.md` 承接 V2 接口迁移；代码生成/性能/产物治理先由本计划持有并在 owner 稳定后再分流 |
| **测试债分流** | `AngelscriptSubsystemScenarioTests.cpp`、`Plan_KnownTestFailureFixes.md`、`Plan_TestCoverageExpansion.md` | negative tests / known failures / zero-weak coverage 必须拆开 | `Plan_KnownTestFailureFixes.md`、`Plan_TestCoverageExpansion.md`、`Plan_TestSystemNormalization.md`、`Plan_TestModuleStandardization.md` |
| **Hazelight parity** | `Plan_HazelightCapabilityGap.md`、`EditorSubsystemLibrary.h`、`FunctionLibraries/*.h`、`References.HazelightAngelscriptEngineRoot/...` | 需要先分成 engine / plugin / workflow 三层，且注解漂移不能再只靠单一 Editor 文件代表 | `Plan_HazelightCapabilityGap.md` 承接；engine-side 项必须显式 blocked |
| **观测 / 性能治理** | `AngelscriptStateDump.h`、`TestPerformance.md`、`AngelscriptCodeCoverageTests.cpp` | runtime dump、性能产物和 JIT/热重载验证基础已经存在，但生产边界和长期 owner 仍不稳定 | 继续留在本计划；待与现有验证/benchmark 主线边界稳定后再决定是否拆独立 sibling plan |
| **插件交付债** | `README.md`、`Angelscript.uplugin` | 已足够明确，不应继续混在泛化技术债里 | `Plan_PluginEngineeringHardening.md` 承接 |

## 分阶段执行计划

### Phase 0：重算事实口径并固定 debt matrix

> 目标：先让所有后续计划基于同一组数字、同一组主题和同一套 owner 语言工作。

- [ ] **P0.1** 重算 Plan 数量与测试口径
  - 统一核对 `Documents/Plans/` 根目录下 `Plan_*.md` 的实际数量，明确哪些属于活跃执行 Plan、哪些只是索引 / 附录 / 归档文档，消除 `45` 份活跃 Plan 的旧口径。
  - 同步澄清 `275/275`、`324 / 89`、`443 / 436 / 7` 三套测试数字的定义与用途，避免任何后续文档再把其中一组拿去替代另外两组。
  - 本项产物必须是一组可直接引用的基线句式，而不是只在会话里口头解释。
- [ ] **P0.1** 📦 Git 提交：`[Docs/Debt] Chore: reconcile active plan counts and test baseline terminology`

- [ ] **P0.2** 以八个主题重写 live debt matrix
  - 按“口径漂移 / runtime 架构 / Bind 与生成治理 / JIT / StaticJIT / 测试分流 / Hazelight parity / 观测与性能治理 / 插件交付”八类，把当前仍有效的债务全部归入矩阵，并为每条记录补齐证据路径、当前状态、owner、下一步动作。
  - 只允许纳入有明确锚点的问题；没有文件证据的“感觉还差点什么”一律不进矩阵。
  - 矩阵完成后，`TechnicalDebtInventory.md` 只保留快照与回归事实，不再承担总 backlog 角色。
- [ ] **P0.2** 📦 Git 提交：`[Docs/Debt] Feat: refresh technical debt matrix by evidence and owner`

- [ ] **P0.3** 为每个 debt item 固定 disposition
  - 每一项都必须落到六选一：已关闭、继续留在本计划、现有 sibling plan 承接、需要新增 sibling plan、blocked by engine / external、accepted divergence。
  - 不允许保留“后续再研究”“可能单独开 plan”这种模糊状态；如果暂时不做，也必须写清楚是谁负责、为什么不做。
  - 本项完成后，本计划里不应再残留“无 owner 的 backlog”。
- [ ] **P0.3** 📦 Git 提交：`[Docs/Debt] Refactor: classify each debt item by stable execution path`

### Phase 1：拆开 runtime 架构债、Bind / 生成治理债与 JIT 结构债

> 目标：把最容易膨胀的两类结构性债务拆开，避免它们继续吞掉整个技术债计划。

- [ ] **P1.1** 固定 runtime 架构热点的承接边界
  - 以 `AngelscriptEngine.h`、`AngelscriptClassGenerator.h` 和 `Core/AngelscriptBinds.cpp` 为锚点，明确哪些问题属于 `Plan_FullDeGlobalization.md` 的显式上下文 / 去全局化路线，哪些只是“大文件或混合职责”的治理问题。
  - 不在本项中直接展开代码重构；目标是把 runtime debt 的 owner 固定下来，避免它再次回流主计划。
  - 若“大文件治理”已经具备稳定对象和边界，本项可以新增 `Plan_LargeFileSplit.md` 候选；若尚不稳定，则继续保留为 `Plan_FullDeGlobalization.md` 的前置边界说明。
- [ ] **P1.1** 📦 Git 提交：`[Docs/Architecture] Feat: route runtime hotspots to de-globalization or large-file owners`

- [ ] **P1.2** 把 Bind debt 拆成 local hack、parity delta、generated governance 三层
  - `Bind_BlueprintEvent.cpp`、`Bind_FName.cpp`、`Bind_BlueprintType.cpp` 这类本地 hack / 注释缺口，不能继续和 Hazelight 对齐项、generated file 治理项混写。
  - 先判断哪些项是当前仓库本地逻辑债、哪些项是对照 Hazelight 才能成立的 parity debt、哪些项属于生成策略 / 注册策略治理。
  - 本项完成后，每个 Bind 相关问题都应能回答“属于哪个主题、去哪一个计划修”。
- [ ] **P1.2** 📦 Git 提交：`[Docs/Binds] Refactor: split bind debt into local parity and generated-governance layers`

- [ ] **P1.3** 决定是否新增 Bind / 生成治理 sibling plan
  - 如果 local hack 问题已经聚合成稳定主题，则新增 `Plan_BindHackCleanup.md` 候选并同步写入 `Plan_OpportunityIndex.md`；如果 generated governance 已超出现有计划承载能力，则新增单独候选 plan。
  - 若现有 `Plan_HazelightBindModuleMigration.md`、`Plan_BindParallelization.md` 已足够覆盖某一层问题，则在这里明确“沿用现有 plan，不另起新题”。
  - 若本阶段结束时仍未新增 sibling plan，则 local hack / generated governance 继续由本计划持有，直到精确命名和范围稳定为止；本项只决定 owner，不在这里实施治理动作。
- [ ] **P1.3** 📦 Git 提交：`[Docs/Binds] Chore: finalize sibling-plan ownership for bind and generated-code debt`

- [ ] **P1.4** 固定 JIT / StaticJIT 债的分层与 owner
  - 以 `StaticJITConfig.h`、`AngelscriptStaticJIT.h/.cpp`、`PrecompiledData.h/.cpp`、`AngelscriptPrecompiledDataTests.cpp` 为锚点，把当前 JIT 债至少拆成四层：deterministic 单元覆盖、V1/V2 接口迁移、代码生成/产物治理、平台/Editor gating 与性能基线。
  - 明确 `Plan_StaticJITUnitTests.md` 只承接 deterministic 单元测试与 runtime-unit 入口，而 `Plan_AS238JITv2Port.md` 只承接 `asIJITCompiler` → `asIJITCompilerV2` 迁移；像 `.as.jit.hpp` 产物命名、手工 `TIMER_*` profiling、`AS_SKIP_JITTED_CODE` 带来的验证盲区，不能再被错误塞回“测试覆盖”或“AS238 回移”其中任何一条单线里。
  - 本项只负责 **JIT 专项 debt**：coverage、接口迁移、产物命名、平台/Editor gating 与 JIT 专项性能基线；不负责把这些指标如何写进通用报告目录、如何和 dump / coverage / startup-hotreload perf 入口统一，那部分统一交给 `P2.3`。
  - 本项完成后，执行者应能一眼看出：JIT 问题哪些是“先补测试”、哪些是“先做 API 迁移评估”、哪些暂时仍由本计划持有等待稳定 owner。
- [ ] **P1.4** 📦 Git 提交：`[Docs/JIT] Refactor: classify static jit debt by coverage interface governance and gating`

### Phase 2：拆开测试债、Hazelight parity 与插件交付债

> 目标：把“测试还差”“Hazelight 还差”“插件还不够可交付”这三类容易互相污染的话题彻底分层。

- [ ] **P2.1** 重新分类测试债并回挂现有测试计划
  - 把当前测试问题强制拆成四类：zero coverage、weak coverage、negative tests、known failures；`Subsystem` 负向测试属于能力边界证据，不得继续记成已知失败。
  - `Plan_KnownTestFailureFixes.md` 继续只承接当前 `7` 个 live failures；`Plan_TestCoverageExpansion.md` 与 `Plan_StaticJITUnitTests.md` 只承接 zero / weak coverage；其中 StaticJIT 只回挂 deterministic unit coverage，不把 JIT v2 接口迁移、代码生成治理或平台 gating 混进测试债；层级和命名漂移则优先回挂 `Plan_TestSystemNormalization.md` 与 `Plan_TestModuleStandardization.md`。
  - 完成后，任何新的测试债都必须先回答“它属于哪一类”，才能进入计划系统。
- [ ] **P2.1** 📦 Git 提交：`[Docs/TestDebt] Refactor: classify test debt by coverage boundary and failure ownership`

- [ ] **P2.2** 把 Hazelight parity 拆成 engine / plugin / workflow 三层
  - 以 `Plan_HazelightCapabilityGap.md`、`EditorSubsystemLibrary.h`、`FunctionLibraries/*.h`、`References.HazelightAngelscriptEngineRoot/...` 中的 Loader / GAS / EnhancedInput / Script-Examples 为证据，分别标记引擎侧差距、插件侧可行动项、工作流与示例差距。
  - 引擎补丁、UHT、Loader 这类项必须显式标记 `blocked by engine` 或 `accepted divergence`，不再伪装成普通插件 TODO。
  - `BlueprintCallable` 与 `ScriptCallable` 这类有可落证据的注解漂移，必须与“整个 Loader 体系缺失”分开记录，也不能再只用单个 `EditorSubsystemLibrary` 例子代表全局问题。
- [ ] **P2.2** 📦 Git 提交：`[Docs/Hazelight] Refactor: split parity debt into engine plugin and workflow layers`

- [ ] **P2.3** 固定观测 / 性能治理债的当前 owner
  - 以 `AngelscriptStateDump.h`、`TestPerformance.md`、`AngelscriptCodeCoverageTests.cpp` 和 JIT 手工计时器为证据，明确“runtime 观测代码是否继续留在生产模块”“性能产物/报告是否已形成稳定验证入口”“JIT 专项性能指标和 generated artifact 验证由谁承接”。
  - 本项只负责 **跨主题治理入口**：报告目录、日志/metrics 产物、dump 与 coverage 的生产边界、以及 startup/hotreload/JIT 指标如何并入统一 perf/observation 入口；不重新判定 JIT 专项 coverage、V2 接口迁移或 `.as.jit.hpp` 产物命名本身的 owner。
  - 本项不要求立即新建 dedicated plan，但必须给出当前 owner：若边界仍不稳定，则继续由本计划持有；若后续确实演变成稳定主题，再以独立 sibling plan 形式移交。
  - 这一步的目标是避免 dump、coverage、startup/hotreload perf 和 StaticJIT 专项观测长期各自存在、没人负责汇总成 debt routing。
- [ ] **P2.3** 📦 Git 提交：`[Docs/Perf] Chore: define current ownership for runtime observation and performance debt`

- [ ] **P2.4** 把插件交付债完整挂回 hardening 主线
  - `README.md`、`.uplugin`、CI / BuildPlugin / 兼容矩阵 / 发布与支持入口这些问题，已经具备独立主线，不应继续作为泛化 technical debt 留在总 backlog 中。
  - 本项要把这些条目从技术债矩阵中标成“由 `Plan_PluginEngineeringHardening.md` 承接”，同时保留必要的证据路径与优先级说明。
  - 完成后，本计划只保留对 hardening 主线的导航，不再重复 hardening 的实现分解。
- [ ] **P2.4** 📦 Git 提交：`[Docs/Hardening] Chore: route plugin deliverable debt into hardening plan`

### Phase 3：同步入口并冻结下一轮 top 5 起点

> 目标：把本次重排结果写回仓库入口，并给出下一位执行者可以直接开工的 5 个起点。

- [ ] **P3.1** 同步索引、清单与测试入口文档
  - 把重算后的 active / archived plan 数、debt owner 入口和测试口径说明同步回 `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md`。
  - 如果某个 debt item 已被 sibling plan 完整承接，应在入口文档中取消重复登记，避免同一问题出现多个模糊入口。
  - 本项完成后，执行者应能从任一入口文档找到同一套 debt routing 结论。
- [ ] **P3.1** 📦 Git 提交：`[Docs/Debt] Chore: sync debt routing into index inventory and test entry docs`

- [ ] **P3.2** 冻结下一轮 top 5 起点
  - 以影响面、可执行性和当前证据成熟度为准，固定以下 5 个首批入口，并为每项写清楚“为什么现在做、为什么不是别的项先做”：
    - `Plan_PluginEngineeringHardening.md`：先补 `README.md`、`.uplugin` 对外元数据与最小 CI / BuildPlugin 入口。
    - `Plan_KnownTestFailureFixes.md`：继续吃掉当前 `7` 个 live failures，避免 debt inventory 长期带失败快照。
    - `Plan_TestCoverageExpansion.md` + `Plan_StaticJITUnitTests.md` + `Plan_AS238JITv2Port.md`：StaticJIT 先补 deterministic unit coverage，同时并行完成 V1/V2 JIT 接口 owner assessment；GAS / EnhancedInput 零覆盖仍由测试主线承接。
    - `Plan_FullDeGlobalization.md`：选择 `AngelscriptEngine` / `ClassGenerator` 调用链中的第一条显式 owner slice。
    - Bind / generated governance owner：依据 Phase 1 结论，优先启动 `Plan_HazelightBindModuleMigration.md` 的 parity 层工作；若 `Plan_BindHackCleanup.md` 或独立生成治理 plan 尚未落名，则继续由本计划临时持有 local hack / generated governance routing。
  - 这份 top 5 必须可直接交给执行者，不允许写成新的模糊 backlog。
- [ ] **P3.2** 📦 Git 提交：`[Docs/Debt] Feat: freeze next-wave technical debt starting set`

- [ ] **P3.3** 定义本计划的归档闸门
  - 只有当 debt matrix 稳定、全部条目有 owner、入口文档同步完成、top 5 起点固定且无待定 owner，本计划才允许归档。
  - 若执行中新增 sibling plan，必须在本项前完成 plan 名称、作用范围和入口回写；不能带着“待起名 / 待拆分”进入归档。
  - 归档时要明确说明：本计划完成的是“重排与分流”，不是 runtime、Bind、测试、hardening 等技术债已经全部实现完成。
- [ ] **P3.3** 📦 Git 提交：`[Docs/Debt] Chore: define archive gate for debt refresh and routing plan`

## 验收标准

1. `Plan_OpportunityIndex.md`、`TechnicalDebtInventory.md`、`TestCatalog.md` 对 active plan 数、测试基线和 debt owner 的说法不再互相冲突。
2. 技术债矩阵中的每条记录都至少具备：证据路径、分类、owner、下一步动作、当前 disposition。
3. runtime 架构债、Bind / 生成治理债、JIT / StaticJIT 债、测试债、Hazelight parity、观测/性能治理债、插件交付债被明确拆开，不再混写成一个总 backlog。
4. `Plan_FullDeGlobalization.md`、`Plan_HazelightBindModuleMigration.md`、`Plan_AS238JITv2Port.md`、`Plan_KnownTestFailureFixes.md`、`Plan_TestCoverageExpansion.md`、`Plan_StaticJITUnitTests.md`、`Plan_TestSystemNormalization.md`、`Plan_TestModuleStandardization.md`、`Plan_HazelightCapabilityGap.md`、`Plan_PluginEngineeringHardening.md` 与本计划的承接关系清晰且无重复入口。
5. StaticJIT / JIT 相关问题至少被拆成 deterministic unit coverage、V2 接口迁移、代码生成/产物治理、平台/Editor gating 四层，并各自有明确 owner 或当前暂持 owner。
6. 本计划 closeout 时能够直接交付下一轮 top 5 起点，而不是要求执行者重新做一次全仓扫描。

## 风险与注意事项

### 风险

1. **本计划再次膨胀成“新总路线图”**：如果在重排过程中把 hardening、去全局化、Bind 治理、覆盖补测的真实实施步骤都写回本计划，本计划会再次失去边界。
   - **缓解**：坚持“只记录事实、owner 和下一步入口”，把实现细节留给对应 sibling plan。

2. **把结构差异误写成普通缺陷**：Hazelight 的 engine fork、Loader、UHT 级能力与当前插件内可做项不是一回事。
   - **缓解**：所有 parity debt 都必须先完成 engine / plugin / workflow 三层分类，再决定 owner。

3. **把测试债继续混写**：negative tests、known failures、zero / weak coverage 若再次被写成同一类，后续测试计划会继续重复走弯路。
   - **缓解**：在 Phase 2 强制四分法，任何测试债没有类型就不允许进入计划系统。

4. **新增 sibling plan 过多**：如果每个观察点都新开计划，会把 debt routing 再次变成 plan 爆炸。
   - **缓解**：只有当主题、范围和 owner 已稳定时才新增 plan；否则先挂回现有 sibling plan 或继续保留在本计划中。

5. **把 Editor 自动化绿灯误当成真实 JIT 可用**：`AS_SKIP_JITTED_CODE` 使得 EditorContext 更适合验证数据和分发，而不是实际 jitted 执行。
   - **缓解**：JIT 相关 debt 必须显式区分“Editor 下可验证的 deterministic 单元覆盖”和“需要独立运行环境才能验证的真实执行 / 产物链路”。

6. **把 StaticJIT 测试债和 JIT 架构债混写**：`Plan_StaticJITUnitTests.md` 能解决的是 deterministic coverage，不等于 V2 接口迁移、平台 gating 或产物治理已经有 owner。
   - **缓解**：在 Phase 1 先固定四层分类，再决定哪些去测试计划、哪些留在本计划、哪些交给 `Plan_AS238JITv2Port.md`。

### 已知行为变化

1. **Plan 数量口径将被重算**：`Plan_OpportunityIndex.md` 中 `45` 份活跃 Plan 的旧数字将不再沿用，入口文档会改成以当前根目录事实为准。
   - 影响文件：`Documents/Plans/Plan_OpportunityIndex.md`

2. **测试文档将显式维持三套数字**：`275/275` 继续保留为已编目基线，但不会再被当成 live suite 结果；`324 / 89` 与 `443 / 436 / 7` 的语义会被固定。
   - 影响文件：`Documents/Guides/TechnicalDebtInventory.md`、`Documents/Guides/TestCatalog.md`

3. **插件交付债将退出泛化 technical debt backlog**：`README.md`、`.uplugin`、CI / BuildPlugin / 兼容矩阵等问题会被明确归到 `Plan_PluginEngineeringHardening.md`。
   - 影响文件：`Documents/Plans/Plan_TechnicalDebtRefresh.md`、`Documents/Plans/Plan_PluginEngineeringHardening.md`

4. **Bind debt 记录方式会变成分层治理**：本地 hack、Hazelight parity、generated governance 将分别记录，不能继续共享一个模糊“Bind gap”标签。
   - 影响文件：`Documents/Plans/Plan_TechnicalDebtRefresh.md`、`Documents/Plans/Plan_HazelightBindModuleMigration.md`、可能新增的 Bind / generated governance sibling plan

5. **JIT debt 记录方式会从“StaticJIT 零覆盖”扩成多层治理**：deterministic 单元覆盖、V2 接口迁移、代码生成/产物治理、平台/Editor gating 会被分层记录，而不是继续挂成单一测试缺口。
   - 影响文件：`Documents/Plans/Plan_TechnicalDebtRefresh.md`、`Documents/Plans/Plan_StaticJITUnitTests.md`、`Documents/Plans/Plan_AS238JITv2Port.md`
