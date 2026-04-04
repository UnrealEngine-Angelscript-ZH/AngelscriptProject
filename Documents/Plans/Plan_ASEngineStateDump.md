# AS 全插件状态 Dump

## 背景与目标

当前整个 Angelscript 插件几乎没有统一的 dump 机制。`FAngelscriptEngine` 持有大量运行时状态，绑定系统（`FAngelscriptBinds`、`FAngelscriptBindDatabase`、`FAngelscriptType`）有独立的全局注册表，StaticJIT/预编译子系统有运行时查找表和加载缓存，调试器有断点/客户端连接状态，代码覆盖率有命中计数，编辑器有热重载重绑映射和菜单扩展注册表——但开发者调试任何子系统时只能逐个打日志或在调试器里手工展开，效率极低。

**目标**：

1. 在 `AngelscriptRuntime` 模块实现一套**全插件 CSV 状态导出器**（`FAngelscriptStateDump`），将**所有子系统**的运行时状态分类输出为多张 CSV 表格，方便用 Excel / Google Sheets / Python 做离线分析。
2. 在 `AngelscriptTest` 模块新增 `Dump/` 目录，注册 **CVar** 触发 dump，并编写基础功能测试。
3. 导出器作为 Runtime 公共 API，任何模块（Editor、Test、Commandlet）都可调用。

## 范围与边界

- 核心导出逻辑放在 `AngelscriptRuntime`，CVar 触发和测试放在 `AngelscriptTest`。
- 导出文件格式为 **UTF-8 BOM CSV**（兼容中文路径和 Excel 直接打开）。
- 输出目录默认 `{ProjectSavedDir}/Angelscript/Dump/`，每次 dump 产生一个时间戳子目录。
- 第一期只做结构化快照导出，不做增量 diff、不做实时监控、不做 UI 面板。
- Editor 模块的状态（热重载重绑、菜单扩展）在 Phase 7 中通过 Editor 侧的独立 dump 扩展点接入。

## 核心架构原则：Dump 实现与原有代码完全隔离

### 问题

传统做法是在每个类内部添加 `Dump()`/`ToString()` 方法或 `friend` 声明，这会导致：
- 修改分散到数十个原有文件中，增加合并冲突和维护负担
- dump 逻辑与业务逻辑混杂，违反单一职责

### 解决方案：纯外部观察者模式

**所有 dump 逻辑都在 `AngelscriptRuntime/Dump/` 目录内，不修改任何已有文件。**

这得益于一项关键事实——对全插件的公开 API 审计显示，绝大多数状态已经是 public 可达的：

| 类型 | 可见性 | 说明 |
|------|--------|------|
| `FAngelscriptEngine` | **大部分 public** | `Engine`、`Diagnostics`、`AllRootPaths`、`PrecompiledData`、`StaticJIT`、`DebugServer` 等核心字段为 public；`GetActiveModules()`、`GetScriptEngine()`、`GetRuntimeConfig()`、`GetCreationMode()`、`GetInstanceId()`、`OwnsEngine()` 等 accessor 已存在 |
| `FAngelscriptModuleDesc` / `ClassDesc` / `PropertyDesc` / `FunctionDesc` / `EnumDesc` / `DelegateDesc` | **全部 public** | 纯数据 struct，无 private 段 |
| `FAngelscriptType` | **全部 public** | `GetTypes()` 为 public static |
| `FAngelscriptBindDatabase` | **全部 public** | `Get()` 为 public，`Structs`/`Classes`/`BoundEnums`/`BoundDelegateFunctions`/`HeaderLinks` 均为 public 成员 |
| `FAngelscriptBinds` | **大部分 public** | `GetAllRegisteredBindNames()`、`GetBindInfoList()`、`RuntimeClassDB`、`ClassFuncMaps`、`BindModuleNames` 均为 public static；仅 file-static 的内部排序数组不可直接访问 |
| `FAngelscriptDocs` | **全部 public** | `GetUnrealDocumentation()`、`GetFullUnrealDocumentation()` 等全部为 public static |
| `FJITDatabase` | **全部 public** | `Get()` 为 public static，`Functions`/`FunctionLookups` 等均为 public 成员 |
| `FAngelscriptPrecompiledData` | **全部 public** | 纯 struct，`DataGuid`/`Modules`/`OutputTimingData()` 等全部 public |
| `FAngelscriptDebugServer` | **大部分 public** | `Breakpoints`、`DataBreakpoints`、调试标志均为 public；仅 socket/client 容器为 private（可用 `HasAnyClients()` 替代） |
| `FAngelscriptCodeCoverage` | **大部分 public** | `CoverageEnabled`、`GetLineCoverage()` 为 public；`FilesToCoverage` 为 private（可通过已知模块名逐个查询） |
| `UAngelscriptSettings` | **全部 public** | UObject 反射 + `Get()` public static |
| `FToStringHelper` | **仅 Register/Reset** | 无枚举已注册类型的 API |

### 执行策略

1. **Phase 1-2（MVP，27 张表中的 12 张）**：**零修改原有文件**。100% 只用 public API。
2. **Phase 4-6（扩展表）**：对于 `MOSTLY_PUBLIC` 的子系统，用 public API 能覆盖的部分先导出，少量 private 数据在 `DumpSummary` 中标注 `PartialExport`。
3. **Phase 3（可选）**：对确实有高诊断价值但无 public API 的少量数据（如 `GlobalContextPool` 大小、`FileHotReloadState`），**仅在此 Phase** 集中添加最小化 const accessor。每个 accessor 在一次 commit 中完成，便于审查和回退。

### 与原有代码的关系

```
原有代码（不修改）                    新增代码（全在 Dump/）
┌─────────────────────┐              ┌──────────────────────────┐
│ FAngelscriptEngine  │ ──public──▶ │ FAngelscriptStateDump     │
│   .GetActiveModules()│              │   .DumpModules()          │
│   .GetScriptEngine() │              │   .DumpClasses()          │
│   .Diagnostics       │              │   .DumpAll()              │
│   ...                │              │                           │
├─────────────────────┤              │ FCSVWriter                │
│ FAngelscriptType    │ ──public──▶ │   .AddHeader()            │
│   ::GetTypes()       │              │   .AddRow()               │
├─────────────────────┤              │   .SaveToFile()           │
│ FAngelscriptBind    │              │                           │
│   Database::Get()    │ ──public──▶ │                           │
│   .Structs/.Classes  │              │                           │
├─────────────────────┤              └──────────────────────────┘
│ FJITDatabase::Get() │ ──public──▶
│   .Functions         │
│ ...                  │
└─────────────────────┘
```

