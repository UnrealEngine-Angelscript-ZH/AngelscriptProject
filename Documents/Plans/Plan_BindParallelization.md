# Bind 步骤并行化可行性分析与方案

## 背景与目标

### 背景

AngelScript 引擎初始化时，`BindScriptTypes()` 将所有 C++ 类型、方法、全局函数注册到 `asIScriptEngine`。这是引擎启动链路中的关键阶段：

```
Initialize() → Initialize_AnyThread()
  → LoadBindModules()          // 加载生成的绑定分片 DLL
  → BindScriptTypes()          // ★ 本计划关注的阶段
    → CallBinds()              // 排序后逐个执行绑定函数
  → InitialCompile()           // 脚本编译
```

当前仓库中手写绑定（`Binds/` 目录）约 **120 个 `FBind` 注册点**，分布在约 120 个 `Bind_*.cpp` 文件中。加上运行时生成的绑定分片模块（`ASRuntimeBind_0` ~ `ASRuntimeBind_110`、`ASEditorBind_0` ~ `ASEditorBind_30`），总绑定数量可达 **数百个**。

`CallBinds()` 的执行模型非常简单：

```cpp
// AngelscriptBinds.cpp 第 121-145 行
void FAngelscriptBinds::CallBinds(const TSet<FName>& DisabledBindNames)
{
    for (const FBindFunction& Bind : GetSortedBindArray())
    {
        if (DisabledBindNames.Contains(Bind.BindName))
            continue;
        Bind.Function();  // 顺序执行每个绑定
    }
}
```

**纯单线程顺序执行**，没有任何并行化。UEAS2 参考实现也完全相同。

### 当前 Bind 系统架构详情

#### 排序与优先级

唯一的排序键是 `int32 BindOrder`，有三个预定义档位：

| 档位 | 值 | 典型用途 |
| --- | --- | --- |
| `EOrder::Early` | -100 | 类型声明（`RegisterObjectType`），如 `Bind_FString`、`Bind_FVector`、`Bind_Delegates`、`Bind_Primitives` |
| `EOrder::Normal` | 0 | 大部分独立类型的方法绑定 |
| `EOrder::Late` | 100 | 依赖其他类型已存在的绑定，如 `Bind_BlueprintType`（`Late+100`）、`Bind_AActor`（`Late+150`） |

实际使用中存在大量微调（`Early+1`、`Late-1`、`Late+105`、`Late+150` 等），本质是一个**线性优先级序列**，没有显式依赖图。

#### 绑定执行中改变的全局状态

每个 `Bind.Function()` lambda 内部通过 `FAngelscriptBinds` 的 API 操作以下全局状态：

| 状态 | 写入 API | 线程安全性 |
| --- | --- | --- |
| `asCScriptEngine` 内部数组（`registeredObjTypes`、`registeredGlobalFuncs`、`registeredGlobalProps`、`allRegisteredTypes`、`typeLookup`） | `RegisterObjectType`、`RegisterObjectMethod`、`RegisterGlobalFunction`、`RegisterEnum`、`RegisterEnumValue` 等 | ❌ **非线程安全**：Register 系列方法不加锁，直接 `PushLast` 到内部数组；`isPrepared = false` 是非原子赋值 |
| `asCScriptEngine::defaultNamespace` | `SetDefaultNamespace` / 通过 `FNamespace` RAII 临时切换 | ❌ **引擎级全局状态**：`FNamespace` 构造时设新值、析构时恢复旧值，并发会导致 namespace 错乱 |
| `FAngelscriptBinds::PreviouslyBoundFunction` | `OnBind()` 写入，后续 `SetPreviousBindIsPropertyAccessor` 等读取 | ❌ **静态变量**：绑定 A 注册一个方法后立即设置 trait，如果另一个线程同时绑定 B 则 `PreviouslyBoundFunction` 会被覆盖 |
| `FAngelscriptBinds::ClassFuncMaps`、`SkipBinds`、`SkipBindNames`、`SkipBindClasses` | 各绑定函数内部写入 | ❌ **静态 TMap/TSet**：无锁 |
| `FAngelscriptType` 数据库 | `FAngelscriptType::Register`、`RegisterTypeFinder` | ❌ **静态数据结构**：无锁 |

