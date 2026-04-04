# 已知测试失败修复计划

## 背景与目标

`Plan_TestEngineIsolation` 完成后，全量测试套件（443 个）稳定运行无崩溃，但仍有 7 个测试失败。这些失败均为功能待补齐或测试 expectation 不匹配，不涉及 crash 或回归。

本计划将这 7 个失败按根因归类，逐项修复，目标是全量测试 443/443 全绿。

当前基线：**436/443 通过**，验证快照见 `TechnicalDebtInventory.md` Section 17。

---

## Phase 1：Engine Core 生命周期测试修复（3 个）

### 问题描述

`CreateTestingFullEngine()` 创建的引擎在 `InitializeForTesting()` 中调用了 `BindScriptTypes()`，确实向 `FAngelscriptTypeDatabase` 注册了类型元数据。但这三个测试在调用 `FAngelscriptType::GetTypes()` 时没有活跃的 `FAngelscriptEngineScope`，导致 `GetTypeDatabase()` 使用了错误的 isolation key（或 nullptr fallback bucket），查不到刚注册的类型。

`AngelscriptMultiEngineTests.cpp` 中的类似测试之所以通过，正是因为它们用 `FAngelscriptEngineScope` 包裹了 `GetTypes()` 调用。

### 影响文件

- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptEngineCoreTests.cpp`（3 个 `RunTest` 函数）

- [ ] **P1.1** 修复 `Engine.LastFullDestroyClearsTypeState`
  - `FAngelscriptType::GetTypes()` 调用点（约 line 156）缺少 `FAngelscriptEngineScope`。需要在 `CreateFullTestEngine()` 返回后、销毁前，用 `FAngelscriptEngineScope` 包裹 `GetTypes()` 调用，确保 isolation key 正确解析到 Full 引擎的 bucket
  - 同时检查 `FullEngine.Reset()` 后的 `GetTypes().Num()` 是否应该在无 scope 下执行（此时期望 0，无 scope 应该合理）
- [ ] **P1.1** 📦 Git 提交

- [ ] **P1.2** 修复 `Engine.FullDestroyAllowsCleanRecreate`
  - 同理，line 187 和 line 204 的 `GetTypes()` 调用需要在对应 engine 的 scope 内执行
  - FirstEngine 和 SecondEngine 的 scope 需要分别管理，确保 Reset 前 scope 已退出
- [ ] **P1.2** 📦 Git 提交

- [ ] **P1.3** 修复 `Engine.FullDestroyAllowsAnnotatedRecreate`
  - 错误信息 `Class ARecreateAnnotatedActorA has an unknown super type AAngelscriptActor` 说明编译注解模块时引擎上下文不正确
  - `CompileAnnotatedModuleFromMemory()` 需要在 `FAngelscriptEngineScope` 下执行，才能解析 `AAngelscriptActor` 类型
  - 检查 lambda `CompileAnnotatedActor` 内部是否已有 scope，如无则添加
- [ ] **P1.3** 📦 Git 提交

- [ ] **P1.4** 验证 Phase 1：运行 `Engine.LastFullDestroyClearsTypeState`、`Engine.FullDestroyAllowsCleanRecreate`、`Engine.FullDestroyAllowsAnnotatedRecreate` 全部通过
- [ ] **P1.4** 📦 Git 提交

---

## Phase 2：Restore 序列化错误消息修复（1 个）

### 问题描述

`Internals.Restore.EmptyStreamFails` 使用 `AddExpectedErrorPlain(TEXT("Unexpected end of file"), Contains)` 匹配空流加载时的错误消息。实际 AS 引擎对空流报出的消息是 `"Angelscript: :"`（经过 `LogAngelscriptError` 格式化后的空消息），不包含 `"Unexpected end of file"`。

### 影响文件

- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptRestoreTests.cpp`（1 个 `RunTest` 函数，约 line 246）

- [ ] **P2.1** 诊断 `LoadByteCode` 对空流的实际错误输出
  - 在 `as_restore.cpp` 中找到 `LoadByteCode` 对空流的处理路径，确认实际输出的 AS 消息内容
  - 确认 `LogAngelscriptError` 回调收到的原始消息字符串
  - 判断是应该调整测试 expectation 还是在 Restore 路径补充更明确的错误消息
- [ ] **P2.1** 📦 Git 提交

- [ ] **P2.2** 实施修复
  - 优先方案 A：如果 AS 引擎对空流确实不报 `"Unexpected end of file"`，调整测试 expectation 匹配实际消息
  - 备选方案 B：在 `LoadByteCode` 入口添加空流前置检查，主动报出 `"Unexpected end of file"` 消息
  - 无论哪种方案，确保 `TruncatedStreamFails` 不受影响（它目前通过）
