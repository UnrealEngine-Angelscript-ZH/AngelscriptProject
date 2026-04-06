# Angelscript 插件架构源码分析 TodoList

> 生成日期: 2026-04-06
> 分析范围: `Plugins/Angelscript` 相关架构文章梳理；覆盖现有架构文档归并与待补专题，不展开宿主工程业务逻辑

---

## 第一部分：插件总体架构

- [ ] **1.1 插件定位与模块边界**
  - [x] 1.1.1 插件目标与宿主工程边界 → `01_01_01_Plugin_Goal_And_Host_Boundary.md`
  - [x] 1.1.2 `AngelscriptRuntime` / `AngelscriptEditor` / `AngelscriptTest` / `Dump` 目录职责 → `01_01_02_Directory_Responsibilities.md`
  - [x] 1.1.3 现有架构文档导航与阅读顺序 → `01_01_03_Document_Navigation_And_Reading_Order.md`
  - 关键源码: `AGENTS.md`, `Plugins/Angelscript/AGENTS.md`, `Plugins/Angelscript/Angelscript.uplugin`
  - 关键概念: Plugin-centric, Runtime, Editor, Test, Dump
  - 现有材料: `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **1.2 Runtime 总控与生命周期**
  - [x] 1.2.1 `FAngelscriptRuntimeModule` 初始化入口 → `01_02_01_RuntimeModule_Initialization_Entry.md`
  - [x] 1.2.2 `FAngelscriptEngine` 启动、编译、重载主链路 → `01_02_02_Engine_Startup_Compile_Reload_Main_Flow.md`
  - [x] 1.2.3 全局状态入口与状态边界 → `01_02_03_Global_State_Entry_And_Boundaries.md`
  - [x] 1.2.4 Types / Functions / Code / Globals 四阶段编译流 → `01_02_04_Four_Stage_Compilation_Flow.md`
  - [x] 1.2.5 Diagnostics、错误收集与调试输出面 → `01_02_05_Diagnostics_Error_Collection_And_Output_Surface.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`
  - 关键概念: Initialize, InitialCompile, PerformHotReload, Global State, Diagnostics
  - 现有材料: `Documents/Guides/GlobalStateContainmentMatrix.md`, `Documents/Plans/Plan_FullDeGlobalization.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **1.3 Editor / Test / Dump 协作边界**
  - [x] 1.3.1 Editor 扩展点与 Runtime 协作 → `01_03_01_Editor_Extension_Points_And_Runtime_Collaboration.md`
  - [x] 1.3.2 Test 模块分层与 Automation 前缀体系 → `01_03_02_Test_Module_Layering_And_Automation_Prefix_System.md`
  - [x] 1.3.3 Dump 在 Runtime / Test 中的职责拆分 → `01_03_03_Dump_Responsibility_Split_Between_Runtime_And_Test.md`
  - [x] 1.3.4 ClassReloadHelper 的 editor-side 重实例化责任 → `01_03_04_ClassReloadHelper_Editor_Side_Reinstancing_Responsibility.md`
  - [x] 1.3.5 Content Browser Data Source 的脚本资产可见性边界 → `01_03_05_Content_Browser_Data_Source_Visibility_Boundary.md`
  - [x] 1.3.6 Source Code Navigation 的 Editor-Runtime 桥接 → `01_03_06_Source_Code_Navigation_Editor_Runtime_Bridge.md`
  - [x] 1.3.7 Debugger Test Session 的 Test-Runtime 协作模式 → `01_03_07_Debugger_Test_Session_Test_Runtime_Collaboration_Pattern.md`
  - 关键源码: `Plugins/Angelscript/AGENTS.md`, `Plugins/Angelscript/Source/AngelscriptEditor/`, `Plugins/Angelscript/Source/AngelscriptTest/`, `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/`
  - 关键概念: Editor-only, TestModule, Dump Observer, Automation Prefix, Reinstancing, Content Browser Data Source
  - 现有材料: `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`, `Documents/Plans/Archives/Plan_ASEngineStateDump.md`
  - [x] 补充审查：已追加 `1.3.6 Source Code Navigation 的 Editor-Runtime 桥接` 与 `1.3.7 Debugger Test Session 的 Test-Runtime 协作模式`

