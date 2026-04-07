# Angelscript 插件架构源码分析 TodoList

> 生成日期: 2026-04-06
> 分析范围: `Plugins/Angelscript` 相关架构文章梳理；覆盖现有架构文档归并与待补专题，不展开宿主工程业务逻辑

---

## 第一部分：插件总体架构

- [x] **1.1 插件定位与模块边界**
  - [x] 1.1.1 插件目标与宿主工程边界 → `01_01_01_Plugin_Goal_And_Host_Boundary.md`
  - [x] 1.1.2 `AngelscriptRuntime` / `AngelscriptEditor` / `AngelscriptTest` / `Dump` 目录职责 → `01_01_02_Directory_Responsibilities.md`
  - [x] 1.1.3 现有架构文档导航与阅读顺序 → `01_01_03_Document_Navigation_And_Reading_Order.md`
  - 关键源码: `AGENTS.md`, `Plugins/Angelscript/AGENTS.md`, `Plugins/Angelscript/Angelscript.uplugin`
  - 关键概念: Plugin-centric, Runtime, Editor, Test, Dump
  - 现有材料: `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`
  - [x] 补充审查：补齐缺失入口文档 `01_01_01_Plugin_Goal_And_Host_Boundary.md`；本章未发现新的待拆专题

- [x] **1.2 Runtime 总控与生命周期**
  - [x] 1.2.1 `FAngelscriptRuntimeModule` 初始化入口 → `01_02_01_RuntimeModule_Initialization_Entry.md`
  - [x] 1.2.2 `FAngelscriptEngine` 启动、编译、重载主链路 → `01_02_02_Engine_Startup_Compile_Reload_Main_Flow.md`
  - [x] 1.2.3 全局状态入口与状态边界 → `01_02_03_Global_State_Entry_And_Boundaries.md`
  - [x] 1.2.4 Types / Functions / Code / Globals 四阶段编译流 → `01_02_04_Four_Stage_Compilation_Flow.md`
  - [x] 1.2.5 Diagnostics、错误收集与调试输出面 → `01_02_05_Diagnostics_Error_Collection_And_Output_Surface.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`
  - 关键概念: Initialize, InitialCompile, PerformHotReload, Global State, Diagnostics
  - 现有材料: `Documents/Guides/GlobalStateContainmentMatrix.md`, `Documents/Plans/Plan_FullDeGlobalization.md`
  - [x] 补充审查：本章主链已覆盖 Runtime 初始化、编译、重载与状态边界，未追加新专题

- [x] **1.3 Editor / Test / Dump 协作边界**
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

- [x] **1.4 插件模块清单与装载关系**
  - [x] 1.4.1 `Angelscript.uplugin` 中模块声明与 LoadingPhase → `01_04_01_Module_Declarations_And_Loading_Phase.md`
  - [x] 1.4.2 Runtime / Editor / Test 的插件依赖面 → `01_04_02_Module_Dependency_Surface_Runtime_Editor_Test.md`
  - [x] 1.4.3 `StructUtils` / `EnhancedInput` / `GameplayAbilities` 等外部插件关系 → `01_04_03_External_Plugin_Relationships.md`
  - [x] 1.4.4 Runtime 的条件依赖与 Editor-Only 边界处理 → `01_04_04_Runtime_Conditional_Dependencies_And_Editor_Only_Boundary.md`
  - [x] 1.4.5 `.uplugin` `Plugins` 声明与 `Build.cs` 依赖的一致性约束 → `01_04_05_Plugin_Descriptor_And_BuildCs_Consistency.md`
  - 关键源码: `Plugins/Angelscript/Angelscript.uplugin`, `Plugins/Angelscript/Source/*/*.Build.cs`
  - 关键概念: LoadingPhase, Module Type, Plugin Dependency
  - 现有材料: `AGENTS.md`
  - [x] 补充审查：已追加 `1.4.4 Runtime 的条件依赖与 Editor-Only 边界处理` 与 `1.4.5 .uplugin Plugins 声明与 Build.cs 依赖的一致性约束`

- [x] **1.5 UHT 工具链位置与边界**
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

- [x] **2.1 脚本类生成机制**
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

