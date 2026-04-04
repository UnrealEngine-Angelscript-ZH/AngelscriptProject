# 上游 AngelScript 官方语言核心测试集成计划

> **当前状态（2026-04-04 复核）**：本计划并非未开始，仓库中已实际落地大部分实现。
> - 已完成：`P0.1-P5.3` 中除 `P3.2` 外的实现项均已落地。
> - 已关闭：`P3.2` 因当前 fork 不支持 vanilla AngelScript `@` handle 语法，不纳入当前交付范围，依据见 `Documents/Guides/ASSDK_Fork_Differences.md`。
> - 待完成：`P6.1-P6.3`；`P7.1` 仅有通用 Native 文档基础、尚未达到 ASSDK 专项文档要求；`P7.2` 尚未补 coverage matrix。
> - 提交对位：大多数实现已有对位 `[Test/Native]` 提交；少数提交 message 与计划草案字面不同，但文件与测试入口已实际落地。
> - **归档判断**：当前**不满足归档条件**，仍缺 Phase 6 与 Phase 7 的收口工作。

## 背景与目标

### 背景

`Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/` 包含 158 个官方测试文件。其中大部分是验证 AS 语言核心能力的纯引擎测试（编译、执行、类型系统、OOP、调用约定、模块管理、GC 等），不依赖任何 add-on。

当前仓库 `Native/` 层只有 17 个自研测试，覆盖基础编译/执行/注册/诊断。直接复用上游的语言核心测试可以用最低成本大幅提升覆盖。

### 上游测试框架特征

- 每个测试是 `bool TestFoo()` 或 `namespace TestFoo { bool Test(); }`，返回 `true` 表示失败
- `utils.h` 提供 `COutStream`、`CBufferedOutStream`（message callback）、`CBytecodeStream`、`TEST_FAILED` 宏、`Assert()` 全局函数
- 部分测试使用 `ExecuteString()`（来自 `scripthelper` add-on），本计划中这类测试等价改写为 `BuildModule` + `Execute` 模式，不引入 add-on

### 目标

1. 在 `Native/` 层建立上游测试适配基础设施，把上游 `bool TestFoo()` 模式桥接到 UE Automation
2. 集成约 100 个纯语言核心测试，不引入 `scriptarray` / `scriptstdstring` / `scripthelper` 等 add-on
3. 对使用 `ExecuteString()` 的核心测试，改写为标准 `BuildModule` + `Execute` 路径

## 范围与边界

- **纳入**：`test_feature/` 下不依赖 add-on 的语言核心测试（引擎、执行、类型、运算符、OOP、函数、调用约定、编译器、模块、运行时、GC、序列化）
- **不纳入**：
  - `test_addon_*.cpp`（16 个）— add-on 功能测试
  - `test_array*.cpp`、`test_dict.cpp` — 依赖 `scriptarray`
  - `test_scriptstring.cpp`、`test_returnstring.cpp`、`teststdstring.cpp`、`test_cstring.cpp`、`testbstr.cpp` — 依赖 `scriptstdstring` / `scriptstring`
  - `test_scriptmath.cpp`、`test_vector3.cpp`、`test_vector3_2.cpp`、`test_stdvector.cpp` — 依赖自定义数学 add-on
  - `testexecutestring.cpp` — 专门测试 `ExecuteString()` 本身
  - `test_performance/`、`test_multithread/`、`test_build_performance/` — 性能/多线程
- **不修改** `Reference/` 目录下的上游源码
- 保持 `Native/` 不依赖 `FAngelscriptEngine`

## 当前事实状态

### 纳入范围的上游测试清单（约 105 个文件）

按主题分组：

**引擎与执行（~15）**

`testcreateengine.cpp`、`testexecute.cpp`、`testexecute1arg.cpp`、`testexecute2args.cpp`、`testexecute4args.cpp`、`testexecute4argsf.cpp`、`testexecute32args.cpp`、`testexecute32mixedargs.cpp`、`testexecutemixedargs.cpp`、`testexecutethis32mixedargs.cpp`、`testexecutescript.cpp`、`testglobalvar.cpp`、`testenumglobvar.cpp`、`teststack.cpp`、`test_stack2.cpp`

