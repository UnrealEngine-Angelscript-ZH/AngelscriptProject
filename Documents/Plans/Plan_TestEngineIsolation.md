# 测试引擎隔离方案

## 背景与目标

### 背景

当前测试框架与全局 AngelScript 引擎之间存在系统性的隔离缺陷。`FAngelscriptEngine::Get()` 被 200+ 处调用（分布在 40+ 个文件中），全部隐式依赖全局指针 `GAngelscriptEngine`。现有隔离机制 `FScopedTestEngineGlobalScope` 只切换这一个指针，但以下进程级全局状态仍然暴露，导致测试引擎与生产引擎互相干扰、测试之间互相污染：

**未被 scope 覆盖的全局状态清单：**

| 状态 | 类型 | 位置 |
|------|------|------|
| `GAngelscriptEngine` | 进程级全局指针 | `AngelscriptEngine.cpp` L70 |
| `CurrentWorldContext` | `static UObject*` | `AngelscriptEngine.cpp` L89 |
| `FAngelscriptTypeDatabase` | 函数内静态单例 | `AngelscriptType.cpp` L47-51 |
| `FAngelscriptBinds` ClassFuncMaps 等 | 静态容器 | `AngelscriptBinds.cpp` |
| `FToStringHelper` | 静态注册表 | `Helper_ToString.h` |
| `bIsInitialCompileFinished` | `static bool` | `AngelscriptEngine.cpp` L74 |
| `bSimulateCooked` | `static bool` | `AngelscriptEngine.cpp` L72 |
| `bUseEditorScripts` | `static bool` | `AngelscriptEngine.cpp` L73 |
| `bTestErrors` | `static bool` | `AngelscriptEngine.cpp` L76 |
| `bIsHotReloading` | `static bool` | `AngelscriptEngine.cpp` L77 |
| `bUseAutomaticImportMethod` | `static bool` | `AngelscriptEngine.cpp` L80 |
| `GAngelscriptStack` | 全局指针 | `AngelscriptEngine.cpp` L69 |
| `GAngelscriptContextPool` | `thread_local` | `AngelscriptEngine.cpp` L91 |
| `GameThreadTLD` | `static asCThreadLocalData*` | `AngelscriptEngine.cpp` L90 |
| `StaticNames` / `StaticNamesByIndex` | 静态容器 | `AngelscriptEngine.cpp` L66-67 |

**冲突表现形式：**

- 测试引擎与生产引擎通过 `Get()` 互相干扰（`Get()` 的 subsystem 优先路径与 `GAngelscriptEngine` 可能不一致）
- 测试之间的状态泄漏（前一个测试修改的静态标志影响后续测试）
- Clone 引擎和 Full 引擎的 epoch 冲突（单 Full epoch 限制）
- TypeDatabase/BindState 等静态单例在测试间累积状态

### 目标

直接实现 Engine-Local State 架构：将所有进程级全局/静态状态迁移为引擎实例成员，使每个 `FAngelscriptEngine` 实例拥有独立的类型数据库、绑定状态、运行时标志和上下文空间。

最终状态：
- 多个 Full 引擎可同时存活且互不干扰
- 测试引擎与生产引擎完全隔离，无需全局指针 swap
- 消除单 Full Epoch 限制

实施分四阶段推进：
1. **Phase 1**：Context Stack 基础设施 — 引入栈式引擎解析替代全局指针，使 200+ 处 `Get()` 调用自动路由到正确引擎实例
2. **Phase 2**：静态标志迁移 — 将 `static bool` 标志移入引擎实例（最简单、最低风险的状态迁移）
3. **Phase 3**：核心单例迁移 — 将 TypeDatabase、BindState、ToStringHelper 从静态单例迁移为引擎实例成员
4. **Phase 4**：解除 Epoch 限制与验证 — 移除单 Full 约束，完善测试框架，全量回归

### 与其他计划的关系

- `Plan_FullDeGlobalization.md`：本计划是其具体实施路径。Phase 1 的 Context Stack 直接对应该计划 Phase 2 的"显式上下文入口设计"，后续 Phase 可合并推进。
- `Plan_TechnicalDebt.md`：已完成测试侧 helper 的 `FScopedGlobalEngineOverride` 统一和 DebugServer 收口。本计划从该基础出发继续深化。

### 2026-04-03 跟进验证

- 最新一轮 `Automation RunTests Angelscript.TestModule` 已在本仓通过，日志见 `Saved/Logs/TestRun_20260403_170014.log`。
- 这次回归变绿主要依赖**测试侧 containment 收紧**，而不是本计划目标中的 runtime 去全局化已经完成：
  - 新增了更强的测试清理入口 `DestroySharedAndStrayGlobalTestEngine()`，用于同时清理共享测试引擎与“无 subsystem tick owner 的 stray global engine”。
  - 多个晚段 scenario / hot-reload / interface / learning 测试改为通过本地 `AcquireFresh*Engine()` 先清 shared + stray global，再重新获取 clean shared clone，避免全量跑时从前序残留的 global runtime 上 clone。
  - `HotReload` / `Delegate` 的部分失败还包含测试预期漂移，本轮也同步对齐了当前 runtime 的返回值与日志格式。
