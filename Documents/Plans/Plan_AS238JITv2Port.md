# AS 2.38 JIT v2 接口回移计划

## 背景与目标

AngelScript 2.37 引入了 **JIT Compiler V2 接口**（`asIJITCompilerV2`），这是对原 V1 接口（`asIJITCompiler`）的演进。两者的核心差异在于编译时机和语义：

| 方面 | V1（`asIJITCompiler`） | V2（`asIJITCompilerV2`） |
|------|------------------------|---------------------------|
| 编译时机 | `CompileFunction` 同步调用，必须当场决定是否产出 JIT | `NewFunction` 通知性质；JIT 可延后通过 `SetJITFunction` 挂载 |
| 清理 | `ReleaseJITFunction(jit)` | `CleanFunction(scriptFunc, jit)` |
| API 基类 | 直接接口 | 通过 `asIJITCompilerAbstract` 统一 |
| `SetJITFunction` | 返回 `asNOT_SUPPORTED` | 可用，支持异步挂载 JIT 句柄 |

当前项目的公共 API（`Core/angelscript.h`）仅有 V1 接口。`StaticJIT/AngelscriptStaticJIT.h` 中 `FAngelscriptStaticJIT` 继承 `asIJITCompiler`（V1），实现 `CompileFunction` + `ReleaseJITFunction`。引擎内部 `as_scriptengine.h` 的 `jitCompiler` 成员类型也是 `asIJITCompiler*`。

虽然 ThirdParty 的 `as_scriptengine.cpp` 中 `asEP_JIT_INTERFACE_VERSION` 属性已可 Set/Get（1 或 2），但由于缺少 V2 接口定义和脚本函数侧的 V2 分支，设为 2 不会触发任何新行为。

**目标**：将 2.38 的 JIT V2 接口完整回移，使得：

1. V1 路径保持不变（`FAngelscriptStaticJIT` 继续以 V1 模式工作）
2. V2 接口可用（新的 JIT 实现可选择继承 `asIJITCompilerV2`）
3. `asEP_JIT_INTERFACE_VERSION = 2` 时引擎走 V2 路径
4. `SetJITFunction` / `GetJITFunction` API 可用

## V2 相对 V1 的优势分析

### 接口对比

| 维度 | V1（`asIJITCompiler`） | V2（`asIJITCompilerV2`） |
|------|------------------------|---------------------------|
| 核心 API | `CompileFunction(func, &output)` + `ReleaseJITFunction(jit)` | `NewFunction(func)` + `CleanFunction(func, jit)` |
| 编译时机 | **同步阻塞**——`Build()` 尾部逐函数调用 `CompileFunction`，必须当场完成 | **通知 + 延后**——`NewFunction` 仅通知"有新函数"，JIT 可在任意时刻通过 `SetJITFunction` 挂载 |
| `SetJITFunction` | 返回 `asNOT_SUPPORTED`（V1 不能事后挂接） | 可用，支持延后/异步挂载 JIT 句柄 |
| 清理语义 | `ReleaseJITFunction(jit)` — 只拿到 JIT 句柄，不知道对应哪个脚本函数 | `CleanFunction(func, jit)` — 同时拿到脚本函数和 JIT 句柄，可做精确资源解绑 |
| 全局优化 | **不可**——官方文档明确说"被迫只根据当前正在编译的这一个函数来编译 JIT，不能之后再回头更新" | **可**——`NewFunction` 收集后可见全模块所有函数，再统一做内联、跨函数布局等全局优化 |
| 热重载友好度 | 替换需再次 `JITCompile` → 先 `ReleaseJITFunction` 旧的 | `SetJITFunction` 直接替换，引擎自动先 `CleanFunction` 旧的；语义更清晰 |

### 核心优势详解

**1. 延迟绑定与异步编译**

V1 的 `CompileFunction` 在 `asCModule::Build()` 和 `LoadByteCode` 的尾部被同步调用，引擎在此期间阻塞等待。对于函数数量多的模块，这会拉长编译/加载的峰值延迟。

V2 的 `NewFunction` 仅是轻量通知，可立即返回。JIT 实现可以：
- 在 `NewFunction` 中仅登记函数，Build 完成后批量编译
- 将编译任务放入后台线程，主线程不阻塞
- 实现惰性 JIT：只在函数首次执行或被判定为热点时才编译