**类型系统（~8）**

`test_bool.cpp`、`test_bits.cpp`、`test_int8.cpp`、`testint64.cpp`、`test_float.cpp`、`test_enum.cpp`、`test_typedef.cpp`、`test_auto.cpp`

**运算符与控制流（~7）**

`test_operator.cpp`、`test_pow.cpp`、`test_assign.cpp`、`test_multiassign.cpp`、`testnegateoperator.cpp`、`test_condition.cpp`、`test_for.cpp`

**类型转换（~3）**

`test_castop.cpp`、`test_conversion.cpp`、`test_implicitcast.cpp`

**对象模型（~7）**

`test_object.cpp`、`test_object2.cpp`、`test_object3.cpp`、`test_constructor.cpp`、`test_constructor2.cpp`、`test_destructor.cpp`、`test_factory.cpp`

**句柄（~5）**

`test_objhandle.cpp`、`test_objhandle2.cpp`、`test_autohandle.cpp`、`test_implicithandle.cpp`、`test_objzerosize.cpp`

**OOP（~6）**

`test_inheritance.cpp`、`test_interface.cpp`、`test_mixin.cpp`、`testmultipleinheritance.cpp`、`testvirtualinheritance.cpp`、`testvirtualmethod.cpp`

**函数（~8）**

`test_funcoverload.cpp`、`test_2func.cpp`、`test_defaultarg.cpp`、`test_namedargs.cpp`、`test_refargument.cpp`、`test_argref.cpp`、`test_refcast.cpp`、`test_unsaferef.cpp`

**调用约定（~15）**

`testcdecl_class.cpp`、`testcdecl_class_a.cpp`、`testcdecl_class_c.cpp`、`testcdecl_class_d.cpp`、`testcdecl_class_k.cpp`、`test_cdecl_objlast.cpp`、`test_cdecl_return.cpp`、`test_thiscall_as_method.cpp`、`test_thiscall_as_method_config.cpp`、`test_thiscall_asglobal.cpp`、`test_thiscall_class.cpp`、`testnotcomplexstdcall.cpp`、`testnotcomplexthiscall.cpp`、`teststdcall4args.cpp`、`test_generic.cpp`、`test_getargptr.cpp`

**编译器与配置（~9）**

`test_compiler.cpp`、`test_parser.cpp`、`test_optimize.cpp`、`test_nevervisited.cpp`、`test_notinitialized.cpp`、`test_config.cpp`、`test_configaccess.cpp`、`test_dynamicconfig.cpp`、`test_registertype.cpp`

**模块管理（~7）**

`test_module.cpp`、`test_discard.cpp`、`test2modules.cpp`、`test_import.cpp`、`test_import2.cpp`、`test_circularimport.cpp`、`testmoduleref.cpp`

**运行时（~8）**

`test_context.cpp`、`test_suspend.cpp`、`test_exception.cpp`、`test_exceptionmemory.cpp`、`test_garbagecollect.cpp`、`test_saveload.cpp`、`test_stream.cpp`、`test_debug.cpp`

**杂项语言特性（~12）**

`test_getset.cpp`、`test_constobject.cpp`、`test_constproperty.cpp`、`test_nested.cpp`、`test_composition.cpp`、`test_shared.cpp`、`test_namespace.cpp`、`test_singleton.cpp`、`test_vartype.cpp`、`test_any.cpp`、`test_native_defaultfunc.cpp`、`test_custommem.cpp`

**回归修复与其他（~8）**

`test_rz.cpp`、`test_shark.cpp`、`test_propintegerdivision.cpp`、`test_pointer.cpp`、`test_dump.cpp`、`test_postprocess.cpp`、`testswitch.cpp`、`testtempvar.cpp`、`testlongtoken.cpp`、`testoutput.cpp`

**2.38 新特性（~4，版本标记）**

