# `FAngelscriptRuntimeModule` 初始化入口

> **所属模块**: Runtime 总控与生命周期 → 模块初始化入口
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`

这一节只回答一个问题：当前插件 Runtime 层真正的启动入口在哪里，以及这个入口到底负责什么。答案并不是“引擎一启动就直接跑完整编译链”，而是先由 `FAngelscriptRuntimeModule` 作为 Unreal 模块入口接住生命周期，再决定是否初始化 Angelscript 引擎、是否接管主引擎实例、是否在编辑器下补一个 fallback tick。

## 为什么要先看 `FAngelscriptRuntimeModule`

- `FAngelscriptEngine` 是脚本系统真正的总控对象，但它不是 Unreal 模块系统里的第一个入口
- 在 Unreal 插件模型下，最先被模块管理器加载的是 `FAngelscriptRuntimeModule`
- 所以如果想搞清楚“脚本引擎什么时候被创建、什么时候被推入当前上下文、什么时候开始 tick”，必须先看模块入口，而不是直接跳到 `FAngelscriptEngine::Initialize()`

`AngelscriptRuntimeModule.h` 给出的接口已经说明了它不是纯转发层。它公开了：

- `StartupModule()` / `ShutdownModule()`：Unreal 模块生命周期入口
- `InitializeAngelscript()`：Runtime 模块级初始化闸门
- 一组编译/编辑器/调试相关 delegate accessor，例如 `GetPreCompile()`、`GetPostCompile()`、`GetOnInitialCompileFinished()`
- `OwnedPrimaryEngine` 与 `bInitializeAngelscriptCalled`：模块侧持有的生命周期状态

也就是说，`FAngelscriptRuntimeModule` 的职责不是“包装一下 `FAngelscriptEngine`”，而是作为 **插件 Runtime 层与 Unreal 模块系统之间的接缝控制器**。

## 模块启动时到底做了什么

最关键的入口在 `StartupModule()`：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp
// 函数: FAngelscriptRuntimeModule::StartupModule
// 位置: Runtime 模块被 Unreal 模块系统加载后的第一跳
// ============================================================================
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

这段入口代码可以拆成两个判断：

1. **Editor / Commandlet 门禁**：只有在编辑器或命令行工具环境里，模块启动时才主动初始化 Angelscript；
2. **Editor-only fallback tick**：只有编辑器环境下，才安装一个后备 ticker，用来在没有显式 tick owner 时驱动主引擎 tick。

这说明模块入口并不是“无条件创建并启动脚本世界”，而是先根据当前运行环境做分流。这个分流非常关键，因为它决定了 Runtime 模块的职责边界：**模块负责选择何时进入 Angelscript 初始化，而不是把所有环境都一刀切地走同一条启动路径。**

## `InitializeAngelscript()` 是真正的模块级闸门

`StartupModule()` 只是触发点，真正的初始化入口在 `InitializeAngelscript()`：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp
// 函数: FAngelscriptRuntimeModule::InitializeAngelscript
// 位置: Runtime 模块层面的幂等初始化闸门
// ============================================================================
void FAngelscriptRuntimeModule::InitializeAngelscript()
{
    if (bInitializeAngelscriptCalled)
        return;

    bInitializeAngelscriptCalled = true;

    FModuleManager::Get().LoadModuleChecked(TEXT("AngelscriptRuntime"));
    if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
    {
        CurrentEngine->Initialize();
    }
    else
    {
        OwnedPrimaryEngine = MakeUnique<FAngelscriptEngine>();
        FAngelscriptEngineContextStack::Push(OwnedPrimaryEngine.Get());
        OwnedPrimaryEngine->Initialize();
    }
}
```

这段逻辑的职责边界很清楚：

- 它先用 `bInitializeAngelscriptCalled` 做 **模块级幂等保护**，避免重复初始化
- 它再检查当前上下文里是否已经存在 `FAngelscriptEngine`
- 如果已有当前引擎，就直接初始化已有实例；如果没有，就由模块自己创建一个 `OwnedPrimaryEngine`
- 当模块自己创建主引擎时，它还负责把这个引擎推入 `FAngelscriptEngineContextStack`

也就是说，模块入口不仅决定“要不要初始化”，还决定“当前这次初始化是复用已有引擎，还是创建并拥有一个新的主引擎实例”。

## 模块拥有的不是全部逻辑，而是生命周期控制权