- 重要的是：**`GAngelscriptEngine` 仍然存在且仍在使用**。当前 runtime 并未完成 Context Stack 或 Engine-Local State 改造；`FAngelscriptEngine::Get()` 仍是 `Subsystem -> GAngelscriptEngine` 的双路径解析。
- 结论：本轮只证明“通过更严格的测试 helper containment，可以把现有全量回归稳定下来”；它**不等于**本计划已完成，也不替代后续去全局化工作。

### 2026-04-04 构建/测试工具链前置项（暂缓实现，先文档化）

- 在继续推进 runtime 去全局化之前，当前仓库的 build/test 执行链路需要先标准化；否则后续每一轮 Phase 回归都会反复浪费在卡死、残留进程、日志不可观测和 worktree 冲突上。
- 这部分工作**不属于 runtime 去全局化本身**，因此单独拆到了：
  - `Documents/Plans/Plan_BuildTestScriptStandardization.md`
  - `Documents/Knowledges/UBT.md`
- 当前已确认的关键决策如下：
  - 默认构建入口改为 direct UBT：`dotnet <EngineRoot>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll`
  - 默认并发模式使用 `-NoMutex -NoEngineChanges`，允许共享引擎目录上的不同 worktree 并发构建，但在需要改写引擎输出时快速失败
  - 同一 worktree 仍然要求 single-flight；需要引擎改动时再显式切到串行模式
  - 所有构建/测试脚本硬上限统一为 `300000ms`，默认值分别为 build `180000ms`、test `300000ms`
  - timeout 或异常退出后必须清理进程树；测试脚本还必须通过 `-stdout -FullStdOutLogOutput -UTF8Output` 实现逐行实时输出
- 当前仓库只补了脚本级 TDD 起点：
  - `Tools/Tests/RunToolingSmokeTests.ps1`
  - `Tools/Tests/Helpers/WriteLines.ps1`
  - `Tools/Tests/Helpers/SpawnSleepTree.ps1`
- 这些 smoke tests 现在是故意保留的“先红状态”，因为共享执行层和正式脚本还没有实现；因此暂时**不要**把 `AGENTS_ZH.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md` 改成“必须走新脚本”的硬规则。

## 当前隔离机制分析

### 现有隔离手段

当前仓库已有以下隔离机制，但每种都只覆盖部分全局状态：

| 机制 | 覆盖范围 | 局限 |
|------|----------|------|
| `FScopedTestEngineGlobalScope` | 仅 `GAngelscriptEngine` 一个指针 | 静态标志、WorldContext、TypeDatabase 等全部泄漏 |
| `FScopedTestWorldContextScope` | 仅 `CurrentWorldContext` 一个指针 | 需要与引擎 scope 手动组合，容易遗漏 |
| Clone 引擎 | 共享底层 `asCScriptEngine`，避免重复 Full 初始化 | 共享的 TypeDatabase/BindState 仍是进程级单例；模块名通过 `InstanceId` 前缀避免冲突但不隔离其他状态 |
| 单 Full Epoch 限制 | 防止多个 Full 引擎争用进程级 Type/Bind/ToString | 限制了测试灵活性；`CreateTestingFullEngine` 在已有 Full 时直接拒绝 |
| `DestroySharedTestEngine()` | 释放共享 clone 引擎并清理全局 scope | 必须手动调用，遗漏时会导致 epoch 冲突 |
| `TGuardValue<bool>` | 临时修改并恢复单个静态 bool 标志 | 散落在各测试中，没有统一管理 |

### `FAngelscriptEngine::Get()` 的双路径问题

`Get()` 的当前实现（`AngelscriptEngine.cpp` L377-389）：

```cpp
FAngelscriptEngine& FAngelscriptEngine::Get()
{
    if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
    {
        if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
        {
            return *AttachedEngine;
        }
    }
    checkf(GAngelscriptEngine != nullptr, ...);
    return *GAngelscriptEngine;
}
```

Subsystem 优先的设计意味着：当编辑器运行中的 GameInstance 存在时，`Get()` 返回的是 subsystem 持有的引擎，而非 `GAngelscriptEngine`。测试代码通过 `FScopedTestEngineGlobalScope` 切换了 `GAngelscriptEngine`，但如果 subsystem 路径活跃，`Get()` 仍然返回生产引擎——**测试 scope 被绕过**。

### Shared-State Epoch 的连锁限制

`FAngelscriptOwnedSharedState` 管理 `asCScriptEngine*`、`PrimaryContext`、`PrecompiledData`、`StaticJIT`、`DebugServer` 等核心资源。关键约束：