- [x] **2.2 脚本结构体生成机制**
  - [x] 2.2.1 `UASStruct` 与 `FASStructOps` 分层 → `02_02_01_UASStruct_And_FASStructOps_Layering.md`
  - [x] 2.2.2 FakeVTable 注入与生命周期操作 → `02_02_Script_Struct_Generation_Mechanism.md`
  - [x] 2.2.3 与 Hazelight 方案的差异边界 → `02_02_Script_Struct_Generation_Mechanism.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.cpp`
  - 关键概念: UASStruct, ICppStructOps, FakeVTable, Hash, Identical
  - 现有材料: `Documents/Hazelight/ScriptStructImplementation.md`
  - [x] 补充审查：本章已补齐 FakeVTable 与 Hazelight 差异边界，未追加新专题

- [x] **2.3 预处理与模块描述符**
  - [x] 2.3.1 脚本文件发现与 import 解析 → `02_03_Preprocessor_And_Module_Descriptors.md`
  - [x] 2.3.2 `FAngelscriptModuleDesc` / `ClassDesc` / `EnumDesc` / `DelegateDesc` → `02_03_Preprocessor_And_Module_Descriptors.md`
  - [x] 2.3.3 文件到类型的组织边界 → `02_03_Preprocessor_And_Module_Descriptors.md`
  - [x] 2.3.4 Chunk / Macro / Import 的预处理模型 → `02_03_Preprocessor_And_Module_Descriptors.md`
  - [x] 2.3.5 Import 排序、循环依赖与装配顺序 → `02_03_Preprocessor_And_Module_Descriptors.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.h`
  - 关键概念: ModuleDesc, ClassDesc, Script Roots, Import Resolution, Chunk, Macro, ImportChain
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [x] 补充审查：本章已覆盖文件发现、模块描述符、chunk 模型和装配顺序，未追加新专题

- [x] **2.4 Bind 系统与 Native 绑定生成**
  - [x] 2.4.1 手写 Bind 体系如何接入 UE 反射与脚本层 → `02_04_Bind_System_And_Native_Binding_Generation.md`
  - [x] 2.4.2 `GenerateNativeBinds()` 输出链路与产物边界 → `02_04_Bind_System_And_Native_Binding_Generation.md`
  - [x] 2.4.3 Runtime / Editor Bind 模块分工 → `02_04_Bind_System_And_Native_Binding_Generation.md`
  - [x] 2.4.4 `FAngelscriptBindState` 注册时序与执行生命周期 → `02_04_Bind_System_And_Native_Binding_Generation.md`
  - [x] 2.4.5 `FAngelscriptBindDatabase` 在 cooked 场景中的缓存职责 → `02_04_Bind_System_And_Native_Binding_Generation.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`
  - 关键概念: BindDatabase, BindState, ASRuntimeBind, ASEditorBind, Generated Artifacts
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Plans/Plan_HazelightBindModuleMigration.md`
  - [x] 补充审查：本章已覆盖 Bind 注册、生成产物、Runtime/Editor 分工与 cooked 缓存职责，未追加新专题

- [x] **2.5 脚本函数调用桥与 FunctionCaller 体系**
  - [x] 2.5.1 Script Function 到 UE 调用栈的桥接方式 → `02_05_Script_Function_Bridge_And_FunctionCaller_System.md`
  - [x] 2.5.2 `FunctionCallers.h` 中的调用器分层 → `02_05_Script_Function_Bridge_And_FunctionCaller_System.md`
  - [x] 2.5.3 参数封送、返回值与错误传播 → `02_05_Script_Function_Bridge_And_FunctionCaller_System.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/FunctionCallers.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`
  - 关键概念: Function Caller, Marshaling, Return Path, Invocation Bridge
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [x] 补充审查：本章已覆盖调用桥、调用器分层与参数封送模型，未追加新专题

- [x] **2.6 FunctionLibrary 与脚本可见 API 暴露面**
  - [x] 2.6.1 Runtime FunctionLibraries 的组织方式 → `02_06_FunctionLibrary_And_Script_Visible_API_Surface.md`
  - [x] 2.6.2 Editor FunctionLibraries 与运行时隔离 → `02_06_FunctionLibrary_And_Script_Visible_API_Surface.md`
  - [x] 2.6.3 脚本层 API 暴露边界与常见模式 → `02_06_FunctionLibrary_And_Script_Visible_API_Surface.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionLibraries/`, `Plugins/Angelscript/Source/AngelscriptEditor/Public/FunctionLibraries/`
  - 关键概念: Function Library, Script-visible API, Editor-only Exposure
  - 现有材料: `Plugins/Angelscript/AGENTS.md`
  - [x] 补充审查：本章已覆盖 Runtime/Editor 库分离与脚本 API 暴露模式，未追加新专题

- [x] **2.7 BaseClasses 与脚本基类扩展策略**
  - [x] 2.7.1 Runtime 基类封装的目的与范围 → `02_07_BaseClasses_And_Script_Base_Extension_Strategy.md`
  - [x] 2.7.2 脚本继承链与原生继承链的衔接点 → `02_07_BaseClasses_And_Script_Base_Extension_Strategy.md`
  - [x] 2.7.3 BaseClasses 与类生成 / Bind 的耦合边界 → `02_07_BaseClasses_And_Script_Base_Extension_Strategy.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/BaseClasses/`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/`
  - 关键概念: Base Class, Script Inheritance, Native Integration
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [x] 补充审查：本章已覆盖 subsystem 基座与类生成/Bind 的边界，未追加新专题