- [ ] **1.4 插件模块清单与装载关系**
  - [x] 1.4.1 `Angelscript.uplugin` 中模块声明与 LoadingPhase → `01_04_01_Module_Declarations_And_Loading_Phase.md`
  - [x] 1.4.2 Runtime / Editor / Test 的插件依赖面 → `01_04_02_Module_Dependency_Surface_Runtime_Editor_Test.md`
  - [x] 1.4.3 `StructUtils` / `EnhancedInput` / `GameplayAbilities` 等外部插件关系 → `01_04_03_External_Plugin_Relationships.md`
  - [x] 1.4.4 Runtime 的条件依赖与 Editor-Only 边界处理 → `01_04_04_Runtime_Conditional_Dependencies_And_Editor_Only_Boundary.md`
  - [x] 1.4.5 `.uplugin` `Plugins` 声明与 `Build.cs` 依赖的一致性约束 → `01_04_05_Plugin_Descriptor_And_BuildCs_Consistency.md`
  - 关键源码: `Plugins/Angelscript/Angelscript.uplugin`, `Plugins/Angelscript/Source/*/*.Build.cs`
  - 关键概念: LoadingPhase, Module Type, Plugin Dependency
  - 现有材料: `AGENTS.md`
  - [x] 补充审查：已追加 `1.4.4 Runtime 的条件依赖与 Editor-Only 边界处理` 与 `1.4.5 .uplugin Plugins 声明与 Build.cs 依赖的一致性约束`

- [ ] **1.5 UHT 工具链位置与边界**
  - [x] 1.5.1 `AngelscriptUHTTool` 的职责与输出物 → `01_05_01_AngelscriptUHTTool_Responsibilities_And_Outputs.md`
  - [x] 1.5.2 Header 签名解析与函数表导出 → `01_05_02_Header_Signature_Resolution_And_Function_Table_Export.md`
  - [x] 1.5.3 与 Runtime / Editor 生成链路的接口边界 → `01_05_03_UHT_Runtime_Editor_Interface_Boundary.md`
  - [x] 1.5.4 Coverage Diagnostics、过期输出清理与分片命名策略 → `01_05_04_Coverage_Diagnostics_Stale_Output_Cleanup_And_Shard_Naming_Policy.md`
  - [x] 1.5.5 Direct-Bind 回退策略与 UHT 测试/验证接缝 → `01_05_05_Direct_Bind_Fallback_Policy_And_UHT_Test_Validation_Seam.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptUHTTool.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptHeaderSignatureResolver.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableExporter.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`
  - 关键概念: UHT Tool, Header Signature, Function Table Export
  - 现有材料: `Documents/Plans/Plan_UhtPlugin.md`
  - [x] 补充审查：已追加 `1.5.4 Coverage Diagnostics、过期输出清理与分片命名策略` 与 `1.5.5 Direct-Bind 回退策略与 UHT 测试/验证接缝`

---

## 第二部分：类型系统与生成链路

- [ ] **2.1 脚本类生成机制**
  - [x] 2.1.1 `UASClass` 核心字段与状态布局 → `02_01_01_UASClass_Core_Fields_And_State_Layout.md`
  - [x] 2.1.2 `FAngelscriptClassGenerator` 创建 / 重载链路 → `02_01_02_AngelscriptClassGenerator_Creation_And_Reload_Pipeline.md`
  - [x] 2.1.3 对象构造、GC、复制与热重载协作 → `02_01_03_Construction_GC_Copy_And_HotReload_Coordination.md`
  - [x] 2.1.4 `UASFunction` 特化层级与优化调用路径 → `02_01_04_UASFunction_Specialization_And_Optimized_Call_Paths.md`
  - [x] 2.1.5 Reload propagation、依赖扩散与版本链 → `02_01_05_Reload_Propagation_Dependency_Expansion_And_Version_Chains.md`
  - [x] 2.1.6 默认组件与组件覆盖的构造拓扑 → `02_01_06_Default_Component_Composition_And_Override_Resolution.md`
  - [x] 2.1.7 类最终化、默认对象初始化与验证边界 → `02_01_07_Class_Finalization_Default_Object_Initialization_And_Verification_Boundaries.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`
  - 关键概念: UASClass, UASFunction, CodeSuperClass, FullReload, ReferenceSchema, Reload Propagation
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [x] 补充审查：已追加 `2.1.6 默认组件与组件覆盖的构造拓扑` 与 `2.1.7 类最终化、默认对象初始化与验证边界`