**原则**：如果某个 private 数据没有 public accessor，就不导出它，而不是为了 dump 去修改原有类。只有在后续明确证明某个 private 数据的诊断价值极高时，才在单独的 Phase 中添加最小化 accessor。

## 全插件可 dump 子系统索引

在正式设计表结构之前，先列出全插件扫描发现的所有有意义的状态源，按 Phase 分配：

### Phase 1 — 引擎核心（Runtime，12 张表）

| 子系统 | 位置 | 状态要点 |
|--------|------|----------|
| `FAngelscriptEngine` 本体 | `Core/AngelscriptEngine.h` | 实例标识、创建模式、静态标志、编译状态、包/路径 |
| `FAngelscriptEngineConfig` | 同上 | 全量运行时配置 |
| `FAngelscriptModuleDesc` | 同上 | 活跃模块图：代码段、哈希、导入、测试函数 |
| `FAngelscriptClassDesc` | 同上 | 脚本类元数据：继承、接口、标志 |
| `FAngelscriptPropertyDesc` | 同上 | 脚本属性：类型、可见性、复制、序列化标志 |
| `FAngelscriptFunctionDesc` | 同上 | 脚本函数：签名、网络、BP 标志 |
| `FAngelscriptEnumDesc` | 同上 | 脚本枚举及其值 |
| `FAngelscriptDelegateDesc` | 同上 | 脚本委托签名 |
| `FAngelscriptType` 类型数据库 | `Core/AngelscriptType.h` | 所有注册的 C++ ↔ AS 类型映射 |
| `FAngelscriptEngine::Diagnostics` | 同上 | 编译诊断（错误/警告/信息） |
| `asIScriptEngine` VM 状态 | `angelscript.h` | 模块数、函数数、类型数、GC 统计 |
| Dump 元数据 | — | 汇总表 |

### Phase 2 — CVar 触发 + 测试（Test 模块）

### Phase 3 — accessor 补齐 + 文档

### Phase 4 — 绑定基础设施（Runtime，7 张表）

| 子系统 | 位置 | 状态要点 |
|--------|------|----------|
| `FAngelscriptBinds` 全局绑定表 | `Core/AngelscriptBinds.cpp` | `GetAllRegisteredBindNames()`、`SkipBinds`、`SkipBindNames`、`SkipBindClasses`、bound function/property 列表 |
| `FAngelscriptBindDatabase` 单例 | `Core/AngelscriptBindDatabase.h` | `Structs`、`Classes`、`BoundEnums`、`BoundDelegateFunctions`、`HeaderLinks` |
| `FToStringHelper` 注册表 | `Binds/Helper_ToString.h` | 所有注册了 ToString 转换的类型 |
| `FAngelscriptDocs` 文档数据库 | `Core/AngelscriptDocs.cpp` | 文档字符串统计（`UnrealDocumentation`、`UnrealTypeDocumentation` 等） |
| `UAngelscriptSettings` 配置 | `Core/AngelscriptSettings.h` | 所有 Config=Engine 设置项 |
| 热重载文件跟踪 | `FAngelscriptEngine` private | `FileHotReloadState`、`PreviouslyFailedReloadFiles`、`QueuedFullReloadFiles` |
| 上下文池 | `FAngelscriptEngine` private | `GlobalContextPool` 大小、线程局部 `GAngelscriptContextPool` |

### Phase 5 — JIT / 预编译子系统（Runtime，3 张表）

| 子系统 | 位置 | 状态要点 |
|--------|------|----------|
| `FJITDatabase` 单例 | `StaticJIT/AngelscriptStaticJIT.h` | JIT 函数查找表大小、GlobalVar / TypeInfo / PropertyOffset 查找表 |
| `FAngelscriptPrecompiledData` | `StaticJIT/PrecompiledData.h` | `DataGuid`、模块数、函数映射数、加载计时 |
| `FAngelscriptStaticJIT` | 同上 | JIT 文件数、待生成函数数、共享头数 |

### Phase 6 — 调试器 / 覆盖率 / 观测（Runtime，3 张表）

| 子系统 | 位置 | 状态要点 |
|--------|------|----------|
| `FAngelscriptDebugServer` | `Debugging/AngelscriptDebugServer.h` | 客户端连接数、断点列表、数据断点、暂停/调试状态、协议版本 |
| `FAngelscriptCodeCoverage` | 条件编译 `WITH_AS_COVERAGE` | 每文件每行命中次数、录制状态 |
| `FAngelscriptBindExecutionObservation` | `Testing/` | 绑定执行快照：调用次数、禁用/已执行 bind 名 |

### Phase 7 — 编辑器模块状态（Editor，2 张表）

| 子系统 | 位置 | 状态要点 |
|--------|------|----------|
| `FClassReloadHelper::ReloadState()` | `AngelscriptEditor/Private/ClassReloadHelper.h` | `ReloadClasses`、`NewClasses`、`ReloadEnums`、`NewEnums`、`ReloadStructs`、`ReloadDelegates` |
| `UScriptEditorMenuExtension::RegisteredExtensions` | `AngelscriptEditor/Public/EditorMenuExtensions/` | 全局注册的编辑器菜单扩展列表 |

---

## 导出 CSV 表格设计

每次 dump 在时间戳目录下产生以下文件。Phase 1 产生 1-12 号表，后续 Phase 逐步扩展。

### 1. `EngineOverview.csv` — 引擎概览