- [x] **2.8 类型系统核心与脚本值表达**
  - [x] 2.8.1 `FAngelscriptType` / `FAngelscriptTypeUsage` 的桥接职责 → `02_08_Type_System_Core_And_Script_Value_Representation.md`
  - [x] 2.8.2 属性绑定、GC 引用信息与调试值提取 → `02_08_Type_System_Core_And_Script_Value_Representation.md`
  - [x] 2.8.3 类型系统如何服务 ClassGenerator / Debugger / Bind → `02_08_Type_System_Core_And_Script_Value_Representation.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.cpp`
  - 关键概念: Type Usage, Property Binding, GC Reference Info, Debugger Value
  - 现有材料: `Documents/Hazelight/ScriptClassImplementation.md`
  - [x] 补充审查：本章已覆盖 Type / TypeUsage 分层与横向服务边界，未追加新专题

---

## 第三部分：运行时支撑子系统

- [x] **3.1 热重载与文件变更链路**
  - [x] 3.1.1 文件发现与变更感知入口 → `03_01_Hot_Reload_And_File_Change_Pipeline.md`
  - [x] 3.1.2 reload requirement 传播与 class rebind → `03_01_Hot_Reload_And_File_Change_Pipeline.md`
  - [x] 3.1.3 Editor / Runtime 在重载中的职责划分 → `03_01_Hot_Reload_And_File_Change_Pipeline.md`
  - [x] 3.1.4 DirectoryWatcher 与轮询保底策略 → `03_01_Hot_Reload_And_File_Change_Pipeline.md`
  - [x] 3.1.5 编译失败重试、排队与延迟恢复 → `03_01_Hot_Reload_And_File_Change_Pipeline.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/`
  - 关键概念: File Change Detection, Directory Watcher, Reload Requirement, Rebind, Soft Reload, Full Reload
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [x] 补充审查：本章已覆盖变更感知、reload requirement 与恢复链，未追加新专题

- [x] **3.2 StaticJIT 与执行性能路径**
  - [x] 3.2.1 StaticJIT 数据结构与数据库组织 → `03_02_StaticJIT_And_Execution_Performance_Paths.md`
  - [x] 3.2.2 解释执行与 JIT 执行分流 → `03_02_StaticJIT_And_Execution_Performance_Paths.md`
  - [x] 3.2.3 预编译 / 缓存与限制边界 → `03_02_StaticJIT_And_Execution_Performance_Paths.md`
  - [x] 3.2.4 `PrecompiledData` 序列化与三阶段应用 → `03_02_StaticJIT_And_Execution_Performance_Paths.md`
  - [x] 3.2.5 `FJITDatabase` 的函数映射与查找路径 → `03_02_StaticJIT_And_Execution_Performance_Paths.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`
  - 关键概念: StaticJIT, JIT Database, Precompiled Data, Call Path, Performance Tradeoff
  - 现有材料: `Documents/Plans/Plan_StaticJITUnitTests.md`
  - [x] 补充审查：本章已覆盖 JIT 分流、缓存与数据库职责，未追加新专题