`test_foreach.cpp`、`test_template.cpp`、`test_functionptr.cpp`、`test_scriptstruct.cpp`

### 上游 `utils.h` 需适配的核心 API

| 上游工具 | 功能 | 适配策略 |
|----------|------|----------|
| `COutStream` | message callback 打印到 stdout | 已有 `FNativeMessageCollector`，可复用 |
| `CBufferedOutStream` | 缓存 message 到 `std::string buffer` | 新增等价实现，用于断言编译错误消息 |
| `CBytecodeStream` | 内存 bytecode save/load | 新增，用于 save/load 测试 |
| `TEST_FAILED` | `fail = true` + 打印行号 | 映射到 `AddError()` |
| `Assert()` | 注册给脚本的全局函数 | 新增注册 helper |
| `ExecuteString()` | 单行脚本快速执行 | 改写为 `BuildModule` + `Execute`，不引入 scripthelper |

### `ExecuteString()` 替代策略

上游约 60% 的测试使用 `ExecuteString(engine, "someCode()")` 快速执行一行脚本。本计划不引入 `scripthelper` add-on，而是提供一个等价的纯引擎实现：

```cpp
inline int ExecuteString(asIScriptEngine* engine, const char* code)
{
    asIScriptModule* mod = engine->GetModule("_exec_", asGM_ALWAYS_CREATE);
    mod->AddScriptSection("exec", code);
    // 包装成 void _exec_() { <code> } 后编译执行
    ...
}
```

这个 helper 的行为等价于上游 `scripthelper` 中的 `ExecuteString()`，但只依赖公共 API。

## 分阶段执行计划

### Phase 0：建立上游测试适配层

> 目标：提供桥接上游测试模式到 UE Automation 的基础设施，使后续集成只需"包装+调用"。

- [x] **P0.1** 创建 `Native/AngelscriptTestAdapter.h`
  - 上游测试用 `bool` 返回（`true` = 失败），UE Automation 用 `TestTrue()` / `TestEqual()`；需要桥接层把上游断言模式映射到 UE
  - 提供 `FASSDKBufferedOutStream`（缓存消息到 `std::string buffer`，等价于上游 `CBufferedOutStream`）、`FASSDKBytecodeStream`（内存 save/load，等价于上游 `CBytecodeStream`）、`ASSDK_TEST_FAILED` 宏（映射到 `AddError()`）
  - 提供 `RegisterASSDKAssert(asIScriptEngine*)` 注册脚本侧 `Assert()` 全局函数
  - 提供 `ASSDKExecuteString(asIScriptEngine*, const char*)` 作为不依赖 scripthelper 的 `ExecuteString()` 替代
  - 提供 `CreateASSDKTestEngine(FASSDKBufferedOutStream*)` 封装创建引擎 + 注册 callback + 注册 Assert
  - 不依赖 `FAngelscriptEngine`，只依赖 `AngelscriptInclude.h`
- [x] **P0.1** 📦 Git 提交：`[Test/Native] Feat: add AS SDK test adapter infrastructure`

- [x] **P0.2** 创建 `Native/AngelscriptASSDKSmokeTest.cpp`
  - 用适配层创建引擎，编译含 `Assert(1 == 1)` 的脚本并执行，验证 `ASSDKExecuteString` 可用，验证 `FASSDKBufferedOutStream` 可缓存消息
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Smoke`
- [x] **P0.2** 📦 Git 提交：`[Test/Native] Feat: add AS SDK adapter smoke test`

### Phase 1：集成引擎与执行核心测试

> 目标：集成最基础的引擎创建、参数传递、执行、全局变量、栈管理测试。

- [x] **P1.1** 创建 `Native/AngelscriptASSDKEngineTests.cpp`
  - 包装 `testcreateengine.cpp`（引擎创建/多引擎/message callback 复用），这是 AS 引擎最底层的健康检查
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Engine.Create`
- [x] **P1.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK engine creation tests`

- [x] **P1.2** 创建 `Native/AngelscriptASSDKExecuteTests.cpp`
  - 包装 `testexecute.cpp`、`testexecute1arg.cpp`、`testexecute2args.cpp`、`testexecute4args.cpp`、`testexecute4argsf.cpp`、`testexecute32args.cpp`、`testexecute32mixedargs.cpp`、`testexecutemixedargs.cpp`、`testexecutethis32mixedargs.cpp`、`testexecutescript.cpp` 中的核心路径
  - 这 10 个文件覆盖 1/2/4/32/mixed args、float args、this 调用等，是 ABI 正确性的核心回归
  - 对其中用到 `ExecuteString()` 的地方使用 `ASSDKExecuteString` 替代
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Execute.*`
- [x] **P1.2** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK execution and argument tests`

- [x] **P1.3** 创建 `Native/AngelscriptASSDKGlobalVarTests.cpp`
  - 包装 `testglobalvar.cpp`、`testenumglobvar.cpp`、`teststack.cpp`、`test_stack2.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.GlobalVar.*`
- [x] **P1.3** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK global variable and stack tests`