| 列名 | 来源 | 说明 |
|------|------|------|
| InstanceId | `GetInstanceId()` | 引擎实例标识 |
| CreationMode | `GetCreationMode()` | Full / Clone |
| OwnsEngine | `OwnsEngine()` | 是否拥有底层 asIScriptEngine |
| SourceEngineId | `GetSourceEngine()` | Clone 的源引擎 ID（Full 为空） |
| bIsInitialCompileFinished | 静态 | 初始编译是否完成 |
| bDidInitialCompileSucceed | 实例 | 初始编译是否成功 |
| bSimulateCooked | 静态 | 是否模拟 cooked |
| bUseEditorScripts | 静态 | 是否加载编辑器脚本 |
| bTestErrors | 静态 | 是否在测试错误模式 |
| bIsHotReloading | 静态 | 是否正在热重载 |
| bScriptDevelopmentMode | 静态 | 开发模式 |
| bGeneratePrecompiledData | 静态 | 是否生成预编译数据 |
| bUsePrecompiledData | 实例 | 是否使用预编译数据 |
| bCompletedAssetScan | 实例 | 资产扫描是否完成 |
| ActiveModuleCount | `GetActiveModules().Num()` | 活跃模块总数 |
| TotalClassCount | 各模块 Classes 求和 | 脚本类总数 |
| TotalEnumCount | 各模块 Enums 求和 | 脚本枚举总数 |
| TotalDelegateCount | 各模块 Delegates 求和 | 脚本委托总数 |
| RegisteredTypeCount | `FAngelscriptType::GetTypes().Num()` | 注册绑定类型总数 |
| BindRegistrationCount | `FAngelscriptBinds` | 注册 bind 函数总数 |
| JITFunctionCount | `FJITDatabase::Get()` | JIT 函数表大小 |
| DebugServerClients | `DebugServer` | 调试器客户端连接数 |
| ScriptRootPaths | `AllRootPaths` | 脚本根路径列表（分号分隔） |
| DiagnosticsCount | `Diagnostics.Num()` | 诊断条目文件数 |
| ContextPoolSize | 上下文池 | 全局上下文池大小 |
| DumpTimestamp | 系统时间 | dump 时刻 |

### 2. `RuntimeConfig.csv` — 运行时配置

| 列名 | 来源 |
|------|------|
| Key | 配置字段名 |
| Value | 配置字段值 |

逐字段输出 `FAngelscriptEngineConfig` 的所有 bool / int / string 成员，包括 `DisabledBindNames` 集合。

### 3. `Modules.csv` — 模块列表

| 列名 | 来源 | 说明 |
|------|------|------|
| ModuleName | `FAngelscriptModuleDesc::ModuleName` | 模块名 |
| CodeSectionCount | `Code.Num()` | 代码段数量 |
| CodeHash | `CodeHash` | 预处理代码哈希 |
| CombinedDependencyHash | `CombinedDependencyHash` | 组合依赖哈希 |
| ClassCount | `Classes.Num()` | 模块内类数 |
| EnumCount | `Enums.Num()` | 模块内枚举数 |
| DelegateCount | `Delegates.Num()` | 模块内委托数 |
| ImportedModules | 分号分隔列表 | 导入的模块名 |
| UnitTestCount | `UnitTestFunctions.Num()` | 单元测试数 |
| IntegrationTestCount | `IntegrationTestFunctions.Num()` | 集成测试数 |
| bCompileError | | 编译是否报错 |
| bLoadedPrecompiledCode | | 是否加载了预编译 |

### 4. `Classes.csv` — 脚本类列表

| 列名 | 说明 |
|------|------|
| ModuleName | 所属模块 |
| ClassName | 类名 |
| SuperClass | 父类名 |
| bIsStruct | 是否是 struct |
| bIsInterface | 是否是 interface |
| bAbstract | 是否抽象 |
| bTransient | 是否 transient |
| bPlaceable | 是否可放置 |
| bIsStaticsClass | 是否静态类 |
| bHideDropdown | 隐藏下拉 |
| bDefaultToInstanced | 默认实例化 |
| bEditInlineNew | 编辑内联新建 |
| bIsDeprecatedClass | 已废弃 |
| ImplementedInterfaces | 分号分隔接口列表 |
| PropertyCount | 属性数 |
| MethodCount | 方法数 |
| LineNumber | 源文件行号 |
| Namespace | 命名空间（可选） |
| ConfigName | 配置文件名 |
| CodeSuperClass | 原生 C++ 超类名 |

### 5. `Properties.csv` — 脚本属性列表

| 列名 | 说明 |
|------|------|
| ModuleName | 所属模块 |
| ClassName | 所属类 |
| PropertyName | 属性名 |
| LiteralType | 脚本字面类型 |
| bBlueprintReadable | BP 可读 |
| bBlueprintWritable | BP 可写 |
| bEditableOnDefaults | 默认值可编辑 |
| bEditableOnInstance | 实例可编辑 |
| bEditConst | 只读显示 |
| bTransient | 临时属性 |
| bReplicated | 是否复制 |
| ReplicationCondition | 复制条件 |
| bRepNotify | 复制通知 |
| bConfig | 配置属性 |
| bInterp | Matinee/Sequencer |
| bSaveGame | 存档属性 |
| bIsPrivate | 私有 |
| bIsProtected | 受保护 |
| LineNumber | 行号 |

### 6. `Functions.csv` — 脚本函数列表

| 列名 | 说明 |
|------|------|
| ModuleName | 所属模块 |
| ClassName | 所属类 |
| FunctionName | 函数名 |
| ScriptFunctionName | 脚本函数名 |
| ReturnType | 返回类型 |
| ArgumentCount | 参数数 |
| Arguments | 参数签名摘要 |
| bBlueprintCallable | BP 可调用 |
| bBlueprintEvent | BP 事件 |
| bBlueprintPure | BP 纯函数 |
| bNetFunction | 网络函数 |
| bNetMulticast | 多播 |
| bNetClient | 客户端 |
| bNetServer | 服务端 |
| bUnreliable | 不可靠 |
| bExec | 控制台命令 |
| bIsStatic | 静态 |
| bIsConstMethod | const |
| bThreadSafe | 线程安全 |
| bIsNoOp | 空实现 |
| bIsPrivate | 私有 |
| bIsProtected | 受保护 |
| LineNumber | 行号 |

### 7. `Enums.csv` — 脚本枚举列表

| 列名 | 说明 |
|------|------|
| ModuleName | 所属模块 |
| EnumName | 枚举名 |
| ValueCount | 值数量 |
| Values | 名称=数值列表（分号分隔） |
| LineNumber | 行号 |

### 8. `Delegates.csv` — 脚本委托列表

| 列名 | 说明 |
|------|------|
| ModuleName | 所属模块 |
| DelegateName | 委托名 |
| bIsMulticast | 多播 |
| SignatureReturnType | 签名返回类型 |
| SignatureArguments | 签名参数摘要 |
| LineNumber | 行号 |

### 9. `RegisteredTypes.csv` — 绑定类型数据库

| 列名 | 说明 |
|------|------|
| AngelscriptTypeName | AS 类型名 |
| AngelscriptDeclaration | AS 声明 |
| HasUClass | 是否有 UClass 映射 |

### 10. `Diagnostics.csv` — 编译诊断信息