- [x] **3.3 Debugger 与调试协议集成**
  - [x] 3.3.1 Debug Server 与连接生命周期 → `03_03_Debugger_And_Debug_Protocol_Integration.md`
  - [x] 3.3.2 断点、堆栈与脚本控制面 → `03_03_Debugger_And_Debug_Protocol_Integration.md`
  - [x] 3.3.3 调试能力与测试验证边界 → `03_03_Debugger_And_Debug_Protocol_Integration.md`
  - [x] 3.3.4 Data Breakpoint / Watchpoint 机制 → `03_03_Debugger_And_Debug_Protocol_Integration.md`
  - [x] 3.3.5 调试值提取、作用域与变量序列化 → `03_03_Debugger_And_Debug_Protocol_Integration.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/`, `Plugins/Angelscript/Source/AngelscriptTest/Debugger/`
  - 关键概念: Debug Server, Breakpoint, Data Breakpoint, Call Stack, Debugger Scope, Remote Control
  - 现有材料: `Documents/Plans/Plan_DebugAdapter.md`, `Documents/Plans/Plan_ASDebuggerUnitTest.md`
  - [x] 补充审查：本章已覆盖调试协议、值提取与测试边界，未追加新专题

- [x] **3.4 State Dump 可观测性架构**
  - [x] 3.4.1 `FAngelscriptStateDump::DumpAll()` 总入口 → `03_04_State_Dump_Observability_Architecture.md`
  - [x] 3.4.2 Runtime / Test / Editor 三侧导出链路 → `03_04_State_Dump_Observability_Architecture.md`
  - [x] 3.4.3 外部观察者模式与扩展点设计 → `03_04_State_Dump_Observability_Architecture.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/AngelscriptStateDump.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/`
  - 关键概念: State Dump, CSV Export, Observer Pattern, Dump Extension
  - 现有材料: `Documents/Plans/Archives/Plan_ASEngineStateDump.md`
  - [x] 补充审查：本章已覆盖 Dump 总入口与 observer 扩展边界，未追加新专题