- `GAngelscriptActiveOwnedSharedStates` 跟踪当前存活的 owned shared-state 个数
- 只有当该计数归零时，才执行进程级 reset（`ResetTypeDatabase`/`ResetBindState`/`FToStringHelper::Reset`/清空 `CurrentWorldContext`）
- 这意味着即使一个 Full 引擎关闭，只要还有 Clone 存活，TypeDatabase 等单例就保持旧状态
- 新的 Full 引擎创建会被拒绝（`GAngelscriptActiveOwnedSharedStates > 0`），导致需要完整 epoch 的测试必须先销毁所有现有引擎

### `FAngelscriptEngine::Get()` 的调用分布

通过代码搜索，`Get()` 的 200+ 处调用分布在以下子系统中：

| 子系统 | 文件数 | 典型调用场景 |
|--------|--------|-------------|
| 绑定层 (`Binds/`) | 20+ | `Bind_AActor`、`Bind_UObject`、`Bind_BlueprintType` 等在运行时通过 `Get()` 获取引擎来查询类型、编译脚本 |
| ClassGenerator | 2 | `AngelscriptClassGenerator.cpp` 中 80+ 处调用，用于类生成期间获取引擎 |
| 编译器/预处理器 | 3 | `as_compiler.cpp`、`as_builder.cpp`、`AngelscriptPreprocessor.cpp` 通过 `Get()` 查询引擎状态 |
| StaticJIT | 3 | JIT 编译和预编译数据路径依赖全局引擎 |
| DebugServer | 1 | 调试服务器通过 `Get()` 获取引擎上下文 |
| Editor | 3 | 编辑器模块的源码导航、内容浏览器等 |
| Testing | 3 | 运行时测试发现和执行 |
| Core | 1 | `AngelscriptEngine.cpp` 自身的内部调用 |

这些调用全部是隐式的全局依赖——没有任何显式参数传递引擎实例。

## 方案选择

针对上述问题，评估了三种解决策略：

### 方案 A：全面 Scoped Isolation（不选）

将 `FScopedTestEngineGlobalScope` 升级为全状态 RAII scope。

- 优点：改动量小，向后兼容
- 缺点：本质仍是"swap & restore"，并行测试不安全；不解决状态所有权问题，只是治标

### 方案 B：Engine Resolution Context Stack（不选，但作为方案 C 的基础设施）

用上下文栈替代全局指针。

- 优点：对调用方透明，嵌套安全
- 缺点：仍不解决 TypeDatabase 等单例的状态隔离
- 决定：**作为方案 C 的前置基础设施纳入 Phase 1**，而非独立方案

### 方案 C：Engine-Local State（选择）

将所有进程级静态单例迁移为引擎实例成员，每个引擎拥有完整独立的状态空间。

- 优点：真正的引擎隔离，支持并行测试和多引擎共存，从根本上消除全局状态冲突
- 缺点：改动量大，需要分批迁移，与 Hazelight 原版差异显著
- 决定：**直接以此为目标**，Context Stack 作为路由机制嵌入 Phase 1

### 选择理由

方案 A/B 是中间状态，无论如何最终都需要走到 C 才能根治问题。直接瞄准 C，省去中间层的过渡维护成本。Context Stack 作为 C 的必要基础设施保留在 Phase 1，但不作为独立里程碑。

## 分阶段执行计划

---

### Phase 1：Context Stack 基础设施

> 目标：引入栈式引擎解析机制，替代全局指针 swap。这是后续所有状态迁移的基础——静态单例方法需要通过 Context Stack 找到正确的引擎实例来访问 engine-local 状态。

- [ ] **P1.1** 实现 `FAngelscriptEngineContextStack`
  - 当前 `FAngelscriptEngine::Get()` 的解析顺序是 `Subsystem → GAngelscriptEngine`，测试引擎只能通过 swap 全局指针来介入，本质上是互斥的覆盖而非叠加；且在 subsystem 活跃时测试 scope 会被绕过
  - 在 `AngelscriptEngine.h` 中新增 `FAngelscriptEngineContextStack`：
    - `static void Push(FAngelscriptEngine*)` / `static void Pop(FAngelscriptEngine*)`
    - `static FAngelscriptEngine* Peek()` 返回栈顶引擎
    - `static bool IsEmpty()`
    - 内部实现为 `static TArray<FAngelscriptEngine*> Stack`（仅 GameThread，UE 自动化测试不跨线程）
  - `Pop` 时做安全检查：传入的 Engine 必须与栈顶一致，否则 `ensureAlways` 并记录错误（防止 scope 嵌套错乱）
  - 后续 Phase 2/3 中的静态单例方法（`FAngelscriptType::GetByXxx` 等）将通过 `Peek()` 找到引擎实例并访问其 engine-local 状态