| 列名 | 说明 |
|------|------|
| Filename | 文件名 |
| Row | 行 |
| Column | 列 |
| bIsError | 是否错误 |
| bIsInfo | 是否信息 |
| Message | 诊断消息 |

### 11. `ScriptEngineState.csv` — asIScriptEngine 底层状态

| 列名 | 说明 |
|------|------|
| Key | 属性名（ModuleCount / GlobalFunctionCount / RegisteredTypeCount / GCSize 等） |
| Value | 值 |

通过 `asIScriptEngine` 公共 API（`GetModuleCount()`、`GetGlobalFunctionCount()`、`GetObjectTypeCount()`、`GetGCStatistics()` 等）收集。

### 12. `DumpSummary.csv` — Dump 摘要

| 列名 | 说明 |
|------|------|
| Table | CSV 文件名 |
| RowCount | 导出行数 |
| Status | Success / Error |
| ErrorMessage | 错误信息 |

### 13. `BindRegistrations.csv` — C++ 绑定注册表（Phase 4）

| 列名 | 说明 |
|------|------|
| BindName | 绑定名称 |
| BindModule | 所属 bind 模块 |
| bIsSkipped | 是否被跳过 |
| SkipReason | 跳过原因（DisabledBindNames / SkipBindClasses 等） |

通过 `FAngelscriptBinds::GetAllRegisteredBindNames()` 和 skip 列表交叉导出。

### 14. `BindDatabase_Structs.csv` — Bind 缓存：结构体（Phase 4）

| 列名 | 说明 |
|------|------|
| StructName | 结构体名 |
| BindName | 绑定名 |
| PropertyCount | 属性数 |
| MethodCount | 方法数 |

### 15. `BindDatabase_Classes.csv` — Bind 缓存：类（Phase 4）

| 列名 | 说明 |
|------|------|
| ClassName | 类名 |
| BindName | 绑定名 |
| FunctionCount | 函数数 |

### 16. `ToStringTypes.csv` — ToString 注册表（Phase 4）

| 列名 | 说明 |
|------|------|
| TypeName | 类型名称 |

遍历 `FToStringHelper` 的注册列表。

### 17. `DocumentationStats.csv` — 文档数据库统计（Phase 4）

| 列名 | 说明 |
|------|------|
| Category | 文档类别（UnrealDocumentation / TypeDocumentation / PropertyDocumentation 等） |
| EntryCount | 条目数 |

### 18. `EngineSettings.csv` — 引擎配置设置（Phase 4）

| 列名 | 说明 |
|------|------|
| Key | 设置名 |
| Value | 设置值 |
| Category | 分类 |

逐字段输出 `UAngelscriptSettings` 的所有 UPROPERTY。

### 19. `HotReloadState.csv` — 热重载文件跟踪（Phase 4）

| 列名 | 说明 |
|------|------|
| FilePath | 文件路径 |
| State | 状态（Tracked / PendingReload / PreviouslyFailed / QueuedFullReload） |
| LastChangeTime | 最后变更时间 |

### 20. `JITDatabase.csv` — JIT 查找表（Phase 5）

| 列名 | 说明 |
|------|------|
| Category | 表名（Functions / FunctionLookups / GlobalVarLookups / TypeInfoLookups 等） |
| EntryCount | 条目数 |
| Details | 摘要信息 |

### 21. `PrecompiledData.csv` — 预编译数据统计（Phase 5）

| 列名 | 说明 |
|------|------|
| DataGuid | 预编译包 GUID |
| ModuleCount | 预编译模块数 |
| FunctionMappingCount | 函数映射数 |
| ClassesLoadedCount | 已加载的预编译类数 |
| TimingData | 加载耗时统计 |

### 22. `StaticJITState.csv` — Static JIT 生成状态（Phase 5）

| 列名 | 说明 |
|------|------|
| JITFileCount | JIT 文件数 |
| FunctionsToGenerateCount | 待生成函数数 |
| SharedHeaderCount | 共享头数 |
| ComputedOffsetsCount | 已计算偏移数 |

### 23. `DebugServerState.csv` — 调试器状态（Phase 6）

| 列名 | 说明 |
|------|------|
| Key | 属性名 |
| Value | 值 |

输出：客户端连接数、断点总数、数据断点数、`bIsPaused`、`bIsDebugging`、协议版本等。

### 24. `DebugBreakpoints.csv` — 调试器断点列表（Phase 6）

| 列名 | 说明 |
|------|------|
| Filename | 文件路径 |
| Line | 行号 |
| bIsEnabled | 是否启用 |
| Condition | 条件表达式（如有） |

### 25. `CodeCoverage.csv` — 代码覆盖率数据（Phase 6）

| 列名 | 说明 |
|------|------|
| Filename | 文件路径 |
| LineNumber | 行号 |
| HitCount | 命中次数 |

条件编译 `WITH_AS_COVERAGE`，仅在覆盖率录制后有数据。

### 26. `EditorReloadState.csv` — 编辑器热重载重绑映射（Phase 7）

| 列名 | 说明 |
|------|------|
| Category | 类型（ReloadClass / NewClass / ReloadEnum / NewEnum / ReloadStruct / ReloadDelegate） |
| OldName | 原名称（重载项有值） |
| NewName | 新名称 |

### 27. `EditorMenuExtensions.csv` — 编辑器菜单扩展注册表（Phase 7）

| 列名 | 说明 |
|------|------|
| ExtensionPoint | 扩展挂载点 |
| Location | 位置枚举 |
| SectionName | 分区名 |

---

## Phase 1：Runtime 导出器核心实现

> 目标：在 `AngelscriptRuntime` 实现 `FAngelscriptStateDump`，提供公共 API 将引擎状态导出为 CSV 文件集。

- [ ] **P1.1** 新建 `AngelscriptRuntime/Dump/` 目录，创建 `AngelscriptStateDump.h` 和 `AngelscriptStateDump.cpp`
  - 当前 Runtime 模块没有 `Dump/` 目录；`Debugging/` 负责调试器通信，状态 dump 是独立功能，应建立专门目录
  - 在 `AngelscriptStateDump.h` 中声明 `struct FAngelscriptStateDump`，标记 `ANGELSCRIPTRUNTIME_API` 以供外部模块调用
  - 主入口为 `static FString DumpAll(FAngelscriptEngine& Engine, const FString& OutputDir = TEXT(""))`，返回实际输出目录路径
  - 当 `OutputDir` 为空时，自动使用 `{FPaths::ProjectSavedDir()}/Angelscript/Dump/{Timestamp}/`