- [x] **3.5 全局状态治理与外部参考吸收边界**
  - [x] 3.5.1 全局状态 containment 模式与问题分类 → `03_05_Global_State_Governance_And_Reference_Absorption_Boundaries.md`
  - [x] 3.5.2 去全局化阶段规划 → `03_05_Global_State_Governance_And_Reference_Absorption_Boundaries.md`
  - [x] 3.5.3 Hazelight / UnrealCSharp 参考点与不可照搬项 → `03_05_Global_State_Governance_And_Reference_Absorption_Boundaries.md`
  - 关键源码: `Documents/Guides/GlobalStateContainmentMatrix.md`, `Documents/Plans/Plan_FullDeGlobalization.md`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Hazelight/ScriptClassImplementation.md`, `Documents/Hazelight/ScriptStructImplementation.md`
  - 关键概念: Containment, DeGlobalization, Reference Absorption, Non-Target Features
  - 现有材料: `Documents/Guides/TechnicalDebtInventory.md`
  - [x] 补充审查：本章已把 containment 与外部参考吸收并入同一治理视角，未追加新专题

- [x] **3.6 CodeCoverage 与脚本覆盖率统计链路**
  - [x] 3.6.1 覆盖率数据采集入口 → `03_06_CodeCoverage_And_Coverage_Statistics_Pipeline.md`
  - [x] 3.6.2 覆盖率聚合与输出格式 → `03_06_CodeCoverage_And_Coverage_Statistics_Pipeline.md`
  - [x] 3.6.3 覆盖率能力与测试/验证体系的关系 → `03_06_CodeCoverage_And_Coverage_Statistics_Pipeline.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/CodeCoverage/`, `Plugins/Angelscript/Source/AngelscriptRuntime/Testing/`
  - 关键概念: Code Coverage, Hit Data, Report Output
  - 现有材料: `Documents/Guides/Test.md`
  - [x] 补充审查：本章已覆盖 coverage 采集、输出与测试集成，未追加新专题

- [x] **3.7 Hash / 元数据辅助子系统**
  - [x] 3.7.1 Hash 子系统承担的定位与用途 → `03_07_Hash_And_Metadata_Auxiliary_Subsystem.md`
  - [x] 3.7.2 关键数据结构与哈希边界 → `03_07_Hash_And_Metadata_Auxiliary_Subsystem.md`
  - [x] 3.7.3 与预处理、绑定、缓存的协作点 → `03_07_Hash_And_Metadata_Auxiliary_Subsystem.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Hash/`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/`
  - 关键概念: Hashing, Metadata Key, Cache Identity
  - 现有材料: `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
  - [x] 补充审查：本章已覆盖 Hash 辅助定位与协作边界，未追加新专题

- [x] **3.8 ThirdParty AngelScript 内核集成边界**
  - [x] 3.8.1 ThirdParty 源码镜像与本地修改策略 → `03_08_ThirdParty_AngelScript_Kernel_Integration_Boundaries.md`
  - [x] 3.8.2 Parser / ScriptEngine / ScriptFunction 等核心内核点 → `03_08_ThirdParty_AngelScript_Kernel_Integration_Boundaries.md`
  - [x] 3.8.3 上游升级与 fork 差异管理 → `03_08_ThirdParty_AngelScript_Kernel_Integration_Boundaries.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptengine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptfunction.cpp`
  - 关键概念: ThirdParty Fork, Parser, ScriptEngine, Upgrade Strategy
  - 现有材料: `Documents/Plans/Plan_AS238BugfixCherryPick.md`, `Documents/Guides/ASSDK_Fork_Differences.md`
  - [x] 补充审查：本章已覆盖 fork 边界与选择性升级策略，未追加新专题

---

## 第四部分：测试与验证架构

- [x] **4.1 测试模块总体分层**
  - [x] 4.1.1 `Native` / `Learning` / `Shared` / `Validation` 的职责差异 → `04_01_Test_Module_Overall_Layering.md`
  - [x] 4.1.2 按主题目录组织的测试专题图 → `04_01_Test_Module_Overall_Layering.md`
  - [x] 4.1.3 测试目录与 Runtime / Editor 子系统的映射方式 → `04_01_Test_Module_Overall_Layering.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/`, `Plugins/Angelscript/AGENTS.md`
  - 关键概念: Test Layer, Topic-first Layout, Shared Fixture
  - 现有材料: `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`
  - [x] 补充审查：本章已覆盖测试层级矩阵与目录映射，未追加新专题

- [x] **4.2 测试基础设施与 Shared Helper**
  - [x] 4.2.1 `AngelscriptTestEngineHelper` 生命周期管理 → `04_02_Test_Infrastructure_And_Shared_Helpers.md`
  - [x] 4.2.2 `AngelscriptTestUtilities` / `Macros` 的职责边界 → `04_02_Test_Infrastructure_And_Shared_Helpers.md`
  - [x] 4.2.3 Shared Fixture 如何支撑 Debugger / Scenario / Native 测试 → `04_02_Test_Infrastructure_And_Shared_Helpers.md`
  - [x] 4.2.4 `AngelscriptDebuggerTestSession` / Client / Fixture 的协作方式 → `04_02_Test_Infrastructure_And_Shared_Helpers.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestEngineHelper.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h`
  - 关键概念: Test Engine Helper, Shared Fixture, Test Macro, Debugger Test Session
  - 现有材料: `Documents/Guides/TestConventions.md`, `Documents/Plans/Plan_TestEngineIsolation.md`
  - [x] 补充审查：本章已覆盖宏层、工具层与 session fixture 边界，未追加新专题

- [x] **4.3 主题测试簇与架构映射**
  - [x] 4.3.1 `ClassGenerator` / `Preprocessor` / `HotReload` 专题测试如何覆盖主链路 → `04_03_Topic_Test_Clusters_And_Architecture_Mapping.md`
  - [x] 4.3.2 `Actor` / `Component` / `Interface` / `Delegate` 等行为专题的组织方式 → `04_03_Topic_Test_Clusters_And_Architecture_Mapping.md`
  - [x] 4.3.3 `Debugger` / `Dump` / `Subsystem` 等支撑专题的验证入口 → `04_03_Topic_Test_Clusters_And_Architecture_Mapping.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptTest/ClassGenerator/`, `Plugins/Angelscript/Source/AngelscriptTest/Preprocessor/`, `Plugins/Angelscript/Source/AngelscriptTest/HotReload/`, `Plugins/Angelscript/Source/AngelscriptTest/Debugger/`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/`
  - 关键概念: Topic Tests, Coverage Mapping, Validation Entry
  - 现有材料: `Documents/Guides/TestCatalog.md`, `Documents/Guides/TestConventions.md`
  - [x] 补充审查：本章已建立测试簇与运行时子系统映射，未追加新专题

