# `FAngelscriptEngine` 启动、编译、重载主链路

> **所属模块**: Runtime 总控与生命周期 → Engine 主链路
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_Console.h`

上一节把 `FAngelscriptRuntimeModule` 讲清楚之后，这一节要回答的是：当模块入口把控制权交给 `FAngelscriptEngine` 后，真正的主链路是怎样往前走的。当前实现不是一个平铺直叙的“大 Initialize 函数”，而是一条很明确的三段式启动路径，再接一个首编译流程，最后再挂上运行期热重载和后续信号面。

## 先看主链路轮廓

- `FAngelscriptRuntimeModule::InitializeAngelscript()` 把控制权交给 `FAngelscriptEngine::Initialize()`
- `FAngelscriptEngine::Initialize()` 再拆成 `PreInitialize_GameThread()`、`Initialize_AnyThread()`、`PostInitialize_GameThread()` 三段
- `Initialize_AnyThread()` 内部完成绑定初始化、预编译数据接入、首编译和热重载设施安装
- 运行起来之后，热重载再通过 `CheckForHotReload()` / `PerformHotReload()` 把修改过的脚本文件重新送回编译链

如果把这一套机制压成一张最小流程图，大致是：

```text
RuntimeModule::InitializeAngelscript
    -> FAngelscriptEngine::Initialize
        -> PreInitialize_GameThread
        -> Initialize_AnyThread
            -> BindScriptTypes
            -> InitialCompile
        -> PostInitialize_GameThread
    -> running state
        -> CheckForHotReload
            -> PerformHotReload
                -> CompileModules
```

这一条链路最重要的认识是：**启动、首编译、热重载不是三套彼此独立的逻辑，而是同一个 engine 总控对象在不同阶段重复进入同一套“预处理 + 编译 + 状态更新”体系。**

## `Initialize()`：三段式启动总控

`FAngelscriptEngine::Initialize()` 本身并不把所有重逻辑都直接写在一个函数里，而是先做三段调度：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp
// 函数: FAngelscriptEngine::Initialize
// 位置: Engine 主启动入口
// ============================================================================
void FAngelscriptEngine::Initialize()
{
    FAngelscriptEngineScope ScopedInitializingEngine(*this);

    PreInitialize_GameThread();

    if (ShouldInitializeThreaded())
    {
        // ★ 在高优先后台线程里跑 Initialize_AnyThread()
        Initialize_AnyThread();
    }
    else
    {
        Initialize_AnyThread();
    }

    PostInitialize_GameThread();
    InitializeOwnedSharedState();
}
```

这个入口的设计重点不是“调用了几个函数”，而是它把启动职责切成了三类：

1. **GameThread 前置准备**：只做必须由主线程完成的初始化；
2. **AnyThread 重初始化段**：把最重的引擎配置、绑定、编译链放到可并行的阶段；
3. **GameThread 收尾**：广播初始编译完成、把 owned shared state 固化下来。

这也是当前插件 Runtime 总控最关键的一个架构信号：**初始化不是纯粹的脚本 VM 构造，而是“线程模型 + Unreal 资源准备 + 编译链启动”的联合流程。**

## `PreInitialize_GameThread()`：先把引擎壳搭起来

- 这里先设置脚本对象分配器和对象构造函数
- 然后清掉线程本地 context 池里的旧上下文，避免不同引擎 epoch 之间串用旧 context
- 再从 `UAngelscriptSettings` 读取配置，把一些命名空间和自动 import 相关行为预先装载进来
- 最后真正创建底层的 `asCScriptEngine`，并初始化 `GameThreadTLD`

这一步的意义可以概括成一句话：**先把 Angelscript VM 和当前进程的线程/配置基础设施准备好，再进入真正的编译和绑定阶段。**

它还透露出一个很重要的事实：`FAngelscriptEngine` 并不是仅仅持有 `asIScriptEngine*`，而是同时持有一整套围绕 Unreal 运行环境组织起来的状态，包括包、脚本根目录、世界上下文、context 池、调试服务、JIT、预编译数据和诊断信息。

## `Initialize_AnyThread()`：真正把脚本系统跑起来

这一段是整个主链路里最重的一层。它做了四类事情：

- 配 RuntimeConfig，把包、引擎属性、自动导入、消息回调、context 回调等都装进底层 engine
- 装配可选能力，例如 bind database、precompiled data、static JIT、debug server、code coverage
- 调用 `BindScriptTypes()` 把 C++ / Unreal 侧绑定注册进脚本系统
- 调用 `InitialCompile()` 把所有脚本真正编译进来，并在后面安装热重载线程或 editor-side 配合设施

这一段最值得看的不是单个 API 调用，而是这几个顺序：

1. **先建 engine 配置和运行时资源**；
2. **再加载 bind / precompiled / debug / coverage 等配套子系统**；
3. **然后 `BindScriptTypes()`**；
4. **最后 `InitialCompile()`**。

这说明当前插件并不是“先读脚本，再边编译边发现绑定”，而是先把原生侧可用的世界搭好，再让脚本进入这个已经准备好的运行时。

## `BindScriptTypes()`：首编译之前的原生能力装配

`BindScriptTypes()` 很短，但位置非常关键：

```cpp
void FAngelscriptEngine::BindScriptTypes()
{
    FAngelscriptBinds::CallBinds(CollectDisabledBindNames());
}
```

它在主链路中的作用不是“做一个可选优化”，而是**为后续脚本编译提供原生可见类型、函数和绑定面**。如果没有这一层，后面的脚本模块就无法正确解析和引用 Unreal / C++ 侧能力。

所以主链路的前半段可以理解成：