#### AngelScript 引擎的 `engineRWLock` 使用情况

`as_scriptengine.cpp` 中的 `ACQUIREEXCLUSIVE(engineRWLock)` / `ACQUIRESHARED(engineRWLock)` 共出现约 25 次，但**全部用于**：
- `SetUserData` / `GetUserData`（用户数据存取）
- `GetModuleCount` / `GetModuleByIndex`（模块枚举）
- `Set*CleanupCallback`（清理回调注册）

**所有 `Register*` 方法（`RegisterObjectType`、`RegisterObjectMethod`、`RegisterGlobalFunction` 等）内部不使用任何锁**。它们直接操作 `registeredObjTypes.PushLast`、创建 `asCScriptFunction`、设置 `isPrepared = false` 等。

### 隐式依赖分析

虽然没有显式依赖图，但 bind 之间存在以下**隐式依赖**：

1. **类型必须先于方法注册**：`RegisterObjectMethod("FVector", ...)` 要求 `FVector` 已经通过 `RegisterObjectType` 注册。这是 AS 引擎的硬约束——内部会 `ParseDataType` 查找类型
2. **Early → Late 的二阶段模式**：许多类型采用 Early 注册类型声明 + Late 注册方法/转换的模式（如 `Bind_FVector` 的 `Early` 声明 + `Late` 转换）
3. **`FNamespace` RAII**：构造时 `SetDefaultNamespace(name)`，析构时恢复旧值。如果并发执行，namespace 状态会互相覆盖
4. **`PreviouslyBoundFunction` 链式调用**：`BindMethod` → `OnBind` → 设置 `PreviouslyBoundFunction` → 紧接着 `SetPreviousBindIsPropertyAccessor` 读取。这是一个**紧耦合的串行模式**

### UEAS2 对比

| 项 | UEAS2 | 当前仓库 |
| --- | --- | --- |
| `CallBinds` 实现 | 单线程顺序 for 循环 | 同 |
| 排序 | `BindOrder` 整数排序 | 同 |
| 并行化 | **无**（编译阶段有 `ParallelFor` 但绑定无） | 无 |
| 线程化初始化 | `Initialize_AnyThread` 可放到 `AsyncTask` | 同 |

---

## 可行性评估

### 结论：直接并行化 `CallBinds` **不可行**

原因如下：

1. **AngelScript 引擎 API 非线程安全**：`RegisterObjectType`、`RegisterObjectMethod` 等核心 Register 方法**不加锁**，内部直接操作 `registeredObjTypes`、`allRegisteredTypes`、`typeLookup` 等数组和 map。并发调用会导致数据竞争和崩溃

2. **`defaultNamespace` 是引擎级单例状态**：`FNamespace` RAII 通过 `SetDefaultNamespace` / `GetDefaultNamespace` 临时切换 namespace，并发修改会导致类型注册到错误的 namespace

3. **`PreviouslyBoundFunction` 是绑定间紧耦合的静态变量**：绑定代码普遍依赖"注册方法 → 立即设置 trait"的串行语义，并行化会破坏这个约定

4. **隐式顺序依赖广泛存在**：Early/Late 二阶段模式、跨绑定的类型引用，都依赖特定的执行顺序

### 可行的替代优化方向

虽然直接并行化绑定执行不可行，但仍有几个可以加速绑定阶段的方向：

#### 方向 A：预计算与缓存（低风险、中收益）

将绑定阶段的部分工作预先完成或缓存，减少运行时开销。

