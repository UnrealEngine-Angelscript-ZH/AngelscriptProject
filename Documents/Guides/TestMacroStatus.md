# Angelscript 测试宏现状总览

## 目的

本文档用于收拢此前散落在仓库根目录的宏相关状态文件，统一提供仓库级别的现状说明、迁移口径、验证入口与历史说明。

已合并来源：

- `MACRO_SYSTEM_README.md`
- `MACRO_IMPLEMENTATION_SUMMARY.md`
- `MACRO_VALIDATION_REPORT.md`
- `PROJECT_COMPLETION_SUMMARY.txt`

本文档负责“仓库级总览”；具体宏定义、示例与细节规则仍以插件源码内文档和实现为准。

## 当前结论

- 当前生效的测试宏体系是 `ASTEST_*`，不是历史上的 `ANGELSCRIPT_TEST` / `ANGELSCRIPT_ISOLATED_TEST`。
- `IMPLEMENT_SIMPLE_AUTOMATION_TEST(...)` 仍然是测试注册入口；宏层负责引擎创建、生命周期和常见辅助流程。
- `ASTEST_CREATE_ENGINE_*` 与匹配的 `ASTEST_BEGIN_* / ASTEST_END_*` 已经完成主迁移范围内的标准化接入。
- `SHARE` / `SHARE_CLEAN` / `SHARE_FRESH` 的生命周期口径已经在宏定义、验证测试和指南中统一。
- 原先位于根目录的几份宏状态文件不再作为独立事实来源，后续统一以本文档和插件内宏指南为准。

## 当前事实来源

### 一线事实来源

- 宏定义：`Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h`
- 中文宏说明：`Plugins/Angelscript/Source/AngelscriptTest/Shared/README_MACROS_ZH.md`
- 中文测试指南补充：`Plugins/Angelscript/Source/AngelscriptTest/TESTING_GUIDE_ZH.md`
- 验证测试：`Plugins/Angelscript/Source/AngelscriptTest/Validation/AngelscriptMacroValidationTests.cpp`
- 编译侧验证测试：`Plugins/Angelscript/Source/AngelscriptTest/Validation/AngelscriptCompilerMacroValidationTests.cpp`

### 仓库级归档记录

- 宏优化计划归档：`Documents/Plans/Archives/Plan_TestMacroOptimization.md`
- 归档索引：`Documents/Plans/Archives/README.md`

## 宏使用口径

### 当前有效宏族

- 引擎创建：`ASTEST_CREATE_ENGINE_FULL()`、`ASTEST_CREATE_ENGINE_SHARE()`、`ASTEST_CREATE_ENGINE_SHARE_CLEAN()`、`ASTEST_CREATE_ENGINE_SHARE_FRESH()`、`ASTEST_CREATE_ENGINE_CLONE()`、`ASTEST_CREATE_ENGINE_NATIVE()`
- 生命周期：`ASTEST_BEGIN_*` / `ASTEST_END_*`
- 辅助宏：`ASTEST_COMPILE_RUN_INT`、`ASTEST_COMPILE_RUN_INT64`、`ASTEST_BUILD_MODULE`

### 迁移时的判断原则

- 保留 `IMPLEMENT_SIMPLE_AUTOMATION_TEST(...)` 作为 automation 注册入口。
- 只有在语义匹配时，才将旧测试迁移到对应的 `ASTEST_CREATE_ENGINE_*` 与 `ASTEST_BEGIN_* / ASTEST_END_*` 组合。
- 不要仅根据旧 helper 命名推断运行时语义，必须以真实行为为准。

### 需要特别注意的 helper 语义

在 `AngelscriptTestUtilities.h` 相关 helper 中：

- `GetOrCreateSharedCloneEngine()` 当前实际创建的是 shared Full engine。
- `AcquireCleanSharedCloneEngine()` 会先重置该 shared engine，再返回。
- `AcquireFreshSharedCloneEngine()` 会先销毁 shared/global 状态，再重新获取 clean engine。

因此，`CLONE`、`SHARE_CLEAN`、`SHARE_FRESH` 一类迁移决策不能只看名字，必须看行为。

## 已落地结果

结合当前代码与归档计划，仓库里已经稳定落地的内容包括：

- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h` 已作为当前 `ASTEST_*` 双层宏体系的唯一核心定义点。
- `SHARE_CLEAN` 与 `SHARE_FRESH` 的配套生命周期宏已经补齐，并在验证测试中覆盖。
- 主迁移范围内的测试已经按匹配语义批量接入 `ASTEST_BEGIN_* / ASTEST_END_*`。
- 宏指南、中文补充与验证测试已经围绕 `ASTEST_*` 统一口径，不再把历史 `ANGELSCRIPT_*` 包装宏写成当前 API。
- `Documents/Plans/Archives/Plan_TestMacroOptimization.md` 已记录本轮宏优化工作的收口状态与验证结果。

## 验证口径

当前宏相关验证应证明以下几点：

1. 验证测试使用的是当前 `ASTEST_*` API，而不是历史包装名。
2. 宏化后的测试仍保留原始断言意图、Automation 路径与引擎生命周期语义。
3. 涉及 shared/clean/fresh 的路径没有在迁移中被静默改语义。
4. 构建与测试统一通过仓库标准脚本入口执行。

标准入口：

- `Tools/RunBuild.ps1`
- `Tools/RunTests.ps1`
- `Tools/RunTestSuite.ps1`

常用验证示例：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -TimeoutMs 180000 -- -NoXGE
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Validation." -TimeoutMs 300000
```

如需扩展验证范围，应按迁移目录或 automation group 继续收口，而不是恢复对根目录旧报告的维护。

## 非目标与保留项

以下路径在宏优化计划归档时被明确列为保留项，不属于那一轮 `BEGIN/END` 标准化 closeout 范围：

- `Plugins/Angelscript/Source/AngelscriptTest/Native/*`
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Native/*`
- `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptNativeScriptHotReloadTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptEngineParityTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptBindConfigTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptEnginePerformanceTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptRestoreTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Runtime/AngelscriptLearningExecutionTraceTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Runtime/AngelscriptLearningPreprocessorTraceTests.cpp`
- `Plugins/Angelscript/Source/AngelscriptTest/Learning/Runtime/AngelscriptLearningRestoreAndBytecodePersistenceTests.cpp`

这些路径涉及 Native / ASSDK / production-like engine / performance / preprocessor-only 等特定语义，不能和主迁移范围的低风险标准化混在一起处理。

## 历史说明

- `ANGELSCRIPT_TEST`、`ANGELSCRIPT_ISOLATED_TEST` 及类似名字，应视为历史设计探索或已废弃口径，除非未来明确重新引入兼容层。
- 根目录原有几份宏状态文件已经退役，不再作为后续维护入口。
- 如果未来宏边界、测试入口或迁移状态再次变化，应优先更新本文档，以及插件内的中文宏指南与测试指南补充。