- [ ] **P1.1** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: add FAngelscriptStateDump skeleton with DumpAll entry point`

- [ ] **P1.2** 实现 CSV 写入辅助工具
  - 新建 `AngelscriptCSVWriter.h`（同目录），提供 `FCSVWriter` 辅助类：`AddHeader()` / `AddRow()` / `SaveToFile()`
  - 输出 UTF-8 BOM 前缀（`\xEF\xBB\xBF`）以兼容 Excel
  - CSV 字段含逗号或换行时自动用双引号包裹，双引号用 `""` 转义
  - 这是纯工具类，不依赖引擎状态，便于单测
- [ ] **P1.2** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: add FCSVWriter utility for CSV serialization`

- [ ] **P1.3** 实现 `DumpEngineOverview()` — EngineOverview.csv
  - 读取 `FAngelscriptEngine` 的静态标志（`bSimulateCooked`、`bUseEditorScripts` 等）和实例属性
  - 通过 `GetActiveModules()` 遍历所有模块，汇总 Class / Enum / Delegate 总数
  - 通过 `FAngelscriptType::GetTypes()` 获取注册类型总数
  - 单行输出所有概览字段
- [ ] **P1.3** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpEngineOverview CSV export`

- [ ] **P1.4** 实现 `DumpRuntimeConfig()` — RuntimeConfig.csv
  - 通过 `Engine.GetRuntimeConfig()` 获取 `FAngelscriptEngineConfig`，逐字段以 Key/Value 格式输出
  - `DisabledBindNames` 集合序列化为分号分隔字符串
- [ ] **P1.4** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpRuntimeConfig CSV export`

- [ ] **P1.5** 实现 `DumpModules()` — Modules.csv
  - 遍历 `GetActiveModules()` 返回的所有 `FAngelscriptModuleDesc`
  - 每行输出模块名、代码段数、哈希、各类型计数、导入列表、测试计数、编译错误标记
- [ ] **P1.5** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpModules CSV export`

- [ ] **P1.6** 实现 `DumpClasses()` — Classes.csv
  - 双重遍历：模块 → 模块内 Classes
  - 每行输出一个 `FAngelscriptClassDesc` 的全字段，`CodeSuperClass` 通过 `GetFName()` 转为字符串
  - `ImplementedInterfaces` 用分号分隔
- [ ] **P1.6** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpClasses CSV export`

- [ ] **P1.7** 实现 `DumpProperties()` — Properties.csv
  - 三重遍历：模块 → 类 → 属性
  - 每行输出一个 `FAngelscriptPropertyDesc` 的全字段
  - `ReplicationCondition` 输出枚举的整数值和名称
- [ ] **P1.7** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpProperties CSV export`

- [ ] **P1.8** 实现 `DumpFunctions()` — Functions.csv
  - 三重遍历：模块 → 类 → 方法
  - 参数摘要格式：`TypeName ArgName, TypeName ArgName`（简化可读）
  - `ReturnType` 输出 `FAngelscriptTypeUsage` 的 `GetAngelscriptDeclaration()`（若可用）或字面字符串
- [ ] **P1.8** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpFunctions CSV export`

- [ ] **P1.9** 实现 `DumpEnums()` 和 `DumpDelegates()` — Enums.csv / Delegates.csv
  - 枚举值格式：`ValueName=IntValue;ValueName=IntValue`
  - 委托签名参数与 Functions 相同的摘要格式
- [ ] **P1.9** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpEnums and DumpDelegates CSV export`

- [ ] **P1.10** 实现 `DumpRegisteredTypes()` — RegisteredTypes.csv
  - 遍历 `FAngelscriptType::GetTypes()`
  - 对每个类型输出 `GetAngelscriptTypeName()`、是否有对应 UClass
- [ ] **P1.10** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpRegisteredTypes CSV export`

- [ ] **P1.11** 实现 `DumpDiagnostics()` — Diagnostics.csv
  - 遍历 `Engine.Diagnostics` map，展开每个文件的 `TArray<FDiagnostic>`
  - 每行一条诊断记录
- [ ] **P1.11** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpDiagnostics CSV export`

- [ ] **P1.12** 实现 `DumpScriptEngineState()` — ScriptEngineState.csv
  - 通过 `Engine.GetScriptEngine()` 获取 `asIScriptEngine*`
  - 收集：`GetModuleCount()`、全局函数数（循环 `GetModuleByIndex()` → `GetFunctionCount()`）、`GetObjectTypeCount()`、GC 统计（`GetGCStatistics()`）
  - Key/Value 格式输出
- [ ] **P1.12** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpScriptEngineState CSV export`

- [ ] **P1.13** 实现 `DumpSummary()` — DumpSummary.csv 和 `DumpAll()` 组装
  - `DumpAll()` 依次调用上述所有 `Dump*()` 方法，收集每张表的行数和错误
  - 最后写 `DumpSummary.csv` 作为本次 dump 的元数据
  - `DumpAll()` 返回输出目录路径
  - 在 `UE_LOG` 输出简要摘要（总文件数、总行数、输出路径）
- [ ] **P1.13** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpSummary and DumpAll orchestrator`

- [ ] **P1.14** 验证 `AngelscriptRuntime.Build.cs` 的 include path 覆盖 `Dump/`
  - 当前 Build.cs 已将模块根目录加为 include path，`Dump/` 子目录应自动可达
  - 验证 `#include "Dump/AngelscriptStateDump.h"` 在模块内外均能正常 include
  - 如不可达则添加 `PrivateIncludePaths`
- [ ] **P1.14** 📦 Git 提交：`[AngelscriptRuntime] Chore: verify Dump/ include path in Build.cs`

---

## Phase 2：Test 模块 CVar 触发与 Dump 目录

> 目标：在 `AngelscriptTest` 新增 `Dump/` 目录，注册控制台命令触发 dump，编写基础功能测试。

- [ ] **P2.1** 新建 `AngelscriptTest/Dump/` 目录，创建 `AngelscriptDumpCommand.cpp`
  - 注册 `FAutoConsoleCommand`，命令名 `as.DumpEngineState`
  - 命令执行时调用 `FAngelscriptStateDump::DumpAll(FAngelscriptEngine::Get())`
  - 支持可选参数指定输出目录：`as.DumpEngineState [OutputDir]`
  - dump 完成后在控制台打印输出目录路径