- [ ] **P1.1** 📦 Git 提交：`[Runtime/EngineIsolation] Feat: add FAngelscriptEngineContextStack for stack-based engine resolution`

- [ ] **P1.2** 修改 `FAngelscriptEngine::Get()` 解析顺序，消除 `GAngelscriptEngine` 依赖
  - 当前 `Get()` 实现（`AngelscriptEngine.cpp` L377-389）先查 `UAngelscriptGameInstanceSubsystem::GetCurrent()`，再 fallback 到 `GAngelscriptEngine`
  - 修改解析路径为：`ContextStack::Peek() → Subsystem`，**不再 fallback 到 `GAngelscriptEngine`**
  - `FAngelscriptRuntimeModule` 初始化引擎后改为 `ContextStack::Push(Engine)` 代替 `SetGlobalEngine(Engine)`；Shutdown 时 `Pop`
  - Editor / commandlet 等原本依赖 `GAngelscriptEngine` 兜底的场景，统一改为在入口处 push 到 ContextStack
  - 同步修改 `IsInitialized()` 使其检查 ContextStack 是否非空或 Subsystem 是否存在
  - `GAngelscriptEngine`、`SetGlobalEngine()`、`TryGetGlobalEngine()`、`DestroyGlobal()` 标记为 deprecated，Phase 4 中彻底移除
  - 200+ 处现有 `Get()` 调用无需修改，解析路径自动切换
- [ ] **P1.2** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: replace GAngelscriptEngine with context stack in Get() resolution`

- [ ] **P1.3** 实现 `FAngelscriptEngineScope` RAII scope
  - 统一的引擎上下文管理 scope，替代现有的 `FScopedTestEngineGlobalScope` + `FScopedTestWorldContextScope` 手动组合
  - 构造时 `Push(Engine)` 到 ContextStack，同时保存/恢复 `CurrentWorldContext`、`GAngelscriptStack`；析构时 `Pop(Engine)` 并恢复
  - 可选接收 `UObject* WorldContext` 参数，非 null 时自动设置 world context
  - 禁止拷贝、支持移动语义
  - 这是所有引擎上下文切换的唯一入口，不再有"swap 全局指针"的操作
- [ ] **P1.3** 📦 Git 提交：`[Runtime/EngineIsolation] Feat: add FAngelscriptEngineScope RAII scope using context stack`

- [ ] **P1.4** Context Stack 基础回归测试
  - 在 `AngelscriptTest/Core/` 下新建 `AngelscriptEngineIsolationTests.cpp`，覆盖以下场景：
    - `Get()` 在栈为空时行为不变（走 Subsystem → Global 路径）
    - `Get()` 在栈有值时返回栈顶引擎
    - 多层 Push/Pop 后 `Get()` 返回正确引擎
    - Pop 传入错误引擎时触发 ensure
    - `FAngelscriptEngineScope` 析构后全局引擎指针和 WorldContext 正确恢复
- [ ] **P1.4** 📦 Git 提交：`[Test/EngineIsolation] Test: add context stack and engine scope regression tests`

---

### Phase 2：静态标志迁移

> 目标：将 `FAngelscriptEngine` 的 `static bool` 标志迁移为引擎实例成员。这是全局状态中最简单、影响面最窄的一类，适合作为 engine-local 迁移的首批实践。

- [ ] **P2.1** 审查所有静态 bool 标志的读写点
  - `bIsInitialCompileFinished`、`bSimulateCooked`、`bUseEditorScripts`、`bTestErrors`、`bIsHotReloading`、`bForcePreprocessEditorCode`、`bGeneratePrecompiledData`、`bUseAutomaticImportMethod`、`bScriptDevelopmentMode`、`bStaticJITTranspiledCodeLoaded` 共约 10 个标志
  - 逐一搜索每个标志在绑定层、编译器、JIT、测试代码中的所有读写点
  - 对每个标志分类：**实例标志**（应随引擎而异，如 `bTestErrors`、`bIsHotReloading`）vs. **进程标志**（所有引擎共享，如 `bStaticJITTranspiledCodeLoaded`）
  - 产出：分类清单，标注哪些进入 `FAngelscriptEngineConfig`，哪些保留为 static
- [ ] **P2.1** 📦 Git 提交：`[Docs/EngineIsolation] Docs: classify static bool flags into instance vs process scope`

- [ ] **P2.2** 将实例标志移入 `FAngelscriptEngine`
  - 根据 P2.1 的分类结果，将归类为"实例标志"的 `static bool` 改为 `FAngelscriptEngine` 的普通成员变量
  - 每个被迁移的标志，其读写点改为通过 `FAngelscriptEngine::Get().bXxx` 或 `ContextStack::Peek()->bXxx` 访问
  - 对于绑定层中无法方便获取引擎实例的位置，通过 `FAngelscriptEngineContextStack::Peek()` 获取当前上下文引擎
  - 初始值从 `FAngelscriptEngineConfig` 中设置，构造时初始化
  - 保留旧的 `static bool` 声明但标记为 deprecated，指向新的实例成员
- [ ] **P2.2** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate instance-scoped static bool flags to engine members`

