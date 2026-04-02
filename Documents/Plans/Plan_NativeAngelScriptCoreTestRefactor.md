# 原生 AngelScript 单元测试重构计划

## 背景与目标

### 背景

当前 `Plugins/Angelscript/Source/AngelscriptTest/` 下的大量测试虽然验证的是 AngelScript 语言内核能力（编译、执行、参数传递、返回值），但执行路径全部穿过 `FAngelscriptEngine` 封装层：

- `GetSharedTestEngine()` → `FAngelscriptEngine::CreateForTesting()` / `CreateTestingFullEngine()`
- `BuildModule()` → `FAngelscriptPreprocessor` + `FAngelscriptEngine::CompileModules()`
- `ExecuteIntFunction()` → `FScopedTestEngineGlobalScope` + `Engine.CreateContext()`
- `CompileModuleFromMemory()` → `FAngelscriptModuleDesc` + `Engine.CompileModules()`

这导致：

1. **分层不清** — 测试失败时无法判断问题来自 AS 原生层还是 `FAngelscriptEngine` 封装层。
2. **依赖过重** — 本可纯内存完成的语言验证被绑定到 UE 编译管线、预处理器和模块管理。
3. **基线缺失** — 没有一组独立的原生测试可以在 UE 集成层出问题时快速确认 AS 核心是否正常。

### 目标

1. 建立纯原生 AngelScript 测试层（`Native/`），只依赖 `asIScriptEngine` / `asIScriptModule` / `asIScriptContext` 原生 API，不依赖 `FAngelscriptEngine`。
2. 保留现有 Runtime / Scenario 测试，明确归类为 `FAngelscriptEngine` 封装测试和 UE 场景测试。
3. 建立最小可信基线：基础编译、执行、参数传递、返回值、注册、错误回调。

## 范围与边界

- 本次只新增 `Native/` 测试层和迁移明确属于语言内核的测试用例。
- 不修改 `Shared/AngelscriptTestUtilities.h` 的现有 helper 逻辑。
- 不迁移类生成、热重载、Blueprint / Actor / UClass 相关测试。
- 不大规模搬迁现有 `Angelscript/`、`Bindings/`、`Internals/` 目录文件。

## 当前事实状态

```text
AngelscriptTest/
  AngelscriptTest.Build.cs          ← 模块定义，依赖 AngelscriptRuntime
  Angelscript/                       ← 语言层测试（实际依赖 FAngelscriptEngine）
    AngelscriptTestSupport.h         ← 仅 include AngelscriptTestUtilities.h
    AngelscriptExecutionTests.cpp    ← 9 个测试，全部通过 GetSharedTestEngine() + BuildModule()
    AngelscriptCoreExecutionTests.cpp← 10+ 个测试，同上 + CompileModuleFromMemory()
    AngelscriptControlFlowTests.cpp
    AngelscriptFunctionTests.cpp
    AngelscriptHandleTests.cpp
    AngelscriptInheritanceTests.cpp
    AngelscriptMiscTests.cpp
    AngelscriptNativeScriptHotReloadTests.cpp
    AngelscriptObjectModelTests.cpp
    AngelscriptOperatorTests.cpp
    AngelscriptTypeTests.cpp
  Shared/
    AngelscriptTestUtilities.h       ← 核心 helper：GetSharedTestEngine(), BuildModule(), ExecuteIntFunction()
    AngelscriptTestEngineHelper.h    ← 高级 helper：CompileModuleFromMemory(), FindGeneratedClass()
    AngelscriptTestEngineHelper.cpp
    AngelscriptTestEngineHelperTests.cpp
    AngelscriptNativeScriptTestObject.h
  Core/
    AngelscriptTestModule.h          ← IModuleInterface
    AngelscriptEngineCoreTests.cpp   ← Engine lifecycle / compile / execute 测试
  Internals/                         ← Parser / Bytecode / Compiler / GC 等
  Bindings/                          ← UE 类型绑定测试
  Compiler/                          ← 编译管线测试
  Preprocessor/                      ← 预处理器测试
  HotReload/                         ← 热重载测试
  Scenarios/                         ← UE 场景测试（已独立目录）
  Editor/                            ← 编辑器测试
  FileSystem/                        ← 文件系统测试
  Examples/                          ← 示例测试支持
```