- [ ] **P2.1** 📦 Git 提交：`[AngelscriptTest/Dump] Feat: register as.DumpEngineState console command`

- [ ] **P2.2** 创建 `AngelscriptDumpTests.cpp` — dump 功能基础测试
  - 测试 1：`FCSVWriter` 基本功能 — 写入 header、添加行、保存文件，验证文件内容符合 CSV 格式
  - 测试 2：`FCSVWriter` 特殊字符转义 — 字段含逗号、双引号、换行时的正确转义
  - 测试 3：`DumpAll` 端到端 — 获取生产引擎或测试引擎，调用 `DumpAll`，验证输出目录存在且包含所有预期的 CSV 文件
  - 测试 4：`DumpAll` 输出验证 — 读取 `DumpSummary.csv`，验证所有表状态为 Success 且行数 > 0
  - 使用 `FScopedTestEngineGlobalScope` 管理测试引擎作用域
- [ ] **P2.2** 📦 Git 提交：`[AngelscriptTest/Dump] Feat: add dump functionality unit tests`

- [ ] **P2.3** 更新 `AngelscriptTest.Build.cs`，将 `Dump/` 加入 include path
  - 添加 `PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Dump"))`
- [ ] **P2.3** 📦 Git 提交：`[AngelscriptTest] Chore: add Dump/ to include paths in Build.cs`

---

## Phase 3：文档更新与可选 accessor 补齐

> 目标：更新文档；如果 Phase 1-2 实现过程中发现个别高价值 private 数据无替代方案，在此集中补齐最小 accessor。**Phase 3 不是必须的**——如果 public API 已足够，只做文档更新即可。

- [ ] **P3.1** 更新 `AGENTS.md` 的文档导航表，添加 Dump 目录说明
  - 在插件级 `AGENTS.md` 中说明 `AngelscriptRuntime/Dump/` 的用途
  - 在项目级 `AGENTS.md` 的架构决策中说明 dump 能力的入口和用法
  - 明确 dump 的架构原则：纯外部观察者、不侵入原有代码
- [ ] **P3.1** 📦 Git 提交：`[Docs] Docs: update AGENTS.md with state dump documentation`

- [ ] **P3.2** （可选）审计 Phase 1/2 中 `DumpSummary` 标记为 `PartialExport` 的表
  - Phase 1/2 的实现如果遇到确实无法通过 public API 获取但诊断价值极高的数据（如 `GlobalContextPool` 大小、`FileHotReloadState` 条目数），在此统一评估
  - 不需要的跳过此步

- [ ] **P3.3** （可选）在原有类中添加最小化 const accessor
  - 只为在 P3.2 中确认的高价值 private 数据添加 accessor
  - 每个 accessor 为 `const` 方法，只返回值或 `int32` 计数，不暴露可变状态或容器引用
  - 所有 accessor 集中在一次 commit 中，便于审查和回退
  - **禁止使用 friend 声明**——accessor 方法是唯一允许的侵入方式
  - 预期候选（仅在确认需要时添加）：
    - `FAngelscriptEngine::GetContextPoolSize() const → int32`
    - `FAngelscriptEngine::GetHotReloadTrackedFileCount() const → int32`
    - `FToStringHelper::GetRegisteredTypeCount() → int32`（如需要）
- [ ] **P3.3** 📦 Git 提交：`[AngelscriptRuntime] Feat: add minimal const accessors for state dump (optional)`

---

## Phase 4：绑定基础设施与配置 Dump

> 目标：将绑定系统的全局注册表、Bind 缓存、ToString 注册、文档数据库、引擎设置、热重载文件跟踪、上下文池等导出为 CSV（表 13-19）。

- [ ] **P4.1** 实现 `DumpBindRegistrations()` — BindRegistrations.csv
  - 通过 `FAngelscriptBinds::GetAllRegisteredBindNames()` 获取所有已注册的 bind 名称
  - 与 `FAngelscriptEngine` 的 `DisabledBindNames` 和 `FAngelscriptBinds` 的 `SkipBinds`/`SkipBindNames`/`SkipBindClasses` 交叉比对，标注每个 bind 是否被跳过及原因
  - 对于部分 static 列表不直接暴露的情况，可能需要在 `FAngelscriptBinds` 中添加 `GetSkipBindNames()` 等最小化 accessor
- [ ] **P4.1** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpBindRegistrations CSV export`

- [ ] **P4.2** 实现 `DumpBindDatabase()` — BindDatabase_Structs.csv 和 BindDatabase_Classes.csv
  - 通过 `FAngelscriptBindDatabase::Get()` 获取单例，遍历 `Structs` 和 `Classes`
  - 每行输出一个绑定结构体/类的名称、绑定名、属性/方法数等统计信息
  - `BoundEnums` 和 `BoundDelegateFunctions` 可合并输出或追加为额外列
- [ ] **P4.2** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpBindDatabase CSV export`

- [ ] **P4.3** 实现 `DumpToStringTypes()` — ToStringTypes.csv
  - `FToStringHelper` 当前只有 `Register()` 和 `Reset()`，无枚举已注册类型的 public API
  - **隔离策略**：在 `DumpSummary` 中标记此表为 `NotAvailable (no public enumeration API)`，输出空表 + 注释
  - **如果后续确认需要**，可在 Phase 3 的可选步骤中为 `FToStringHelper` 添加一个 `GetRegisteredTypeCount()` 或 `GetRegisteredTypes()` accessor
  - 当前 Phase 不修改 `Helper_ToString.h` 或 `Bind_FString.cpp`
- [ ] **P4.3** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpToStringTypes CSV stub (pending public API)`

- [ ] **P4.4** 实现 `DumpDocumentationStats()` — DocumentationStats.csv
  - `FAngelscriptDocs` 有 `Get*()` 系列静态方法可获取各文档 map 的引用或大小
  - 输出各文档类别（`UnrealDocumentation`、`UnrealTypeDocumentation`、`GlobalVariableDocumentation`、`UnrealPropertyDocumentation`）的条目总数
  - 不导出文档正文内容（体积过大），只导出统计
- [ ] **P4.4** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpDocumentationStats CSV export`

- [ ] **P4.5** 实现 `DumpEngineSettings()` — EngineSettings.csv
  - 通过 `UAngelscriptSettings::Get()` 获取设置对象
  - 利用 UObject 反射遍历所有 `UPROPERTY`，以 Key/Value/Category 格式输出
  - 数组和集合类型序列化为分号分隔字符串