在 `SetJITFunction` 被调用前，函数走解释执行路径（VM 在 `jitFunction == 0` 时自动回退到字节码）。

**2. 全模块可见性与全局优化**

V1 在 Build 循环中逐函数调 `CompileFunction`，每次只能看到当前这一个函数的信息。

V2 允许等到 Build 完成后，通过 `GetLastFunctionId()` + `GetFunctionById()` 遍历整个模块的所有函数，然后再决定：
- 哪些函数值得 JIT（热点分析）
- 跨函数的内联机会
- 调用图优化和代码布局
- 批量生成以减少编译器启动开销

官方文档原文（`doc_adv_jit.h`）："With the V2 interface the JIT compiler can **postpone the JIT compilation until after the entire script has been compiled or loaded**, allowing the JIT compiler to **see all the functions and do global optimizations, e.g. inlining**."

**3. 更精确的资源生命周期管理**

V1 的 `ReleaseJITFunction(asJITFunction jit)` 只传入 JIT 句柄，JIT 实现需要自行维护句柄到脚本函数的反向映射才能做精确清理。

V2 的 `CleanFunction(asIScriptFunction* func, asJITFunction jit)` 同时传入脚本函数指针和 JIT 句柄，覆盖两种场景：
- **替换**：`SetJITFunction` 挂载新 JIT 前，引擎先 `CleanFunction` 旧 JIT
- **销毁**：脚本函数析构时，引擎 `CleanFunction` 并置零

这使得 JIT 侧可以在清理时访问脚本函数的元数据（如名称、模块等），便于日志和调试。

**4. 编译/加载延迟优化**

| 场景 | V1 | V2 |
|------|----|----|
| `Module::Build()` 尾部 | 阻塞：N 个函数 × 单函数编译耗时 | 接近零：N 个 `NewFunction` 通知（微秒级） |
| `LoadByteCode` 尾部 | 同上 | 同上 |
| 首帧可用时间 | 必须等所有 JIT 编译完成 | 可立即开始执行（解释模式），后台逐步挂载 JIT |

### 对当前 `FAngelscriptStaticJIT` 的适用性

当前 `FAngelscriptStaticJIT` 的 V1 使用模式有些特殊：

- `CompileFunction` 并不真正做运行时 JIT 编译，而是将函数登记到 `FunctionsToGenerate`，返回 `1`（成功）但 `*output = nullptr`
- `ReleaseJITFunction` 为空实现（静态生成的 C++ 函数无法通过句柄释放）
- 实际的代码生成发生在 Build 之后的独立步骤

这种"先登记、后批量生成"的模式与 V2 的语义**天然契合**：

| 当前 V1 模式（变通用法） | 自然对应的 V2 模式 |
|------------------------|-------------------|
| `CompileFunction` 中登记函数 | `NewFunction` 中登记函数 |
| 返回 `1` + `nullptr`（无真实 JIT） | `NewFunction` 无返回值，无需假装成功 |
| `ReleaseJITFunction` 空实现 | `CleanFunction` 可做精确清理（知道是哪个函数） |
| 后续批量生成无法通过 API 挂载 | `SetJITFunction` 可在生成完成后正式挂载 |

V2 迁移的核心收益：消除当前 V1 下的"变通用法"，用正规 API 表达"先收集、后生成、再挂载"的意图。

### 引用来源