- **Bind DB 缓存已存在**：`AS_USE_BIND_DB` 路径会从 `Binds.Cache` 加载预计算的绑定数据库（非 Editor 场景），这已经是一种缓存优化
- **预排序绑定数组**：当前 `GetSortedBindArray()` 每次调用都对静态数组做一次 `Sort()`。可以在首次排序后缓存结果，避免重复排序

#### 方向 B：绑定注册的批量化（中风险、中收益）

AngelScript 的 `Register*` 方法每次调用都做字符串解析（`asCBuilder::ParseDataType`、签名解析等）。可以：

- 收集同一类型的所有方法注册，合并为批量调用减少重复的类型查找
- 预解析绑定签名字符串，缓存类型查找结果

#### 方向 C：分阶段绑定 + 阶段内并行准备（中风险、高收益）

将绑定分为严格的三个阶段：

1. **Phase 1（类型声明）**：所有 `RegisterObjectType` / `RegisterEnum` / `RegisterInterface`。这些操作相对独立（不依赖其他已注册类型），但仍需串行执行（AS 引擎 API 限制）
2. **Phase 2（并行准备）**：在工作线程上并行准备每个绑定的参数数据（签名字符串构造、函数指针收集、类型查找缓存），不调用 AS 引擎 API
3. **Phase 3（方法注册）**：将准备好的注册数据顺序提交给 AS 引擎

这种方式的核心思想是把"准备数据"和"提交给引擎"分离，前者可以并行。

#### 方向 D：解决 `PreviouslyBoundFunction` 瓶颈（低风险、低收益）

当前 `PreviouslyBoundFunction` 是一个静态变量，绑定代码依赖"注册 → 立即设置 trait"的模式。改为让 `BindMethod` / `BindGlobalFunction` 等直接返回 function ID 并由调用方持有，消除静态变量依赖。这不直接实现并行化，但移除了一个并行化的前置障碍。

---

## 推荐方案与执行计划

综合可行性和收益，推荐先实施**方向 A（预排序缓存）** + **方向 D（消除 PreviouslyBoundFunction 瓶颈）**，再视绑定耗时数据决定是否深入方向 C。

---

## Phase 1：测量当前绑定耗时基线

> 目标：在做任何优化之前，先获取精确的绑定耗时分解数据，为后续决策提供量化依据。

- [ ] **P1.1** 在 `CallBinds` 中增加 per-bind 计时，按 BindOrder 分档统计耗时
  - 当前只有 `FAngelscriptBindExecutionObservation` 记录整体 `CallBinds` 耗时（`BeginObservationPass` / `EndObservationPass`），没有 per-bind 粒度
  - 在 `CallBinds` 循环内，用 `FPlatformTime::Seconds()` 包裹每个 `Bind.Function()` 调用，收集 `{BindName, BindOrder, DurationMs}` 三元组
  - 输出到日志或存入 `FAngelscriptBindExecutionObservation` 的新数据结构中，按 BindOrder 分档（Early / Normal / Late / Late+100~150）汇总
  - 条件编译在 `AS_PRINT_STATS` 或 `WITH_DEV_AUTOMATION_TESTS` 下