- [ ] **2.2 脚本结构体生成机制**
  - [x] 2.2.1 `UASStruct` 与 `FASStructOps` 分层 → `02_02_01_UASStruct_And_FASStructOps_Layering.md`
  - [ ] 2.2.2 FakeVTable 注入与生命周期操作
  - [ ] 2.2.3 与 Hazelight 方案的差异边界
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.cpp`
  - 关键概念: UASStruct, ICppStructOps, FakeVTable, Hash, Identical
  - 现有材料: `Documents/Hazelight/ScriptStructImplementation.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.3 预处理与模块描述符**
  - [ ] 2.3.1 脚本文件发现与 import 解析
  - [ ] 2.3.2 `FAngelscriptModuleDesc` / `ClassDesc` / `EnumDesc` / `DelegateDesc`
  - [ ] 2.3.3 文件到类型的组织边界
  - [ ] 2.3.4 Chunk / Macro / Import 的预处理模型
  - [ ] 2.3.5 Import 排序、循环依赖与装配顺序
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.h`
  - 关键概念: ModuleDesc, ClassDesc, Script Roots, Import Resolution, Chunk, Macro, ImportChain
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.4 Bind 系统与 Native 绑定生成**
  - [ ] 2.4.1 手写 Bind 体系如何接入 UE 反射与脚本层
  - [ ] 2.4.2 `GenerateNativeBinds()` 输出链路与产物边界
  - [ ] 2.4.3 Runtime / Editor Bind 模块分工
  - [ ] 2.4.4 `FAngelscriptBindState` 注册时序与执行生命周期
  - [ ] 2.4.5 `FAngelscriptBindDatabase` 在 cooked 场景中的缓存职责
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`
  - 关键概念: BindDatabase, BindState, ASRuntimeBind, ASEditorBind, Generated Artifacts
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Plans/Plan_HazelightBindModuleMigration.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.5 脚本函数调用桥与 FunctionCaller 体系**
  - [ ] 2.5.1 Script Function 到 UE 调用栈的桥接方式
  - [ ] 2.5.2 `FunctionCallers/` 中的调用器分层
  - [ ] 2.5.3 参数封送、返回值与错误传播
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionCallers/`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASFunction.cpp`
  - 关键概念: Function Caller, Marshaling, Return Path, Invocation Bridge
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.6 FunctionLibrary 与脚本可见 API 暴露面**
  - [ ] 2.6.1 Runtime FunctionLibraries 的组织方式
  - [ ] 2.6.2 Editor FunctionLibraries 与运行时隔离
  - [ ] 2.6.3 脚本层 API 暴露边界与常见模式
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionLibraries/`, `Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/`
  - 关键概念: Function Library, Script-visible API, Editor-only Exposure
  - 现有材料: `Plugins/Angelscript/AGENTS.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.7 BaseClasses 与脚本基类扩展策略**
  - [ ] 2.7.1 Runtime 基类封装的目的与范围
  - [ ] 2.7.2 脚本继承链与原生继承链的衔接点
  - [ ] 2.7.3 BaseClasses 与类生成 / Bind 的耦合边界
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/BaseClasses/`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/`
  - 关键概念: Base Class, Script Inheritance, Native Integration
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **2.8 类型系统核心与脚本值表达**
  - [ ] 2.8.1 `FAngelscriptType` / `FAngelscriptTypeUsage` 的桥接职责
  - [ ] 2.8.2 属性绑定、GC 引用信息与调试值提取
  - [ ] 2.8.3 类型系统如何服务 ClassGenerator / Debugger / Bind
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.cpp`
  - 关键概念: Type Usage, Property Binding, GC Reference Info, Debugger Value
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

---

## 第三部分：运行时支撑子系统

- [ ] **3.1 热重载与文件变更链路**
  - [ ] 3.1.1 文件发现与变更感知入口
  - [ ] 3.1.2 reload requirement 传播与 class rebind
  - [ ] 3.1.3 Editor / Runtime 在重载中的职责划分
  - [ ] 3.1.4 DirectoryWatcher 与轮询保底策略
  - [ ] 3.1.5 编译失败重试、排队与延迟恢复
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/`
  - 关键概念: File Change Detection, Directory Watcher, Reload Requirement, Rebind, Soft Reload, Full Reload
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.2 StaticJIT 与执行性能路径**
  - [ ] 3.2.1 StaticJIT 数据结构与数据库组织
  - [ ] 3.2.2 解释执行与 JIT 执行分流
  - [ ] 3.2.3 预编译 / 缓存与限制边界
  - [ ] 3.2.4 `PrecompiledData` 序列化与三阶段应用
  - [ ] 3.2.5 `FJITDatabase` 的函数映射与查找路径
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASFunction.cpp`
  - 关键概念: StaticJIT, JIT Database, Precompiled Data, Call Path, Performance Tradeoff
  - 现有材料: `Documents/Plans/Plan_StaticJITUnitTests.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.3 Debugger 与调试协议集成**
  - [ ] 3.3.1 Debug Server 与连接生命周期
  - [ ] 3.3.2 断点、堆栈与脚本控制面
  - [ ] 3.3.3 调试能力与测试验证边界
  - [ ] 3.3.4 Data Breakpoint / Watchpoint 机制
  - [ ] 3.3.5 调试值提取、作用域与变量序列化
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/`, `Plugins/Angelscript/Source/AngelscriptTest/Debugger/`
  - 关键概念: Debug Server, Breakpoint, Data Breakpoint, Call Stack, Debugger Scope, Remote Control
  - 现有材料: `Documents/Plans/Plan_DebugAdapter.md`, `Documents/Plans/Plan_ASDebuggerUnitTest.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.4 State Dump 可观测性架构**
  - [ ] 3.4.1 `FAngelscriptStateDump::DumpAll()` 总入口
  - [ ] 3.4.2 Runtime / Test / Editor 三侧导出链路
  - [ ] 3.4.3 外部观察者模式与扩展点设计
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/AngelscriptStateDump.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/`
  - 关键概念: State Dump, CSV Export, Observer Pattern, Dump Extension
  - 现有材料: `Documents/Plans/Archives/Plan_ASEngineStateDump.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.5 全局状态治理与外部参考吸收边界**
  - [ ] 3.5.1 全局状态 containment 模式与问题分类
  - [ ] 3.5.2 去全局化阶段规划
  - [ ] 3.5.3 Hazelight / UnrealCSharp 参考点与不可照搬项
  - 关键源码: `Documents/Guides/GlobalStateContainmentMatrix.md`, `Documents/Plans/Plan_FullDeGlobalization.md`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Hazelight/ScriptClassImplementation.md`, `Documents/Hazelight/ScriptStructImplementation.md`
  - 关键概念: Containment, DeGlobalization, Reference Absorption, Non-Target Features
  - 现有材料: `Documents/Guides/TechnicalDebtInventory.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.6 CodeCoverage 与脚本覆盖率统计链路**
  - [ ] 3.6.1 覆盖率数据采集入口
  - [ ] 3.6.2 覆盖率聚合与输出格式
  - [ ] 3.6.3 覆盖率能力与测试/验证体系的关系
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/CodeCoverage/`, `Plugins/Angelscript/Source/AngelscriptRuntime/Testing/`
  - 关键概念: Code Coverage, Hit Data, Report Output
  - 现有材料: `Documents/Guides/Test.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.7 Hash / 元数据辅助子系统**
  - [ ] 3.7.1 Hash 子系统承担的定位与用途
  - [ ] 3.7.2 关键数据结构与哈希边界
  - [ ] 3.7.3 与预处理、绑定、缓存的协作点
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Hash/`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/`
  - 关键概念: Hashing, Metadata Key, Cache Identity
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **3.8 ThirdParty AngelScript 内核集成边界**
  - [ ] 3.8.1 ThirdParty 源码镜像与本地修改策略
  - [ ] 3.8.2 Parser / ScriptEngine / ScriptFunction 等核心内核点
  - [ ] 3.8.3 上游升级与 fork 差异管理
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptengine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptfunction.cpp`
  - 关键概念: ThirdParty Fork, Parser, ScriptEngine, Upgrade Strategy
  - 现有材料: `Documents/Plans/Plan_AS238BugfixCherryPick.md`, `Documents/Guides/ASSDK_Fork_Differences.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