- 接口定义：`Reference/angelscript-v2.38.0/sdk/angelscript/include/angelscript.h:1425-1449`
- V1/V2 编译分支：`Reference/angelscript-v2.38.0/sdk/angelscript/source/as_scriptfunction.cpp:1588-1649`
- V1/V2 清理分支：`Reference/angelscript-v2.38.0/sdk/angelscript/source/as_scriptfunction.cpp:1432-1440`
- `SetJITFunction` 实现：`Reference/angelscript-v2.38.0/sdk/angelscript/source/as_scriptfunction.cpp:1559-1577`
- 官方 V2 文档：`Reference/angelscript-v2.38.0/sdk/docs/doxygen/source/doc_adv_jit.h:15-41`
- SDK 测试（V2）：`Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_compiler.cpp:1984-2043`
- 当前 StaticJIT：`StaticJIT/AngelscriptStaticJIT.cpp:66-85`

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| `asIJITCompilerAbstract` | 不存在 | `angelscript.h:1425-1428` |
| `asIJITCompilerV2` | 不存在 | `angelscript.h:1443-1448`（`NewFunction` + `CleanFunction`） |
| `SetJITCompiler` 参数类型 | `asIJITCompiler*` | `asIJITCompilerAbstract*` |
| `jitCompiler` 成员类型 | `asIJITCompiler*` | `asIJITCompilerAbstract*` |
| `asEP_JIT_INTERFACE_VERSION` Set/Get | 存在 | 存在 |
| `JITCompile()` V1/V2 分支 | 仅 V1 | V1/V2 双路径 |
| `SetJITFunction` / `GetJITFunction` | 不存在 | `asIScriptFunction` 上的方法 |
| `FAngelscriptStaticJIT` | 继承 `asIJITCompiler`（V1） | N/A（项目特有） |

## 分阶段执行计划

### Phase 0 — StaticJIT 影响评估

> 目标：评估 V2 接口对当前 `FAngelscriptStaticJIT` 的影响，决定是保持 V1 兼容还是迁移到 V2。此 Phase 不修改代码。

- [ ] **P0.1** 分析 `FAngelscriptStaticJIT` 的 V1 使用模式
  - 阅读 `StaticJIT/AngelscriptStaticJIT.cpp` 中 `CompileFunction` 和 `ReleaseJITFunction` 的实现
  - 评估其编译模式是否更适合 V2 的异步语义（`NewFunction` + 延迟 `SetJITFunction`）或保持 V1 同步模式
  - 评估是否需要同时支持 V1 和 V2（即 `FAngelscriptStaticJIT` 保持 V1，但新的 JIT 实现可用 V2）
  - 输出决策：**方案 A**（保持 V1，仅添加 V2 基础设施）或 **方案 B**（迁移到 V2）
  - 将决策记录在本文档"决策记录"小节
- [ ] **P0.1** 📦 Git 提交：`[Docs] Docs: StaticJIT V2 migration assessment`

### Phase 1 — 公共 API 与接口定义

> 目标：在公共头文件中添加 V2 接口定义和相关 API，不改变运行时行为。

- [ ] **P1.1** 在 `Core/angelscript.h` 中添加 `asIJITCompilerAbstract` 基类
  - 空基类，仅含虚析构函数
  - 修改 `asIJITCompiler` 使其继承 `asIJITCompilerAbstract`（不改变 V1 行为）
  - 参考 2.38 `angelscript.h:1425-1448`
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add asIJITCompilerAbstract base class`

- [ ] **P1.2** 在 `Core/angelscript.h` 中添加 `asIJITCompilerV2` 接口
  - 继承 `asIJITCompilerAbstract`
  - 纯虚方法：`void NewFunction(asIScriptFunction*)` + `void CleanFunction(asIScriptFunction*, asJITFunction)`
  - 参考 2.38 `angelscript.h:1443-1448`
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: add asIJITCompilerV2 interface`

- [ ] **P1.3** 修改 `SetJITCompiler` / `GetJITCompiler` 签名
  - 参数类型从 `asIJITCompiler*` 改为 `asIJITCompilerAbstract*`
  - 返回类型对应修改
  - 由于 `asIJITCompiler` 已继承 `asIJITCompilerAbstract`，现有代码（传 `asIJITCompiler*`）无需修改即可编译
  - 同时修改 `asIScriptEngine` 接口声明和 `asCScriptEngine` 实现
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: widen SetJITCompiler signature to abstract base`

- [ ] **P1.4** 在 `asIScriptFunction` 中添加 `SetJITFunction` / `GetJITFunction`
  - `int SetJITFunction(asJITFunction)`：V2 模式下用于异步挂载 JIT 函数
  - `asJITFunction GetJITFunction() const`：查询当前 JIT 函数
  - V1 模式下 `SetJITFunction` 返回 `asNOT_SUPPORTED`
  - 在 `as_scriptfunction.h/cpp` 中添加实现
  - 参考 2.38 `angelscript.h` 接口声明和 `as_scriptfunction.cpp` 实现
- [ ] **P1.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: add SetJITFunction and GetJITFunction API`