关键依赖链：

- `AngelscriptTestUtilities.h` → `#include "AngelscriptEngine.h"` / `AngelscriptGameInstanceSubsystem.h` / `AngelscriptPreprocessor.h`
- `BuildModule()` 写磁盘文件 → 预处理 → `Engine.CompileModules()` → 返回 `asIScriptModule*`
- `ExecuteIntFunction()` → `FScopedTestEngineGlobalScope` → `Engine.CreateContext()`
- 所有 `Angelscript/*.cpp` 测试通过 `AngelscriptTestSupport.h` → `AngelscriptTestUtilities.h` 间接依赖 `FAngelscriptEngine`

## 分阶段执行计划

### Phase 1：建立 Native 测试基础设施

> 目标：新增 `Native/` 目录，提供纯原生 AngelScript helper，代码中不出现 `FAngelscriptEngine`。

- [ ] **P1.1** 创建 `Native/AngelscriptNativeTestSupport.h`
  - 提供以下 helper 函数（全部 `inline`，位于 `AngelscriptNativeTestSupport` 命名空间）：
    - `asIScriptEngine* CreateNativeEngine()` — 调用 `asCreateScriptEngine()` + 注册默认 message callback
    - `void DestroyNativeEngine(asIScriptEngine*)` — `ShutDownAndRelease()`
    - `asIScriptModule* BuildNativeModule(asIScriptEngine*, const char* ModuleName, const char* Source)` — `GetModule(GM_ALWAYS_CREATE)` + `AddScriptSection()` + `Build()`
    - `asIScriptFunction* GetNativeFunctionByDecl(asIScriptModule*, const char* Declaration)` — `GetFunctionByDecl()`
    - `int PrepareAndExecute(asIScriptContext*, asIScriptFunction*)` — `Prepare()` + `Execute()`
    - `FString CollectMessages(...)` — message callback 收集器
  - 约束：只 `#include` AngelScript 原生头文件（`angelscript.h`）和 UE 基础头文件（`CoreMinimal.h`、`Misc/AutomationTest.h`）
  - 不得 `#include` 任何 `Angelscript*.h`（项目封装层头文件）
- [ ] **P1.1** 📦 Git 提交：`[Test/Native] Feat: add AngelscriptNativeTestSupport header`

- [ ] **P1.2** 创建 `Native/AngelscriptNativeTestSupport.cpp`（如有非 inline 实现）
  - 若 message callback 需要持有状态（收集诊断），则在此实现
  - 若全部 inline 可省略此文件
- [ ] **P1.2** 📦 Git 提交：`[Test/Native] Feat: add NativeTestSupport implementation`

- [ ] **P1.3** 更新 `AngelscriptTest.Build.cs`
  - 在 `PrivateIncludePaths` 中新增 `Path.Combine(ModuleDirectory, "Native")`
  - 确保 ThirdParty AngelScript 头文件路径可被 `Native/` 下的文件直接 include
- [ ] **P1.3** 📦 Git 提交：`[Test/Native] Chore: update Build.cs include paths for Native`

- [ ] **P1.4** 创建 `Native/AngelscriptNativeSmokeTest.cpp`
  - 1 个冒烟测试：创建 engine → 编译 `int Test() { return 1; }` → 执行 → 验证返回值 → 销毁 engine
  - 测试路径：`Angelscript.TestModule.Native.Smoke`
  - 确认整条 Native helper 链路跑通
- [ ] **P1.4** 📦 Git 提交：`[Test/Native] Feat: add native smoke test`

- [ ] **P1.5** 在编辑器中运行 `Angelscript.TestModule.Native.Smoke`，确认通过
- [ ] **P1.5** 📦 Git 提交：`[Test/Native] Test: verify native smoke test passes`

### Phase 2：迁移执行类基础测试

> 目标：将 `Angelscript/AngelscriptExecutionTests.cpp` 中的核心执行测试以"纯原生"方式重写到 `Native/`。

