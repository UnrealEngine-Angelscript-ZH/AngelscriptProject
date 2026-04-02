# Angelscript 技术债实时盘点

> 快照提交：`ac16f3a`
>
> 目的：将 `Documents/Plans/Plan_TechnicalDebt.md` 中提到的 live inventory 固化为仓库内可追溯文档，避免后续执行阶段重复从零扫描。

## 1. 已编目基线 vs 实时扫描

- `Documents/Guides/TestCatalog.md` 仍以 `275/275 PASS` 作为**已编目基线**。
- 当前源码对 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`、`IMPLEMENT_COMPLEX_AUTOMATION_TEST`、`IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST`、`BEGIN_DEFINE_SPEC`、`DEFINE_SPEC` 的实时扫描命中 **319** 处定义，覆盖 **89** 个文件。
- 这两组数字不能直接等价：前者是文档化、已整理的基线；后者是当前源码中的实时定义规模。当前差值为 **+44**，应作为后续 `TestCatalog` 同步与回归验收的起点，而不是继续把 `275` 视为源码现状总数。

### 当前 live inventory 热点

- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptEngineParityTests.cpp`：14 处
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestEngineHelperTests.cpp`：11 处
- `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptTypeTests.cpp`：11 处
- `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptExecutionTests.cpp`：9 处
- `Plugins/Angelscript/Source/AngelscriptTest/Compiler/AngelscriptCompilerPipelineTests.cpp`：8 处

## 2. 测试 helper 命名现状

### 当前入口定义位置

- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:30`：`FScopedGlobalEngineOverride`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:47`：`TryGetRunningProductionEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:61`：`CreateIsolatedFullEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:70`：`CreateIsolatedCloneEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:77`：`GetOrCreateSharedCloneEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:88`：`ResetSharedCloneEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:100`：`AcquireCleanSharedCloneEngine()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:107`：`AcquireCleanSharedCloneEngineAndOverrideGlobal()`
- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:115`：`RequireRunningProductionEngine()`

### 迁移结论

- 旧 helper 兼容别名已从 `Shared/AngelscriptTestUtilities.h` 删除。
- `Plugins/Angelscript/Source/AngelscriptTest/` 下的 helper 调用点已完成向显式命名迁移；后续不再把 `Initialized` / `Shared` / `Production` 的混合语义保留为并行入口。

## 3. 构建与运行时开放债务锚点

- `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs:26`：仍保留 `// TODO: 不要删除, 开发期间，关闭优化方便debug`
- `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs:27`：仍无条件 `OptimizeCode = CodeOptimization.Never`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h:731`：仍保留 `//WILL-EDIT`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h:732`：仍无条件声明 `TBaseStructure<FBox>`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h:737`：仍无条件声明 `TBaseStructure<FBoxSphereBounds>`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp:144`：`ExecutePreamble()` 仍缺事件签名校验
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp:169`：`ExecuteEvent()` 仍缺事件签名校验
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp:343`：delegate 调用路径仍保留 `// TODO: Signature checking?`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp:353`：multicast delegate 调用路径仍保留 `// TODO: Signature checking?`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/AngelscriptDebugServer.cpp:161`：仍保留线程挂起策略待审计注释
- `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/AngelscriptDebugServer.cpp:185`：异常路径多线程保护仍为 TODO
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/StaticJITHeader.cpp:255`：异常路径返回对象销毁仍未收口

## 4. 弃用 API 与警告压制盘点

- `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_string.h:108`：仍使用 `FCrc::Strihash_DEPRECATED`
- `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/StaticJITHeader.h:65`：仍在文件级使用 `PRAGMA_DISABLE_DEPRECATION_WARNINGS`

## 5. 全局状态依赖盘点

- 对 `FAngelscriptEngine::Get()` 与 `CurrentWorldContext` 的实时扫描在 `Plugins/Angelscript/Source/` 下共命中 **325** 处，覆盖 **57** 个文件。
- 当前热点文件：
  - `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`：81 处
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.cpp`：32 处
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/AngelscriptDebugServer.cpp`：23 处
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`：19 处
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp`：12 处
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_AActor.cpp`：12 处
- 结合代码位置可先分三类：
  - **编译 / 类生成路径**：`ClassGenerator/AngelscriptClassGenerator.cpp`、`ClassGenerator/ASClass.cpp`、`Core/AngelscriptBinds.cpp`
  - **世界上下文绑定路径**：`Binds/Bind_SystemTimers.cpp`、`Binds/Bind_UUserWidget.cpp`、`Core/AngelscriptGameInstanceSubsystem.cpp`
  - **测试 / 调试辅助路径**：`Debugging/AngelscriptDebugServer.cpp`、`Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h`、`Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestEngineHelperTests.cpp`

