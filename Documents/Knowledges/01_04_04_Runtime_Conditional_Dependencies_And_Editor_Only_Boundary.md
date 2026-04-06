# Runtime 的条件依赖与 Editor-Only 边界处理

> **所属模块**: 插件模块清单与装载关系 → Runtime 条件依赖 / Editor-Only Boundary
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp`, `Documents/Knowledges/01_04_02_Module_Dependency_Surface_Runtime_Editor_Test.md`

这一节真正要讲清楚的，不是 `AngelscriptRuntime` “也会在 editor build 下加几个依赖” 这么一句话，而是这背后实际存在两道边界：**编译时边界** 和 **运行时边界**。编译时，`AngelscriptRuntime.Build.cs` 用 `Target.bBuildEditor` 条件地把 `UnrealEd`、`EditorSubsystem`、`UMGEditor` 挂进来，让 Runtime 在 editor 目标下具备接触这些模块的能力；运行时，`FAngelscriptRuntimeModule` 又继续用 `GIsEditor`、`IsRunningCommandlet()` 和 fallback tick 这类门禁，决定哪些逻辑真的要在当前进程里生效。也就是说，当前 Runtime 的 editor-only 处理不是一层判断，而是“先允许构建，再决定启用”的两级边界。

## 第一层边界：`Build.cs` 决定 Runtime 是否**可以**看见 editor 模块

`AngelscriptRuntime.Build.cs` 里最关键的条件分支就是：

```csharp
if (Target.bBuildEditor)
{
    PublicDependencyModuleNames.AddRange(new string[]
    {
        "UnrealEd",
        "EditorSubsystem",
    });

    PrivateDependencyModuleNames.AddRange(new string[]
    {
        "UMGEditor",
    });
}
```

这段代码首先说明一件事：`AngelscriptRuntime` 并不是“绝对纯 runtime、绝不碰 editor 类型”的模块。它在 editor 目标下明确允许自己链接部分 editor-only 模块。

但这层判断也同时说明另一件事：这种依赖是**条件性的**，不是 Runtime 的常驻身份。因为：

- 在非 editor 目标下，这些模块不会被拉进来；
- Runtime 常驻依赖面仍然以 `Core`、`Engine`、`Json`、`GameplayTags`、`StructUtils` 这些运行时/公共模块为主；
- 因此 Runtime 并没有因为支持 editor 构建，就在装载层变成 `Editor` 模块。

换句话说，这里的 `Target.bBuildEditor` 不是在改 Runtime 的身份，而是在声明：**当宿主是 editor build 时，Runtime 可以有一层 editor-aware 的编译能力。**

## 第二层边界：Runtime 模块自己再决定是否**真的启用** editor 行为

仅有编译条件还不够，因为“能编进去”不等于“当前进程里就该跑起来”。这一层在 `FAngelscriptRuntimeModule::StartupModule()` 里继续做了运行时门禁：

```cpp
void FAngelscriptRuntimeModule::StartupModule()
{
    if (GIsEditor || IsRunningCommandlet())
    {
        InitializeAngelscript();
    }

    if (GIsEditor)
    {
        FallbackTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FAngelscriptRuntimeModule::TickFallbackPrimaryEngine));
    }
}
```

这里其实又切了两层：

- **初始化门禁**：只有 Editor 或 Commandlet 环境才主动 `InitializeAngelscript()`
- **Editor 专属兜底**：只有 `GIsEditor` 时才注册 fallback tick

这说明当前 Runtime 的 editor-only 边界不是“依赖进来了就自动工作”，而是：

1. 先在构建层允许 editor 能力存在；
2. 再在运行层判断当前是不是 Editor / Commandlet；
3. 最后才真正启用相应逻辑。

因此 Runtime 当前的 editor-only 处理应理解成**两级防线**，而不是单点宏判断。

## 为什么要做成两级边界，而不是只靠 `#if WITH_EDITOR`

当前仓里当然也能看到大量 `#if WITH_EDITOR` / `#if WITH_EDITORONLY_DATA` 这样的编译宏触点，例如：

- `Bind_Deprecations.cpp`
- `Bind_FunctionLibraryMixins.cpp`
- `Bind_TSoftObjectPtr.cpp`
- `Bind_UEnum.cpp`
- `AngelscriptLevelStreamingLibrary.h`

但这些宏更多是在**具体代码片段**上做条件裁剪，而不是替代模块级的边界设计。当前 Runtime 实际上同时用了三种层次：

1. **Build.cs 条件依赖**：决定模块是否能看见 editor-only 模块
2. **模块启动门禁**：决定当前进程里要不要真的初始化相关逻辑
3. **源码级宏裁剪**：决定某些 editor-only API 或数据是否编译进来

这三层叠在一起，才形成了比较完整的边界控制。也正因为这样，Runtime 才能既保持 `Runtime` 身份，又在 editor 目标下承担足够的编辑器协作能力。

## Commandlet 是这条边界里最容易忽略的一层

`StartupModule()` 里使用的不是单纯的 `GIsEditor`，而是：

- `GIsEditor || IsRunningCommandlet()`