- [ ] **P4.5** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpEngineSettings CSV export`

- [ ] **P4.6** 实现 `DumpHotReloadState()` — HotReloadState.csv
  - **public 可用部分**：`FileChangesDetectedForReload` 和 `FileDeletionsDetectedForReload` 已是 public 成员，可直接遍历导出
  - **private 不可用部分**：`FileHotReloadState`（跟踪时间戳）、`PreviouslyFailedReloadFiles`、`QueuedFullReloadFiles` 为 private
  - **隔离策略**：仅导出 public 的变更/删除检测队列；private 部分在表中标注 `(private - not exported)` 列，在 `DumpSummary` 中标记 `PartialExport`
  - 如果后续确认需要完整热重载跟踪，在 Phase 3 可选步骤中添加 `GetHotReloadTrackedFileCount()` 等最小 accessor
- [ ] **P4.6** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpHotReloadState CSV export (public data only)`

- [ ] **P4.7** 更新 `DumpAll()` 和 `DumpSummary` 以包含 Phase 4 的所有新表
  - 在 `DumpAll()` 中插入对 P4.1-P4.6 导出方法的调用
  - `DumpSummary.csv` 自动覆盖新增的表
- [ ] **P4.7** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: integrate Phase 4 tables into DumpAll`

---

## Phase 5：JIT / 预编译子系统 Dump

> 目标：将 JIT 查找表、预编译数据包、Static JIT 生成状态导出为 CSV（表 20-22）。

- [ ] **P5.1** 实现 `DumpJITDatabase()` — JITDatabase.csv
  - 通过 `FJITDatabase::Get()` 获取单例
  - 输出各查找表的大小：`Functions`、`FunctionLookups`、`GlobalVarLookups`、`TypeInfoLookups`、`PropertyOffsetLookups` 等
  - Key/Value/Details 格式，便于概览 JIT 内存占用
- [ ] **P5.1** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpJITDatabase CSV export`

- [ ] **P5.2** 实现 `DumpPrecompiledData()` — PrecompiledData.csv
  - 通过 `FAngelscriptEngine::PrecompiledData` 指针访问
  - 输出 `DataGuid`、模块数、函数映射数、已加载类数
  - 如果 `PrecompiledData` 为 null（未使用预编译），输出 "NotLoaded" 状态
  - 有条件地输出 `OutputTimingData()` 收集的计时信息
- [ ] **P5.2** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpPrecompiledData CSV export`

- [ ] **P5.3** 实现 `DumpStaticJITState()` — StaticJITState.csv
  - 通过 `FAngelscriptEngine::StaticJIT` 指针访问
  - 在 `AS_CAN_GENERATE_JIT` 宏下输出 JIT 文件数、待生成函数数、共享头数、已计算偏移数
  - 如果 StaticJIT 为 null 或未启用 JIT，输出对应状态
- [ ] **P5.3** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpStaticJITState CSV export`

- [ ] **P5.4** 更新 `DumpAll()` 以包含 Phase 5 的新表
- [ ] **P5.4** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: integrate Phase 5 tables into DumpAll`

---

## Phase 6：调试器 / 覆盖率 / 观测 Dump

> 目标：将调试服务器状态、断点列表、代码覆盖率数据导出为 CSV（表 23-25）。条件编译控制，仅在对应功能启用时导出。

- [ ] **P6.1** 实现 `DumpDebugServerState()` — DebugServerState.csv
  - 条件编译 `WITH_AS_DEBUGSERVER`
  - 通过 `FAngelscriptEngine::DebugServer`（public 指针）访问
  - **public 可用**：`Breakpoints`（public TMap）、`DataBreakpoints`（public）、调试标志（`bIsPaused` 等 public）、`HasAnyClients()` 方法、`GetOwnerEngine()` 方法
  - **private 不可用**：`Clients` 容器（精确客户端数）、`Listener` socket
  - **隔离策略**：用 `HasAnyClients()` 输出 bool 值替代精确客户端计数；断点数通过遍历 public `Breakpoints` map 求和；协议版本通过 `AngelscriptDebugServer::DebugAdapterVersion` extern 访问
- [ ] **P6.1** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpDebugServerState CSV export`

- [ ] **P6.2** 实现 `DumpDebugBreakpoints()` — DebugBreakpoints.csv
  - 条件编译 `WITH_AS_DEBUGSERVER`
  - `FAngelscriptDebugServer::Breakpoints` 是 **public** 成员（`TMap<FString, TSharedPtr<FFileBreakpoints>>`），可直接遍历
  - 无需任何 accessor 或 friend，完全外部访问
  - 每行输出一个断点的文件、行号、启用状态
- [ ] **P6.2** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpDebugBreakpoints CSV export`

- [ ] **P6.3** 实现 `DumpCodeCoverage()` — CodeCoverage.csv
  - 条件编译 `WITH_AS_COVERAGE`
  - 通过 `FAngelscriptEngine::CodeCoverage`（public 指针）访问
  - **隔离策略**：`FilesToCoverage` 是 private，但 `GetLineCoverage()` 是 public 方法，可通过已知脚本文件名（从 `GetActiveModules()` 的 `Code` 段获取）逐文件查询覆盖率
  - `CoverageEnabled` 是 public，可判断是否有数据
  - 仅输出命中次数 > 0 的行，避免生成巨大文件
- [ ] **P6.3** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: implement DumpCodeCoverage CSV export`

- [ ] **P6.4** 更新 `DumpAll()` 以包含 Phase 6 的新表
  - 条件编译包裹，未启用的子系统对应的表在 `DumpSummary` 中标记 "Skipped (not compiled)"