- [ ] **P2.1** 创建 `Native/AngelscriptNativeExecutionTests.cpp`，覆盖以下测试点（每个 IMPLEMENT_SIMPLE_AUTOMATION_TEST 一一对应）：
  - `Native.Execute.VoidFunction` — void 函数编译与执行
  - `Native.Execute.ReturnValue` — int 函数返回值读取
  - `Native.Execute.OneArg` — 单参数 `SetArgDWord()` + 执行
  - `Native.Execute.TwoArgs` — 双参数传递
  - `Native.Execute.ThreeArgs` — 三参数传递
  - 所有测试只用 `AngelscriptNativeTestSupport` 中的 helper，不引用 `FAngelscriptEngine`
- [ ] **P2.1** 📦 Git 提交：`[Test/Native] Feat: add native execution tests (void, return, 1-3 args)`

- [ ] **P2.2** 创建 `Native/AngelscriptNativeExecutionAdvancedTests.cpp`，覆盖：
  - `Native.Execute.FloatReturn` — float 返回值
  - `Native.Execute.StringReturn` — string 返回值（如适用）
  - `Native.Execute.NegativeValue` — 负数参数传递
  - `Native.Execute.MultipleReturnPaths` — 多分支返回路径
- [ ] **P2.2** 📦 Git 提交：`[Test/Native] Feat: add advanced native execution tests`

- [ ] **P2.3** 运行全部 `Angelscript.TestModule.Native.Execute.*` 测试，确认通过
- [ ] **P2.3** 📦 Git 提交：`[Test/Native] Test: verify all native execution tests pass`

### Phase 3：补齐 Native 编译与诊断测试

> 目标：覆盖原生编译成功 / 失败路径和 message callback 诊断信息收集。

- [ ] **P3.1** 创建 `Native/AngelscriptNativeCompileTests.cpp`，覆盖：
  - `Native.Compile.SimpleFunction` — 单函数编译成功
  - `Native.Compile.MultipleFunctions` — 多函数模块编译
  - `Native.Compile.GlobalVariables` — 全局变量声明编译
  - `Native.Compile.SyntaxError` — 语法错误编译失败，`Build()` 返回负值
  - `Native.Compile.ErrorMessage` — message callback 收到错误信息，验证行号和消息文本非空
- [ ] **P3.1** 📦 Git 提交：`[Test/Native] Feat: add native compile and diagnostics tests`

- [ ] **P3.2** 创建 `Native/AngelscriptNativeRegistrationTests.cpp`，覆盖：
  - `Native.Register.GlobalFunction` — 注册 C++ 全局函数，脚本中调用
  - `Native.Register.GlobalProperty` — 注册全局属性，脚本中读取
  - `Native.Register.SimpleValueType` — 注册 POD value type，脚本中构造使用
- [ ] **P3.2** 📦 Git 提交：`[Test/Native] Feat: add native registration tests`

- [ ] **P3.3** 运行全部 `Angelscript.TestModule.Native.*` 测试，确认通过
- [ ] **P3.3** 📦 Git 提交：`[Test/Native] Test: verify all Phase 3 native tests pass`

### Phase 4：标记现有测试分层归属

> 目标：不搬文件，但在现有测试文件头部添加注释说明其归属层级，为后续目录整理做准备。

- [ ] **P4.1** 在以下文件头部（`#if WITH_DEV_AUTOMATION_TESTS` 之前）添加层级注释 `// Test Layer: Runtime Integration`：
  - `Angelscript/AngelscriptExecutionTests.cpp`
  - `Angelscript/AngelscriptCoreExecutionTests.cpp`
  - `Angelscript/AngelscriptControlFlowTests.cpp`
  - `Angelscript/AngelscriptFunctionTests.cpp`
  - `Angelscript/AngelscriptHandleTests.cpp`
  - `Angelscript/AngelscriptMiscTests.cpp`
  - `Angelscript/AngelscriptObjectModelTests.cpp`
  - `Angelscript/AngelscriptOperatorTests.cpp`
  - `Angelscript/AngelscriptTypeTests.cpp`
- [ ] **P4.1** 📦 Git 提交：`[Test] Chore: annotate Angelscript/ tests as Runtime Integration layer`