- [x] **4.4 Runtime 内部测试与覆盖边界**
  - [x] 4.4.1 `AngelscriptRuntime/Tests` 与 `AngelscriptTest` 的边界 → `04_04_Runtime_Internal_Tests_And_Coverage_Boundaries.md`
  - [x] 4.4.2 `Angelscript.CppTests.*` 自动化前缀的作用域 → `04_04_Runtime_Internal_Tests_And_Coverage_Boundaries.md`
  - [x] 4.4.3 内部测试如何服务运行时重构与验证 → `04_04_Runtime_Internal_Tests_And_Coverage_Boundaries.md`
  - [x] 4.4.4 Native Core 适配层与原始 AngelScript API 测试边界 → `04_04_Runtime_Internal_Tests_And_Coverage_Boundaries.md`
  - 关键源码: `Plugins/Angelscript/Source/AngelscriptRuntime/Tests/`, `Plugins/Angelscript/AGENTS.md`
  - 关键概念: Runtime Internal Tests, CppTests, Native Core Adapter, Refactor Safety Net
  - 现有材料: `Documents/Guides/TestConventions.md`
  - [x] 补充审查：本章已覆盖 Runtime 内部测试、Native Core 与 TestModule 的三分边界，未追加新专题

---

## 第五部分：工程治理与演进路线

- [x] **5.1 技术债与全局状态问题地图**
  - [x] 5.1.1 技术债条目如何映射回具体子系统 → `05_01_Technical_Debt_And_Global_State_Problem_Map.md`
  - [x] 5.1.2 全局状态问题的主题聚类 → `05_01_Technical_Debt_And_Global_State_Problem_Map.md`
  - [x] 5.1.3 哪些问题适合做架构文章，哪些只保留在 debt inventory → `05_01_Technical_Debt_And_Global_State_Problem_Map.md`
  - 关键源码: `Documents/Guides/TechnicalDebtInventory.md`, `Documents/Guides/GlobalStateContainmentMatrix.md`
  - 关键概念: Technical Debt, Global State Cluster, Documentation Boundary
  - 现有材料: `Documents/Plans/Plan_TechnicalDebtRefresh.md`
  - [x] 补充审查：本章已建立技术债到子系统与架构文章边界的映射，未追加新专题

- [x] **5.2 外部参考仓库吸收路线**
  - [x] 5.2.1 Hazelight 参考点的吸收方式 → `05_02_External_Reference_Repository_Absorption_Roadmap.md`
  - [x] 5.2.2 UnrealCSharp 架构参考的适用边界 → `05_02_External_Reference_Repository_Absorption_Roadmap.md`
  - [x] 5.2.3 与官方 AngelScript 版本演进的关系 → `05_02_External_Reference_Repository_Absorption_Roadmap.md`
  - 关键源码: `Documents/Hazelight/`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Plans/Plan_AS238*.md`
  - 关键概念: Reference Repo, Absorption, Non-goals, Upstream Sync
  - 现有材料: `Reference/README.md`, `AGENTS.md`
  - [x] 补充审查：本章已建立三类外部参考的分工与吸收边界，未追加新专题

- [x] **5.3 插件工程化硬化与发布准备**
  - [x] 5.3.1 可复用插件的工程硬化基线 → `05_03_Plugin_Engineering_Hardening_And_Release_Preparation.md`
  - [x] 5.3.2 构建 / 测试 / 发布资料如何与架构文档配套 → `05_03_Plugin_Engineering_Hardening_And_Release_Preparation.md`
  - [x] 5.3.3 哪些文档属于架构主线，哪些属于治理附录 → `05_03_Plugin_Engineering_Hardening_And_Release_Preparation.md`
  - 关键源码: `Documents/Plans/Plan_PluginEngineeringHardening.md`, `Documents/Guides/Build.md`, `Documents/Guides/Test.md`
  - 关键概念: Plugin Hardening, Release Readiness, Documentation Stack
  - 现有材料: `Documents/Rules/GitCommitRule.md`
  - [x] 补充审查：本章已把工程化硬化、发布准备与文档分层一起收束，未追加新专题

---

## 附录（可选）

- [x] A.1 源码文件索引表 → `Appendix_A1_Source_File_Index.md`
- [x] A.2 模块 / 子系统关系图 → `Appendix_A2_Module_And_Subsystem_Relationship_Map.md`
- [x] A.3 外部参考对照表 → `Appendix_A3_External_Reference_Comparison_Table.md`
- [x] A.4 测试专题到运行时子系统映射表 → `Appendix_A4_Test_Topic_To_Runtime_Subsystem_Mapping.md`
- [x] A.5 关键生成链路 ASCII 流程图索引 → `Appendix_A5_Key_Generation_Pipeline_ASCII_Flow_Index.md`
