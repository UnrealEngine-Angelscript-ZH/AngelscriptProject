# AS 2.38 函数模板特性回移计划

## 背景与目标

AngelScript 2.38.0 新增了**函数模板**（Template Functions）特性，允许 C++ 宿主通过 `RegisterGlobalFunction` / `RegisterObjectMethod` 注册带模板形参（如 `<T>`、`<T,U>`）的函数，脚本侧可以通过 `func<int>(...)` 的语法显式实例化并调用。

当前项目使用的 AngelScript 版本为 2.33.0 WIP，**不具备函数模板能力**。项目架构决策已将整体 2.38 升级降为 Backlog（需求驱动），但函数模板是一个相对独立、边界清晰的特性，可以作为**选择性合入**的首个目标。

**目标**：将 2.38 中函数模板的完整能力（注册、解析、编译实例化、调用、序列化）迁移到当前 2.33 基础上，使得：

1. C++ 侧可用 generic 调用约定注册模板函数
2. 脚本侧可用 `name<Type>(args)` 语法调用已注册的模板函数
3. Bytecode 序列化/反序列化正确处理模板函数
4. 不引入 2.38 中其他无关的变更（foreach、可变参数等）

## 可行性评估

### 有利因素

- **基础设施已存在**：2.33 已有 `templateSubTypes` 字段（`asCScriptFunction`、`asCObjectType`）、`asOBJ_TEMPLATE_SUBTYPE` 占位类型机制、`DetermineTypeForTemplate` 类型替换函数——这些是模板类型（`TArray<T>` 等）使用的基础设施，函数模板直接复用
- **变更边界清晰**：2.38 函数模板的核心实现集中在约 6-8 个源文件中，涉及约 300-400 行新增代码（不含测试），不涉及虚拟机指令集变更
- **对现有代码影响小**：新增枚举值 `asFUNC_TEMPLATE = 7` 不影响已有枚举；注册路径仅在检测到 `templateSubTypes` 非空时分流；编译器新增独立函数 `InstantiateTemplateFunctions` 仅在调用带模板参数的函数时触发

### 风险点

- **2.33→2.38 间有其他交叉变更**：同文件中可能有 2.34-2.37 的修复也修改了 builder/compiler/parser 的结构，需要逐行确认上下文是否兼容
- **序列化格式差异**：`as_restore.cpp` 在 2.38 中对模板函数有专门的保存/加载逻辑，需确认其依赖的数据结构在 2.33 中已存在
- **UE 侧标志位加宽**：当前 2.33 已有 `[UE++]` 标志位扩展，新增的引擎字段和 API 需与现有 UE 修改保持一致

### 结论

**可行**。核心工作量在于精确摘取 2.38 中函数模板相关的代码块，适配到 2.33 的上下文中。估计总代码改动量约 500-700 行（含修改和新增），分 4 个阶段完成。

## 范围与边界

### 在范围内

- `asFUNC_TEMPLATE` 枚举和相关 API（`GetSubTypeCount`/`GetSubTypeId`/`GetSubType`）
- Parser 中 `ParseTemplateDeclTypeList` 及其在 `ParseFunctionDefinition` 中的调用
- Builder 中 `ParseFunctionDeclaration` 对函数模板形参的解析
- Engine 中 `GetTemplateSubTypeByName` 的重构（从内联查找抽为方法）、`registeredTemplateGlobalFuncs`、`generatedTemplateFunctionInstances`、`GetTemplateFunctionInstance` 的实现
- Compiler 中 `InstantiateTemplateFunctions` 及其在全局/成员函数调用路径中的插入
- `RegisterGlobalFunction` / `RegisterObjectMethod` 中对 `asFUNC_TEMPLATE` 的识别和 generic-only 校验
- `as_restore.cpp` 中模板函数的序列化/反序列化
- 测试覆盖

### 不在范围内