- [ ] **P4.2** 在以下文件头部添加层级注释 `// Test Layer: Runtime Integration`：
  - `Angelscript/AngelscriptInheritanceTests.cpp`
  - `Angelscript/AngelscriptNativeScriptHotReloadTests.cpp`
  - `Core/AngelscriptEngineCoreTests.cpp`
  - `Shared/AngelscriptTestEngineHelperTests.cpp`
- [ ] **P4.2** 📦 Git 提交：`[Test] Chore: annotate Core/ and inheritance tests as Runtime Integration`

- [ ] **P4.3** 确认 `Scenarios/` 目录中所有文件已带 `// Test Layer: UE Scenario` 或无需标注（已在独立目录）
- [ ] **P4.3** 📦 Git 提交：`[Test] Chore: annotate Scenarios/ tests as UE Scenario layer`

### Phase 5：文档与测试指南同步

> 目标：更新相关文档，明确三层测试的目标、边界和新增约定。

- [ ] **P5.1** 在 `Documents/Guides/` 下新增或更新 `Test.md`，写明：
  - 三层测试定义（Native Core / Runtime Integration / UE Scenario）
  - 每层测试的职责、允许的依赖、文件位置约定
  - `Native/` 下测试的命名规范：`Angelscript.TestModule.Native.<Category>.<TestName>`
  - 新增测试时的选层决策流程
- [ ] **P5.1** 📦 Git 提交：`[Docs] Feat: add test layer guide to Documents/Guides/Test.md`

- [ ] **P5.2** 在 `Plugins/Angelscript/AGENTS.md` 中补充 Native 测试层说明（1-2 句简述 + 指向 `Documents/Guides/Test.md`）
- [ ] **P5.2** 📦 Git 提交：`[Docs] Chore: mention Native test layer in plugin AGENTS.md`

## 验收标准

1. `Native/AngelscriptNativeTestSupport.h` 中不出现任何 `FAngelscriptEngine` / `FAngelscriptPreprocessor` / `UObject` 相关引用。
2. `Native/*.cpp` 中所有测试只通过 `asIScriptEngine` / `asIScriptModule` / `asIScriptContext` 原生 API 运行。
3. `Angelscript.TestModule.Native.Smoke` 冒烟测试通过。
4. `Angelscript.TestModule.Native.Execute.*`（≥5 个）全部通过。
5. `Angelscript.TestModule.Native.Compile.*`（≥5 个）全部通过。
6. `Angelscript.TestModule.Native.Register.*`（≥3 个）全部通过。
7. 现有 Runtime / Scenario 测试回归通过（不因本次重构引入失败）。
8. `Documents/Guides/Test.md` 明确定义三层测试边界。

## 风险与注意事项

### 风险 1：AngelScript 原生头文件 include 路径问题

`AngelscriptTest.Build.cs` 当前通过 `AngelscriptRuntime` 模块间接获取 ThirdParty 头文件路径。`Native/` 层仍需 include `angelscript.h`，需要确保 `Build.cs` 中的 include path 覆盖 ThirdParty 目录。

**缓解**：P1.3 中显式验证 `#include "angelscript.h"` 在 `Native/` 文件中可编译。

### 风险 2：Native helper 能力边界蠕变

如果后续为了"方便"向 `AngelscriptNativeTestSupport.h` 中塞入 `FAngelscriptEngine` 相关逻辑，分层就会失效。

**缓解**：在 `AngelscriptNativeTestSupport.h` 文件头部添加约束注释，明确禁止引入项目封装层依赖。CI 中可补 include 检查。

### 风险 3：测试迁移窗口期覆盖面下降

先新增 Native 测试，不删除原有 Runtime 层测试。两层测试暂时共存，直到 Native 层稳定后再评估是否精简 Runtime 层中的重复用例。

**缓解**：采用"先新增 → 并行运行 → 确认覆盖 → 再评估删除"的顺序，不在本计划中删除任何现有测试。

### 风险 4：asIScriptEngine 无默认 message callback 导致静默失败

原生 `asCreateScriptEngine()` 创建的引擎没有 message callback，编译错误不会输出。

**缓解**：P1.1 中的 `CreateNativeEngine()` 必须注册 message callback，将消息转发到 `UE_LOG` 或测试框架输出。