### Phase 2 — 引擎内部 V1/V2 分支

> 目标：在引擎内部实现 V1/V2 双路径，根据 `asEP_JIT_INTERFACE_VERSION` 选择对应的 JIT 编译和清理流程。

- [ ] **P2.1** 修改 `as_scriptengine.h` 中 `jitCompiler` 成员类型
  - 从 `asIJITCompiler*` 改为 `asIJITCompilerAbstract*`
  - 同步修改 `SetJITCompiler` 实现中的存储逻辑
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: change jitCompiler member to abstract base type`

- [ ] **P2.2** 在 `as_scriptfunction.cpp` 的 `JITCompile()` 中添加 V2 路径
  - 当 `engine->ep.jitInterfaceVersion == 1` 时：保持现有 `static_cast<asIJITCompiler*>(...)->CompileFunction(...)` 调用
  - 当 `engine->ep.jitInterfaceVersion == 2` 时：调用 `static_cast<asIJITCompilerV2*>(...)->NewFunction(this)`
  - 参考 2.38 `as_scriptfunction.cpp:1432-1649` 中的 V1/V2 编译分支
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: add V2 JIT compile path in script function`

- [ ] **P2.3** 在 `as_scriptfunction.cpp` 的析构/释放路径中添加 V2 清理
  - 当 `jitFunction` 存在时：
    - V1：`static_cast<asIJITCompiler*>(...)->ReleaseJITFunction(jitFunction)`
    - V2：`static_cast<asIJITCompilerV2*>(...)->CleanFunction(this, jitFunction)`
  - 参考 2.38 `as_scriptfunction.cpp` 中的释放分支
- [ ] **P2.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: add V2 JIT cleanup path in script function`

- [ ] **P2.4** 实现 `SetJITFunction` 的 V2 逻辑
  - V2 模式下：将传入的 `asJITFunction` 存入 `scriptData->jitFunction`
  - V1 模式下：返回 `asNOT_SUPPORTED`
  - 若已有 jitFunction，先调用 V2 的 `CleanFunction` 清理旧的
- [ ] **P2.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement SetJITFunction V2 logic`

### Phase 3 — StaticJIT 适配（根据 Phase 0 决策）

> 目标：确保 `FAngelscriptStaticJIT` 在新接口体系下正常工作。

#### 方案 A：保持 V1

- [ ] **P3A.1** 确认 `FAngelscriptStaticJIT` 编译通过
  - `FAngelscriptStaticJIT` 继承 `asIJITCompiler`（V1），后者现继承 `asIJITCompilerAbstract`
  - `SetJITCompiler` 签名已改为接受 `asIJITCompilerAbstract*`，传 `asIJITCompiler*` 应自动向上转型
  - 确认引擎在 `jitInterfaceVersion == 1` 时仍走 V1 路径
- [ ] **P3A.1** 📦 Git 提交：`[ThirdParty/AS238] Test: verify StaticJIT V1 compatibility with new interface hierarchy`

#### 方案 B：迁移到 V2

- [ ] **P3B.1** 将 `FAngelscriptStaticJIT` 改为继承 `asIJITCompilerV2`
  - 将 `CompileFunction` + `ReleaseJITFunction` 重构为 `NewFunction` + `CleanFunction`
  - `NewFunction` 中登记待编译函数，后续通过 `SetJITFunction` 挂载
  - 确认引擎需要设置 `asEP_JIT_INTERFACE_VERSION = 2`