- [ ] **P2.2** 📦 Git 提交

---

## Phase 3：预处理器 import 解析修复（3 个）

### 问题描述

预处理器的 `import` 语句处理有两个缺失行为：
1. `ImportedModules` 没有被记录（`import Tests.Preprocessor.Shared;` 执行后 `ImportingModule->ImportedModules` 为空）
2. import 语句没有从处理后的代码中移除（`ImportingModule->Code[0].Code` 仍包含 `"import Tests.Preprocessor.Shared;"` 原文）

文件发现测试 `FileSystemAndModuleResolution` 的失败是因为 `DiscoveryWithEditorScripts.Num() > DiscoveryWithoutEditorScripts.Num()` 断言不满足，需要调查测试环境下的脚本文件布局。

### 影响文件

- `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp`（import 解析逻辑）
- `Plugins/Angelscript/Source/AngelscriptTest/Preprocessor/AngelscriptPreprocessorTests.cpp`（`ImportParsing` 测试）
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Runtime/AngelscriptLearningPreprocessorTraceTests.cpp`（`Learning.Runtime.Preprocessor` 测试）
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Runtime/AngelscriptLearningFileSystemAndModuleTraceTests.cpp`（`Learning.Runtime.FileSystemAndModuleResolution` 测试）

- [ ] **P3.1** 分析预处理器 import 处理路径
  - 在 `AngelscriptPreprocessor.cpp` 中定位 `import` 关键字的处理逻辑
  - 确认当前是否存在 import 语句识别代码但行为不完整，还是完全没有实现
  - 参考 UEAS2 源码中 import 处理的实现方式
- [ ] **P3.1** 📦 Git 提交

- [ ] **P3.2** 实现 import 语句记录到 `ImportedModules`
  - 在预处理阶段识别 `import <ModuleName>;` 语法
  - 将解析到的 module name 加入 `FAngelscriptModuleDesc::ImportedModules`
- [ ] **P3.2** 📦 Git 提交

- [ ] **P3.3** 实现 import 语句从处理后代码中移除
  - 在写回 `Code[0].Code` 时剥除 import 行
  - 确保只移除 import 语句行，不影响其余代码
- [ ] **P3.3** 📦 Git 提交

- [ ] **P3.4** 诊断并修复 `FileSystemAndModuleResolution` 失败
  - 确认测试环境中 `Script/` 目录下是否存在 editor-only 脚本文件
  - 如果文件布局正确但发现逻辑有 bug，修复发现逻辑
  - 如果文件布局不满足测试假设，调整测试 fixture 或 expectation
- [ ] **P3.4** 📦 Git 提交

- [ ] **P3.5** 验证 Phase 3：运行 `Preprocessor.ImportParsing`、`Learning.Runtime.Preprocessor`、`Learning.Runtime.FileSystemAndModuleResolution` 全部通过
- [ ] **P3.5** 📦 Git 提交

---

## Phase 4：全量回归验证

- [ ] **P4.1** 运行 `Automation RunTests Angelscript.TestModule` 全量回归
  - 目标：443/443 全绿
  - 如有新增失败，回退分析并补修
- [ ] **P4.1** 📦 Git 提交

- [ ] **P4.2** 更新文档
  - 更新 `TechnicalDebtInventory.md` Section 17 为已关闭状态
  - 更新 `TestCatalog.md` 基线数字
- [ ] **P4.2** 📦 Git 提交

---

## 验收标准

1. 全量测试套件 443/443 通过，无崩溃、无跳过
2. 无新增 Warning 或 Error 级别日志（已有的 Verbose/VeryVerbose 诊断日志不算）
3. 所有修改通过编译（`RunBuild.ps1` exit code 0）

## 风险与注意事项

### 风险

1. **预处理器 import 实现复杂度**：如果 UEAS2 的 import 机制依赖较深的预处理器状态（如模块依赖排序、循环 import 检测），P3 可能需要拆分更细
   - **缓解**：先实现最小可行的单层 import 识别+记录+移除，不做深度依赖解析
2. **FileSystemAndModuleResolution 环境依赖**：该测试依赖磁盘上的脚本文件布局，可能受 CI / worktree 环境影响
   - **缓解**：如果确认是环境问题，将测试改为自包含 fixture 而不是依赖外部文件

### 已知行为变化

1. **P1 的 scope 添加不改变测试语义**：仅在 `GetTypes()` 调用处补充缺失的 `FAngelscriptEngineScope`，不改变测试逻辑
2. **P2 的 expectation 调整**：如果选择方案 A，错误消息匹配字符串会变化，需确保 `TruncatedStreamFails` 不受影响
