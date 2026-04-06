# 测试宏优化计划

归档状态：已归档（已完成）
归档日期：2026-04-05
完成判断：`AngelscriptTest` 主迁移范围已完成 `ASTEST_CREATE_ENGINE_*` 与匹配 `ASTEST_BEGIN_* / ASTEST_END_*` 的配套接入，`SHARE` / `SHARE_CLEAN` / `SHARE_FRESH` 生命周期口径已在宏定义、验证测试与测试指南中统一，并已通过标准构建、关键前缀回归以及 `AngelscriptFast` / `AngelscriptScenario` group 验证。
结果摘要：本计划完成了 `BEGIN/END` 批量接入、`SHARE_CLEAN` / `SHARE_FRESH` 宏扩展与验证、测试指南收口，以及为最终回归所需的 automation group 配置、共享引擎上下文恢复与错误日志空 banner 三个阻塞项修复。Native / production / custom create / performance 路径继续作为显式保留项，不阻塞归档。

## 背景与目标

本计划的目标不是重新设计 `ASTEST_*` 体系，而是把已经大面积落地的 `ASTEST_CREATE_ENGINE_*` 进一步收口到一致的生命周期入口，并清理旧 helper 语义尾项，最终让 `AngelscriptTest` 的主干测试全部走统一、可验证的 `BEGIN/END` 写法。

用户在执行中额外强调了两条边界：

- `ASTEST_BEGIN_XX` 必须与对应的引擎创建族配套，不能把 `SHARE` / `SHARE_CLEAN` / `SHARE_FRESH` 混用。
- `BEGIN/END` 的引入主要是为了未来全局控制；当前 `BEGIN` 负责替代 scope 接入，`END` 对 `SHARE*` 家族暂时不做额外收尾，但要保留成对入口。

归档时，上述目标已经在主迁移范围内完成，剩余 Native / production / custom create / performance 路径均已明确为本轮不阻塞保留项。

## 执行结果

- 在 `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h` 中固定 `SHARE` 家族的生命周期入口：
  - `ASTEST_BEGIN_SHARE` 统一建立 `FAngelscriptEngineScope`
  - 新增 `ASTEST_BEGIN_SHARE_CLEAN` / `ASTEST_END_SHARE_CLEAN`
  - 新增 `ASTEST_BEGIN_SHARE_FRESH` / `ASTEST_END_SHARE_FRESH`
- 在 `Plugins/Angelscript/Source/AngelscriptTest/Validation/AngelscriptMacroValidationTests.cpp` 中补齐 `SHARE_CLEAN` 与 `SHARE_FRESH` 的验证用例，确保新配套宏可直接编译执行。
- 在 `Plugins/Angelscript/Source/AngelscriptTest/TESTING_GUIDE.md` 中把 `SHARE` / `SHARE_CLEAN` / `SHARE_FRESH` 的推荐写法统一为成对的 `ASTEST_BEGIN_* / ASTEST_END_*`，并明确 `END` 目前作为未来生命周期收口点保留。
- 对 `Plugins/Angelscript/Source/AngelscriptTest/` 主迁移范围做了批量标准化，约 58 个测试源文件、187 个 `RunTest` 入口统一补齐与 `ASTEST_CREATE_ENGINE_*` 同族的 `ASTEST_BEGIN_* / ASTEST_END_*`。
- 验证过程中补齐了三个真实阻塞项，以保证标准入口下的完整回归可通过：
  - `Config/DefaultEngine.ini` 与 `Tools/Shared/UnrealCommandUtils.ps1`：修正 automation group 的配置 section 与读取逻辑
  - `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptEngineCoreTests.cpp`：避免 full-destroy 测试把已销毁 engine 指针恢复回上下文栈
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`：避免空 `section` 错误打印出额外空 banner 干扰组回归

## 非阻塞保留项

以下路径在本计划关闭时继续保留，不纳入本轮 `BEGIN/END` 宏化 closeout：

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

这些文件对应 production engine、ASSDK / Native、custom create path、performance 或 preprocessor-only 语义，不属于本轮低风险批量接入范围，因此明确记为保留项而非未完成项。

## 验证结果

本计划按仓库约定，仅使用标准脚本入口完成收口验证。

- `Tools/RunBuild.ps1 -TimeoutMs 180000 -- -NoXGE`
  - 结果：通过
  - 输出目录：`Saved/Build/NoXGE/20260405_132320/`
- `Tools/RunTests.ps1 -TestPrefix 'Angelscript.TestModule.Validation.' -TimeoutMs 600000`
  - 结果：`5/5 PASS`
  - 输出目录：`Saved/Tests/Angelscript.TestModule.Validation./20260405_132757/`
- `Tools/RunTests.ps1 -TestPrefix 'Angelscript.TestModule.Shared.EngineHelper.' -TimeoutMs 600000`
  - 结果：`16/16 PASS`
  - 输出目录：`Saved/Tests/Angelscript.TestModule.Shared.EngineHelper./20260405_132838/`
- `Tools/RunTests.ps1 -TestPrefix 'Angelscript.TestModule.Bindings.' -TimeoutMs 600000`
  - 结果：`56/56 PASS`
  - 输出目录：`Saved/Tests/Angelscript.TestModule.Bindings./20260405_132918/`
- `Tools/RunTests.ps1 -TestPrefix 'Angelscript.TestModule.HotReload.' -TimeoutMs 600000`
  - 结果：`25/25 PASS`
  - 输出目录：`Saved/Tests/Angelscript.TestModule.HotReload./20260405_132957/`
- `Tools/RunTests.ps1 -TestPrefix 'Angelscript.TestModule.Learning.Runtime.' -TimeoutMs 600000`
  - 结果：`16/16 PASS`
  - 输出目录：`Saved/Tests/Angelscript.TestModule.Learning.Runtime./20260405_133059/`
- `Tools/RunTests.ps1 -Group 'AngelscriptFast' -TimeoutMs 600000`
  - 结果：`221/221 PASS`
  - 输出目录：`Saved/Tests/Group-AngelscriptFast/20260405_132332/`
- `Tools/RunTests.ps1 -Group 'AngelscriptScenario' -TimeoutMs 600000`
  - 结果：`50/50 PASS`
  - 输出目录：`Saved/Tests/Group-AngelscriptScenario/20260405_132448/`

## 关闭说明

本计划可以归档的原因是：

- 主迁移范围内的 `BEGIN/END` 生命周期接入已经完成，不再存在“已切到 `ASTEST_CREATE_ENGINE_*` 但仍缺统一生命周期入口”的系统性缺口。
- `SHARE` / `SHARE_CLEAN` / `SHARE_FRESH` 的规则已在宏定义、验证测试和指南三个层面一致化。
- 标准 build / test 入口下的关键前缀回归与两大 group 已全部通过，能够证明这轮批量替换没有引入主干回归。
- 剩余未纳入宏化的路径已经明确归类为保留项，不再以模糊 backlog 的形式悬空。