### Phase 2：集成类型系统与运算符测试

> 目标：覆盖基本类型、运算符、控制流、类型转换。

- [x] **P2.1** 创建 `Native/AngelscriptASSDKTypeTests.cpp`
  - 包装 `test_bool.cpp`、`test_bits.cpp`、`test_int8.cpp`、`testint64.cpp`、`test_float.cpp`、`test_enum.cpp`、`test_typedef.cpp`、`test_auto.cpp`
  - 覆盖布尔/位运算/整型边界/浮点精度/枚举/typedef/auto 推导
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Type.*`
- [x] **P2.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK type system tests`

- [x] **P2.2** 创建 `Native/AngelscriptASSDKOperatorTests.cpp`
  - 包装 `test_operator.cpp`、`test_pow.cpp`、`test_assign.cpp`、`test_multiassign.cpp`、`testnegateoperator.cpp`、`test_condition.cpp`、`test_for.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Operator.*`
- [x] **P2.2** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK operator and control-flow tests`

- [x] **P2.3** 创建 `Native/AngelscriptASSDKConversionTests.cpp`
  - 包装 `test_castop.cpp`、`test_conversion.cpp`、`test_implicitcast.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Conversion.*`
- [x] **P2.3** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK type conversion tests`

### Phase 3：集成对象模型与 OOP 测试

> 目标：覆盖构造/析构、对象、句柄、继承、接口。

- [x] **P3.1** 创建 `Native/AngelscriptASSDKObjectTests.cpp`
  - 包装 `test_object.cpp`、`test_object2.cpp`、`test_object3.cpp`、`test_constructor.cpp`、`test_constructor2.cpp`、`test_destructor.cpp`、`test_factory.cpp`
  - 覆盖值类型/引用类型构造、析构、工厂函数的对象生命周期
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Object.*`
- [x] **P3.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK object model tests`

- [x] **P3.2** 关闭 `Native/AngelscriptASSDKHandleTests.cpp` 迁移项
  - 上游 `test_objhandle.cpp`、`test_objhandle2.cpp`、`test_autohandle.cpp`、`test_implicithandle.cpp`、`test_objzerosize.cpp` 依赖 vanilla AngelScript `@` handle / auto-handle 语义。
  - 当前 fork 的对象/handle 语义不兼容，见 `Documents/Guides/ASSDK_Fork_Differences.md` 中的“Handle/指针语法差异”章节。
  - 当前计划按“显式关闭”处理；若未来 fork 恢复该语义兼容，再单独重开子计划或新增 Phase。
- [x] **P3.2** 📦 Git 提交：不适用（关闭项，无单独实现提交）

- [x] **P3.3** 创建 `Native/AngelscriptASSDKOOPTests.cpp`
  - 包装 `test_inheritance.cpp`、`test_interface.cpp`、`test_mixin.cpp`、`testmultipleinheritance.cpp`、`testvirtualinheritance.cpp`、`testvirtualmethod.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.OOP.*`
