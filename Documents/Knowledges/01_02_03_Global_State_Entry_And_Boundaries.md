# 全局状态入口与状态边界

> **所属模块**: Runtime 总控与生命周期 → 全局状态 / 状态边界
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptGameInstanceSubsystem.cpp`, `Documents/Guides/GlobalStateContainmentMatrix.md`, `Documents/Plans/Plan_FullDeGlobalization.md`

说到 Angelscript Runtime 的“全局状态”，最容易掉进一个误区：以为这里还在用一个单一的 `GlobalEngine` 指针把所有问题都糊住。当前实现已经不是这么简单了。现在真正的状态入口是一个混合模型：用 `FAngelscriptEngineContextStack` 解析当前 engine，用 `GAmbientWorldContext` 补世界上下文，用 `thread_local` context pool 托住脚本执行上下文，再由每个 `FAngelscriptEngine` 实例自己拥有更重的共享状态。也正因为它不再是“一个全局变量”，状态边界才更值得单独讲清。

## 先把状态分层说清楚

当前 Runtime 里的状态大致分成四层：

- **进程级静态状态**：整个进程共享的一组静态变量，例如 `GAngelscriptEngineContextStack`、`GAmbientWorldContext`、`bGeneratePrecompiledData`
- **线程级状态**：例如 `thread_local GAngelscriptContextPool` 和 `GameThreadTLD`
- **engine 实例级状态**：每个 `FAngelscriptEngine` 自己持有的模块表、诊断、世界上下文、运行时配置、热重载状态等
- **engine-owned shared state**：挂在 `FAngelscriptOwnedSharedState` 里的底层 `asCScriptEngine`、主 context、StaticJIT、PrecompiledData、BindState、TypeDatabase、BindDatabase

这四层叠在一起，构成了当前插件真正的“全局状态边界图”。因此后面讨论 containment 或去全局化时，不能只问“有没有 static 变量”，而要问：**这块状态到底属于进程、线程、当前 engine 还是 engine 共享资源。**

## 当前 engine 的真正入口：上下文栈优先，不是单一全局指针

`FAngelscriptEngine::TryGetCurrentEngine()` 是现在最核心的状态入口：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp
// 函数: FAngelscriptEngine::TryGetCurrentEngine
// 位置: 当前 engine 解析入口
// ============================================================================
FAngelscriptEngine* FAngelscriptEngine::TryGetCurrentEngine()
{
    if (FAngelscriptEngine* ScopedEngine = FAngelscriptEngineContextStack::Peek())
    {
        return ScopedEngine;
    }

    if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
    {
        if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
        {
            return AttachedEngine;
        }
    }

    return nullptr;
}
```

这段代码把当前实现的优先级讲得很清楚：

1. **先看 `FAngelscriptEngineContextStack`** —— 也就是当前作用域里是否显式安装了一个 engine；
2. **再退回 `UAngelscriptGameInstanceSubsystem::GetCurrent()`** —— 也就是生产运行时里当前游戏实例附着的 engine；
3. **最后才是空值**。

这说明当前系统已经不是“所有人都去抓一个裸的 GlobalEngine 指针”。它更像是一个**分层解析器**：优先使用显式 scope 中的当前 engine，再回落到 game subsystem 附着的生产 engine。

## `TryGetGlobalEngine()` 还在，但语义已经变了

`GlobalStateContainmentMatrix.md` 里还提到 `TryGetGlobalEngine()` / `SetGlobalEngine()` / `CurrentWorldContext` 这些名字，但要注意，当前源码已经发生了重要演进：

- `TryGetGlobalEngine()` 现在只是转发到 `TryGetCurrentEngine()`
- `SetGlobalEngine()` 也不再承担“硬切全局 engine”的旧语义，只剩下同步 ambient world context 的过渡壳
- 真正的 engine 解析已经转移到 context stack + subsystem 路线

所以如果要理解今天的状态边界，应该以当前 `TryGetCurrentEngine()` 为准，把文档里的旧术语理解成**containment 历史背景**，而不是当下唯一的实现模型。

## `GAmbientWorldContext`：world context 的全局桥，而不是 engine 本体

除了当前 engine 解析，另一个关键入口是世界上下文：

- `TryGetCurrentWorldContextObject()`：优先从当前 engine 取 `WorldContextObject`
- 如果当前 engine 不可用，再回落到 `GAmbientWorldContext`
- `AssignWorldContext()` 会同时写入当前 engine 的 `WorldContextObject` 和 `GAmbientWorldContext`

这层设计说明：**world context 和 engine 不是同一块状态。** engine 解析靠 context stack / subsystem，world context 则额外保留了一条 ambient 全局桥，以便某些调用点在没有直接 engine handle 的时候，仍能从当前环境恢复出世界对象。

`UAngelscriptGameInstanceSubsystem::GetCurrent()` 正好展示了这条桥是怎么工作的：

- 它先调用 `FAngelscriptEngine::GetAmbientWorldContext()`
- 再交给 `GEngine->GetWorldFromContextObject(...)`
- 最后从世界里拿到当前 `UGameInstance` 和 `UAngelscriptGameInstanceSubsystem`

也就是说，ambient world context 实际上是一条 **从“当前环境”反推回 subsystem/engine”的全局回路**。这也是为什么它仍然难以完全消失：没有它，很多只拿到环境对象而没拿到明确 engine 的路径就没法回到当前 runtime。