这意味着当前 Runtime 的边界处理并不是简单的“编辑器 vs 非编辑器”，而是至少三类环境：

1. **Editor**：初始化 Angelscript，并挂 fallback tick
2. **Commandlet**：初始化 Angelscript，但不挂 editor fallback tick
3. **普通非 editor 运行时**：模块存在，但不会在这里主动走 editor/commandlet 初始化路径

这点非常重要，因为它说明 Runtime 的 editor-only 边界其实也是一条**工具链边界**：某些非交互式工具环境虽然不是 Editor UI，但仍然需要脚本系统初始化。

因此把这条逻辑叫做“Editor-Only 边界”其实要更精确一点：它处理的是**Editor-aware / tool-aware runtime boundary**。

## `TickFallbackPrimaryEngine()` 体现了典型的 editor-aware runtime glue

`TickFallbackPrimaryEngine()` 特别适合用来理解这条边界的味道。它不是脚本系统核心算法，而是 Runtime 在 editor 环境里补的一层 glue：

- 只有在没有 `UAngelscriptGameInstanceSubsystem` tick owner 时才兜底；
- 只有在当前 `FAngelscriptEngine` `ShouldTick()` 时才真正 tick；
- 整个机制只在 `GIsEditor` 下注册。

这说明 Runtime 的 editor-only 条件依赖不是为了让它“变成 Editor 模块”，而是为了让 Runtime 在 editor 宿主里更可靠地工作。也就是说，当前这些 editor 条件依赖的角色更接近：

- `editor-aware runtime glue`
- 而不是 `editor-owned core logic`

这个区分很关键，因为前者仍然属于 Runtime 的责任，后者则应该下沉到 `AngelscriptEditor`。

## `InitializeOverrideForTesting` 说明边界还会被测试环境进一步改写

`FAngelscriptRuntimeModule` 里还有一条只在 `WITH_DEV_AUTOMATION_TESTS` 下存在的测试 override：

- `InitializeOverrideForTesting`
- `SetInitializeOverrideForTesting(...)`
- `ResetInitializeStateForTesting()`

这说明当前 Runtime 的边界并不止 editor/non-editor 两分，它还显式为测试环境留了 seam。也就是说，模块初始化边界至少受到三类因素影响：

- 构建目标是否为 editor build
- 当前进程是否为 editor / commandlet
- 当前是否处于 automation 测试 override 场景

从架构角度看，这再次证明 Runtime 的边界处理并不是随手加几个 `#if`，而是一套围绕不同宿主环境精细分流的机制。

## 这条边界为什么不该被理解成“分层不纯”

最常见的误读是：既然 Runtime 在 editor build 下会依赖 `UnrealEd`、`EditorSubsystem`、`UMGEditor`，那它是不是已经失去“基础层”的纯度了？

当前代码和前面几篇文章给出的答案是否定的。因为真正的判断标准不是“有没有任何 editor 条件依赖”，而是：

- Runtime 是否在默认关系上反向依赖 `AngelscriptEditor`？——**没有**
- editor-only 能力是否通过条件依赖和运行时门禁被限制在 editor/tool 场景里？——**是**
- Runtime 是否仍然承担唯一的脚本系统核心状态与引擎总控？——**是**

因此更准确的说法不是“分层不纯”，而是：**Runtime 保持基础层身份，同时显式承担一小部分 editor-aware 的宿主兼容责任。**

## 这条条件依赖边界应该怎么记

如果把当前 Runtime 的条件依赖和 editor-only 处理压成一句工程化判断，可以这样记：

**`AngelscriptRuntime` 在构建层通过 `Target.bBuildEditor` 允许少量 editor-only 模块进入，在运行层再通过 `GIsEditor` / `IsRunningCommandlet()` / 测试 override 决定这些能力是否真正启用，因此它保持了 Runtime 身份，同时具备 editor-aware 的宿主兼容边界。**

换成更实用的阅读过滤器就是：

- 看到 `Build.cs` 的 editor 条件依赖 → 理解成“允许编译进 editor-aware 支撑”
- 看到 `StartupModule()` 里的 `GIsEditor` / `IsRunningCommandlet()` → 理解成“真正决定是否启用”
- 看到 `#if WITH_EDITOR` / `WITH_EDITORONLY_DATA` → 理解成“更细粒度的源码裁剪”

这样就不会把三层边界机制混成一种东西。

## 小结

- `AngelscriptRuntime.Build.cs` 用 `Target.bBuildEditor` 条件地引入 `UnrealEd`、`EditorSubsystem`、`UMGEditor`，这决定了 Runtime 在 editor 构建下**可以**具备 editor-aware 能力
- `FAngelscriptRuntimeModule::StartupModule()` 再用 `GIsEditor` / `IsRunningCommandlet()` / fallback tick 门禁决定这些能力是否在当前进程里**真的启用**
- 再往下还有 `WITH_EDITOR` / `WITH_EDITORONLY_DATA` 这种源码级裁剪，因此当前 Runtime 的 editor-only 处理是多层边界，不是单点判断
- 这套设计并没有让 Runtime 失去基础层身份，而是让它在保持 Runtime 主体的前提下，承担了必要的 editor/tool 宿主兼容责任