- [x] **P3.3** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK inheritance and interface tests`

### Phase 4：集成函数与调用约定测试

> 目标：覆盖函数重载、默认参数、引用参数、cdecl/thiscall/stdcall/generic 调用约定。

- [x] **P4.1** 创建 `Native/AngelscriptASSDKFunctionTests.cpp`
  - 包装 `test_funcoverload.cpp`、`test_2func.cpp`、`test_defaultarg.cpp`、`test_namedargs.cpp`、`test_refargument.cpp`、`test_argref.cpp`、`test_refcast.cpp`、`test_unsaferef.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Function.*`
- [x] **P4.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK function tests`

- [x] **P4.2** 创建 `Native/AngelscriptASSDKCallingConvTests.cpp`
  - 包装 `testcdecl_class.cpp` 系列（5 个）、`test_cdecl_objlast.cpp`、`test_cdecl_return.cpp`、`test_thiscall_*.cpp`（4 个）、`testnotcomplexstdcall.cpp`、`testnotcomplexthiscall.cpp`、`teststdcall4args.cpp`、`test_generic.cpp`、`test_getargptr.cpp`
  - 覆盖 cdecl / thiscall / stdcall / generic 调用约定的 ABI 正确性
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.CallingConv.*`
- [x] **P4.2** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK calling convention tests`

### Phase 5：集成编译器、模块与运行时测试

> 目标：覆盖编译器行为、模块管理、上下文、异常、GC、序列化。

- [x] **P5.1** 创建 `Native/AngelscriptASSDKCompilerTests.cpp`
  - 包装 `test_compiler.cpp`、`test_parser.cpp`、`test_optimize.cpp`、`test_nevervisited.cpp`、`test_notinitialized.cpp`、`test_config.cpp`、`test_configaccess.cpp`、`test_dynamicconfig.cpp`、`test_registertype.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Compiler.*`
- [x] **P5.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK compiler and config tests`

- [x] **P5.2** 创建 `Native/AngelscriptASSDKModuleTests.cpp`
  - 包装 `test_module.cpp`、`test_discard.cpp`、`test2modules.cpp`、`test_import.cpp`、`test_import2.cpp`、`test_circularimport.cpp`、`testmoduleref.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Module.*`
- [x] **P5.2** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK module management tests`

- [x] **P5.3** 创建 `Native/AngelscriptASSDKRuntimeTests.cpp`
  - 包装 `test_context.cpp`、`test_suspend.cpp`、`test_exception.cpp`、`test_exceptionmemory.cpp`、`test_garbagecollect.cpp`、`test_saveload.cpp`、`test_stream.cpp`、`test_debug.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Runtime.*`
- [x] **P5.3** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK runtime and GC tests`

### Phase 6：集成杂项语言特性与 2.38 版本标记测试

> 目标：覆盖属性、const、命名空间、共享实体等杂项语言特性，以及 2.38 新特性（版本标记）。

- [ ] **P6.1** 创建 `Native/AngelscriptASSDKLangMiscTests.cpp`
  - 包装 `test_getset.cpp`、`test_constobject.cpp`、`test_constproperty.cpp`、`test_nested.cpp`、`test_composition.cpp`、`test_shared.cpp`、`test_namespace.cpp`、`test_singleton.cpp`、`test_vartype.cpp`、`test_any.cpp`、`test_native_defaultfunc.cpp`、`test_custommem.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.LangMisc.*`
- [ ] **P6.1** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK language miscellaneous tests`

- [ ] **P6.2** 创建 `Native/AngelscriptASSDKRegressionTests.cpp`
  - 包装 `test_rz.cpp`、`test_shark.cpp`、`test_propintegerdivision.cpp`、`test_pointer.cpp`、`test_dump.cpp`、`test_postprocess.cpp`、`testswitch.cpp`、`testtempvar.cpp`、`testlongtoken.cpp`、`testoutput.cpp`
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.Regression.*`
- [ ] **P6.2** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK regression fix tests`