## 线程级状态：真正危险的是 thread-local 和 pooled context

当前 Runtime 里还有两类很容易被忽略，但对边界极其重要的状态：

- `class asCThreadLocalData* FAngelscriptEngine::GameThreadTLD`
- `thread_local FAngelscriptContextPool GAngelscriptContextPool`

这说明脚本执行上下文并不是简单地挂在 engine 实例里，而是分成：

- **game thread 主 context**
- **线程本地 free context 池**
- **必要时再回收到 engine 的 global context pool**

这层设计决定了“状态边界”不只是逻辑所有权问题，还包括**线程所有权**。例如 `PreInitialize_GameThread()` 会清理旧 epoch 的本地 context 池，避免不同 engine 生命周期之间复用到旧 context；`Initialize()` 在 threaded init 时还会临时切换 `GameThreadTLD`。这类逻辑之所以难 containment，正是因为它们已经深入到线程局部状态模型，而不只是几个静态 getter。

## engine 实例级状态：真正重的东西并不全局

虽然外面看到很多静态入口，但 `FAngelscriptEngine` 自己仍然持有大量明确的实例状态，例如：

- `RuntimeConfig`
- `AllRootPaths`
- `ActiveModules` / `ModulesByScriptModule`
- `Diagnostics` / `LastEmittedDiagnostics`
- `WorldContextObject`
- `bIsHotReloading` / `bUseEditorScripts` / `bUseAutomaticImportMethod`
- `GlobalContextPool`

这说明当前架构并不是“所有状态都全局乱飘”，而是一个混合体：**全局入口负责解析当前上下文，真正重的业务状态仍然落在 engine 实例里。**

这个边界非常关键，因为后续去全局化不是要把整个 `FAngelscriptEngine` 拆散，而是要逐步减少那些“必须通过静态入口才能摸到实例状态”的路径。

## `FAngelscriptOwnedSharedState`：实例里还套着一层共享资源边界

`FAngelscriptOwnedSharedState` 又把 engine 内部状态继续分了一层：

- `ScriptEngine`
- `PrimaryContext`
- `PrecompiledData`
- `StaticJIT`
- `DebugServer`
- `TypeDatabase`
- `BindState`
- `ToStringList`
- `BindDatabase`

这层 shared state 的意义不是再造一个全局，而是区分：

- 哪些东西属于当前 engine 的直接字段；
- 哪些底层资源需要在 full engine / clone engine 之间共享生命周期或延迟释放。

所以从边界上说，它更接近 **engine-owned shared resource boundary**，而不是“新的 global state”。

## 作用域 wrapper 是当前 containment 的主策略

当前代码和文档都在说明一个趋势：对全局状态的治理，不是暴力删除所有静态入口，而是优先用 scope / wrapper 显式化访问边界。

当前已经能看到的 wrapper 包括：

- `FAngelscriptEngineScope`：显式 push/pop 当前 engine，并同步世界上下文
- `FAngelscriptGameThreadScopeWorldContext`：在 game thread 范围内显式安装 / 恢复 world context
- 文档里列出的测试 containment wrapper：`FScopedGlobalEngineOverride`、`FScopedTestWorldContextScope`

这意味着当前体系的核心策略不是“假装没有全局状态”，而是：

- 对核心启动链接受最小静态边界；
- 对测试、debug、绑定这些外围路径优先用 wrapper 收口；
- 让调用方尽量显式说明“我当前依赖哪个 engine、哪个 world context”。

`Plan_FullDeGlobalization.md` 也明确把这条边界写出来了：测试 helper 和 debug server 的低风险 containment 已经完成，但 `ClassGenerator`、`Core/AngelscriptEngine.cpp`、`AngelscriptGameInstanceSubsystem.cpp`、以及部分 world-context bind 仍然属于后续完整去全局化的核心路径。

## 当前边界应该怎么记

如果把今天的全局状态入口压成一句工程化结论，可以这样记：

- **当前 engine 入口**：`TryGetCurrentEngine()`，优先 context stack，再回落 subsystem
- **当前 world context 入口**：`TryGetCurrentWorldContextObject()` / `GetAmbientWorldContext()`
- **线程执行上下文入口**：`GameThreadTLD` + `thread_local GAngelscriptContextPool`
- **真正的重状态承载体**：`FAngelscriptEngine` 实例与 `FAngelscriptOwnedSharedState`

这样分层之后，很多“全局状态”问题其实就能被重新表述成更准确的问题：

- 这是进程级静态入口问题？
- 这是线程局部上下文问题？
- 这是 engine 实例所有权问题？
- 还是 shared resource 生命周期问题？

一旦问题被拆对，containment 或去全局化路线才不会误把核心 bootstrapping 路径当成普通工具函数去重构。

## 小结

- 当前 Runtime 不再依赖单一 `GlobalEngine`，而是使用 `FAngelscriptEngineContextStack` + `UAngelscriptGameInstanceSubsystem` 的双层 engine 解析入口
- `GAmbientWorldContext` 仍然是关键的全局桥，用来把环境对象反推回当前 world / subsystem / engine 语义
- `GameThreadTLD` 与 `thread_local GAngelscriptContextPool` 说明状态边界还深入到了线程级执行上下文
- 真正重的业务状态仍主要落在 `FAngelscriptEngine` 实例与 `FAngelscriptOwnedSharedState`，当前 containment 的重点是减少外围路径对静态入口的直接依赖，而不是假装这些核心边界可以一次性消失