## 6. Bind 审计候选清单

- 本轮计划中的首批候选文件仍然是：
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_TOptional.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_UStruct.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintType.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_AActor.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_USceneComponent.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_Delegates.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FHitResult.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FMath.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FVector2f.cpp`
- 其中已明确存在的本地逻辑缺口锚点：`Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintType.cpp:156`，`CPF_TObjectPtr` 判断仍处于注释状态。
- 当前执行环境已配置 `AgentConfig.ini` 的 `References.HazelightAngelscriptEngineRoot`，因此 `P0.4` 可直接进入参考源对照，而不需要先补本地引用路径。
- 需要额外注意：当前 clean `HEAD` 上存在 `Documents/Plans/Plan_InterfaceBinding.md` 与 `Documents/Plans/Plan_AS238LambdaPort.md`，但 `Plan_TechnicalDebt.md` 中提到的 `Plan_AS238NonLambdaPort.md` 与 `Plan_HazelightBindModuleMigration.md` 尚未出现在该基线分支上；后续 `P4.3` 需要决定是补齐 sibling plan 还是回写计划引用。

## 7. 宿主工程边界快照

- `Source/AngelscriptProject/AngelscriptProject.Build.cs:11` 仍公开依赖 `EnhancedInput`。
- 这说明 `P5.3` 不是空白任务：当前宿主工程最小化边界尚未形成文档结论。

## 8. 后续执行建议

- `P0.3` 直接基于当前 worktree 的 `AgentConfig.ini` 做参数展开级 smoke check，优先验证 `ProjectFile` 空值回退是否正确解析到仓库根 `.uproject`。
- `P0.4` 以本文件第 6 节作为候选文件清单，补齐“本地符号 / 参考行为 / 风险 / 测试落点 / sibling plan 去向”的矩阵列。
- `P2.4` 与 `P6.2` 更新 `TestCatalog` 时，不再复用单一的 `275/275 PASS` 叙述，而应明确区分“已编目基线”与“实时扫描规模”。

## 9. Phase 1 验证快照

- 编辑器目标构建验证：`AngelscriptProjectEditor Win64 Development` 在当前 `technical-debt-plan` worktree 中可成功构建。
- 本地前置说明：构建前需要先生成 `Plugins/Angelscript/Intermediate/Build/as_callfunc_x64_msvc_asm.lib`；这是当前仓库沿用的本地中间产物前置，不是本轮 `P1` 改动新引入的问题。
- `P1.3` 目标测试验证：
  - `Angelscript.TestModule.Delegate.UnicastSignatureMismatch`：PASS
  - `Angelscript.TestModule.Delegate.MulticastSignatureMismatch`：PASS
- `Automation RunTests Angelscript.TestModule` 全量回归结果：**未全绿**，但当前失败项均未直接指向 `P1.1`~`P1.5` 改动面。

### 当前 full-suite 失败项（2026-04-03）

- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2A`
  - 失败摘要：`Expected 'Phase2A should load source from Script/Tests/Test_Enums.as' to be true.`
  - 归因判断：更像测试输入文件缺失 / 路径基线问题，未触及本轮 `Binds`、`StaticJIT`、`DebugServer` 修复面。
- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2B`
  - 失败摘要：`Expected 'Phase2B should load source from Script/Tests/Test_GameplayTags.as' to be true.`
  - 归因判断：同样更像 hot-reload 测试资产路径问题，未触及本轮 `P1` 改动面。
- `Angelscript.TestModule.Editor.SourceNavigation.Functions`
  - 失败摘要：`Unable to open script file .../Saved/Automation/Automation/RuntimeFunctionNavigationTest.as`，随后 `Generated function navigation class should exist` 断言失败。
  - 归因判断：属于 editor/source-navigation 测试文件生成或清理路径问题，与 `BlueprintEvent`、`DebugServer`、`StaticJIT` 修复面无直接交集。
- `Angelscript.TestModule.ScriptExamples.Actor`
  - 失败摘要：`Cannot declare class AExampleActorType in module ScriptExamples.Example_Actor. A class with this name already exists in module Example_Actor.`
  - 归因判断：属于 script example 模块命名 / 清理冲突，未触及本轮 `P1` 改动面。

## 10. Phase 2 验证快照

- 显式弃用扫描结果：对 `Plugins/Angelscript/Source/AngelscriptRuntime/` 的 `_DEPRECATED` 与 `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 定向扫描结果均为 **0 命中**。
- `P2` 目标测试验证：
  - `Angelscript.TestModule.Angelscript.Upgrade.CStringHash`：PASS
  - `Angelscript.TestModule.Internals.Restore.EmptyStreamFails`：PASS
  - `Angelscript.TestModule.Internals.Restore.TruncatedStreamFails`：PASS