---

## 第四部分：测试与验证架构

- [ ] **4.1 测试模块总体分层**
  - [ ] 4.1.1 `Native` / `Learning` / `Shared` / `Validation` 的职责差异
  - [ ] 4.1.2 按主题目录组织的测试专题图
  - [ ] 4.1.3 测试目录与 Runtime / Editor 子系统的映射方式
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/`, `Plugins/Angelscript/AGENTS.md`
  - 关键概念: Test Layer, Topic-first Layout, Shared Fixture
  - 现有材料: `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **4.2 测试基础设施与 Shared Helper**
  - [ ] 4.2.1 `AngelscriptTestEngineHelper` 生命周期管理
  - [ ] 4.2.2 `AngelscriptTestUtilities` / `Macros` 的职责边界
  - [ ] 4.2.3 Shared Fixture 如何支撑 Debugger / Scenario / Native 测试
  - [ ] 4.2.4 `AngelscriptDebuggerTestSession` / Client / Fixture 的协作方式
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestEngineHelper.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h`
  - 关键概念: Test Engine Helper, Shared Fixture, Test Macro, Debugger Test Session
  - 现有材料: `Documents/Guides/TestConventions.md`, `Documents/Plans/Plan_TestEngineIsolation.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **4.3 主题测试簇与架构映射**
  - [ ] 4.3.1 `ClassGenerator` / `Preprocessor` / `HotReload` 专题测试如何覆盖主链路
  - [ ] 4.3.2 `Actor` / `Component` / `Interface` / `Delegate` 等行为专题的组织方式
  - [ ] 4.3.3 `Debugger` / `Dump` / `Subsystem` 等支撑专题的验证入口
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/ClassGenerator/`, `Plugins/Angelscript/Source/AngelscriptTest/Preprocessor/`, `Plugins/Angelscript/Source/AngelscriptTest/HotReload/`, `Plugins/Angelscript/Source/AngelscriptTest/Debugger/`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/`
  - 关键概念: Topic Tests, Coverage Mapping, Validation Entry
  - 现有材料: `Documents/Guides/TestCatalog.md`, `Documents/Guides/TestConventions.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **4.4 Runtime 内部测试与覆盖边界**
  - [ ] 4.4.1 `AngelscriptRuntime/Tests` 与 `AngelscriptTest` 的边界
  - [ ] 4.4.2 `Angelscript.CppTests.*` 自动化前缀的作用域
  - [ ] 4.4.3 内部测试如何服务运行时重构与验证
  - [ ] 4.4.4 Native Core 适配层与原始 AngelScript API 测试边界
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Tests/`, `Plugins/Angelscript/AGENTS.md`
  - 关键概念: Runtime Internal Tests, CppTests, Native Core Adapter, Refactor Safety Net
  - 现有材料: `Documents/Guides/TestConventions.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

---

## 第五部分：工程治理与演进路线

- [ ] **5.1 技术债与全局状态问题地图**
  - [ ] 5.1.1 技术债条目如何映射回具体子系统
  - [ ] 5.1.2 全局状态问题的主题聚类
  - [ ] 5.1.3 哪些问题适合做架构文章，哪些只保留在 debt inventory
  - 关键源码: `Documents/Guides/TechnicalDebtInventory.md`, `Documents/Guides/GlobalStateContainmentMatrix.md`
  - 关键概念: Technical Debt, Global State Cluster, Documentation Boundary
  - 现有材料: `Documents/Plans/Plan_TechnicalDebtRefresh.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **5.2 外部参考仓库吸收路线**
  - [ ] 5.2.1 Hazelight 参考点的吸收方式
  - [ ] 5.2.2 UnrealCSharp 架构参考的适用边界
  - [ ] 5.2.3 与官方 AngelScript 版本演进的关系
  - 关键源码: `Documents/Hazelight/`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Plans/Plan_AS238*.md`
  - 关键概念: Reference Repo, Absorption, Non-goals, Upstream Sync
  - 现有材料: `Reference/README.md`, `AGENTS.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

- [ ] **5.3 插件工程化硬化与发布准备**
  - [ ] 5.3.1 可复用插件的工程硬化基线
  - [ ] 5.3.2 构建 / 测试 / 发布资料如何与架构文档配套
  - [ ] 5.3.3 哪些文档属于架构主线，哪些属于治理附录
  - 关键源码: `Documents/Plans/Plan_PluginEngineeringHardening.md`, `Documents/Guides/Build.md`, `Documents/Guides/Test.md`
  - 关键概念: Plugin Hardening, Release Readiness, Documentation Stack
  - 现有材料: `Documents/Rules/GitCommitRule.md`
  - [ ] 补充审查：回顾本章内容，将发现的关联主题追加到 TodoList

---

## 附录（可选）

- [ ] A.1 源码文件索引表
- [ ] A.2 模块 / 子系统关系图
- [ ] A.3 外部参考对照表
- [ ] A.4 测试专题到运行时子系统映射表
- [ ] A.5 关键生成链路 ASCII 流程图索引
