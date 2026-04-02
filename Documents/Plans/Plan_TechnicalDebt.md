# Angelscript 插件技术债清理计划

## 背景与目标

### 背景

当前 `Plugins/Angelscript` 已完成 P0-P8 主线（模块改名、引擎重构、Subsystem 托管、多实例基础设施、目录重构、场景测试模板），275/275 C++ 测试全部通过。但在快速推进主线功能的过程中，积累了以下几类技术债：

1. **构建稳定性风险**：`TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 在 `AngelscriptType.h` 中的模板特化可能与 UE 引擎侧定义冲突，缺少条件保护。
2. **代码安全隐患**：`Bind_BlueprintEvent.cpp` 缺少运行时参数类型校验、`AngelscriptDebugServer.cpp` 线程挂起路径缺 mutex 保护、`StaticJIT/StaticJITHeader.cpp` 异常路径对象生命周期不明确。
3. **过时 API 残留**：`PrecompiledData.cpp` / `as_string.h` 使用 `FCrc::StrCrc_DEPRECATED` / `FCrc::Strihash_DEPRECATED`，`StaticJITHeader.h` 全局压制弃用警告，`AngelscriptType.cpp` 存在空默认值分支。
4. **内部模块测试缺口**：`AngelscriptMemoryTests.cpp` 缺少内存池分配/释放/泄漏检测用例；`AngelscriptRestoreTests.cpp` 缺少 `SaveByteCode`/`LoadByteCode` 往返与损坏数据处理用例。
5. **Bind API 差距**：`Bind_TOptional.cpp` 缺 Intrusive Optional、`Bind_UStruct.cpp` 缺 `TStructType<T>` 模板、`Bind_BlueprintType.cpp` 缺 `HasGetter`/`HasSetter`、6 个 Minor Drift 文件与 UEAS2 参考源存在功能缺口。
6. **测试 Helper 命名混杂**：`GetSharedTestEngine()` / `GetResetSharedTestEngine()` / `GetProductionEngine()` 等 helper 命名不够显式，存在语义误用风险，但批量改名影响面过大，需分层迁移。
7. **全局状态依赖**：`FAngelscriptEngine::Get()` / `CurrentWorldContext` 仍在编译/绑定/测试路径中充当全局入口，去全局化仅为规划阶段。

### 目标

1. 消除构建阶段的宏/模板重定义风险，确保全量编译稳定。
2. 修补运行时安全隐患，防止生产环境下参数类型不匹配导致崩溃、线程竞争导致死锁。
3. 替换全部已弃用 UE API 调用，消除编译警告。
4. 补齐内部模块（Memory / Restore）测试基线。
5. 缩小 Bind API 与 UEAS2 参考源的差距。
6. 为测试 Helper 命名重构和去全局化提供可执行的分层迁移路线。

## 当前事实状态

```text
AngelScript 版本：2.33.0 WIP
模块：AngelscriptRuntime（3 模块） + AngelscriptEditor + AngelscriptTest
Binds：148 个文件（集中在 AngelscriptRuntime/Binds/）
C++ 测试：275/275 PASS
原生 .as 测试：241 个

已知过时 API 调用：
  PrecompiledData.cpp L2712,2718,2720 → FCrc::StrCrc_DEPRECATED
  ThirdParty/angelscript/source/as_string.h L108 → FCrc::Strihash_DEPRECATED
  StaticJITHeader.h L65 → PRAGMA_DISABLE_DEPRECATION_WARNINGS（全局压制）
  AngelscriptAbilitySystemComponent.h L178,182 → UE_DEPRECATED 标记

已知安全隐患：
  Bind_BlueprintEvent.cpp L144,169,343,353 → ProcessEvent 前无参数类型校验
  AngelscriptDebugServer.cpp L161,185 → 线程挂起/异常处理缺 mutex
  StaticJIT/StaticJITHeader.cpp L255 → 异常路径 new 对象无 delete

已知构建风险：
  AngelscriptType.h L732-740 → TBaseStructure<FBox>/TBaseStructure<FBoxSphereBounds> 特化
  AngelscriptType.cpp L859,867 → 对应实现