- `Automation RunTests Angelscript.TestModule` 全量回归结果：**仍未全绿**，但当前保留失败项与 `P2.1`~`P2.4` 的弃用 API / Restore 测试改动不直接相关。

### Phase 2 完成后的 full-suite 保留失败项（2026-04-03）

- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2A`
  - 失败摘要：`Expected 'Phase2A should load source from Script/Tests/Test_Enums.as' to be true.`
  - 归因判断：hot-reload 测试输入文件路径/存在性问题，未触及本轮 `P2` 改动面。
- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2B`
  - 失败摘要：`Expected 'Phase2B should load source from Script/Tests/Test_GameplayTags.as' to be true.`
  - 归因判断：同样属于 hot-reload 测试输入文件路径问题，未触及本轮 `P2` 改动面。
- `Angelscript.TestModule.Editor.SourceNavigation.Functions`
  - 失败摘要：`Unable to open script file .../Saved/Automation/Automation/RuntimeFunctionNavigationTest.as`，随后 `Generated function navigation class should exist` 断言失败。
  - 归因判断：editor/source-navigation 测试文件生成或清理路径问题，未触及 `P2` 的 ThirdParty hash / StaticJIT 警告压制 / Restore 负向覆盖改动面。
- `Angelscript.TestModule.ScriptExamples.Actor`
  - 失败摘要：`Cannot declare class AExampleActorType in module ScriptExamples.Example_Actor. A class with this name already exists in module Example_Actor.`
  - 归因判断：script example 模块命名或清理冲突，未触及本轮 `P2` 改动面。

## 11. Phase 3 验证快照

- helper 命名迁移结果：`Plugins/Angelscript/Source/AngelscriptTest/` 下旧 helper 名称定向 grep 结果为 **0 命中**。
- `P3.1` / `P3.2` focused regression：
  - `Angelscript.TestModule.Shared.EngineHelper`：PASS
  - `Angelscript.TestModule.Internals.Restore.*`：PASS
- `P3.3` 主题化集成 focused regression：`Actor`、`BlueprintChild`、`Component`、`Delegate`、`GC`、`HotReload`、`Inheritance`、`Interface`、`WorldSubsystem`、`ClassGenerator`：PASS（已知旧失败项未增加）。
- `P3.4` 行为 / Bindings / FileSystem / Editor / ScriptExamples focused regression：helper 改名未新增失败；仍保留与此前一致的 4 个已知失败项。
- `Automation RunTests Angelscript.TestModule` 全量回归结果：**仍未全绿**，失败项与 Phase 2 结束时一致，没有新增 helper 命名迁移相关失败。

### Phase 3 完成后的 full-suite 保留失败项（2026-04-03）

- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2A`
  - 失败摘要：`Expected 'Phase2A should load source from Script/Tests/Test_Enums.as' to be true.`
  - 归因判断：hot-reload 测试输入文件路径/存在性问题，helper 命名迁移未触及该路径。
- `Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2B`
  - 失败摘要：`Expected 'Phase2B should load source from Script/Tests/Test_GameplayTags.as' to be true.`
  - 归因判断：同样属于 hot-reload 测试输入文件路径问题，helper 命名迁移未触及该路径。
- `Angelscript.TestModule.Editor.SourceNavigation.Functions`
  - 失败摘要：`Unable to open script file .../Saved/Automation/Automation/RuntimeFunctionNavigationTest.as`，随后 `Generated function navigation class should exist` 断言失败。
  - 归因判断：editor/source-navigation 测试文件生成或清理路径问题，helper 命名迁移未触及该路径。
- `Angelscript.TestModule.ScriptExamples.Actor`
  - 失败摘要：`Cannot declare class AExampleActorType in module ScriptExamples.Example_Actor. A class with this name already exists in module Example_Actor.`
  - 归因判断：script example 模块命名或清理冲突，helper 命名迁移未触及该路径。