- 2.38 的 foreach 语法
- 2.38 的可变参数函数（variadic）
- 2.38 中非模板函数相关的 bugfix
- 脚本侧定义模板函数（AngelScript 本身也不支持）
- 模板参数推导（2.38 要求显式指定子类型，不做隐式推导）

## 当前事实状态

| 项目 | 2.33（当前） | 2.38（参考） |
|------|-------------|-------------|
| `asFUNC_TEMPLATE` 枚举 | 无（枚举止于 `asFUNC_DELEGATE = 6`） | `asFUNC_TEMPLATE = 7` |
| `ParseTemplateDeclTypeList` | 无 | `as_parser.cpp:330-407`，约 78 行 |
| `ParseFunctionDefinition` 中模板参数解析 | 直接 Identifier → ParameterList | Identifier → `ParseTemplateDeclTypeList` → ParameterList |
| Builder 中 `ParseFunctionDeclaration` | 不处理函数模板形参 | 检测 Identifier 后续节点，调用 `GetTemplateSubTypeByName` 填充 `templateSubTypes` |
| `GetTemplateSubTypeByName` | 无（内联在 `RegisterObjectType` 中） | 独立方法，`as_scriptengine.cpp:1790-1814` |
| `registeredTemplateSubTypes` 数组 | 命名为 `templateSubTypes`（引擎成员） | 改名为 `registeredTemplateSubTypes` |
| `registeredTemplateGlobalFuncs` | 无 | 新增，跟踪注册的全局模板函数基函数 |
| `generatedTemplateFunctionInstances` | 无 | 新增，跟踪已实例化的模板函数实例 |
| `GetTemplateFunctionInstance` | 无 | `as_scriptengine.cpp:3187-3249`，约 63 行 |
| `InstantiateTemplateFunctions` | 无 | `as_compiler.cpp:12702-12738`，约 37 行 |
| `IsTemplateFn` | 无 | `as_scriptengine.cpp:6518-6526`，约 9 行 |
| `asIScriptFunction::GetSubType*` | 无 | 3 个虚方法 + 实现 |
| `as_restore.cpp` 模板函数处理 | 无 | 多处差异（保存/加载时处理 `templateSubTypes`） |

## 分阶段执行计划

### Phase 1 — 数据结构与 API 声明

> 目标：在不改变任何运行时行为的前提下，补齐函数模板所需的数据结构定义和公共 API 声明。完成后项目应能正常编译，所有现有测试不受影响。

- [ ] **P1.1** 在 `angelscript.h` 的 `asEFuncType` 枚举中添加 `asFUNC_TEMPLATE = 7`
  - 当前枚举以 `asFUNC_DELEGATE = 6` 结尾，需在其后追加新值
  - 这是纯声明变更，不影响任何运行时逻辑
  - 使用 `//[UE++]` 注释标注此修改来自 2.38 函数模板回移
- [ ] **P1.2** 在 `asIScriptFunction` 接口中添加函数模板查询 API
  - 添加三个纯虚方法：`GetSubTypeCount()`、`GetSubTypeId(asUINT)`、`GetSubType(asUINT)`
  - 参考 2.38 中 `angelscript.h:1183-1186` 的声明
  - 同时在 `as_scriptfunction.h` / `as_scriptfunction.cpp` 中添加对应实现，从 `templateSubTypes` 数组读取
- [ ] **P1.3** 在 `as_scriptengine.h` 中添加函数模板所需的成员和方法声明
  - 添加 `registeredTemplateGlobalFuncs`（`asCArray<asCScriptFunction*>`）：跟踪注册的全局模板基函数
  - 添加 `generatedTemplateFunctionInstances`（`asCArray<asCScriptFunction*>`）：跟踪已实例化的函数模板实例
  - 添加方法声明：`GetTemplateSubTypeByName`、`GetTemplateFunctionInstance`、`IsTemplateFn`
  - 将现有 `templateSubTypes` 数组重命名为 `registeredTemplateSubTypes` 以与 2.38 对齐（需同步更新引用处）