- `PreInitialize_GameThread()`：准备 VM 与运行时壳；
- `Initialize_AnyThread()` 前半：准备配置、包、上下文、配套子系统；
- `BindScriptTypes()`：把原生世界暴露给脚本；
- `InitialCompile()`：再真正让脚本进入编译链。

## `InitialCompile()`：首编译不是一跳，而是一条小流水线

`InitialCompile()` 的工作可以拆成三层：

1. **决定输入来源**：是直接用 `PrecompiledData` 里的模块描述符，还是扫描磁盘上的 `.as` 文件并跑 `FAngelscriptPreprocessor`
2. **把模块送进 `CompileModules(ECompileType::Initial, ...)`**
3. **根据结果决定后续行为**：成功则标记初编译完成并挂测试发现；失败则进入 commandlet 退出或 editor 下的修错对话框循环

也就是说，首编译不是单纯的“扫脚本文件然后编译”，它其实已经内含了：

- 脚本根目录发现；
- 预处理或预编译描述符恢复；
- 首次模块编译；
- 初始错误恢复与 editor 等待修复机制；
- `OnPostEngineInit` 后的测试发现挂接；
- `bDidInitialCompileSucceed` / `bIsInitialCompileFinished` 两个关键状态位的落定。

这也是为什么像 `Bind_Console.h` 这种代码，会订阅 `FAngelscriptRuntimeModule::GetOnInitialCompileFinished()`：很多系统真正安全的初始化时机，不是 RuntimeModule 启动完成，而是**Engine 首编译已经完成**。

## `CompileModules()`：启动和热重载共享的一条编译总线

无论是 `InitialCompile()` 还是 `PerformHotReload()`，最后都会把模块送进 `CompileModules()`。这意味着当前插件把“编译”抽成了一条统一总线，而不是首编译和热重载各自维护一套独立编译器。

`AngelscriptEngine.h` 已经直接把这条总线的阶段写出来了：

- `CompileModule_Types_Stage1()`
- `CompileModule_Functions_Stage2()`
- `CompileModule_Code_Stage3()`
- `CompileModule_Globals_Stage4()`

这四阶段是理解后续 `1.2.4` 的前置背景，这一节只先建立总印象：**当前引擎编译不是黑盒的一次 Build，而是按类型、函数、代码、全局四层递进推进。**

同时，`CompileModules()` 还承担了几个首编译 / 重编译都共享的职责：

- 触发 `GetPreCompile()` delegate
- 建立正在编译中的模块和类的查找表
- 在非初始编译时，先把旧模块从当前引擎可见性里移除
- 处理 recompile avoidance、依赖传播和部分引用更新
- 在编译成功后触发 `GetPostCompileClassCollection()` 等后续阶段信号

所以它不仅是“执行编译”，还是 **引擎编译状态机** 的中心。

## `PerformHotReload()`：运行中的脚本如何重新进入编译链

热重载主入口在 `PerformHotReload()`，这条链和 `InitialCompile()` 有很多共性，但起点不同：

- 首编译从“全量脚本输入”出发；
- 热重载从“变更文件列表”出发。

`PerformHotReload()` 的核心步骤是：

1. 把改动文件和先前失败的文件合并成当前 reload 集；
2. 根据自动 import / 依赖关系，把需要一起重编译的模块和文件扩散出来；
3. 用 `FAngelscriptPreprocessor` 重新预处理这些文件；
4. 再次调用 `CompileModules()`，但这次 compile type 可能是 `SoftReloadOnly` 或 `FullReload`；
5. 编译成功后广播 post-compile class collection，并让 debug server 重新应用断点。

这一段最重要的架构意义是：**热重载不是绕开首编译总线的旁路，而是把“增量输入”重新送回同一个编译核心。**

也正因此，当前系统里“启动、首编译、热重载”并不是三套互不相干的代码，而是：

- 启动负责把 engine 与运行时环境建起来；
- 首编译负责第一次把脚本世界导入 engine；
- 热重载负责把后续变化重新导入这同一套 engine。

## `PostInitialize_GameThread()` 与初编译完成信号

`PostInitialize_GameThread()` 目前看起来很薄，只广播 `GetOnInitialCompileFinished()`。但这层薄不代表不重要，恰恰相反，它说明当前系统把“初始可用”这个时机显式做成了阶段信号。

这类信号的价值在 `Bind_Console.h` 里非常直观：如果首编译还没完成，某些 CVar 注册会延后到 `GetOnInitialCompileFinished()` 再做，避免在初编译线程还没稳定时就提早触碰不安全的 editor/runtime 设施。

所以 `PostInitialize_GameThread()` 的地位应该理解成：它是 Engine 主链路从“构建期”切换到“可被其他子系统安全依赖”的那个边界点。

## 把这条主链路压成一句话

如果必须用一句话概括当前 `FAngelscriptEngine` 的主链路，可以这样记：

**先在 `Initialize()` 里把运行时壳、底层 VM 和配套设施搭起来，再通过 `BindScriptTypes()` 和 `InitialCompile()` 把脚本世界首次装进 engine，之后所有运行期变化再由 `PerformHotReload()` 重新送回同一条编译总线。**

## 小结

- `FAngelscriptEngine::Initialize()` 是一条三段式启动链：`PreInitialize_GameThread()` → `Initialize_AnyThread()` → `PostInitialize_GameThread()`
- `Initialize_AnyThread()` 里真正完成了配置、bind、precompiled/JIT/debug 支撑以及 `InitialCompile()` 启动
- `InitialCompile()` 决定输入来源、进入统一 `CompileModules()` 编译总线，并负责初始失败恢复与初编译完成状态落定
- `PerformHotReload()` 不是旁路，而是把变更文件重新送回同一条编译总线，实现运行中增量更新