- [ ] **P2.3** 静态标志迁移回归测试
  - 在 `AngelscriptEngineIsolationTests.cpp` 中新增测试：
    - 两个引擎实例的实例标志互不影响（修改 A 的 `bTestErrors` 不影响 B）
    - `FAngelscriptEngineScope` 切换引擎后，通过 `Get()` 访问的标志值跟随当前引擎
    - 进程标志（保留为 static 的）在所有引擎间共享
  - 在 `AngelscriptBindConfigTests.cpp` 等已有测试中验证迁移后行为不变
- [ ] **P2.3** 📦 Git 提交：`[Test/EngineIsolation] Test: verify static flag migration with multi-engine scenarios`

---

### Phase 3：核心单例迁移

> 目标：将 `FAngelscriptTypeDatabase`、`FAngelscriptBinds` 状态、`FToStringHelper`、`FAngelscriptBindDatabase`、`CurrentWorldContext` 从进程级静态单例迁移为引擎实例成员。这是隔离的核心步骤，完成后每个引擎拥有独立的类型和绑定空间。

#### 待迁移单例清单

| 单例 | 定义位置 | 访问方式 | 内部数据 | 作用 | 引用规模 | 清理方式 |
|------|----------|----------|----------|------|----------|----------|
| **`FAngelscriptTypeDatabase`** | `AngelscriptType.cpp` L37-52 | 函数内 `static` 单例，通过 `FAngelscriptTypeDatabase::Get()` 访问 | `RegisteredTypes` (TArray)、`TypesByAngelscriptName` (TMap)、`TypesByClass` (TMap)、`TypesByData` (TMap)、`TypeFinders` (TArray)、`TypesImplementingProperties` (TArray) | 所有 AS 类型的注册中心：脚本类型名 → C++ 类型映射、UClass → AS 类型映射、属性类型查询。绑定层、ClassGenerator、编译器查询类型时的唯一入口 | 70+ 个文件中约 200+ 处通过 `FAngelscriptType::GetByXxx` 间接访问 | `FAngelscriptType::ResetTypeDatabase()` 清空所有容器；仅在最后一个 shared-state release 时调用 |
| **`FAngelscriptBinds` 静态状态** | `AngelscriptBinds.h` L472-483、`AngelscriptBinds.cpp` L101-114 | `static` 类成员，直接通过类名访问 | `ClassFuncMaps` (TMap\<UClass*, TMap\<FString, FFuncEntry>>)、`RuntimeClassDB` (TMap)、`EditorClassDB` (TMap)、`BindModuleNames` (TArray)、`SkipBinds` (TMap)、`SkipBindNames` (TSet)、`SkipBindClasses` (TSet)、`PreviouslyBoundFunction` (int)、`PreviouslyBoundGlobalProperty` (int) | UClass 方法到 AS 函数的映射注册表、绑定跳过规则、绑定模块名索引。`BindScriptTypes()` 时填充，绑定层查询 UClass 方法可用性时使用 | `AngelscriptBinds.h/cpp` 内部 45+ 处直接访问；`Bind_BlueprintCallable.cpp`、`Bind_BlueprintType.cpp` 等外部 5+ 处 | `ResetBindState()` 清空所有容器和索引；仅在最后一个 shared-state release 时调用 |
| **`FToStringHelper` 注册表** | `Bind_FString.cpp` L391-395（`GetToStringList()` 函数内 `static`） | 通过 `FToStringHelper::Register()` 写入，`GetToStringList()` 读取 | `TArray<FToStringType>`：每条记录包含 TypeName、TypeInfo 指针、ToString 函数指针、是否隐式转换、是否 Handle 类型 | 为 AS 值类型注册 ToString 能力：`FString` 的 `+`/`Append` 运算符和 `Print` 函数通过此表查找如何将任意值类型转为字符串 | 34 个 `Bind_*.cpp` 文件中共 40+ 处 `Register()` 调用；`Bind_FString.cpp` 中 10+ 处 `GetToStringList()` 读取 | `FToStringHelper::Reset()` 清空列表；仅在最后一个 shared-state release 时调用 |
| **`FAngelscriptBindDatabase`** | `AngelscriptBindDatabase.h` L123-148、`AngelscriptBindDatabase.cpp` L15 | `static FAngelscriptBindDatabase Instance` 类成员，通过 `Get()` 访问 | `Structs` (TArray)、`Classes` (TArray)、`BoundEnums` (TArray)、`BoundDelegateFunctions` (TArray)、`HeaderLinks` (TMap) | Cooked game 用的绑定缓存文件（`Binds.Cache`）：编辑器生成、运行时加载，避免 cooked 环境依赖完整 editor metadata。`Save()`/`Load()`/`Clear()` 管理缓存生命周期 | `AngelscriptEngine.cpp` 3 处（Save/Load/Clear）、`Bind_UStruct.cpp` 3 处、`Bind_Delegates.cpp` 3 处、`Bind_UEnum.cpp` 1 处、`Bind_BlueprintType.cpp` 4 处，共 15 处 | `Clear()` 清空所有容器；在引擎 `BindScriptTypes` 流程末尾或 `#if AS_USE_BIND_DB` 路径中调用 |
| **`CurrentWorldContext`** | `AngelscriptEngine.h` L123、`AngelscriptEngine.cpp` L89 | `static UObject*` 类成员 | 单个 `UObject*` 指针 | 绑定层读取当前 world 上下文的入口：Timer、Widget、Subsystem、UWorld 等绑定函数通过此指针解析 `GetWorld()` | `AssignWorldContext` 写入，`FAngelscriptGameThreadScopeWorldContext` RAII 管理；Tick 末尾 Fatal 检查 | `SetGlobalEngine(nullptr)` 时清空；最后一个 shared-state release 时清空 |