- [ ] **P6.3** 创建 `Native/AngelscriptASSDKAS238FeatureTests.cpp`
  - 包装 `test_foreach.cpp`、`test_template.cpp`、`test_functionptr.cpp`、`test_scriptstruct.cpp`
  - 当前 AS 2.33 WIP 下部分用例预期失败；对不支持的子用例使用条件跳过，随 `Plan_AS238NonLambdaPort.md` / `Plan_AS238LambdaPort.md` 推进逐步翻为正例
  - 测试路径：`Angelscript.TestModule.Native.ASSDK.AS238.*`
- [ ] **P6.3** 📦 Git 提交：`[Test/Native] Feat: integrate AS SDK 2.38 feature tests with version gates`

### Phase 7：文档收口

> 目标：更新文档，记录集成模式和运行入口。

- [ ] **P7.1** 更新 `Documents/Guides/Test.md`
  - 补充 `Native/` 子目录说明、测试命名规范、推荐运行命令
  - 记录上游测试适配模式，便于后续增量集成新的上游测试
- [ ] **P7.1** 📦 Git 提交：`[Docs] Feat: document AS SDK test integration guide`

- [ ] **P7.2** 在本 Plan 中更新覆盖矩阵
  - 记录 ~105 个上游测试中哪些已集成、哪些因版本差异待后续
- [ ] **P7.2** 📦 Git 提交：`[Docs] Feat: add AS SDK test coverage matrix`

## 验收标准

1. `Native/AngelscriptTestAdapter.h` 提供完整的上游 → UE Automation 桥接，包含 `FASSDKBufferedOutStream`、`ASSDK_TEST_FAILED`、`RegisterASSDKAssert`、`ASSDKExecuteString`，不依赖 `FAngelscriptEngine` 和任何 add-on
2. `Angelscript.TestModule.Native.ASSDK.Smoke` 冒烟测试通过
3. Phase 1-5 的核心语言测试全部通过（预计 ≥80 个上游测试函数）
4. Phase 6 的杂项与 2.38 标记测试中，已支持部分通过，未支持部分有显式跳过
5. 现有 `Native/` 自研测试（17 个）回归不受影响
6. `Documents/Guides/Test.md` 包含 ASSDK 子目录说明与运行入口
7. 每个测试文件控制在 500 行以内
8. 不引入任何 SDK add-on（`scriptarray`、`scriptstdstring`、`scripthelper`、`autowrapper` 等）

## 风险与注意事项

### 风险 1：`ExecuteString()` 替代实现的行为差异

上游 `ExecuteString()` 来自 `scripthelper`，支持在指定模块上下文中执行、访问模块全局变量等。自研替代可能在模块作用域、临时变量生命周期上有差异。

**缓解**：`ASSDKExecuteString` 只需覆盖"编译单行代码并执行"的最简场景；对需要复杂模块上下文的用例，改写为显式 `BuildModule` + `GetFunction` + `Execute`。

### 风险 2：上游测试内部 `std::string` message 断言与 2.33 WIP 差异

上游测试直接对比完整 message 字符串，当前 AS 2.33 WIP 的错误消息措辞或格式可能不同。

**缓解**：对 message 断言采用"包含关键词"而非"完全匹配"；在已知差异处标注版本。

### 风险 3：调用约定测试依赖平台 ABI

`testcdecl_class_*.cpp`、`test_thiscall_*.cpp` 的正确性依赖 Win64/MSVC ABI。部分 stdcall 在 x64 上与 cdecl 等价。

**缓解**：对平台相关的失败标记为条件跳过。

### 风险 4：单文件行数膨胀

上游单个测试可能 600+ 行，直接全量包装会超 500 行/文件约束。

**缓解**：大型上游测试只提取核心路径，或拆分为多个 UE Automation 用例。

## 推荐实施顺序

1. **M1** = Phase 0 + Phase 1 — 适配层 + 引擎/执行核心
2. **M2** = Phase 2 + Phase 3 — 类型系统 + 对象模型 + OOP
3. **M3** = Phase 4 + Phase 5 — 函数/调用约定 + 编译器/运行时
4. **M4** = Phase 6 + Phase 7 — 杂项/2.38 标记 + 文档