- [ ] **P1.4** 在 `as_parser.h` 中声明 `ParseTemplateDeclTypeList` 方法
  - 签名：`bool ParseTemplateDeclTypeList(asCScriptNode* node, bool required)`
  - 仅声明，实现在 Phase 2
- [ ] **P1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add function template data structures and API declarations`

### Phase 2 — 注册与解析

> 目标：实现函数模板的 C++ 注册路径和声明解析，使得宿主可以用 `RegisterGlobalFunction` / `RegisterObjectMethod` + generic 调用约定注册模板函数，引擎正确识别并存储其模板形参信息。

- [ ] **P2.1** 实现 `GetTemplateSubTypeByName` 方法
  - 从 `RegisterObjectType` 中的内联查找逻辑抽取为独立方法
  - 在 `registeredTemplateSubTypes` 中按名查找，找不到时创建新的 `asOBJ_TEMPLATE_SUBTYPE` 类型并追加
  - 参考 2.38 `as_scriptengine.cpp:1790-1814`
  - 同时将 `RegisterObjectType` 中的内联查找替换为调用此方法
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Refactor: extract GetTemplateSubTypeByName from inline lookup`
- [ ] **P2.2** 实现 `as_parser.cpp` 中的 `ParseTemplateDeclTypeList`
  - 直接从 2.38 `as_parser.cpp:330-407` 迁入，解析 `< [class] IDENT [, [class] IDENT]* >` 格式的模板形参列表
  - 处理 `>>` / `>>>` token 拆分（与模板类型解析共用的已有逻辑一致）
  - 修改 `ParseFunctionDefinition`：在 `ParseIdentifier()` 之后、`ParseParameterList()` 之前，插入 `ParseTemplateDeclTypeList(node, false)` 调用
  - 验证：现有不使用函数模板的脚本解析不受影响（`ParseTemplateDeclTypeList` 在非 required 模式下遇到非 `<` token 会直接返回 false）
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: parse template type list in function definitions`
- [ ] **P2.3** 修改 `as_builder.cpp` 的 `ParseFunctionDeclaration` 以填充 `templateSubTypes`
  - 在解析函数名后，若存在后续子节点且不是参数列表，按名调用 `GetTemplateSubTypeByName` 获取占位类型
  - 将获取到的 subtype 存入 `func->templateSubTypes`
  - 参考 2.38 `as_builder.cpp:1351-1365`
- [ ] **P2.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: populate templateSubTypes in ParseFunctionDeclaration`
- [ ] **P2.4** 修改 `RegisterGlobalFunction` 和 `RegisterObjectMethod` 中的模板函数识别
  - 在函数声明解析完成后，若 `func->templateSubTypes` 非空：
    - 设置 `func->funcType = asFUNC_TEMPLATE`
    - 校验调用约定必须为 `asCALL_GENERIC`，否则返回 `asNOT_SUPPORTED`
  - 对全局函数，将基函数追加到 `registeredTemplateGlobalFuncs`
  - 参考 2.38 `as_scriptengine.cpp:2977-2984`（ObjectMethod）和对应的 GlobalFunction 路径