- [ ] **P3B.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: migrate FAngelscriptStaticJIT to V2 interface`

### Phase 4 — 测试与文档

> 目标：验证 V1 不退化、V2 基本路径正确、StaticJIT 正常工作。

- [ ] **P4.1** 编写 JIT 接口测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptJITInterfaceTests.cpp`
  - 测试用例清单：
    - **V1_StillWorks**：设置 `asEP_JIT_INTERFACE_VERSION = 1`，注册 V1 JIT，编译脚本，断言 `CompileFunction` 被调用
    - **V2_NewFunctionCalled**：设置 `asEP_JIT_INTERFACE_VERSION = 2`，注册 V2 JIT，编译脚本，断言 `NewFunction` 被调用
    - **V2_SetJITFunction**：V2 模式下，在 `NewFunction` 回调中通过 `SetJITFunction` 挂载虚假 JIT 函数，断言 `GetJITFunction` 返回正确值
    - **V1_SetJITFunction_Rejected**：V1 模式下调用 `SetJITFunction` 返回 `asNOT_SUPPORTED`
    - **V2_CleanFunctionCalled**：V2 模式下销毁模块，断言 `CleanFunction` 被调用
  - 测试使用简单的 mock JIT compiler 实现
- [ ] **P4.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add JIT V1/V2 interface verification tests`

- [ ] **P4.2** 全量回归验证
  - 运行全量测试套件，确认 V1 模式（默认）下零回归
  - StaticJIT 相关测试通过
- [ ] **P4.2** 📦 Git 提交：`[ThirdParty/AS238] Test: verify JIT v2 port with full regression`

- [ ] **P4.3** 更新 `AngelscriptChange.md`
  - 登记公共 API 变更和内部分支修改
- [ ] **P4.3** 📦 Git 提交：`[Docs] Docs: document JIT v2 interface backport changes`

## 决策记录

> 此区域在 Phase 0 完成后填写。

**StaticJIT 迁移决策**：（待评估后填写）
**选择理由**：（待评估后填写）

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `Core/angelscript.h` | 修改 | `asIJITCompilerAbstract`、`asIJITCompilerV2`、`SetJITCompiler` 签名、`SetJITFunction`/`GetJITFunction` |
| `ThirdParty/.../as_scriptengine.h` | 修改 | `jitCompiler` 成员类型 |
| `ThirdParty/.../as_scriptengine.cpp` | 修改 | `SetJITCompiler` 实现 |
| `ThirdParty/.../as_scriptfunction.h` | 修改 | `SetJITFunction`/`GetJITFunction` 声明 |
| `ThirdParty/.../as_scriptfunction.cpp` | 修改 | `JITCompile()` V2 路径、释放路径、`SetJITFunction` 实现 |
| `StaticJIT/AngelscriptStaticJIT.h` | 修改（方案 B）或审查（方案 A） | 接口继承 |
| `StaticJIT/AngelscriptStaticJIT.cpp` | 修改（方案 B）或不变（方案 A） | 方法实现 |
| `AngelscriptTest/Native/AngelscriptJITInterfaceTests.cpp` | 新增 | JIT V1/V2 测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. `asIJITCompilerAbstract` 和 `asIJITCompilerV2` 接口定义可用
2. `asEP_JIT_INTERFACE_VERSION = 1` 时 V1 路径不变，现有测试通过
3. `asEP_JIT_INTERFACE_VERSION = 2` 时 V2 路径正确（`NewFunction`/`CleanFunction` 被调用）
4. `SetJITFunction` 在 V2 模式下可用，V1 模式下返回 `asNOT_SUPPORTED`
5. `FAngelscriptStaticJIT` 正常工作（V1 或 V2 取决于决策）
6. 所有现有测试通过
7. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **公共 API 变更**：`SetJITCompiler` 的参数类型从 `asIJITCompiler*` 改为 `asIJITCompilerAbstract*`，虽然向上兼容（子类指针自动转型），但如果有外部代码显式使用了旧签名可能需要调整
2. **`static_cast` 安全性**：V1/V2 分支中的 `static_cast` 依赖 `jitInterfaceVersion` 与实际注册的 JIT 类型一致，不一致时会导致未定义行为。`SetJITCompiler` 实现中应校验类型匹配
3. **V2 异步语义**：V2 的 `NewFunction` + 延迟 `SetJITFunction` 模式可能需要线程安全考虑（如果 JIT 编译在后台线程进行），但当前 AngelScript 引擎整体非线程安全，此风险在现阶段可接受
4. **StaticJIT 预编译模式**：当前 `FAngelscriptStaticJIT` 实际上是在代码生成阶段使用，不是运行时 JIT。V2 的异步语义可能更适合这种"编译后挂载"的模式，但迁移需验证时序正确性