- [ ] **P1.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: add per-bind timing instrumentation to CallBinds`

- [ ] **P1.2** 运行一次完整的编辑器启动 + 非编辑器启动，收集绑定耗时数据
  - 记录总绑定耗时、各档位（Early/Normal/Late）的累计耗时、top-10 最慢的单个绑定
  - 数据记录在本计划文档的 Phase 1 完成注释中
  - 如果总绑定耗时 < 500ms，并行化优化的优先级应下调；如果 > 2s，方向 C 的投入回报才显著
- [ ] **P1.2** 📦 Git 提交：`[AngelscriptRuntime] Doc: bind timing baseline measurement`

---

## Phase 2：消除 `PreviouslyBoundFunction` 全局耦合

> 目标：移除绑定代码对静态变量 `PreviouslyBoundFunction` 的依赖，为后续可能的分离"准备"与"提交"阶段扫清障碍。

- [ ] **P2.1** 审计所有使用 `PreviouslyBoundFunction` / `GetPreviousBind` 的调用点
  - `PreviouslyBoundFunction` 在 `OnBind()` 中被设置（`AngelscriptBinds.cpp` 第 360 行），然后被 `MarkAsImplicitConstructor`、`DeprecatePreviousBind`、`SetPreviousBindIsPropertyAccessor`、`SetPreviousBindIsEditorOnly`、`SetPreviousBindIsCallable`、`SetPreviousBindNoDiscard`、`CompileOutPreviousBind` 等方法读取
  - 逐一检查 `Binds/` 目录下所有使用这些方法的位置，确认每个调用点是否都紧跟在 `BindMethod` / `BindExternMethod` / `BindGlobalFunction` 之后
  - 评估能否改为让 `BindMethod` 等返回一个包装对象（如 `FBoundFunction`），调用方在包装对象上链式设置 trait
- [ ] **P2.1** 📦 Git 提交：`[AngelscriptRuntime] Doc: audit PreviouslyBoundFunction usage across all binds`

- [ ] **P2.2** 引入 `FBoundFunction` 返回值模式，逐步替代 `PreviouslyBoundFunction`
  - 设计 `FBoundFunction` 结构体，持有 `int32 FunctionId`，提供 `MarkAsImplicitConstructor()`、`Deprecate()`、`SetPropertyAccessor()` 等方法
  - 让 `BindMethod`、`BindExternMethod`、`BindGlobalFunction` 等返回 `FBoundFunction`（已有部分方法返回 `int FunctionId`，可以直接包装）
  - 在不破坏现有 API 的前提下，对新写的绑定代码推荐使用新模式；现有代码可逐步迁移
  - `PreviouslyBoundFunction` 短期保留为向后兼容路径，长期标记 deprecated
- [ ] **P2.2** 📦 Git 提交：`[AngelscriptRuntime] Refactor: introduce FBoundFunction to replace PreviouslyBoundFunction pattern`

---

## Phase 3：预排序缓存与注册去重

> 目标：减少 `CallBinds` 路径上的冗余计算开销。

- [ ] **P3.1** 缓存排序后的绑定数组，避免每次 `CallBinds` 重新排序
  - 当前 `GetSortedBindArray()` 每次调用都拷贝 + 排序整个静态数组。由于绑定注册只在静态构造期发生，排序结果在运行时不会变化
  - 增加一个 `static bool bSorted` 标志（或懒初始化模式），首次调用时排序并缓存，后续调用直接返回已排序数组的引用
  - `ResetBindState()` 中清除缓存标志
- [ ] **P3.1** 📦 Git 提交：`[AngelscriptRuntime] Opt: cache sorted bind array to avoid redundant sorting`

- [ ] **P3.2** 检查 `RegisterObjectType` 的 `asALREADY_REGISTERED` 回退路径的频率
  - 一些绑定文件中，同一个类型可能被多个绑定点尝试注册（返回 `asALREADY_REGISTERED` 后 fallback 到 `GetTypeInfoByName`）。这不影响正确性，但每次都做字符串解析和查找
  - 通过 P1.1 的计时数据识别是否有高频重复注册的热点
  - 如果有，考虑在 `FAngelscriptBinds` 层增加一个 `TMap<FBindString, asITypeInfo*>` 缓存，减少对 AS 引擎的重复调用
- [ ] **P3.2** 📦 Git 提交：`[AngelscriptRuntime] Opt: reduce redundant RegisterObjectType calls via type cache`

---

## Phase 4：分阶段绑定架构评估（视 Phase 1 数据决定）

> 目标：如果 Phase 1 的测量数据显示绑定耗时显著（> 2s），评估将绑定拆分为"类型声明阶段"和"方法注册阶段"，并在方法注册前并行准备注册数据。

- [ ] **P4.1** 设计分阶段绑定原型：类型声明 vs 方法注册分离
  - 当前许多绑定文件已经自然分为 `Early`（类型声明）和 `Late`（方法注册）两个 `FBind`
  - 评估是否可以强制所有绑定按此模式拆分，使 Early 阶段完成后所有类型已知，Late 阶段的方法注册可以并行准备参数数据
  - "并行准备"指在工作线程上构造签名字符串、收集函数指针等不涉及 AS 引擎状态修改的工作；最终 `RegisterObjectMethod` 仍在单线程上串行调用
  - 这需要解决 `FNamespace` RAII 的线程安全问题——准备阶段需要预计算 namespace 而非运行时切换
- [ ] **P4.1** 📦 Git 提交：`[AngelscriptRuntime] Doc: staged binding architecture prototype evaluation`

- [ ] **P4.2** 如果 P4.1 原型可行，实现最小化的分阶段 `CallBinds`
  - 将 `GetSortedBindArray()` 的结果按 `BindOrder` 阈值分为两批：`<= 0`（Early+Normal）和 `> 0`（Late）
  - 第一批串行执行（类型声明）
  - 第二批：先并行调用一个"准备"回调收集注册参数，再串行提交给 AS 引擎
  - 这需要对 `FBind` 的注册 API 做拆分：`FBind` 可以注册一个 `PrepareFunction` 和一个 `CommitFunction`，而非单一的 `Function`
  - 验证绑定结果与纯串行路径完全一致（通过比对 `asIScriptEngine` 的类型/函数数量和 ID 分配）
- [ ] **P4.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: implement staged CallBinds with parallel preparation`

