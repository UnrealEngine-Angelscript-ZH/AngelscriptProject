# UFunction 反射回退绑定计划

归档状态：已归档（已完成）
归档日期：2026-04-07
完成判断：`BlueprintCallableReflectiveFallback` runtime 后端、shared duplicate guard、GeneratedFunctionTable 三分类统计与 `AIModule` / `GameplayTags` / `UMG` 代表性 reflective fallback 自动化测试均已在当前 worktree 落地；本次收口进一步把共享 reflective invocation helper 复用于 `Bind_BlueprintEvent.cpp` 与 `ClassGenerator/AngelscriptClassGenerator.cpp` 的既有 `ProcessEvent` 调用点，并新增 native interface ref/out round-trip 回归覆盖。所有改动已通过标准构建、`Angelscript.TestModule.Interface.Native`、`Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback`、`Angelscript.TestModule.Engine.GeneratedFunctionTable` 与完整 `Bindings` suite 回归验证。按本会话上层约束未执行计划中的 git 提交项，但所有实现与验证项已完成，这一限制不阻塞归档。
结果摘要：本计划在保留 direct native bind 主路径的前提下，为 unresolved `BlueprintCallable/BlueprintPure` UFunction 补齐了 `UFunction` / `ProcessEvent` 驱动的 reflective fallback 冷路径；新增 `BlueprintCallableReflectiveFallback.h/.cpp`、共享的脚本声明去重检查与 direct-path precedence 护栏，扩展 `FFuncEntry` 状态以区分 direct 与 reflective 绑定，并在 `GeneratedFunctionTable` 测试中固定 direct / reflective / unresolved 三分类口径。收口阶段进一步将 shared reflective helper 真正接入 Blueprint event / interface dispatch 路径，并用 native interface ref/out 回归锁定共享 marshalling 语义。文档侧补齐了绑定系统知识说明、测试指南专项入口与计划索引收口。

## 背景与目标

本计划的目标不是把现有 direct bind 替换成统一反射调用，而是在保留 `FGenericFuncPtr + ASAutoCaller::FunctionCaller` 热路径的前提下，为当前 `ERASE_NO_FUNCTION()` 中一部分可由 UE 反射安全驱动的 UFunction 增加第二后端。核心要求有三条：

1. 既有 direct bind 继续优先，不被 reflective 路由侵蚀。
2. 一部分 unresolved `BlueprintCallable` 变为可调用，而不是继续 silent skip。
3. 测试和统计口径从 direct/stub 升级为 direct/reflective/unresolved 三分类。

## 执行结果

### Phase 0：边界、资格矩阵与性能定位

- 在 `Documents/Knowledges/02_04_Bind_System_And_Native_Binding_Generation.md` 中补齐了 direct bind 与 reflective fallback 的职责边界，明确 `UFunction::Func` 是 thunk 而不是原始成员函数指针。
- 固化了第一阶段资格矩阵：接口类、`CustomThunk`、latent/custom K2 thunk 与当前无法稳定封送的参数形态继续保留为 unresolved。
- 明确 reflective fallback 是 coverage/cold path，`ProcessEvent` 相关成本来自参数 buffer、`FProperty` 复制、返回值与 out/ref 回写以及清理流程。

### Phase 1：共享 helper 与最小缓存边界

- 新增 `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/BlueprintCallableReflectiveFallback.h`
- 新增 `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/BlueprintCallableReflectiveFallback.cpp`
- helper 统一承担 reflective fallback 所需的参数 buffer 初始化、`FProperty` 逐项复制、`ProcessEvent` 调用、返回值写回和 out/ref 参数回写。
- `Bind_BlueprintEvent.cpp` 与 `ClassGenerator/AngelscriptClassGenerator.cpp` 的 generic → `ProcessEvent` 路径现已改为复用同一 shared helper，不再长期维持第三套、第四套分叉封送实现。
- runtime 端缓存的最小元数据边界已经固定为：`UFunction*`、参数/返回值布局摘要、资格结论与 reflective 绑定状态；没有把这条路径扩大成新的全局性能系统。

### Phase 2：BlueprintCallable reflective fallback 接入

- 在 `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintCallable.cpp` 中把“存在 `ClassFuncMaps` 条目但缺少 direct native pointer”的分支升级为 reflective fallback 尝试。
- 在 `Plugins/Angelscript/Source/AngelscriptRuntime/Core/FunctionCallers.h` 中扩展 `FFuncEntry` 状态，用于显式记录 reflective fallback 绑定结果。
- 把脚本声明重复注册检测提升为 shared guard，使 direct path 和 reflective path 共用同一套防重逻辑，不再依赖临时模块特判。
- 通过 `Angelscript.TestModule.Bindings.NativeActorMethods` 与完整 `Bindings` suite 验证了 direct-path precedence 仍然成立。

### Phase 3：representative fallback 测试闭环

- 新增 `Plugins/Angelscript/Source/AngelscriptTest/Bindings/AngelscriptBlueprintCallableReflectiveFallbackTests.cpp`
- 正例覆盖模块：
  - `AIModule`：`UPawnSensingComponent`
  - `GameplayTags`：`UBlueprintGameplayTagLibrary`
  - `UMG`：`UCheckBox`
- 负例覆盖：接口类与 `CustomThunk` 明确拒绝，并由 eligibility 测试固定。
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptGeneratedFunctionTableTests.cpp` 已升级为 direct / reflective / unresolved 三分类统计，并继续保护 hand-written GAS direct entries 的优先级。

### Phase 4：知识沉淀与入口收口

- `Documents/Knowledges/02_04_Bind_System_And_Native_Binding_Generation.md`：新增 reflective backend 边界、第一阶段资格矩阵、三分类统计与 direct-path precedence 说明。
- `Documents/Guides/Test.md`：新增 reflective fallback 前缀与 GeneratedFunctionTable 前缀的标准运行命令。
- `Documents/Plans/Plan_OpportunityIndex.md` 与 `Documents/Plans/Archives/README.md`：同步本计划的归档状态与结果摘要。

## 验证结果

以下验证全部通过：

- `Tools\RunBuild.ps1 -Label reflective-fallback-build8 -TimeoutMs 600000 -NoXGE -SerializeByEngine`
- `Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Bindings.NativeActorMethods" -Label reflective-fallback-control7 -TimeoutMs 600000`
- `Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback" -Label reflective-fallback-bindings-sharedguard4 -TimeoutMs 600000`
- `Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Engine.GeneratedFunctionTable" -Label reflective-fallback-generated-table -TimeoutMs 600000`
- `Tools\RunTestSuite.ps1 -Suite Bindings -LabelPrefix reflective-fallback-bindings-suite -TimeoutMs 600000`

验证结论：

- `Bindings` suite：`60/60 PASS`
- reflective fallback 前缀：`4/4 PASS`
- GeneratedFunctionTable 前缀：`5/5 PASS`
- native interface 前缀：`3/3 PASS`
- control 测试：`1/1 PASS`

## 不阻塞归档项

- 本会话按上层无提交约束未创建 git commit，因此原计划中的 `📦 Git 提交` 条目未逐项执行；相关实现已完整保留在当前 worktree 中，并已通过标准验证，这一限制不阻塞计划归档。