已知 Bind 差距（vs UEAS2）：
  Bind_TOptional.cpp → 缺 Intrusive Optional / 热重载比较 / 调试器
  Bind_UStruct.cpp → 缺 TStructType<T> / 静态结构体全局变量 / GC schema
  Bind_BlueprintType.cpp → 缺 HasGetter / HasSetter / CPF_TObjectPtr
  Bind_AActor.cpp → 缺 DeprecateOldActorGenericMethods / GetVerifySpawnActor()
  Bind_USceneComponent.cpp → 缺 FScopedMovementUpdate
  Bind_FMath.cpp → 部分数学函数缺失
  Bind_FVector2f.cpp → 部分方法不完整
  Bind_Delegates.cpp → 缺 DelegateTypeInfo / MulticastTypeInfo
  Bind_FHitResult.cpp → 部分方法/属性缺失
```

## 分阶段执行计划

### Phase 1：构建系统稳定性修复

> 目标：消除模板/宏重定义风险，确保插件在不同 UE 版本编译时不出现 `redefinition` 错误。

- [ ] **P1.1** 分析 `AngelscriptType.h` L732-740 的 `TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 特化
  - 确认 UE 引擎侧是否已有同名特化（对照 `NoExportTypes.h` / `ScriptStruct.h`）
  - 确认冲突条件：哪些 UE 版本已内建这两个特化
- [ ] **P1.2** 为 `TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 添加 `#if !defined(...)` 或版本条件保护
  - 修改 `AngelscriptType.h` 中的 `template<>` 声明
  - 修改 `AngelscriptType.cpp` 中的 `::Get()` 实现
  - 确保 `Bind_UStruct.cpp` L1050,1076 和 `AngelscriptStaticJIT.cpp` L2730-2731 的使用侧不受影响
- [ ] **P1.3** 编译验证：全量构建通过
- [ ] **P1.3** 📦 Git 提交：`[Build] Fix: guard TBaseStructure<FBox/FBoxSphereBounds> specialization against UE redefinition`

### Phase 2：代码安全性修复

> 目标：修补三处运行时安全隐患，防止生产崩溃和线程竞争。

- [ ] **P2.1** `Bind_BlueprintEvent.cpp` L144,169,343,353：在 `ProcessEvent` 前校验参数类型与 `UFunction` 签名匹配
  - 不匹配时 `UE_LOG(Error)` 并安全返回，不进入 `ProcessEvent`
  - 不改变正常路径性能
- [ ] **P2.1** 📦 Git 提交：`[Binds] Fix: add runtime parameter type validation before ProcessEvent in BlueprintEvent`

- [ ] **P2.2** `AngelscriptDebugServer.cpp` L161,185：为线程挂起和异常处理添加 `FCriticalSection` 或 `FRWLock` 保护
  - 确认当前线程模型（主线程 vs 调试线程）
  - 为共享状态添加最小范围锁保护
- [ ] **P2.2** 📦 Git 提交：`[Debug] Fix: add mutex protection for thread suspend and exception handling in DebugServer`

- [ ] **P2.3** `StaticJIT/StaticJITHeader.cpp` L255：审计异常路径的对象生命周期
  - 找出 `new` 出来的对象在异常路径是否有对应 `delete`
  - 补齐 RAII 封装或 `TUniquePtr` 替换裸 `new`
- [ ] **P2.3** 📦 Git 提交：`[StaticJIT] Fix: ensure exception-path object lifecycle safety in StaticJITHeader`

- [ ] **P2.4** 编译验证 + 运行全量测试确认无回归
- [ ] **P2.4** 📦 Git 提交：`[Test] Test: verify safety fixes regression — 275/275 pass`

### Phase 3：过时 API 替换

> 目标：替换全部已弃用 UE API 调用，消除编译警告，移除不再需要的兼容代码。

- [ ] **P3.1** `PrecompiledData.cpp` L2712,2718,2720：将 `FCrc::StrCrc_DEPRECATED` 替换为 `FCrc::StrCrc32` 或 `GetTypeHash`
  - 确认替换后哈希值兼容性（是否影响已有缓存数据）
  - 若影响缓存兼容，增加版本号标记