- [ ] **P6.4** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: integrate Phase 6 tables into DumpAll`

---

## Phase 7：编辑器模块状态 Dump

> 目标：将编辑器侧的热重载重绑映射和菜单扩展注册表导出为 CSV（表 26-27）。编辑器 dump 仅在 `WITH_EDITOR` 下可用。

- [ ] **P7.1** 在 `AngelscriptEditor` 模块中实现 `FAngelscriptEditorStateDump` 扩展
  - 在 `AngelscriptEditor/Private/` 新建 `AngelscriptEditorStateDump.cpp`
  - 不创建独立头文件；通过 Runtime 模块的 `FAngelscriptStateDump` 提供的 **扩展点委托** 注册编辑器 dump 逻辑
  - `FAngelscriptStateDump` 需在 Phase 1 中预留 `static TMulticastDelegate<void(const FString& OutputDir)> OnDumpExtensions` 委托
  - Editor 模块在 `FAngelscriptEditorModule::StartupModule()` 中绑定此委托
- [ ] **P7.1** 📦 Git 提交：`[AngelscriptEditor] Feat: register editor state dump extension`

- [ ] **P7.2** 实现 `DumpEditorReloadState()` — EditorReloadState.csv
  - 通过 `FClassReloadHelper::ReloadState()` 获取静态 `FReloadState`
  - 遍历 `ReloadClasses`、`NewClasses`、`ReloadEnums`、`NewEnums`、`ReloadStructs`、`ReloadDelegates`
  - 每行输出一个重绑条目的类别、原名和新名
- [ ] **P7.2** 📦 Git 提交：`[AngelscriptEditor] Feat: implement DumpEditorReloadState CSV export`

- [ ] **P7.3** 实现 `DumpEditorMenuExtensions()` — EditorMenuExtensions.csv
  - 通过 `UScriptEditorMenuExtension::RegisteredExtensions` 静态数组访问
  - 需确认可见性；如果是 private static，则需添加 public accessor
  - 每行输出一个已注册扩展的挂载点、位置、分区名
- [ ] **P7.3** 📦 Git 提交：`[AngelscriptEditor] Feat: implement DumpEditorMenuExtensions CSV export`

- [ ] **P7.4** 更新 Phase 1 的 `DumpAll()` 以触发扩展委托
  - 在 `DumpAll()` 最后、`DumpSummary` 之前广播 `OnDumpExtensions` 委托
  - 编辑器 dump 的表自动纳入 `DumpSummary`
- [ ] **P7.4** 📦 Git 提交：`[AngelscriptRuntime/Dump] Feat: broadcast dump extension delegate from DumpAll`

---

## 验收标准

### Phase 1-3 完成后（MVP）

1. **编译通过**：`AngelscriptRuntime` 和 `AngelscriptTest` 模块在 Development Editor Win64 下完整编译通过。
2. **CVar 可用**：在 UE 编辑器控制台输入 `as.DumpEngineState`，成功在 `Saved/Angelscript/Dump/` 下生成时间戳目录。
3. **CSV 完整性**：输出目录包含全部 12 个核心 CSV 文件（`EngineOverview.csv` 到 `DumpSummary.csv`）。
4. **CSV 格式正确**：所有 CSV 文件可被 Python `csv.reader` 和 Excel 正确解析，含中文路径不乱码。
5. **数据准确性**：`DumpSummary.csv` 中所有表状态为 `Success`，行数与实际引擎状态一致。
6. **测试通过**：`AngelscriptTest/Dump/` 下所有自动化测试通过。
7. **API 可用性**：外部模块可通过 `#include "Dump/AngelscriptStateDump.h"` 使用 `FAngelscriptStateDump::DumpAll()`。

### Phase 4-7 完成后（全覆盖）

8. **全量表覆盖**：输出目录包含最多 27 个 CSV 文件（条件编译的表可能标记 Skipped）。
9. **绑定基础设施可审计**：通过 `BindRegistrations.csv` + `BindDatabase_*.csv` 可以完整追踪每个 C++ 绑定从注册到缓存的状态。
10. **JIT/预编译可诊断**：通过 `JITDatabase.csv` + `PrecompiledData.csv` 可以判断 JIT 查找表完整性和预编译包匹配状态。
11. **调试器状态可见**：通过 `DebugServerState.csv` + `DebugBreakpoints.csv` 可以看到调试器连接和断点配置。
12. **编辑器扩展点可用**：Editor 模块可通过 `OnDumpExtensions` 委托注入自定义 dump 逻辑。

## 风险与注意事项

1. **隔离原则的坚持**：Dump 代码必须保持与原有代码的完全隔离。如果实现中发现某个数据确实无法通过 public API 获取，默认做法是**跳过并在 Summary 中标注**，而不是去修改原有类。只有经过明确评估后才在 Phase 3 中集中添加最小 accessor。
2. **线程安全**：Dump 操作应在 GameThread 执行，避免与热重载或编译线程冲突。CVar 回调默认在 GameThread，但需确认。特别注意 `FAngelscriptDebugServer` 的 `Breakpoints`（public 成员）可能在调试线程被修改，dump 时可能需要在调试服务器暂停状态下导出。
3. **大型项目性能**：模块数、类数可能很大（数百个模块、数千个类），CSV 写入应一次性构建字符串再写文件，避免频繁磁盘 IO。代码覆盖率数据量可能特别大（每文件每行），应提供可选过滤。
4. **asIScriptEngine API 兼容性**：当前使用 AS 2.33.0 WIP，部分 `asIScriptEngine` 方法（如 `GetGCStatistics`）的签名可能与标准 2.33.0 不同，实现时需查验 `angelscript.h` 中的实际声明。
5. **UTF-8 BOM**：Windows Excel 默认用 ANSI 打开 CSV，需加 BOM 前缀才能正确识别 UTF-8。`FFileHelper::SaveStringToFile` 配合 `EEncodingOptions::ForceUTF8WithoutBOM` 后手工写 BOM，或使用 `ForceUTF8` 选项。
6. **测试引擎状态**：测试中使用的引擎可能是 Clone 模式，部分状态（如 `ActiveModules`）可能为空或与 Full 引擎不同，测试应考虑两种模式。
7. **条件编译**：`FAngelscriptDebugServer`（`WITH_AS_DEBUGSERVER`）、`FAngelscriptCodeCoverage`（`WITH_AS_COVERAGE`）、Editor 状态（`WITH_EDITOR`）均受条件编译控制。Dump 代码需对应使用 `#if` 包裹，未编译的表在 Summary 中标记 Skipped。
8. **编辑器依赖方向**：Runtime 不应依赖 Editor。编辑器 dump 通过**委托扩展点**实现（Editor 向 Runtime 注册回调），而非 Runtime 直接 include 编辑器头文件。
9. **MOSTLY_PUBLIC 子系统的降级策略**：对于 `FAngelscriptCodeCoverage`（`FilesToCoverage` private），可通过已知模块名调用 `GetLineCoverage()` 逐个查询；对于 `FAngelscriptDebugServer` 的客户端数，可用 `HasAnyClients()` 导出布尔值替代精确计数。这些降级在功能上足够诊断使用。