#### 执行任务

- [ ] **P3.1** 迁移 `FAngelscriptTypeDatabase` 为引擎实例成员
  - `FAngelscriptTypeDatabase` 目前是 `AngelscriptType.cpp` 中的函数内静态单例（`static FAngelscriptTypeDatabase Database`），被 `FAngelscriptType::GetTypes()`、`Register()`、`GetByAngelscriptTypeName()`、`GetByClass()`、`GetByData()`、`GetByProperty()` 等间接访问
  - 在 `FAngelscriptEngine` 中新增 `FAngelscriptTypeDatabase TypeDatabase` 成员，在 `Initialize` / `InitializeForTesting` 中填充
  - `FAngelscriptType` 的静态方法内部改为：`FAngelscriptEngineContextStack::Peek()` 非空时用实例 TypeDatabase，否则 fallback 到旧静态单例（过渡期兼容）
  - `ResetTypeDatabase()` 改为引擎实例方法 + 旧静态路径的兼容调用
  - 所有绑定层的 `FAngelscriptType::GetByXxx` 调用在 `FAngelscriptEngineScope` 激活时自动路由到正确引擎的 TypeDatabase，无需逐个修改
- [ ] **P3.1** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate FAngelscriptTypeDatabase to engine instance member`

- [ ] **P3.2** 迁移 `FAngelscriptBinds` 静态状态为引擎实例成员
  - `FAngelscriptBinds` 的 7 个静态容器（`ClassFuncMaps`、`RuntimeClassDB`、`EditorClassDB`、`BindModuleNames`、`SkipBinds`、`SkipBindNames`、`SkipBindClasses`）加 2 个静态索引（`PreviouslyBoundFunction`、`PreviouslyBoundGlobalProperty`）在 `ResetBindState()` 时才被清理
  - 将这 9 个静态数据移入 `FAngelscriptEngine` 的一个 `FAngelscriptBindState` 聚合结构中
  - `FAngelscriptBinds` 中的静态方法（`AddFunctionEntry`、`SkipFunctionEntry`、`CheckForSkip` 等）改为通过 ContextStack 路由到引擎实例的 BindState
  - `ResetBindState()` 改为引擎实例方法
  - 同样保留旧静态路径作过渡期 fallback
- [ ] **P3.2** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate FAngelscriptBinds state to engine instance member`

- [ ] **P3.3** 迁移 `FToStringHelper` 注册表为引擎实例成员
  - `GetToStringList()` 函数内的 `static TArray<FToStringType>` 是 ToString 注册表的实际存储，34 个 `Bind_*.cpp` 文件中共 40+ 处 `Register()` 调用在 `BindScriptTypes` 时填充
  - 将 `TArray<FToStringType>` 移入 `FAngelscriptEngine` 作为实例成员
  - `FToStringHelper::Register()` 改为写入 ContextStack 当前引擎的注册表
  - `FToStringHelper::Generic_AppendToString()` 改为从 ContextStack 当前引擎的注册表中查找
  - `FToStringHelper::Reset()` 改为引擎实例方法
  - 注意：`Bind_FString.cpp` 中绑定阶段遍历 `GetToStringList()` 生成 `FString` 重载方法，此路径也需要路由到实例成员
- [ ] **P3.3** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate FToStringHelper to engine instance member`

- [ ] **P3.4** 迁移 `FAngelscriptBindDatabase` 为引擎实例成员
  - `FAngelscriptBindDatabase::Instance` 是类级 `static` 成员，持有 cooked game 的绑定缓存数据
  - 将 `Instance` 改为 `FAngelscriptEngine` 的实例成员，`Get()` 改为通过 ContextStack 路由
  - `Save()`/`Load()`/`Clear()` 改为操作引擎实例的 BindDatabase
  - 影响面较小（15 处引用），可与 P3.2 的 BindState 迁移协同完成
- [ ] **P3.4** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate FAngelscriptBindDatabase to engine instance member`