- [ ] **P3.1** 📦 Git 提交：`[StaticJIT] Refactor: replace FCrc::StrCrc_DEPRECATED with non-deprecated hash API`

- [ ] **P3.2** `ThirdParty/angelscript/source/as_string.h` L108：将 `FCrc::Strihash_DEPRECATED` 替换为现代 API
  - 使用 `//[UE++]` 标记修改
  - 更新 `AngelscriptChange.md`
- [ ] **P3.2** 📦 Git 提交：`[ThirdParty] Fix: replace FCrc::Strihash_DEPRECATED in as_string.h [UE++]`

- [ ] **P3.3** `StaticJITHeader.h` L65：评估 `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 全局压制
  - 若压制是为了掩盖已弃用调用，修复根因后移除
  - 若压制是为了第三方兼容，缩小作用范围到最小必要区域
- [ ] **P3.3** 📦 Git 提交：`[StaticJIT] Refactor: narrow or remove PRAGMA_DISABLE_DEPRECATION_WARNINGS scope`

- [ ] **P3.4** `AngelscriptType.cpp`：补齐空默认值分支的实现
  - 定位空分支（预期在类型默认值处理路径中）
  - 参照 UEAS2 对应逻辑补齐或添加显式 `checkNoEntry()` 标记不可达
- [ ] **P3.4** 📦 Git 提交：`[Runtime] Fix: implement or guard empty default value branch in AngelscriptType`

- [ ] **P3.5** `AngelscriptDebugServer.cpp`：评估 VSCode 旧版本兼容代码
  - 确认当前最低支持的 VSCode DAP 版本
  - 若兼容代码对应的版本已 EOL，移除
- [ ] **P3.5** 📦 Git 提交：`[Debug] Refactor: remove obsolete VSCode compatibility code in DebugServer`

- [ ] **P3.6** 编译验证 + 全量回归
- [ ] **P3.6** 📦 Git 提交：`[Test] Test: verify deprecated API replacement regression`

### Phase 4：内部模块测试补全

> 目标：补齐 Memory 和 Restore 模块的测试用例，建立内部模块行为基线。

#### Memory 测试补全

> 文件：`Internals/AngelscriptMemoryTests.cpp`（已存在，需补充用例）

- [ ] **P4.1** 补充测试：内存池分配/释放/重用
  - 验证 `asCMemoryMgr` 的池分配器在多次 alloc/free 后能正确重用内存块
- [ ] **P4.2** 补充测试：内存泄漏检测
  - 创建引擎 → 编译脚本 → 销毁引擎 → 验证 `FreeUnusedMemory()` 后分配计数归零
- [ ] **P4.3** 编译运行验证
- [ ] **P4.3** 📦 Git 提交：`[Test/Internals] Feat: add Memory pool allocation and leak detection tests`

#### Restore（序列化）测试补全

> 文件：`Internals/AngelscriptRestoreTests.cpp`（已存在，需补充用例）
> 注意：当前 AS 2.33.0 的 `SaveByteCode`/`LoadByteCode` 返回 `asNOT_SUPPORTED`，测试应验证此边界行为。

- [ ] **P4.4** 补充测试：SaveByteCode → LoadByteCode 往返
  - 编译模块 → 调用 `SaveByteCode` → 验证返回值（当前预期 `asNOT_SUPPORTED`）
- [ ] **P4.5** 补充测试：损坏数据处理
  - 向 `LoadByteCode` 传入空/损坏的字节流，验证不崩溃
- [ ] **P4.6** 编译运行验证
- [ ] **P4.6** 📦 Git 提交：`[Test/Internals] Feat: add Restore SaveByteCode/LoadByteCode boundary and corruption handling tests`

### Phase 5：Bind API 差距补齐

> 目标：缩小与 UEAS2 参考源的绑定功能差距。
> 技术参考：UEAS2 对应 Bind 文件（通过 `AgentConfig.ini` 中 `References.HazelightAngelscriptEngineRoot` 获取）

#### Sprint A：容器与结构体补齐

- [ ] **P5.A.1** `Bind_TOptional.cpp`：补齐 Intrusive Optional 全套
  - `HasIntrusiveUnsetOptionalState` / `InitializeIntrusiveUnsetOptionalValue`
  - 热重载比较：`CanCompareForHotReload` / `IsValueEqualForHotReload`
  - 调试器：`GetDebuggerValue` / `GetDebuggerScope` / `GetDebuggerMember`
- [ ] **P5.A.2** `Bind_UStruct.cpp`：补齐 `TStructType<T>` 模板 + 静态结构体全局变量 + GC schema `PropertyLink` 发射
- [ ] **P5.A.3** 编译验证 + 全量回归
- [ ] **P5.A.3** 📦 Git 提交：`[Binds] Feat: TOptional intrusive + UStruct TStructType + hot-reload completion`

#### Sprint B：类型系统补齐

- [ ] **P5.B.1** `Bind_BlueprintType.cpp`：补齐 `HasGetter` / `HasSetter` / getter-setter 方法 / `CPF_TObjectPtr` 处理
- [ ] **P5.B.2** 编译验证
- [ ] **P5.B.2** 📦 Git 提交：`[Binds] Feat: BlueprintType HasGetter/HasSetter + CPF_TObjectPtr`

#### Sprint C：Minor Drift 收尾（6 个文件）

- [ ] **P5.C.1** `Bind_AActor.cpp`：补齐 `DeprecateOldActorGenericMethods` / `GetVerifySpawnActor()`
- [ ] **P5.C.2** `Bind_USceneComponent.cpp`：补齐 `FScopedMovementUpdate` 值类型注册
  - 构造/析构 + `RevertMove()` / `IsTransformDirty()` 等方法
- [ ] **P5.C.3** `Bind_FMath.cpp`：与 UEAS2 diff 补齐缺失数学函数
- [ ] **P5.C.4** `Bind_FVector2f.cpp`：与 UEAS2 diff 补齐不完整方法
- [ ] **P5.C.5** `Bind_Delegates.cpp`：补齐 `DelegateTypeInfo` / `MulticastTypeInfo` 暴露
- [ ] **P5.C.6** `Bind_FHitResult.cpp`：与 UEAS2 diff 补齐缺失方法和属性
- [ ] **P5.C.7** 全部编译验证 + 全量回归
- [ ] **P5.C.7** 📦 Git 提交：`[Binds] Feat: minor drift closure — AActor, SceneComponent, Math, Vector2f, Delegates, HitResult`

### Phase 6：测试 Helper 命名重构

> 目标：将测试 helper 从当前混杂命名逐步迁移到语义显式的命名，减少误用风险。
> 约束：不做一次性全替换，必须分层逐批迁移，每批迁移后立即全量构建验证。

#### 影响面分析

```text
L0 公共入口层：AngelscriptTestUtilities.h、AngelscriptTestEngineHelper.h/.cpp
L1 直接依赖层：Shared/*Tests.cpp、Core/*Tests.cpp
L2 场景层：Scenarios/*.cpp、HotReload/*.cpp
L3 模板层：Template/*.cpp
L4 绑定/内部层：Bindings/*.cpp、Internals/*.cpp、Preprocessor/*.cpp、Compiler/*.cpp
```

#### 高风险文件类型

- 混用 `GetSharedInitializedTestEngine()` / `GetSharedTestEngine()` / `GetResetSharedTestEngine()` 的测试
- 同时调用 `CompileModuleFromMemory()`、`CompileModuleWithResult()`、`AnalyzeReloadFromMemory()` 的测试
- 依赖 `FScopedTestEngineGlobalScope` / `FScopedTestWorldContextScope` 的运行期测试

#### 迁移步骤

- [ ] **P6.1** L0 层：在 `AngelscriptTestEngineHelper.h` 中添加语义显式的新 helper 声明
  - "会切换全局引擎"的 helper 名称带 `WithGlobalScope`
  - "会附着生产主引擎"的 helper 名称带 `Production`
  - 旧名字保留为兼容别名（`inline` 转发），不删除
- [ ] **P6.1** 📦 Git 提交：`[Test] Refactor: add explicit-named helper declarations with compat aliases`

- [ ] **P6.2** L1 层：迁移 `Shared/*Tests.cpp` 与 `Core/*Tests.cpp` 到新 helper 名称
- [ ] **P6.2** 📦 Git 提交：`[Test] Refactor: migrate Shared and Core tests to explicit helper names`

- [ ] **P6.3** L2 层：迁移 `HotReload/*.cpp` 与 `Scenarios/*.cpp`
- [ ] **P6.3** 📦 Git 提交：`[Test] Refactor: migrate HotReload and Scenarios tests to explicit helper names`

- [ ] **P6.4** L3 层：迁移 `Template/*.cpp`
- [ ] **P6.4** 📦 Git 提交：`[Test] Refactor: migrate Template tests to explicit helper names`

- [ ] **P6.5** L4 层：迁移 `Bindings/*.cpp`、`Internals/*.cpp`、`Preprocessor/*.cpp`、`Compiler/*.cpp`
- [ ] **P6.5** 📦 Git 提交：`[Test] Refactor: migrate Bindings and Internals tests to explicit helper names`

- [ ] **P6.6** 清理：移除旧兼容别名，确认无残留引用
- [ ] **P6.6** 📦 Git 提交：`[Test] Chore: remove deprecated helper aliases after full migration`

- [ ] **P6.7** 全量回归
- [ ] **P6.7** 📦 Git 提交：`[Test] Test: verify helper naming refactor regression — all tests pass`

### Phase 7：去全局化路线（规划项）

> 目标：为 `FAngelscriptEngine::Get()` / `CurrentWorldContext` 去全局化提供逐步迁移路线。
> 当前阶段仅为规划，不进入代码改造。后续按需启动。

#### 迁移路线

- [ ] **P7.1** Phase A — 测试 helper 全部封装（已由 Phase 6 覆盖）
  - 测试代码不再散落直接切全局状态
  - 统一收进 `FScopedTestEngineGlobalScope` / `FScopedTestWorldContextScope`
- [ ] **P7.2** Phase B — 编译路径优先改为显式 `Engine&`
  - `CompileModules()` / `AngelscriptClassGenerator` 改为接受 `FAngelscriptEngine&` 参数
  - 减少对 `FAngelscriptEngine::Get()` 的依赖
- [ ] **P7.3** Phase C — 高频运行时路径减少 `FAngelscriptEngine::Get()` 依赖
  - `ASClass` / `Binds` / `DebugServer` 中的全局引擎引用改为上下文传参
- [ ] **P7.4** Phase D — 评估"当前执行引擎"绑定方案
  - 选项 A：绑定到脚本 context（`asIScriptContext` UserData）
  - 选项 B：绑定到 TLS（Thread-Local Storage）
  - 选项 C：保留全局但加 assert 保护

> **注意**：Phase B-D 每一步都是跨模块重构，不适合与其他主线并行。启动前必须先完成 Phase 6 的 helper 迁移。

### Phase 8：性能验证

> 目标：验证已完成的性能优化改动确实有效。

- [ ] **P8.1** 验证 `Bind_UEnum.cpp` 枚举值哈希查找改动
  - 运行包含大量枚举值查找的测试
  - 对比改动前后耗时（使用 `FPlatformTime::Seconds()` 计时）
  - 记录结果到文档
- [ ] **P8.1** 📦 Git 提交：`[Binds] Test: verify hash-based enum value lookup performance`

## 验收标准

1. **构建稳定性**：`TBaseStructure` 特化有条件保护，全量编译无 `redefinition` 警告/错误。
2. **代码安全**：`Bind_BlueprintEvent` 参数校验生效（构造不匹配参数时 log error 而非崩溃）、`DebugServer` 线程操作有锁保护、`StaticJITHeader` 异常路径无内存泄漏。
3. **零弃用调用**：插件源码中不再出现 `_DEPRECATED` 后缀的 UE API 调用（ThirdParty 修改使用 `//[UE++]` 标记）。
4. **内部测试基线**：Memory 池测试 ≥2 个用例通过、Restore 边界测试 ≥2 个用例通过。
5. **Bind 差距**：TOptional / UStruct / BlueprintType / 6 个 Minor Drift 文件与 UEAS2 对齐（功能层面，不要求行级一致）。
6. **Helper 命名**：所有测试文件使用语义显式的 helper 名称，旧别名已移除，全量回归通过。
7. **全量回归**：每个 Phase 完成后 275/275（+ 新增用例）测试通过。

## 风险与注意事项

### 风险 1：TBaseStructure 条件保护可能导致运行时行为差异

若 UE 引擎已内建 `TBaseStructure<FBox>` 特化且返回不同 `UScriptStruct*`，条件保护后插件使用的 `Get()` 语义可能变化。

**缓解**：P1.1 中先对比引擎侧实现与插件侧实现的返回值是否等价；若等价，直接跳过插件侧定义；若不等价，需要在绑定层做适配。

### 风险 2：FCrc 替换影响缓存兼容

`PrecompiledData.cpp` 中的哈希用于 StaticJIT 预编译数据。替换哈希算法后旧缓存失效。

**缓解**：P3.1 中增加缓存版本号，新版本自动重新生成而非读取旧缓存。

### 风险 3：Helper 命名重构的级联破坏

Phase 6 涉及 93 个测试 `.cpp` 文件，批量改名可能引发 include 缺失、语法损坏、`RunTest()` 缺实现等连锁问题。

**缓解**：严格按 L0→L4 分层迁移，每层迁移后立即全量构建。出现批量错误时先回到"补 include / 修括号"层面逐个收敛，不继续扩散。

### 风险 4：Bind API 差距补齐引入新的编译依赖

部分 UEAS2 Bind 实现依赖可选引擎插件（如 `EnhancedInput`、`GAS`）。补齐时需确认依赖在 `Build.cs` 中正确声明。

**缓解**：每个 Sprint 提交前验证 `Build.cs` 依赖完整性，对可选插件使用 `ConditionalAddModuleToCompileEnvironment`。

### 风险 5：去全局化路线（Phase 7）影响面极大

一旦从规划转入代码改造，会同时触及编译管线、类生成器、绑定层、测试上下文，容易迅速放大为跨模块重构。

**缓解**：Phase 7 当前仅为规划项。启动前必须先完成 Phase 6，且作为独立阶段推进，不与其他主线并行。

## 优先级与依赖关系

```text
Phase 1（构建稳定性）── 无前置依赖，最高优先级
  ↓
Phase 2（代码安全）── 依赖 Phase 1（确保编译通过）
  ↓
Phase 3（过时 API）── 与 Phase 2 可部分并行
  ↓
Phase 4（内部测试）── 依赖 Phase 1-3（确保基线稳定）
  ↓
Phase 5（Bind 差距）── 独立于 Phase 4，可部分并行
  ↓
Phase 6（Helper 命名）── 依赖 Phase 1-3（确保基线稳定）
  ↓
Phase 7（去全局化）── 依赖 Phase 6 完成
  ↓
Phase 8（性能验证）── 独立，任何阶段后均可执行
```

**推荐执行顺序**：P1 → P2 → P3 → P4 + P5（并行）→ P6 → P8 → P7（按需）

## 参考文档索引

| 文档 | 位置 | 用途 |
|------|------|------|
| 上游 todo.md | `J:\UnrealEngine\todo.md` §7 | 技术债原始条目来源 |
| 接口绑定计划 | `Documents/Plans/Plan_InterfaceBinding.md` | 接口重构独立计划（不在本 Plan 范围内） |
| 原生测试重构计划 | `Documents/Plans/Plan_NativeAngelScriptCoreTestRefactor.md` | Native 测试层独立计划（与 Phase 4 互补） |
| 构建指南 | `Documents/Guides/Build.md` | 编译命令与环境配置 |
| 测试指南 | `Documents/Guides/Test.md` | 测试运行与分类说明 |
| Git 提交规范 | `Documents/Rules/GitCommitRule.md` | 提交格式 |
| UEAS2 参考源 | 通过 `AgentConfig.ini` → `References.HazelightAngelscriptEngineRoot` | Bind 差距对比基准 |