- `FAngelscriptEngine` 里真正承载了配置、预处理、编译、热重载、调试、JIT、类型绑定等重逻辑
- 但 `FAngelscriptRuntimeModule` 掌握的是这些逻辑之前的一层：谁来拥有主引擎、谁把它装进当前上下文、谁在模块卸载时回收它
- 这就是“模块入口”和“引擎总控”最容易混淆、但必须拆开的地方

从头文件也能看出这层边界：

- `FAngelscriptRuntimeModule` 保存 `OwnedPrimaryEngine`
- `FAngelscriptEngine` 暴露 `TryGetCurrentEngine()`、`Initialize()`、`Tick()`、`ShouldTick()`

这说明两者分工是：

- **RuntimeModule**：决定是否进入初始化、是否创建主引擎、是否安装 tick 兜底、是否在关闭时清理 owned engine
- **AngelscriptEngine**：真正执行脚本系统初始化与后续运行时主链路

## 测试 override 也是模块入口职责的一部分

`InitializeAngelscript()` 里还有一条只在自动化测试下生效的路径：

- `InitializeOverrideForTesting`
- `SetInitializeOverrideForTesting()`
- `ResetInitializeStateForTesting()`

这意味着 Runtime 模块不仅是生产环境入口，也明确承担了**测试环境可替换初始化入口**的职责。对于架构理解来说，这一点很重要：测试不是从外部偷偷绕过 RuntimeModule，而是通过模块自己提供的 seam 注入替代引擎。

这条设计有两个意义：

1. 测试能复用真实的模块级初始化路径，而不是另起一套“假启动流程”；
2. 生产逻辑与测试 seam 都收口在同一个初始化闸门里，减少了环境分叉带来的理解成本。

## fallback tick 为什么属于模块入口而不是引擎内部

`TickFallbackPrimaryEngine()` 放在 `FAngelscriptRuntimeModule` 而不是 `FAngelscriptEngine` 里，也说明了一层职责判断：

- 这个 ticker 不是脚本系统核心执行语义的一部分
- 它是 Runtime 模块在 **Editor 环境** 下，为了保证没有显式 tick owner 时主引擎仍能推进而加的一层宿主集成兜底

换句话说，`TickFallbackPrimaryEngine()` 更像模块与宿主运行环境之间的 glue，而不是脚本引擎本身的核心算法。把它放在 RuntimeModule 里，正好把“核心引擎逻辑”和“模块级宿主集成逻辑”分开。

## delegate accessor 暗示了它还是跨模块信号面

`FAngelscriptRuntimeModule` 头文件里还公开了大量静态 delegate accessor，例如：

- `GetPreCompile()`
- `GetPostCompile()`
- `GetOnInitialCompileFinished()`
- `GetClassAnalyze()`
- `GetEditorCreateBlueprint()`

这说明 RuntimeModule 还有第三层职责：**它不仅是启动入口，还是 Runtime 向其他模块暴露生命周期信号的统一表面。**

这层信号面解释了为什么像 `Bind_Console.h` 这类绑定逻辑会挂到 `GetOnInitialCompileFinished()` 上：别的子系统不需要直接接管 RuntimeModule 初始化细节，只需要订阅它暴露出来的阶段信号。

所以从架构上看，`FAngelscriptRuntimeModule` 至少同时承担三件事：

1. 模块生命周期入口；
2. 主引擎拥有者 / 安装者；
3. Runtime 生命周期信号面。

## 读这一层时最容易犯的误区

- 误区一：把 `StartupModule()` 当成“完整引擎启动流程”本身。实际上它只是 Runtime 模块进入初始化链路的第一跳
- 误区二：把 `FAngelscriptEngine` 看成唯一入口。实际上在 Unreal 插件模型下，先被加载的是 RuntimeModule，再由它把引擎装起来
- 误区三：忽略 `OwnedPrimaryEngine` 和上下文栈。这样会看不懂“已有引擎复用”和“模块创建主引擎”之间的区别

一旦把这三点看清，后面再看 `FAngelscriptEngine::Initialize()`、`InitialCompile()` 和热重载链路时，就不会把“模块入口层”和“引擎总控层”搅在一起。

## 小结

- `FAngelscriptRuntimeModule` 是 Angelscript Runtime 层进入 Unreal 生命周期的第一入口
- `StartupModule()` 负责环境门禁与 editor-only fallback tick 安装
- `InitializeAngelscript()` 负责幂等初始化、已有引擎复用、主引擎创建与上下文栈安装
- `FAngelscriptEngine` 承担真正的重逻辑，而 RuntimeModule 负责把它接进模块生命周期并向外暴露阶段信号