- [ ] **P3.5** 迁移 `CurrentWorldContext` 为引擎实例成员
  - `FAngelscriptEngine::CurrentWorldContext` 目前是 `static UObject*`，所有引擎共享同一个 world context 指针
  - 改为 `FAngelscriptEngine` 的普通成员变量，每个引擎有独立的 world context
  - `AssignWorldContext` / `FAngelscriptGameThreadScopeWorldContext` 改为操作当前上下文引擎的实例成员
  - 现有 `Tick` 末尾的 `CurrentWorldContext != nullptr` Fatal 检查改为检查实例成员
- [ ] **P3.5** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: migrate CurrentWorldContext to engine instance member`

- [ ] **P3.6** 核心单例迁移回归测试
  - 在 `AngelscriptEngineIsolationTests.cpp` 中新增测试：
    - 两个独立引擎各自注册类型，TypeDatabase 互不可见
    - 两个引擎各自 `BindScriptTypes`，BindState 互不影响
    - 两个引擎的 ToStringHelper 注册互不影响
    - 两个引擎各自持有不同的 CurrentWorldContext
    - Clone 引擎与 Source 引擎共享 TypeDatabase（Clone 语义不变）
  - 现有测试全量回归，确认无行为变化
- [ ] **P3.6** 📦 Git 提交：`[Test/EngineIsolation] Test: verify core singleton migration with multi-engine isolation`

---

### Phase 4：解除 Epoch 限制与测试框架完善

> 目标：核心状态已 engine-local 化后，单 Full Epoch 限制的根因消除。移除该限制，完善测试框架入口，全量验证。

- [ ] **P4.1** 移除单 Full Epoch 限制
  - 当前 `GAngelscriptActiveOwnedSharedStates > 0` 时拒绝创建新 Full 引擎，因为 TypeDatabase/BindState/ToStringHelper 是进程级单例
  - Phase 3 完成后这些状态已经 engine-local，多个 Full 引擎不再争用同一套单例
  - 移除 `CreateTestingFullEngine` / `Initialize` 中的 epoch 检查拒绝逻辑
  - `ReleaseOwnedSharedStateResources` 中的进程级 reset（`ResetTypeDatabase`/`ResetBindState`/`FToStringHelper::Reset`）改为各引擎在自己的 `Shutdown` 中清理实例成员，不再依赖全局计数
  - `GAngelscriptActiveOwnedSharedStates` 可保留做诊断计数，但不再作为创建阻塞条件
- [ ] **P4.1** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: remove single Full epoch restriction after engine-local state migration`

- [ ] **P4.2** 实现 `FAngelscriptTestFixture`
  - 提供标准化的测试入口，封装引擎创建、scope 管理、编译 helper 为一体
  - 在 `AngelscriptTestUtilities.h` 中新增 `FAngelscriptTestFixture`，封装三种测试模式：
    - `SharedClone`：使用共享 clone 引擎（默认模式，最轻量）
    - `IsolatedFull`：创建独立 Full 引擎（现在不再受 epoch 限制）
    - `ProductionLike`：尝试获取生产引擎，失败时创建 Full
  - Fixture 内部持有 `FAngelscriptEngineScope`，构造时自动建立隔离 scope
  - 提供便捷方法：`GetEngine()`、`BuildModule(Name, Source)`、`ExecuteInt(Function, OutResult)` 等
  - 对于 `SharedClone` 模式，构造时自动 reset 确保每个测试从干净状态开始
- [ ] **P4.2** 📦 Git 提交：`[Test/EngineIsolation] Feat: add FAngelscriptTestFixture as standardized test entry point`

- [ ] **P4.3** 迁移现有测试到新 API（首批）
  - 首批选择以下高频使用 scope 的测试文件进行迁移：
    - `AngelscriptEngineCoreTests.cpp`：引擎核心测试，包含多种 scope 使用模式
    - `AngelscriptBindConfigTests.cpp`：配置测试，依赖静态标志状态
    - `AngelscriptDelegateScenarioTests.cpp`：委托测试，涉及 WorldContext
  - 迁移原则：用 `FAngelscriptTestFixture` 替换手动 scope 组合；不改变测试逻辑本身
  - 后续新增测试必须使用 `FAngelscriptTestFixture` 或 `FAngelscriptEngineScope`，不得直接使用 `FScopedTestEngineGlobalScope`
- [ ] **P4.3** 📦 Git 提交：`[Test/EngineIsolation] Refactor: migrate first batch of tests to FAngelscriptTestFixture`