---

## 验收标准

- [ ] Phase 1 完成后，有精确的 per-bind 耗时数据，记录在本计划文档中
- [ ] Phase 2 完成后，新绑定代码不再依赖 `PreviouslyBoundFunction` 静态变量
- [ ] Phase 3 完成后，`CallBinds` 不再每次重新排序绑定数组
- [ ] 所有改动不影响现有绑定的注册结果（类型数量、函数 ID 等与改动前一致）
- [ ] 所有现有测试继续通过
- [ ] `AngelscriptProjectEditor Win64 Development` 构建通过

## 风险与注意事项

- **AngelScript 引擎 API 不可并发**：`RegisterObjectType`、`RegisterObjectMethod` 等**不加锁**，并发调用会导致数据竞争。任何并行化方案都必须保证最终的 AS 引擎 API 调用仍是串行的
- **`defaultNamespace` 全局状态**：`FNamespace` RAII 修改引擎级 namespace。并行准备阶段如果需要 namespace 信息，必须预计算而非运行时切换
- **`PreviouslyBoundFunction` 紧耦合**：绑定代码普遍依赖"注册 → 立即设置 trait"的串行语义。Phase 2 的 `FBoundFunction` 模式在迁移过程中需要与旧模式共存
- **绑定分片模块的加载顺序**：`ASRuntimeBind_*` 模块在 `LoadModule` 时注册绑定到静态数组，排序在 `CallBinds` 中完成。如果分阶段绑定需要不同的注册模式（Prepare + Commit），生成的绑定代码也需要同步修改
- **收益依赖实际耗时数据**：如果 Phase 1 测量显示总绑定耗时 < 500ms，Phase 4 的分阶段并行化投入产出比较低，应将精力转向其他优化（如脚本编译并行化——UEAS2 已有 `BuildParallelParseScripts` 先例）
- **生成绑定代码的兼容性**：`AngelscriptEditorModule.cpp` 中 `GenerateNativeBinds` 生成的分片模块代码使用 `FAngelscriptBinds::RegisterBinds(EOrder::Late, ...)` 模式。如果 Phase 4 引入新的 Prepare + Commit 拆分，需要同步更新代码生成器