- [ ] **P2.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: recognize template functions in RegisterGlobalFunction and RegisterObjectMethod`
- [ ] **P2.5** 编写注册路径验证测试
  - Phase 2 完成后，C++ 宿主已经可以通过 `RegisterGlobalFunction` / `RegisterObjectMethod` + `asCALL_GENERIC` 注册模板函数，引擎能正确识别并设置 `asFUNC_TEMPLATE`；此时脚本侧编译调用尚未接通，但注册侧行为已可独立验证
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptFunctionTemplateRegistrationTests.cpp`，遵循 Native Core 层规则（只用 `AngelscriptInclude.h` / `angelscript.h` 公共 API）
  - 使用 `CreateNativeEngine()` 创建独立原生引擎，不依赖 `FAngelscriptEngine`
  - 测试用例清单：
    - **RegisterSingleParam**：注册 `void TestFunc<T>(int)` 形式的单形参全局模板函数，调用约定为 `asCALL_GENERIC`，断言 `RegisterGlobalFunction` 返回值 >= 0
    - **RegisterMultiParam**：注册 `void TestFunc<T,U>(int)` 形式的双形参全局模板函数，断言注册成功
    - **NonGenericRejected**：用 `asCALL_CDECL` 注册模板函数，断言返回 `asNOT_SUPPORTED`（模板函数必须 generic 约定）
    - **RegisterObjectMethodTemplate**：先注册一个值类型，再用 `RegisterObjectMethod` 注册该类型上的模板方法，断言注册成功
    - **SubTypeQueryAPI**：注册成功后，通过 `asIScriptFunction` 的 `GetSubTypeCount()` / `GetSubType()` / `GetSubTypeId()` 查询模板形参信息，断言单形参函数返回 1、双形参函数返回 2，子类型名正确
  - 参考现有 `AngelscriptNativeRegistrationTests.cpp` 的 `RegisterGlobalFunction` / `RegisterSimpleValueType` 测试模式
- [ ] **P2.5** 📦 Git 提交：`[ThirdParty/AS238] Test: add function template registration verification tests`

### Phase 3 — 编译期实例化与调用

> 目标：脚本中 `name<Type>(args)` 语法能正确编译，编译器为每组具体类型参数生成唯一的函数实例，调用链路走通。

- [ ] **P3.1** 实现 `GetTemplateFunctionInstance`
  - 在 `as_scriptengine.cpp` 中实现，根据基函数和具体类型数组查找或创建实例化函数
  - 复用已有的 `DetermineTypeForTemplate` 做返回类型和参数类型替换
  - 实例化函数的 `funcType` 设为 `asFUNC_SYSTEM`（与模板类型方法实例化一致）
  - 将实例加入 `generatedTemplateFunctionInstances` 跟踪
  - 参考 2.38 `as_scriptengine.cpp:3187-3249`
- [ ] **P3.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement GetTemplateFunctionInstance for function template instantiation`
- [ ] **P3.2** 实现 `InstantiateTemplateFunctions`
  - 在 `as_compiler.cpp` 中实现，遍历候选函数数组，对带 `templateSubTypes` 的函数从 AST 中读取具体类型列表，调用 `GetTemplateFunctionInstance` 获取实例化函数 id 并替换
  - 参考 2.38 `as_compiler.cpp:12702-12738`
- [ ] **P3.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement InstantiateTemplateFunctions in compiler`
- [ ] **P3.3** 在 `CompileFunctionCall` 的全局和成员函数调用路径中接入实例化
  - 需要在 `CompileFunctionCall` 中识别调用节点上的类型列表子节点
  - 在匹配候选函数后、最终调用前，对候选列表调用 `InstantiateTemplateFunctions`
  - 全局函数调用和对象方法调用两条路径都需要处理
  - 参考 2.38 `as_compiler.cpp:12880-12882`（成员）和 `12938-12941`（全局）
  - 同时需要实现 `IsTemplateFn` 辅助方法供函数查找时使用