- [ ] **P4.4** 移除 `GAngelscriptEngine` 及旧静态 fallback 路径
  - P1.2 中 `GAngelscriptEngine`、`SetGlobalEngine()`、`TryGetGlobalEngine()`、`DestroyGlobal()` 已标记 deprecated，此步彻底删除
  - 删除 `GAngelscriptEngine` 全局变量声明和定义（`AngelscriptEngine.cpp` L70、`StaticJITHeader.h` 中的 `extern`）
  - 删除 `FScopedTestEngineGlobalScope` / `FScopedGlobalEngineOverride` 类型定义
  - 搜索仓库中所有残余的 `GAngelscriptEngine`、`SetGlobalEngine`、`TryGetGlobalEngine`、`DestroyGlobal` 引用并清除
  - Phase 3 中各单例的静态方法保留了"ContextStack 为空时 fallback 到旧静态单例"的兼容路径，在此一并审查：
    - 如果 fallback 无调用方，移除 fallback 和旧静态单例
    - 如果某些 bootstrapping 路径仍需 fallback（引擎创建期间 ContextStack 尚未 push），保留并标注为 `// Required: bootstrapping before any engine exists`
- [ ] **P4.4** 📦 Git 提交：`[Runtime/EngineIsolation] Refactor: remove GAngelscriptEngine and legacy static fallback paths`

- [ ] **P4.5** 更新文档
  - `Documents/Guides/Test.md` 新增"测试引擎隔离"章节，说明 `FAngelscriptTestFixture` 的三种模式选择标准
  - `AGENTS.md` / `AGENTS_ZH.md` 中更新引擎架构决策章节，反映 engine-local state 新架构
  - 更新 `Plan_FullDeGlobalization.md` 标注本计划已覆盖的部分
- [ ] **P4.5** 📦 Git 提交：`[Docs/EngineIsolation] Docs: update test guide and architecture docs for engine-local state`

---

## 验收标准

### Phase 1

- `FAngelscriptEngineContextStack` 正确实现 Push/Pop/Peek
- `FAngelscriptEngine::Get()` 解析路径为 `ContextStack::Peek() → Subsystem`，不再 fallback 到 `GAngelscriptEngine`
- `GAngelscriptEngine`、`SetGlobalEngine()`、`TryGetGlobalEngine()` 已标记 deprecated
- `FAngelscriptRuntimeModule` 初始化引擎后通过 ContextStack push 而非 `SetGlobalEngine`
- `FAngelscriptEngineScope` 可替代 `FScopedTestEngineGlobalScope` + `FScopedTestWorldContextScope` 的手动组合
- Context Stack 回归测试全部通过

### Phase 2

- 实例标志在不同引擎实例间互相独立
- 进程标志保持全局共享语义
- 现有测试行为不变

### Phase 3

- 两个独立 Full 引擎各自拥有独立的 TypeDatabase、BindState、ToStringHelper、CurrentWorldContext
- Clone 引擎与 Source 引擎的 TypeDatabase 共享语义保持不变
- 所有现有测试通过率不低于迁移前水平

### Phase 4

- 单 Full Epoch 限制已移除，多个 Full 引擎可同时创建
- `GAngelscriptEngine` 全局指针已从代码中彻底移除
- `FScopedTestEngineGlobalScope` / `FScopedGlobalEngineOverride` 已移除
- `FAngelscriptTestFixture` 的三种模式均可正常工作
- 首批迁移的测试文件使用新 API 后测试结果不变
- 旧静态 fallback 已清理或明确标注
- 文档已更新

## 风险与注意事项

- **Phase 1 风险低**：Context Stack 在栈为空时完全不改变现有行为，只有 Push 后才会影响 `Get()` 解析路径
- **Phase 2 风险低-中**：静态 bool 标志的读写点有限且模式统一，迁移可机械化完成。需注意区分实例标志和进程标志的边界
- **Phase 3 风险中-高**：TypeDatabase 和 BindState 是绑定层的核心基础设施，30+ 处直接引用。迁移策略是"双路径 + fallback"逐步过渡，不做一次性大爆炸切换。每个单例单独迁移、单独提交、单独回归
- **Phase 4 风险中**：移除 epoch 限制后，原本被拒绝的多 Full 场景会变为可用路径，需要确保 Phase 3 的 engine-local 迁移确实覆盖了所有竞争点
- **与 Hazelight 原版的差异**：Phase 3 完成后，TypeDatabase/BindState/ToStringHelper 的访问模式与原版显著不同。后续从 Hazelight 合入代码时需要额外注意这些路径的适配
- **崩溃恢复**：RAII scope 依赖析构器恢复 ContextStack 状态；如果测试中发生未被 catch 的异常或 Fatal，栈展开可能不完整。建议在 UE 的 `FAutomationTestBase::RunTest` 外围增加安全网（但这是 UE 框架限制，不在本计划范围内）
- **与 `Plan_FullDeGlobalization.md` 的协调**：本计划是 `Plan_FullDeGlobalization` 的具体实施路径。Phase 1 完成后应同步更新该计划的状态
