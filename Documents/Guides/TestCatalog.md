# Angelscript 插件测试目录

> 自动化测试基线：**275/275 PASS**（0 失败，0 禁用）
>
> 测试模块路径：`Plugins/Angelscript/Source/AngelscriptTest/`
>
> 启动 bind / watcher / 性能分层矩阵：`Documents/Guides/AngelscriptValidationMatrix.md`

---

## 目录

- [1. Shared — 测试基础设施](#1-shared--测试基础设施)
- [2. Core — 引擎核心](#2-core--引擎核心)
- [3. Angelscript — 脚本引擎行为](#3-angelscript--脚本引擎行为)
  - [3.1 Core — 创建/编译/执行](#31-core--创建编译执行)
  - [3.2 Execute — 执行与调用约定](#32-execute--执行与调用约定)
  - [3.3 Types — 类型系统](#33-types--类型系统)
  - [3.4 ControlFlow — 控制流](#34-controlflow--控制流)
  - [3.5 Functions — 函数特性](#35-functions--函数特性)
  - [3.6 Objects — 对象模型](#36-objects--对象模型)
  - [3.7 Operators — 运算符](#37-operators--运算符)
  - [3.8 Handles — 句柄与引用](#38-handles--句柄与引用)
  - [3.9 Inheritance — 继承](#39-inheritance--继承)
  - [3.10 Misc — 杂项](#310-misc--杂项)
  - [3.11 NativeScriptHotReload — 原生脚本热重载](#311-nativescripthotreload--原生脚本热重载)
  - [3.12 Upgrade — 版本升级兼容](#312-upgrade--版本升级兼容)
- [4. Bindings — UE API 绑定](#4-bindings--ue-api-绑定)
- [5. HotReload — 热重载](#5-hotreload--热重载)
- [6. Internals — 内部机制](#6-internals--内部机制)
- [7. Compiler — 编译管线](#7-compiler--编译管线)
- [8. Preprocessor — 预处理器](#8-preprocessor--预处理器)
- [9. ClassGenerator — 类生成器](#9-classgenerator--类生成器)
- [10. FileSystem — 文件系统](#10-filesystem--文件系统)
- [11. Editor — 编辑器](#11-editor--编辑器)
- [12. Themed Integration Tests — 主题化集成回归](#12-themed-integration-tests--主题化集成回归)
  - [12.1 Actor 生命周期](#121-actor-生命周期)
  - [12.2 Actor 交互](#122-actor-交互)
  - [12.3 Actor 属性](#123-actor-属性)
  - [12.4 ScriptClass 创建](#124-scriptclass-创建)
  - [12.5 ScriptActor 重载执行](#125-scriptactor-重载执行)
  - [12.6 BlueprintSubclass 蓝图子类化](#126-blueprintsubclass-蓝图子类化)
  - [12.7 BlueprintChild 运行时](#127-blueprintchild-运行时)
  - [12.8 Component 组件](#128-component-组件)
  - [12.9 DefaultComponent 默认组件](#129-defaultcomponent-默认组件)
  - [12.10 Inheritance 继承场景](#1210-inheritance-继承场景)
  - [12.11 Interface 接口](#1211-interface-接口)
  - [12.12 Delegate 委托](#1212-delegate-委托)
  - [12.13 GC 垃圾回收](#1213-gc-垃圾回收)
  - [12.14 Subsystem 子系统](#1214-subsystem-子系统)
  - [12.15 HotReload 热重载场景](#1215-hotreload-热重载场景)
- [13. Examples — 示例脚本编译](#13-examples--示例脚本编译)
- [14. Template — 模板场景](#14-template--模板场景)

---

## 1. Shared — 测试基础设施

> 源文件：`Shared/AngelscriptTestEngineHelperTests.cpp`、`Shared/AngelscriptNativeScriptTestObjectTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Shared.EngineHelper.CompileModuleFromMemory | 内存字符串模块能成功编译 |
| Shared.EngineHelper.ExecuteIntFunction | `ExecuteIntFunction` 能执行入口并返回正确整型结果（42） |
| Shared.EngineHelper.GeneratedSymbolLookup | 带注解模块编译后，`FindGeneratedClass` / `FindGeneratedFunction` 能定位生成的类与 UFUNCTION |
| Shared.EngineHelper.FailedAnnotatedModuleDoesNotPolluteLaterCompiles | 无效注解模块编译失败后，后续有效模块仍能编译并生成符号 |
| Shared.EngineHelper.SharedTestEngineNeverSilentlyAttachesToProductionEngine | 共享测试引擎不会静默附着到生产引擎 |
| Shared.EngineHelper.ProductionHelperRejectsMissingProductionEngine | 无生产子系统/全局引擎时 `TryGetCurrentProductionEngine` 为 null |
| Shared.EngineHelper.CompileUsesScopedGlobalEngine | 在 `FScopedTestEngineGlobalScope` 下编译后，全局引擎指针恢复为共享引擎 |
| Shared.EngineHelper.NestedGlobalEngineScopeRestoresPreviousEngine | 嵌套 scope 先装 B 再退出后恢复 A |
| Shared.EngineHelper.WorldContextScopeRestoresPreviousContext | `FScopedTestWorldContextScope` 正确设置/恢复 `CurrentWorldContext` |
| Shared.EngineHelper.ExecutingOneTestEngineDoesNotLeakContextIntoNextTest | 两个 clone 引擎分别编译执行不同模块，结果互不串线 |
| Shared.EngineHelper.SubsystemAttachedProductionEngineDoesNotHijackIsolatedTestEngine | 隔离引擎编译的模块不出现在共享测试引擎中 |
| Shared.NativeScriptTestObject.Instantiate | 原生测试用 `UAngelscriptNativeScriptTestObject` 可实例化 |

---

## 2. Core — 引擎核心

> 源文件：`Core/AngelscriptEngineCoreTests.cpp`、`Core/AngelscriptBindConfigTests.cpp`、`Core/AngelscriptEngineParityTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Engine.CreateDestroy | 引擎创建与销毁生命周期正常 |
| Engine.CompileSnippet | 代码片段编译 |
| Engine.ExecuteSnippet | 代码片段执行 |
| Engine.BindConfig.GlobalDisabledBindNames | 全局设置中禁用绑定名后，对应绑定不执行 |
| Engine.BindConfig.EngineDisabledBindNames | 引擎级禁用绑定集合生效 |
| Engine.BindConfig.UnnamedBindBackwardCompatibility | 未命名绑定的向后兼容行为 |
| Parity.SkinnedMeshCompile | 生产引擎中 `USkinnedMeshComponent` 类型及关键方法存在 |
| Parity.DelegateWithPayloadCompile | `FAngelscriptDelegateWithPayload` 及 `IsBound`/`ExecuteIfBound` |
| Parity.CollisionProfileCompile | `CollisionProfile` 常量片段可编译并与 `FName` 比较 |
| Parity.CollisionQueryParamsCompile | 碰撞查询参数绑定编译 |
| Parity.WorldCollisionCompile | 世界碰撞相关 API 编译 |
| Parity.FIntPointCompile | `FIntPoint` 绑定编译 |
| Parity.FVector2fCompile | `FVector2f` 绑定编译 |
| Parity.SoftReferenceCppForm | 软引用 C++ 形式在脚本侧可用性 |
| Parity.SoftReferenceCompile | 软引用脚本编译 |
| Parity.UserWidgetPaintCompile | `UserWidget` 绘制相关编译 |
| Parity.LevelStreamingCompile | 关卡流送相关编译 |
| Parity.RuntimeCurveLinearColorCompile | `RuntimeCurveLinearColor` 编译 |
| Parity.HitResultCompile | `FHitResult` 编译 |
| Parity.DeprecationsMetadata | 弃用元数据暴露正确 |
| Engine.LastFullDestroyClearsTypeState | 最后一个 full owner 销毁后类型元数据清空 |
| Engine.FullDestroyAllowsCleanRecreate | full owner 销毁后可干净重建并再次编译执行 |
| Core.Performance.Startup.Full | fresh full 启动基线与 startup 指标产物写出 |
| Core.Performance.Startup.Clone | clone 启动基线与 0 bind replay 指标 |
| Core.Performance.Startup.CreateForTestingFallbackFull | 无 source engine 时 CreateForTesting fallback 全量启动基线 |
| Core.Performance.Startup.CreateForTestingClone | 有 source engine 时 CreateForTesting clone 启动基线 |
| Core.Performance.ArtifactGeneration | metrics.json 产物结构与落盘回归 |

---

## 3. Angelscript — 脚本引擎行为

### 3.1 Core — 创建/编译/执行

> 源文件：`Angelscript/AngelscriptCoreExecutionTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Core.CreateCompileExecute | 内存模块编译并执行 `Run()`：`DoubleValue(21)` 返回 42 |
| Core.GlobalState | 全局 `const` 与 `Step` 调用：`Run()` 返回 7（3+4） |
| Core.CreateEngine | 两次 `CreateForTesting` 得到非空独立 `asIScriptEngine`；版本号为 23300 |
| Core.CompilerBasic | 简单/多函数/全局变量模块编译成功；语法错误模块编译失败 |
| Core.Parser | 算术/逻辑/if、嵌套块可编译；括号不匹配脚本编译失败 |
| Core.Optimize | 常量折叠 `1+2+3` 执行得 6；含死代码的 `return` 路径仍返回 1 |

### 3.2 Execute — 执行与调用约定

> 源文件：`Angelscript/AngelscriptExecutionTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Execute.Basic | `void TestVoid()` 与 `int TestValue()` 的 Prepare/Execute 成功，返回值 42 |
| Execute.OneArg | `int Test(int)` 传入 21，返回 42 |
| Execute.TwoArgs | `int Test(int,int)` 20+22=42 |
| Execute.FourArgs | 四参数相加为 42 |
| Execute.MixedArgs | `int`+`float`/`double`+`int` 混合参数，返回值约 42.5 |
| Execute.Context | 两次 `CreateContext` 指针不同；Prepare/Execute 前后上下文状态为 Prepared/Finished |
| Execute.Nested | `Outer(20)` 调用 `Inner`，应得 41 |
| Execute.Discard | `DiscardModule` 前后模块描述存在性；重复 discard 应失败 |
| Execute.Script | `Calculate(1,10)` 含 for 循环累加，应得 55 |

### 3.3 Types — 类型系统

> 源文件：`Angelscript/AngelscriptTypeTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Types.PrimitiveAndEnum | `bool`、`float` 与枚举 `EState::Running` 混合运算，应得 9 |
| Types.Int64AndTypedef | `int64` 左移 40 位再加 7，与宽整数常量一致 |
| Types.Bool | `true && !false` 应得 1 |
| Types.Float | `3.14*2` 应约 6.28（按引擎 float/double 选择脚本） |
| Types.FloatDebuggerFormatting | `FAngelscriptTypeUsage` 对小浮点取调试字符串，应含科学计数法 |
| Types.Int8 | `int8` 相加再转 `int`，应得 150 |
| Types.Bits | 按位或/与/异或掩码判断，应得 1 |
| Types.Enum | `Color::Green` 的 `int` 值应为 1 |
| Types.Auto | `auto Value = 42` 应返回 42 |
| Types.Conversion | `float`/`double` 显式转 `int` 向零截断，3.7→3 |
| Types.ImplicitCast | `int` 隐式提升为 `float`/`double`，返回值应约 42.0 |

### 3.4 ControlFlow — 控制流

> 源文件：`Angelscript/AngelscriptControlFlowTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| ControlFlow.ForLoop | `Index` 从 0 到 <5 累加，应得 10 |
| ControlFlow.SwitchAndConditional | `switch` 与三元运算，`Run()` 应得 7 |
| ControlFlow.Condition | 嵌套三元 `Evaluate` 组合，应得 210 |
| ControlFlow.NeverVisited | `if` 早退后存在"可能永不执行"的块，仍能编译 |
| ControlFlow.NotInitialized | 未初始化 `int` 直接返回：应捕获含 "may not be initialized" 的 Warning |

### 3.5 Functions — 函数特性

> 源文件：`Angelscript/AngelscriptFunctionTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Functions.DefaultArguments | `Add(7)` 使用默认参数 `B=5`，应得 12 |
| Functions.NamedArguments | `Mix(C:3,A:1,B:2)` 应得 321 |
| Functions.OverloadResolution | `int`/`float` 版 `Convert` 重载，应得 5+6=11 |
| Functions.Pointer | `funcdef` + `@Callback` 函数指针语法应编译失败 |
| Functions.Constructor | 双构造函数脚本类能成功 `BuildModule` |
| Functions.Destructor | 含析构函数的类：`Run()` 执行后返回 1 |
| Functions.Template | 泛型类 `TemplateCarrier<T>` 应编译失败 |
| Functions.Factory | `FactoryCarrier @CreateCarrier` 返回句柄应编译失败 |

### 3.6 Objects — 对象模型

> 源文件：`Angelscript/AngelscriptObjectModelTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Objects.ValueTypeConstruction | `FIntPoint(3,4)` 的 `X+Y` 应得 7 |
| Objects.ValueTypeCopyAndArithmetic | `FIntPoint` 拷贝与算术操作结果应为 57 |
| Objects.Basic | 脚本类方法 `Set`/`Get` 能编译（仅编译/符号覆盖） |
| Objects.Composition | 嵌套脚本对象成员赋值能编译 |
| Objects.Singleton | 占位：单例式全局类变量为已知不支持的分支约束，测试恒通过 |
| Objects.ZeroSize | 空脚本类实例化后 `Run()` 返回 1 |

### 3.7 Operators — 运算符

> 源文件：`Angelscript/AngelscriptOperatorTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Operators.Overload | 脚本类 `opAdd` 与 `A+B` 能编译 |
| Operators.GetSet | `get_value`/`set_value` 属性访问脚本 `Build()` 为 `asSUCCESS` |
| Operators.Const | `const` 成员方法脚本能编译 |
| Operators.Power | `2.0f ** 3.0f` 转 `int` 应为 8 |

### 3.8 Handles — 句柄与引用

> 源文件：`Angelscript/AngelscriptHandleTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Handles.Basic | 脚本类句柄声明与赋值应编译失败（当前不支持） |
| Handles.Implicit | 以值类型隐式传脚本对象给函数能编译 |
| Handles.Auto | 返回 `HandleAutoObject@` 的工厂式句柄应编译失败 |
| Handles.RefArgument | `int &out` 参数：`Modify(Value)` 后 `Test()` 应返回 42 |

### 3.9 Inheritance — 继承

> 源文件：`Angelscript/AngelscriptInheritanceTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Inheritance.Basic | `Base`/`Derived` 继承与成员调用脚本能编译且模块可解析 |
| Inheritance.Interface | 含 `interface`/`implements` 的脚本应编译失败（当前分支不支持原生 AS 接口语法） |
| Inheritance.VirtualMethod | 派生类重写 `GetValue` 能编译 |
| Inheritance.CastOp | 脚本类句柄 `CastOpClass@` 构造应编译失败 |
| Inheritance.Mixin | `mixin class` 语法应编译失败（解析不支持） |

### 3.10 Misc — 杂项

> 源文件：`Angelscript/AngelscriptMiscTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Misc.Namespace | 命名空间内函数与常量，`MyNamespace::GetValue()` 应得 42 |
| Misc.Any | 原始模块下 `any` 存取 `int`，`Module->Build()` 为 `asSUCCESS` |
| Misc.GlobalVar | 模块级 `const int GlobalValue`，`Test()` 应得 42 |
| Misc.MultiAssign | 链式赋值 `A=B=C=42`，三数之和应 126 |
| Misc.Assign | `+=`、`-=`、`*=`、`/=` 序列运算结果应为 8 |
| Misc.DuplicateFunction | 两个同名 `Test()` 函数，`Build()` 仍为 `asSUCCESS`（记录当前行为） |

### 3.11 NativeScriptHotReload — 原生脚本热重载

> 源文件：`Angelscript/AngelscriptNativeScriptHotReloadTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| NativeScriptHotReload.Phase2A | 对 `Test_Enums.as`/`Test_Inheritance.as`/`Test_Handles.as` 全量编译再软重载，重载结果为 Fully/Partially Handled |
| NativeScriptHotReload.Phase2B | 对 `Test_GameplayTags`/`Test_SystemUtils`/`Test_ActorLifecycle`/`Test_MathNamespace` 同样流程 |

### 3.12 Upgrade — 版本升级兼容

> 源文件：`Angelscript/AngelscriptUpgradeCompatibilityTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Upgrade.HeaderCompatibility | AngelScript 升级头文件兼容层 |
| Upgrade.EngineProperties | 引擎属性相关兼容 |
| Upgrade.MessageCallback | 消息回调兼容 |
| Upgrade.RegisterObjectTypeFlags | `RegisterObjectType` 标志兼容 |

---

## 4. Bindings — UE API 绑定

> 源文件：`Bindings/` 目录下 13 个测试文件

### 值类型与引擎核心

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.ValueTypes | `int32`/`double`/`FString`/`FName`/`FVector`/`FRotator`/`FTransform`/`FText` 运算与比较 | AngelscriptEngineBindingsTests.cpp |
| Bindings.FNameArrayCompat | `FName[]` 与 `TArray<FName>` 别名/显式、`Add`、索引、`Contains` | AngelscriptEngineBindingsTests.cpp |
| Bindings.TArrayMutationCompat | `int[]`：`FindIndex`/`AddUnique`/`Insert`/`RemoveSingle`/`Remove`/`Reset` | AngelscriptEngineBindingsTests.cpp |
| Bindings.ForeachCompat | `int[]`/`TArray` 的 `for (x : arr)` 与 `const FVector&` 范围 for | AngelscriptEngineBindingsTests.cpp |
| Bindings.TArrayIteratorCompat | `TArrayIterator`/`TArrayConstIterator` 可变迭代、常量遍历、别名迭代器累加 | AngelscriptEngineBindingsTests.cpp |

### 容器

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.OptionalCompat | `TOptional<int>`：`IsSet`/`Get`/`Set`/`GetValue`/`Reset`；`TOptional<FName>` | AngelscriptContainerBindingsTests.cpp |
| Bindings.SetCompat | `TSet<int>`：去重 `Add`/`Contains`/拷贝/`Remove`/`Reset`；`TSet<FName>` | AngelscriptContainerBindingsTests.cpp |
| Bindings.MapCompat | `TMap<FName,int>`：`Add` 覆盖/`Find`/`FindOrAdd`/复制/`Remove`/`Reset` | AngelscriptContainerBindingsTests.cpp |
| Bindings.ArrayForeach | `TArray` 的 `foreach (值, 索引)` 累加元素和与索引和 | AngelscriptContainerBindingsTests.cpp |
| Bindings.SetForeach | `TSet` 的 `foreach` 累加为 7 | AngelscriptContainerBindingsTests.cpp |
| Bindings.MapForeach | `TMap` 的 `foreach (值, 键)` 和为 7、键计数为 2 | AngelscriptContainerBindingsTests.cpp |

### 容器比较与调试

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.SetCompareCompat | 两个 `TSet` 元素相同顺序不同仍 `==`，增元素后不等 | AngelscriptContainerCompareBindingsTests.cpp |
| Bindings.MapCompareCompat | 两个 `TMap` 键值相同插入顺序不同时 `==` 为真 | AngelscriptContainerCompareBindingsTests.cpp |
| Bindings.OptionalTypeCompareCompat | `TOptional<int>` 的 `CanCompare()`、未设置/相同/不同值的 `IsValueEqual` | AngelscriptContainerCompareBindingsTests.cpp |
| Bindings.MapDebuggerCompat | `TMap` 的 `GetDebuggerValue` 摘要、`Num` 成员、键调试值 | AngelscriptContainerCompareBindingsTests.cpp |

### 迭代器

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.SetIteratorCompat | `TSetIterator`：`CanProceed`/`Proceed` 遍历求和为 7 | AngelscriptIteratorBindingsTests.cpp |
| Bindings.MapIteratorCompat | `TMapIterator`：`Proceed`/`GetKey`/`GetValue` 遍历求和与键识别 | AngelscriptIteratorBindingsTests.cpp |

### 类与类型查找

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.ClassLookupCompat | `FindClass("AActor")`/`GetAllClasses` 能在全类列表中找到 `AActor` | AngelscriptClassBindingsTests.cpp |
| Bindings.TSubclassOfCompat | `TSubclassOf<AActor>`：空/有效/`Get`/`==`/`GetDefaultObject` | AngelscriptClassBindingsTests.cpp |
| Bindings.TSoftClassPtrCompat | `TSoftClassPtr<AActor>`：空/构造/隐式/`Get`/`ToString`/`Reset` | AngelscriptClassBindingsTests.cpp |
| Bindings.StaticClassCompat | `AActor::StaticClass` 与 `TSubclassOf`；注解 Actor 内 `StaticClass` | AngelscriptClassBindingsTests.cpp |
| Bindings.NativeStaticClassNamespace | `SetDefaultNamespace("AActor")` 下存在全局 `UClass StaticClass()` | AngelscriptClassBindingsTests.cpp |
| Bindings.NativeStaticTypeGlobal | 全局 `__StaticType_AActor`：`IsValid`/`Get`/`==`/`IsChildOf`/`GetDefaultObject` | AngelscriptClassBindingsTests.cpp |

### 对象指针与引用

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.ObjectPtrCompat | `TObjectPtr<UTexture2D>`：空/构造/赋值/比较/`Get`/与裸指针转换 | AngelscriptObjectBindingsTests.cpp |
| Bindings.SoftObjectPtrCompat | `TSoftObjectPtr<UTexture2D>`：空/有效/`==`/`Get`/`ToSoftObjectPath`/路径构造/`Reset` | AngelscriptObjectBindingsTests.cpp |

### 兼容性（Cast/DateTime/Timespan）

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.ObjectCastCompat | `Cast<UPackage>`、`n"..."` 与 `FName`；注解模块 `Cast` 到生成类 | AngelscriptCompatBindingsTests.cpp |
| Bindings.ObjectEditorOnlyCompat | `UPackage::IsEditorOnly()` 对暂存包为 false | AngelscriptCompatBindingsTests.cpp |
| Bindings.TimespanCompat | `FTimespan`：Zero/FromSeconds/Hours/构造/`opCmp`/运算/`ToString` | AngelscriptCompatBindingsTests.cpp |
| Bindings.DateTimeCompat | `FDateTime`：Unix 纪元/构造/比较/与 `FTimespan` 加减/闰年/`ToIso8601`/Today/Now/UtcNow | AngelscriptCompatBindingsTests.cpp |

### 数学与平台

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.MathExtendedCompat | `Math::` 扩展：`RandHelper`/`IsPowerOfTwo`/`VRand`/`ClampAngle`/`Clamp`/插值系列/三次插值等 | AngelscriptMathAndPlatformBindingsTests.cpp |
| Bindings.PlatformProcessCompat | `FPlatformProcess`：用户目录/设置/临时/可执行路径/计算机名/用户名/`CanLaunchURL` | AngelscriptMathAndPlatformBindingsTests.cpp |
| Bindings.Logging | `Log`/`LogDisplay`/`Warning`/`Error` 可执行；`AddExpectedError` 捕获 Error 输出 | AngelscriptMathAndPlatformBindingsTests.cpp |

### 杂项工具

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.HashCompat | `Hash::CityHash32/64`、带种子哈希的确定性与区分 | AngelscriptUtilityBindingsTests.cpp |
| Bindings.UtilityCompat | `FCommandLine::Get` 非空/`Parse` 与 C++ 一致/`FApp::GetProjectName` 一致 | AngelscriptUtilityBindingsTests.cpp |
| Bindings.ParseCompat | `FParse::Value` 解析 int/float/FString，`FParse::Bool` 解析 bool | AngelscriptUtilityBindingsTests.cpp |
| Bindings.RandomStreamCompat | `FRandomStream`：种子/`RandRange` 可重复/`GetFraction`/双精度范围/拷贝/`GenerateNewSeed`/`ToString` | AngelscriptUtilityBindingsTests.cpp |
| Bindings.StringRemoveAtCompat | `FString::RemoveAt` 删除子串后内容正确 | AngelscriptUtilityBindingsTests.cpp |

### GUID 与路径

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.GuidCompat | `FGuid`：构造/`ToString`/`Parse`/`ParseExact`/`Invalidate`/`NewGuid`/`GetTypeHash` | AngelscriptCoreMiscBindingsTests.cpp |
| Bindings.PathsCompat | `FPaths`：`ProjectDir`/`CombinePaths`/`IsRelative`/`GetExtension`/`DirectoryExists`/`FileExists` | AngelscriptCoreMiscBindingsTests.cpp |
| Bindings.NumberFormattingOptionsCompat | `FNumberFormattingOptions` 链式 Set、`IsIdentical`、`GetTypeHash`；默认预设 | AngelscriptCoreMiscBindingsTests.cpp |

### GameplayTag

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.GameplayTagCompat | `RequestGameplayTag`/空 Tag/`RequestGameplayTag(NAME_None,false)` | AngelscriptGameplayTagBindingsTests.cpp |
| Bindings.GameplayTagContainerCompat | `FGameplayTagContainer`：空/增删/`HasTag`/`HasAny`/`HasAll`/`AppendTags`/`Reset` | AngelscriptGameplayTagBindingsTests.cpp |
| Bindings.GameplayTagQueryCompat | `FGameplayTagQuery`：`MakeQuery_MatchAny/All/No/Exact`、与 Container 匹配逻辑 | AngelscriptGameplayTagBindingsTests.cpp |

### 委托与文件

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.ScriptDelegateCompat | `delegate`/`event`：`BindUFunction`/`AddUFunction`/`Unbind`/`Clear`/`IsBound` | AngelscriptFileAndDelegateBindingsTests.cpp |
| Bindings.SoftPathCompat | `FSoftObjectPath`/`FSoftClassPath` 有效性/资源名/包名/`IsSubobject`/相等 | AngelscriptFileAndDelegateBindingsTests.cpp |
| Bindings.SourceMetadataCompat | 磁盘 `.as` 编译后：`UClass`/`UFunction` 源路径/模块名/脚本声明/行号 | AngelscriptFileAndDelegateBindingsTests.cpp |
| Bindings.FileHelperCompat | `FFileHelper::SaveStringToFile` / `LoadFileToString` 读写一致 | AngelscriptFileAndDelegateBindingsTests.cpp |

### 原生引擎方法

| 测试名 | 验证内容 | 源文件 |
|--------|----------|--------|
| Bindings.NativeActorMethods | `GetActorLocation`/`GetActorRotation`/`GetClass`/`GetName`/`IsA` 等原生 Actor 桥接调用 | AngelscriptNativeEngineBindingsTests.cpp |
| Bindings.NativeComponentMethods | `USceneComponent`：`Activate`/`Deactivate`/相对变换/`GetComponent`/标签 | AngelscriptNativeEngineBindingsTests.cpp |
| Bindings.ComponentDestroyCompat | 注解组件上 `DestroyComponent()` 可编译执行，组件进入 `IsBeingDestroyed()` | AngelscriptNativeEngineBindingsTests.cpp |

---

## 5. HotReload — 热重载

### 函数与模块

> 源文件：`HotReload/AngelscriptHotReloadFunctionTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| HotReload.ModuleRecordTracking | 模块记录/追踪热重载相关状态 |
| HotReload.DiscardModule | 丢弃模块后引擎状态符合预期 |
| HotReload.DiscardAndRecompile | 丢弃后能重新编译同一/新模块 |
| HotReload.ModuleWatcherQueuesFileChanges | 文件监视器将变更入队供重载 |
| HotReload.Performance.SoftReloadLatency | body-only soft reload 延迟基线 |
| HotReload.Performance.FullReloadLatency | 结构变化 full reload 延迟基线 |
| HotReload.Performance.RenameWindowLatency | rename-window 建模下 full reload 延迟基线 |
| HotReload.Performance.BurstChurnLatency | repeated soft/full/soft burst churn 延迟基线 |

### 属性保留

> 源文件：`HotReload/AngelscriptHotReloadPropertyTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| HotReload.SoftReload.Basic | 软重载基本路径可用 |
| HotReload.SoftReload.PreservesOtherModules | 软重载时其他模块状态保留 |
| HotReload.FullReload.Basic | 全量重载基本路径 |
| HotReload.FullReload.EnumBasic | 枚举变更触发的全量重载场景 |

### 重载分析

> 源文件：`HotReload/AngelscriptHotReloadAnalysisTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| HotReload.AnalyzeReload.NoChange | 源码未变时分析为 `SoftReload`，不要求全量 |
| HotReload.AnalyzeReload.PropertyCountChange | 属性数量变化时重载需求分析正确 |
| HotReload.AnalyzeReload.SuperClassChange | 父类变化被分析识别 |
| HotReload.AnalyzeReload.SoftReloadRequirement | 需软重载的情形被正确标记 |
| HotReload.AnalyzeReload.ClassAdded | 新增类对分析结果的影响 |
| HotReload.AnalyzeReload.ClassRemoved | 删除类对分析结果的影响 |
| HotReload.AnalyzeReload.FunctionSignatureChanged | 函数签名变化对重载分析的影响 |

---

## 6. Internals — 内部机制

### Builder 构建器

> 源文件：`Internals/AngelscriptBuilderTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Builder.SingleModulePipeline | 单模块构建管线端到端 |
| Internals.Builder.CompileErrors | 编译错误收集与报告 |
| Internals.Builder.RebuildModule | 模块重建行为 |
| Internals.Builder.ImportBinding | import/绑定导入路径 |

### Restore 序列化

> 源文件：`Internals/AngelscriptRestoreTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Restore.RoundTrip | 脚本节点/字节码等序列化往返一致 |
| Internals.Restore.StripDebugInfoRoundTrip | 去掉调试信息后仍可往返 |

### Compiler 编译器

> 源文件：`Internals/AngelscriptCompilerTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Compiler.BytecodeGeneration | 编译器生成字节码 |
| Internals.Compiler.VariableScopes | 变量作用域 |
| Internals.Compiler.FunctionCalls | 函数调用编译 |
| Internals.Compiler.TypeConversions | 类型转换编译 |

### DataType 数据类型

> 源文件：`Internals/AngelscriptDataTypeTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.DataType.Primitives | 内部原始类型表示 |
| Internals.DataType.Comparisons | 类型比较/相等语义 |
| Internals.DataType.ObjectHandles | 对象句柄类型 |
| Internals.DataType.SizeAndAlignment | 大小与对齐 |

### GC 垃圾回收内部

> 源文件：`Internals/AngelscriptGCInternalTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.GC.Statistics | GC 统计接口 |
| Internals.GC.EmptyCollect | 空集合回收行为 |
| Internals.GC.InvalidLookup | 无效对象查找处理 |
| Internals.GC.ReportUndestroyedEmpty | 未销毁报告为空场景 |
| Internals.GC.ManualCycleCollection | 手动环收集 |
| Internals.GC.CycleDetection | 环检测 |

### Parser 解析器

> 源文件：`Internals/AngelscriptParserTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Parser.Declarations | 解析器：声明解析 |
| Internals.Parser.ExpressionAst | 表达式 AST |
| Internals.Parser.ControlFlow | 控制流解析 |
| Internals.Parser.SyntaxErrors | 语法错误处理 |

### ScriptNode 脚本节点

> 源文件：`Internals/AngelscriptScriptNodeTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.ScriptNode.Types | 脚本节点类型 |
| Internals.ScriptNode.Traversal | 节点遍历 |
| Internals.ScriptNode.Copy | 节点拷贝 |

### Bytecode 字节码

> 源文件：`Internals/AngelscriptBytecodeTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Bytecode.InstructionSequence | 字节码指令序列 |
| Internals.Bytecode.Append | 字节码追加 |
| Internals.Bytecode.JumpResolution | 跳转解析/回填 |
| Internals.Bytecode.Output | 字节码输出 |

### Memory 内存管理

> 源文件：`Internals/AngelscriptMemoryTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Memory.Construction | 内存管理器构造 |
| Internals.Memory.FreeUnused | 释放未使用内存 |
| Internals.Memory.ScriptNodeReuse | ScriptNode 池复用 |
| Internals.Memory.ByteInstructionReuse | 字节指令复用 |
| Internals.Memory.PoolLeakTracking | 池泄漏追踪 |

### Tokenizer 词法分析

> 源文件：`Internals/AngelscriptTokenizerTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.Tokenizer.BasicTokens | 基本 token |
| Internals.Tokenizer.Keywords | 关键字 |
| Internals.Tokenizer.CommentsAndStrings | 注释与字符串 |
| Internals.Tokenizer.ErrorRecovery | 错误恢复 |

### StructCppOps

> 源文件：`Internals/AngelscriptStructCppOpsTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Internals.StructCppOps.NotBlueprintTypeByDefault | 结构体 C++ 操作默认非 BlueprintType 行为 |

---

## 7. Compiler — 编译管线

> 源文件：`Compiler/AngelscriptCompilerPipelineTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Compiler.EndToEnd.DelegateEnumClassCompile | 委托与 `enum class` 等端到端编译 |
| Compiler.EndToEnd.FunctionDefaultsAndClassLikeCompile | 函数默认值与类式声明编译 |
| Compiler.EndToEnd.PropertyDefaultsCompile | 属性默认值编译 |
| Compiler.EndToEnd.GeneratedClassConsistency | 生成 `UClass` 一致性 |
| Compiler.EndToEnd.ModuleFunctionInspection | 模块内函数反射/检查 |
| Compiler.EndToEnd.EnumAvailability | 枚举在生成侧可用性 |
| Compiler.EndToEnd.DelegateSignatureConsistency | 委托签名一致性 |
| Compiler.EndToEnd.ClassLikeReflectionShape | 类式反射形状 |

---

## 8. Preprocessor — 预处理器

> 源文件：`Preprocessor/PreprocessorTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Preprocessor.BasicParse | 预处理器基础解析 |
| Preprocessor.MacroDetection | 宏检测与展开 |
| Preprocessor.ImportParsing | `#import` 等解析 |

---

## 9. ClassGenerator — 类生成器

> 源文件：`ClassGenerator/ClassGeneratorTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| ClassGenerator.EmptyModuleSetup | 空模块下类生成器初始化/搭建 |

---

## 10. FileSystem — 文件系统

> 源文件：`FileSystem/AngelscriptFileSystemTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| FileSystem.ModuleLookupByFilename | 按文件名查找模块 |
| FileSystem.CompileFromDisk | 从磁盘路径编译 |
| FileSystem.PartialFailurePreservesGoodModules | 部分模块失败时其余好模块仍保留 |
| FileSystem.Discovery | 脚本文件发现 |
| FileSystem.SkipRules | 跳过规则（不扫描某些路径） |

---

## 11. Editor — 编辑器

> 源文件：`AngelscriptEditor/Private/Tests/AngelscriptDirectoryWatcherTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Editor.DirectoryWatcher.Queue.ScriptAddAndRemove | `.as` 文件 add/remove 进入对应队列 |
| Editor.DirectoryWatcher.Queue.IgnoresNonScriptFiles | 非 `.as` 文件被忽略 |
| Editor.DirectoryWatcher.Queue.FolderAddScansContainedScripts | 新增目录递归扫描脚本 |
| Editor.DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator | 删除目录通过已加载脚本枚举生成删除队列 |
| Editor.DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd | rename-window 建模下 remove+add 同时入队 |

> 源文件：`Editor/AngelscriptSourceNavigationTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Editor.SourceNavigation.Functions | 编译注解类后，`UASFunction` 保留源路径与行号，`FSourceCodeNavigation::CanNavigateTo*` 为真 |

---

## 12. Themed Integration Tests — 主题化集成回归

> 说明：
>
> - 这组测试原先集中放在 Scenarios/ 目录下，现已按主题拆分到 Actor/、Blueprint/、ClassGenerator/、Component/、Delegate/、GC/、HotReload/、Inheritance/、Interface/、Subsystem/ 等目录。
> - 自动化测试路径已去掉历史上的 Scenario 中间层，目录分类与测试发现路径现在保持一致。

### 12.1 Actor 生命周期

> 源文件：`Actor/AngelscriptActorLifecycleTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Actor.BeginPlay | 脚本重写 `BeginPlay`，`BeginPlayCalled` 为 1 |
| Actor.Tick | 启用 Actor Tick 后多次世界 tick，`TickCount` ≥ 5 |
| Actor.ReceiveEndPlay | 销毁后 `EndPlay` 调用，`EndPlayCalled` 为 1 |
| Actor.ReceiveDestroyed | 销毁流程中 `Destroyed` 调用，`DestroyedCalled` 为 1 |
| Actor.Reset | 修改 `ResetValue` 后 `Actor->Reset()` 触发脚本 `OnReset`，值为 7 |

### 12.2 Actor 交互

> 源文件：`Actor/AngelscriptActorInteractionTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Actor.PointDamage | `ApplyPointDamage` 后脚本的 `PointDamage` 用伤害值设置 `ActorTickInterval`（42） |
| Actor.RadialDamage | 球体重叠路径施加径向伤害，脚本 `RadialDamage` 将缩放设为伤害值（24） |
| Actor.MultiSpawn | 连续生成 3 个实例，BeginPlay 计数总和 ≥3 且三者指针互异 |
| Actor.CrossCall | A 的 Tick 调用 B 的 `UFUNCTION`，B 的 `CallCount` ≥ 1 |

### 12.3 Actor 属性

> 源文件：`Actor/AngelscriptActorPropertyTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Actor.UProperty | 反射 int/FString `UPROPERTY` 在实例上保持脚本默认值 |
| Actor.UFunction | `GetHealth` 经 `ExecuteGeneratedIntEventOnGameThread` 返回与 `Health` 一致（100） |
| Actor.DefaultValues | `default PrimaryActorTick.TickInterval = 0.5f` 生效 |

### 12.4 ScriptClass 创建

> 源文件：`ClassGenerator/AngelscriptScriptClassCreationTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| ScriptClass.CompilesToUClass | 脚本类生成 `UClass` 且派生自 `AAngelscriptActor`，`SpawnMarker` 默认为 7 |
| ScriptClass.CanSpawnInTestWorld | 在测试世界中生成脚本 Actor，`BeginPlay` 后 `BeginPlayObserved` 为 1 |
| ScriptClass.MultiSpawnKeepsStateIsolation | 同一类多实例：改其一 `LocalState` 不影响另一实例 |
| ScriptClass.BlueprintChildCompiles | 以脚本类为父的蓝图可编译、继承关系正确 |
| ScriptClass.CDOHasExpectedDefaults | CDO 与实例上 int/bool/FString 默认值与脚本一致 |
| ScriptClass.RecompileDoesNotCrashClassSwitch | 同模块重编译后新属性与更新默认值可见 |
| ScriptClass.NonUClassTypeCannotSpawn | 非 Actor 的 `UObject` 脚本类可 `NewObject`，但 `SpawnActor` 返回 null |

### 12.5 ScriptActor 重载执行

> 源文件：`Actor/AngelscriptScriptSpawnedActorOverrideTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| ScriptActor.BeginPlayRunsInWorld | 脚本 Actor 进入世界后 `BeginPlayObserved` 为 1 |
| ScriptActor.NativeUFunctionCanBeInvoked | `ProcessEvent` 调用带 int 参数的脚本 `UFUNCTION`，属性反映调用与参数 |
| ScriptActor.BeginPlayCallsAnotherScriptUFunction | `BeginPlay` 内调用另一脚本 `UFUNCTION`，`ScriptDispatchObserved` 为 1 |
| ScriptActor.TickRunsNTimes | 启用 Tick 后手动 tick N 次，`LogicalTickCount` 增量为 N |
| ScriptActor.CrossInstanceCallDoesNotLeakState | 实例 A 在 `BeginPlay` 通过引用调用 B，双方 `LocalState` 互不串扰 |
| ScriptActor.DestroyedActorInvocationFailsSafely | 目标 Actor 销毁后，源侧调用不执行目标体 |
| ScriptActor.MissingFunctionReportsExplicitFailure | 对不存在函数名调用返回失败 |

### 12.6 BlueprintSubclass 蓝图子类化

> 源文件：`Blueprint/AngelscriptBlueprintSubclassActorTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Actor.BlueprintSubclassBeginPlay | 以脚本 Actor 为父创建蓝图子类，生成实例后继承的 `BeginPlay` 执行一次 |

### 12.7 BlueprintChild 运行时

> 源文件：`Blueprint/AngelscriptBlueprintSubclassRuntimeTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| BlueprintChild.InheritsScriptBeginPlay | BP 子类继承并执行脚本层的 `BeginPlay` 重载 |
| BlueprintChild.InheritsScriptTick | BP 子类继承并执行脚本层的 `Tick` 重载 |
| BlueprintChild.ScriptUFunctionStillCallable | BP 子类可以通过 `ProcessEvent` 调用脚本定义的 `UFUNCTION` |
| BlueprintChild.RecreateDoesNotLeakPreviousState | BP 子类重新创建不会泄漏之前实例的状态 |
| BlueprintChild.NoOverrideUsesScriptParentDefault | BP 子类不覆盖属性时正确继承脚本父类的默认值 |
| BlueprintChild.OverrideChainHasDeterministicCounts | 脚本父→子重写链再被蓝图子类化后，各层计数确定性正确 |

### 12.8 Component 组件

> 源文件：`Component/AngelscriptComponentScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Component.BeginPlay | 脚本组件 `BeginPlay` 将 `bReady` 置 true |
| Component.Tick | 启用组件 Tick 后多次世界 tick，`TickCount` ≥ 5 |
| Component.ReceiveEndPlay | 宿主 Actor 销毁后组件 `EndPlay` 将 `bCleanedUp` 置 true |
| Component.ActorOwner | 组件 `BeginPlay` 中 `Cast` 宿主脚本 Actor 并读取 `OwnerValue` 为 42 |

### 12.9 DefaultComponent 默认组件

> 源文件：`Component/AngelscriptComponentScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| DefaultComponent.Basic | `DefaultComponent`+`RootComponent` 生成正确类型的根组件 |
| DefaultComponent.Multiple | 根 Scene + 子 Billboard 默认组件存在且父子附着关系正确 |

### 12.10 Inheritance 继承场景

> 源文件：`Inheritance/AngelscriptInheritanceScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Inheritance.ScriptToScript | 脚本 Actor 间继承+重写 `UFUNCTION` 的 reload 分析（当前分支为 Error） |
| Inheritance.Super | 含 `Super::` 的脚本继承场景分析（当前分支为 Error） |
| Inheritance.IsA | `Cast`/IsA 类脚本变更可分析，且要求 Full reload |

### 12.11 Interface 接口

> 源文件：`Interface/AngelscriptInterfaceDeclareTests.cpp`、`Interface/AngelscriptInterfaceImplementTests.cpp`、`Interface/AngelscriptInterfaceCastTests.cpp`、`Interface/AngelscriptInterfaceAdvancedTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Interface.DeclareBasic | 脚本声明 `UINTERFACE` 生成带 `CLASS_Interface` 的 `UClass` |
| Interface.DeclareInheritance | 子接口继承父接口并生成，子接口类带 `CLASS_Interface` |
| Interface.ImplementBasic | Actor 实现接口后 `ImplementsInterface` 为 true |
| Interface.ImplementMultiple | 单 Actor 同时实现两个接口，二者 `ImplementsInterface` 均为 true |
| Interface.ImplementsInterfaceMethod | `BeginPlay` 中 `this.ImplementsInterface(...)` 为真 |
| Interface.CastSuccess | 实现接口的 Actor `Cast` 到接口成功，`CastSucceeded` 为 1 |
| Interface.CastFail | 未实现接口的 Actor `Cast` 得到 null |
| Interface.MethodCall | `Cast` 成功后通过接口引用调用 `TakeDamage`，`MethodCalled` 为 1 |
| Interface.InheritedInterface | 子接口继承父接口时，实现子接口的 Actor 对父子接口均 `ImplementsInterface` |
| Interface.MissingMethod | 声明接口方法未全实现时编译报错 |
| Interface.NoProperty | 纯接口类上无 `UPROPERTY` 成员 |
| Interface.GCSafe | 实现接口的 Actor 销毁并 GC 后弱引用失效 |
| Interface.HotReload | Full reload 后类仍实现同一接口；行为可按新版更新 |
| Interface.CppInterface | 脚本声明并实现接口的 Actor 可被检测为 `ImplementsInterface` |
| Interface.MultipleInheritanceChain | 多层接口继承链上，实现叶接口的 Actor 对基/中/叶接口均满足 |

### 12.12 Delegate 委托

> 源文件：`Delegate/AngelscriptDelegateScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Delegate.Unicast | 脚本声明 delegate，绑定原生函数，触发后 `NameCounts["Unicast"]` 为 77 |
| Delegate.Multicast | `event` 多播在 `BeginPlay` 绑定脚本处理，C++ 广播后 `EventTriggerCount` 增加 |

### 12.13 GC 垃圾回收

> 源文件：`GC/AngelscriptGCScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| GC.ActorDestroy | 脚本 Actor 销毁并 GC 后弱引用无效 |
| GC.ComponentDestroy | 脚本组件 `DestroyComponent` 并 GC 后弱引用无效 |
| GC.WorldTeardown | `FActorTestSpawner` 作用域结束后世界/Actor/组件弱引用均被释放 |

### 12.14 Subsystem 子系统

> 源文件：`Subsystem/AngelscriptSubsystemScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| WorldSubsystem.Lifecycle | `UScriptWorldSubsystem` 生命周期（当前分支编译失败） |
| WorldSubsystem.Tick | World 子系统 `BP_Tick`（当前分支编译失败） |
| WorldSubsystem.ActorAccess | World 子系统在 Tick 中访问 Actor（当前分支编译失败） |
| GameInstanceSubsystem.Lifecycle | `UScriptGameInstanceSubsystem` 生命周期（当前分支编译失败） |

### 12.15 HotReload 热重载场景

> 源文件：`HotReload/AngelscriptHotReloadScenarioTests.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| HotReload.PropertyPreserved | Soft reload 后类指针不变、实例 `Counter` 保留 |
| HotReload.AddProperty | Full reload 后新属性 `NewValue` 默认 99，原属性仍存在 |
| HotReload.FunctionChange | Soft reload 前后 `GetValue` 分别从 1 变为 2 |
| HotReload.PIEStructuralChangeNeedsFullReload | 分析脚本增属性变更，要求走 Full reload 路径 |

---

## 13. Examples — 示例脚本编译

> 源文件：`Examples/AngelscriptScriptExample*Test.cpp`（各文件对应一个示例 `.as`）
>
> 所有测试通过 `RunScriptExampleCompileTest` 将内嵌示例脚本编译为注解模块，验证文档级示例脚本能完整通过编译。

| 测试名 | 验证内容（示例主题） |
|--------|----------------------|
| ScriptExamples.Actor | Actor 类/UProperty/BlueprintOverride |
| ScriptExamples.AccessSpecifiers | 访问修饰符 |
| ScriptExamples.Array | 数组操作 |
| ScriptExamples.BehaviorTreeNodes | 行为树节点 |
| ScriptExamples.CharacterInput | 角色输入 |
| ScriptExamples.ConstructionScript | 构造脚本 |
| ScriptExamples.Delegates | 委托 |
| ScriptExamples.Enum | 枚举 |
| ScriptExamples.FormatString | 格式化字符串 |
| ScriptExamples.Functions | 函数示例 |
| ScriptExamples.FunctionSpecifiers | 函数说明符 |
| ScriptExamples.Map | Map 容器 |
| ScriptExamples.Math | 数学 |
| ScriptExamples.MixinMethods | Mixin 方法 |
| ScriptExamples.MovingObject | 移动对象 |
| ScriptExamples.Overlaps | 重叠检测 |
| ScriptExamples.PropertySpecifiers | 属性说明符（依赖 `Example_Enum.as`） |
| ScriptExamples.Struct | 结构体 |
| ScriptExamples.Timers | 定时器 |
| ScriptExamples.WidgetUMG | UMG 控件 |

---

## 14. Template — 模板场景

> 源文件：`Template/Template_Blueprint.cpp`、`Template_BlueprintWorldTick.cpp`、`Template_WorldTick.cpp`

| 测试名 | 验证内容 |
|--------|----------|
| Template.Blueprint.ScriptParentChild | 蓝图与脚本父类子类关系模板场景 |
| Template.Blueprint.ActorChildWorldTick | 蓝图 Actor 子类在世界 Tick 下行为模板 |
| Template.WorldTick.ScriptActorLifecycle | 世界 Tick 驱动的脚本 Actor 生命周期模板 |