- [ ] **P3.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: integrate template function instantiation into CompileFunctionCall`
- [ ] **P3.4** 处理引擎清理路径
  - 在 `asCScriptEngine::ShutDownAndRelease` / 析构路径中释放 `generatedTemplateFunctionInstances` 中的所有函数实例
  - 参考 2.38 `as_scriptengine.cpp:910-916`
- [ ] **P3.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: clean up generated template function instances on engine shutdown`
- [ ] **P3.5** 编写编译与执行验证测试
  - Phase 3 完成后，脚本侧 `name<Type>(args)` 语法已经可以正确编译和调用；这是函数模板的核心能力，需要在序列化之前用独立测试锁定正确性基线
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptFunctionTemplateExecutionTests.cpp`，遵循 Native Core 层规则
  - 每个测试先用 `RegisterGlobalFunction` 注册一个 generic 回调模板函数，再编译包含模板调用的脚本，执行并验证返回值
  - generic 回调内通过 `asIScriptGeneric` 读取实参、写入返回值，模拟类型感知的 C++ 宿主逻辑
  - 测试用例清单：
    - **SingleParamInt**：注册 `T Identity<T>(T)`，脚本 `int r = Identity<int>(42); return r;`，断言返回 42
    - **SingleParamFloat**：同上模板，脚本 `float r = Identity<float>(1.5f); return r;`，断言返回 1.5
    - **MultiParam**：注册 `T Convert<T,U>(U)`，脚本 `int r = Convert<int, float>(3.14f); return r;`，断言返回 3（截断）
    - **MultipleInstantiationsInOneScript**：在同一个脚本模块中同时调用 `Identity<int>(10)` 和 `Identity<float>(2.5f)`，断言两个返回值都正确；验证引擎正确地为不同类型参数分别实例化
    - **ObjectMethodTemplate**：注册值类型 + 方法模板 `T GetAs<T>()`，脚本中在对象实例上调用 `obj.GetAs<int>()`，断言返回值正确
    - **CompileError_MissingTemplateArgs**：脚本调用 `Identity(42)` 不带 `<int>`，断言 `Module->Build()` 返回编译失败（返回值 < 0）
    - **CompileError_WrongSubTypeCount**：脚本调用 `Identity<int, float>(42)` 对只有单形参的模板，断言编译失败
    - **SameInstantiationReused**：在同一脚本中多次调用 `Identity<int>(1)` 和 `Identity<int>(2)`，断言引擎只生成一个实例化函数（可通过 `generatedTemplateFunctionInstances` 大小间接验证，或通过两次调用都成功来验证行为正确性）
  - 参考 2.38 测试 `sdk/tests/test_feature/source/test_template.cpp:227-566`
  - 参考现有 `AngelscriptNativeExecutionTests.cpp` 的 `CreateEngineAndBuildModule` + `PrepareAndExecute` 模式
- [ ] **P3.5** 📦 Git 提交：`[ThirdParty/AS238] Test: add function template compilation and execution tests`

### Phase 4 — 序列化与测试

> 目标：Bytecode 保存/加载正确处理模板函数；编写测试覆盖注册、调用、多形参、错误路径。

- [ ] **P4.1** 在 `as_restore.cpp` 中添加模板函数的序列化/反序列化支持
  - 保存时需要额外写入函数的 `templateSubTypes` 信息
  - 加载时需要读取 `templateSubTypes` 并通过 `GetTemplateFunctionInstance` 恢复实例化函数
  - 2.38 中 `as_restore.cpp` 有约 29 处 `templateSubTypes` 引用，但当前 2.33 已有 27 处（用于模板类型），需逐一确认哪些是函数模板新增的
  - 重点关注 2.38 changelog 中提到的 "Fixed saving bytecode with template functions" 对应的改动
- [ ] **P4.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: serialize and deserialize template functions in bytecode`
- [ ] **P4.2** 补充注册路径边界测试与错误路径覆盖
  - P2.5 已建立注册路径的正例基线，此步补充 P2.5 未覆盖的边界和错误路径，统一写入 `AngelscriptFunctionTemplateRegistrationTests.cpp`
  - 新增用例：
    - **RegisterDuplicateTemplate**：对同一签名注册两次模板函数，断言第二次注册不会创建重复条目（返回值行为与普通函数重复注册一致）
    - **RegisterTemplateWithVoidReturn**：注册 `void Process<T>(T)` 形式的无返回值模板函数，断言注册成功；验证返回类型为 void 不影响模板参数解析
    - **RegisterTemplateEmptySubTypes**：注册声明中不含 `<T>` 的普通函数（如 `int Normal(int)`），断言函数类型不是 `asFUNC_TEMPLATE`，`GetSubTypeCount()` 返回 0；确认非模板函数不受影响
    - **InvalidSubTypeName**：尝试注册含非法子类型名（如数字开头 `<1T>`）的函数声明，断言注册失败
  - 目标文件行数控制在 300 行以内
- [ ] **P4.2** 📦 Git 提交：`[ThirdParty/AS238] Test: add function template registration edge case and error path tests`
- [ ] **P4.3** 补充编译执行边界测试
  - P3.5 已建立编译调用的正例基线，此步补充脚本侧边界场景，统一写入 `AngelscriptFunctionTemplateExecutionTests.cpp`
  - 新增用例：
    - **TemplateFunctionInExpression**：在表达式上下文中调用模板函数，如 `int r = Identity<int>(10) + Identity<int>(20);`，断言 r == 30；验证模板调用可参与算术运算
    - **TemplateFunctionAsArgument**：模板调用结果作为另一个函数的实参，如 `int r = Identity<int>(Identity<int>(42));`，断言 r == 42；验证嵌套调用
    - **TemplateFunctionWithUserType**：注册一个值类型 `MyVal`，再注册 `T Get<T>()`，脚本中 `MyVal v = Get<MyVal>();`，断言执行成功；验证用户自定义类型可作为模板实参
    - **CompileError_UnregisteredTypeArg**：脚本调用 `Identity<UnknownType>(42)`，断言编译失败；验证未注册类型不能作为模板实参
    - **CompileError_TemplateCallOnNonTemplate**：对普通（非模板）函数使用 `<int>` 语法调用，如 `NormalFunc<int>(42)`，断言编译失败
  - 目标文件行数控制在 400 行以内
- [ ] **P4.3** 📦 Git 提交：`[ThirdParty/AS238] Test: add function template compilation edge case tests`
- [ ] **P4.4** 编写序列化测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptFunctionTemplateSerializationTests.cpp`，验证 bytecode Save/Load 后模板函数调用行为不变
  - 使用 `AngelscriptTestAdapter.h` 中的 `FASSDKBytecodeStream` 做内存级 Save/Load，不依赖文件系统
  - 测试流程：注册模板函数 → 编译脚本 → 执行一次确认基线 → SaveByteCode → DiscardModule → LoadByteCode → 再次执行 → 断言结果与基线一致
  - 测试用例清单：
    - **SaveLoadSingleInstantiation**：脚本包含 `Identity<int>(42)` 的单次模板调用，Save/Load 后执行返回值不变
    - **SaveLoadMultipleInstantiations**：脚本包含 `Identity<int>(10)` 和 `Identity<float>(2.5f)` 两种实例化，Save/Load 后两个返回值都正确
    - **SaveLoadMultiParamTemplate**：脚本包含 `Convert<int, float>(3.14f)` 的多形参模板调用，Save/Load 后返回值不变
    - **SaveLoadObjectMethodTemplate**：对象方法模板调用在 Save/Load 后仍能正确分发到 generic 回调
  - 参考 2.38 测试 `test_template.cpp:279-348` 的 Save/Load 段落
  - 参考现有 `AngelscriptTestAdapter.h` 中 `FASSDKBytecodeStream` 的 Write/Read/Restart 模式
  - 目标文件行数控制在 300 行以内
- [ ] **P4.4** 📦 Git 提交：`[ThirdParty/AS238] Test: add function template bytecode serialization tests`
- [ ] **P4.5** 更新 `AngelscriptChange.md`
  - 记录所有第三方代码修改的 `[UE++]` 位置和原因
  - 如果文件不存在则创建
- [ ] **P4.5** 📦 Git 提交：`[Docs] Docs: document function template backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `Core/angelscript.h` | 修改 | 添加 `asFUNC_TEMPLATE` 枚举、`asIScriptFunction` 模板查询 API |
| `ThirdParty/.../as_scriptengine.h` | 修改 | 添加字段和方法声明，重命名 `templateSubTypes` |
| `ThirdParty/.../as_scriptengine.cpp` | 修改 | 实现 `GetTemplateSubTypeByName`、`GetTemplateFunctionInstance`、`IsTemplateFn`、注册路径修改、清理路径 |
| `ThirdParty/.../as_parser.h` | 修改 | 添加 `ParseTemplateDeclTypeList` 声明 |
| `ThirdParty/.../as_parser.cpp` | 修改 | 实现 `ParseTemplateDeclTypeList`、修改 `ParseFunctionDefinition` |
| `ThirdParty/.../as_builder.cpp` | 修改 | 修改 `ParseFunctionDeclaration` 以处理函数模板形参 |
| `ThirdParty/.../as_compiler.h` | 修改 | 添加 `InstantiateTemplateFunctions` 声明 |
| `ThirdParty/.../as_compiler.cpp` | 修改 | 实现 `InstantiateTemplateFunctions`、修改 `CompileFunctionCall` |
| `ThirdParty/.../as_scriptfunction.h` | 修改 | 添加 `GetSubTypeCount`/`GetSubTypeId`/`GetSubType` 声明 |
| `ThirdParty/.../as_scriptfunction.cpp` | 修改 | 实现上述方法 |
| `ThirdParty/.../as_restore.cpp` | 修改 | 模板函数的序列化/反序列化 |
| `AngelscriptTest/Native/AngelscriptFunctionTemplateRegistrationTests.cpp` | 新增 | 注册路径验证（P2.5）+ 边界补充（P4.2） |
| `AngelscriptTest/Native/AngelscriptFunctionTemplateExecutionTests.cpp` | 新增 | 编译执行验证（P3.5）+ 边界补充（P4.3） |
| `AngelscriptTest/Native/AngelscriptFunctionTemplateSerializationTests.cpp` | 新增 | Bytecode Save/Load 序列化测试（P4.4） |

## 验收标准

1. 所有现有测试（201/223）仍然通过，不引入回归
2. C++ 宿主可通过 `RegisterGlobalFunction("T Test<T>(T)", ..., asCALL_GENERIC)` 注册模板函数
3. C++ 宿主可通过 `RegisterObjectMethod` 注册对象方法模板函数
4. 脚本中 `Test<int>(42)` 可正确编译并调用到 generic 实现
5. 多形参模板函数 `Get<int, float>(1.5f)` 可正确编译调用
6. 非 generic 约定注册模板函数时返回明确错误
7. 脚本中未指定模板子类型时编译器给出明确错误
8. Bytecode Save/Load 后模板函数调用仍然正确
9. 所有第三方代码修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记
10. 注册测试（`AngelscriptFunctionTemplateRegistrationTests.cpp`）全部通过，覆盖正例、非 generic 拒绝、子类型查询 API、边界情况
11. 编译执行测试（`AngelscriptFunctionTemplateExecutionTests.cpp`）全部通过，覆盖单/多形参实例化、对象方法模板、表达式上下文、嵌套调用、编译错误路径
12. 序列化测试（`AngelscriptFunctionTemplateSerializationTests.cpp`）全部通过，覆盖单/多实例化和对象方法模板的 Save/Load 往返

## 风险与注意事项

1. **上下文漂移**：2.33 和 2.38 之间同文件可能有 5 年间积累的非模板改动（变量名、函数签名微调、新增分支等），不能直接 copy-paste 2.38 代码，必须逐段理解后适配
2. **引用计数**：2.38 changelog 中提到过 "Fixed incorrect reference count management in the library for template functions"——需要确保回移时包含此修复
3. **`DetermineTypeForTemplate` 签名差异**：2.38 中此函数的参数列表可能与 2.33 不同，需确认并适配
4. **`as_restore.cpp` 复杂度**：序列化模块可能是最难准确迁移的部分，因为 2.33 和 2.38 的 bytecode 格式可能有其他差异；建议此部分最后做，且依赖充分的测试验证
5. **UE 绑定层影响**：函数模板是 C++ 注册侧能力，UE 绑定层（`Binds/`）短期内不需要使用此特性，但长期可能用于优化泛型辅助函数的注册方式
