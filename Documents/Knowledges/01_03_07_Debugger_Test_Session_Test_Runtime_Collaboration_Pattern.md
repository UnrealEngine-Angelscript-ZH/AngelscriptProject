# Debugger Test Session 的 Test-Runtime 协作模式

> **所属模块**: Editor / Test / Dump 协作边界 → Debugger Test Session / Test-Runtime Collaboration
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptDebuggerTestSession.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptDebuggerTestSession.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptDebuggerTestClient.h`, `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptDebuggerScriptFixture.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/Debugger/AngelscriptDebuggerSmokeTests.cpp`, `Documents/Guides/TestConventions.md`

这一节真正要讲清楚的，不是“Debugger 测试会连一个 socket”，而是当前仓库如何把 Debugger 验证拆成一套稳定的 Test-Runtime 协作模式。Runtime 持有真正的 `FAngelscriptDebugServer` 和调试状态；Test 模块则通过 `FAngelscriptDebuggerTestSession`、`FAngelscriptDebuggerTestClient` 和 `FAngelscriptDebuggerScriptFixture` 这三层 helper，把“拿到可调试引擎、驱动 debug server、发协议消息、编译固定脚本场景、等待断点/步进结果”压成了可复用的测试骨架。也正因为它们服务的是**多类 Debugger 场景测试**，而不是某一个单测文件，所以它们被放在 `Shared/`，而不是某个具体 case 文件里。

## 先看 `TestConventions` 给出的定位

`Documents/Guides/TestConventions.md` 已经把 Debugger 这一层单独列出来：

- Debugger 场景目录在 `Plugins/Angelscript/Source/AngelscriptTest/Debugger/`
- Automation 前缀使用 `Angelscript.TestModule.Debugger.*`
- 典型 shared helper 明确写成：`Shared/AngelscriptDebuggerTestSession.*`、`Shared/AngelscriptDebuggerTestClient.*`、`Shared/AngelscriptDebuggerScriptFixture.*`

这条规则非常关键，因为它说明这三类 helper 的存在不是偶然的，而是当前测试体系明确承认的一层：**Debugger 场景测试应该共享一套 Test-Runtime 骨架，而不是每个 case 各自拼 socket、engine 和脚本样例。**

## 第一层：`FAngelscriptDebuggerTestSession` 把 Runtime 调试服务装进一个可控会话

`FAngelscriptDebuggerTestSession` 是这套模式里最核心的 Test-Runtime 桥。它做的不是协议发送，而是把 Runtime 世界里的调试服务变成一个测试可驱动、可清理、可轮询的会话对象。

从头文件能直接看出它的责任面：

- `Initialize(...)` / `Shutdown()`
- `PumpOneTick()` / `PumpUntil(...)`
- `GetEngine()` / `GetDebugServer()` / `GetPort()`
- `OwnedEngine` / `Engine` / `DebugServer` / `GlobalScope`

这说明 `Session` 的核心职责不是“发一条调试消息”，而是：

1. 拿到一个可调试的 `FAngelscriptEngine`
2. 确保这个 engine 在当前测试作用域里成为活动 engine
3. 暴露出 `FAngelscriptDebugServer` 和调试端口
4. 提供轮询/等待工具，让测试能在 game thread 和 debug server tick 之间稳定推进

因此它是一个**运行时调试环境包装器**，而不是协议客户端。

## `Initialize()`：Test 侧如何安全地借用或创建 Runtime 调试环境

`FAngelscriptDebuggerTestSession::Initialize(...)` 把这条借用关系写得很清楚：

- 如果传了 `ExistingEngine`，就直接复用现有 production-like engine
- 否则创建一台 `FAngelscriptEngine::CreateTestingFullEngine(...)`
- 自动分配唯一调试端口
- 用 `FAngelscriptEngineScope` 把这台 engine 压进当前作用域
- 再从 `Engine->DebugServer` 拿到运行时调试服务

```cpp
if (Config.ExistingEngine != nullptr)
{
    Engine = Config.ExistingEngine;
}
else
{
    EngineConfig.DebugServerPort = ...MakeUniqueDebugServerPort();
    OwnedEngine = FAngelscriptEngine::CreateTestingFullEngine(EngineConfig, Dependencies);
    Engine = OwnedEngine.Get();
}

GlobalScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
DebugServer = Engine->DebugServer;
Port = Engine->GetRuntimeConfig().DebugServerPort;
```

这里最关键的不是“能创建引擎”，而是它刻意支持两种模式：

- **附着已有 runtime**：更贴近 editor automation 里的 production-like engine 场景
- **自建隔离 runtime**：适合需要完全受控调试环境的测试

这也正是 Test-Runtime 协作的典型模式：Test 不拥有调试服务本体，但可以通过一个受控 wrapper 借用或制造调试上下文。

## `PumpOneTick()` / `PumpUntil()`：把 Runtime 的异步调试状态变成测试可断言的同步步骤

Debugger 测试最难的地方，不是连上 socket，而是怎么稳定等待状态变化。`FAngelscriptDebuggerTestSession` 为此专门提供了：

- `PumpOneTick()`：处理 game thread task graph，再显式 `DebugServer->Tick()`
- `PumpUntil(Predicate, Timeout)`：反复 pump，直到某个状态谓词满足或超时

```cpp
if (IsInGameThread())
{
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
}

DebugServer->Tick();
```

这层设计的架构意义非常大：它把 Runtime 那条本来异步推进的 DebugServer 循环，压成了测试层可控的“推进一步/推进直到条件成立”。因此后面的 smoke、breakpoint、stepping 测试都不需要自己拼 sleep + poll + tick 逻辑，而是统一通过 `Session` 来驱动。

所以 `Session` 真正桥接的不是“引擎对象”，而是**测试断言世界与 Runtime 异步状态机之间的时序差异。**

## 第二层：`FAngelscriptDebuggerTestClient` 把调试协议压成可复用的测试客户端

如果 `Session` 负责拿到 Runtime 环境，那么 `FAngelscriptDebuggerTestClient` 负责的就是协议层：

- `Connect()` / `Disconnect()`
- `SendStartDebugging()` / `SendContinue()` / `SendStepIn()` / `SendRequestCallStack()` / `SendSetBreakpoint()` ...
- `ReceiveEnvelope()` / `WaitForMessageType()` / `DeserializeMessage<T>()`

它的角色不是抽象“某种 socket helper”，而是：**用测试友好的 typed API，把 Runtime debug server 的 envelope 协议封装起来。**

例如：

- `SendTypedMessage(...)` 先序列化消息体，再发 envelope
- `WaitForMessageType(...)` 带超时、错误字符串和消息类型校验
- `DeserializeMessage<T>(...)` 直接把 envelope body 反序列化成调试协议对象

因此 `Client` 把原本零散的 socket send/recv、message type 校验、body deserialize，压成了调试测试里可读、可组合的一套动作语言。这也是为什么它必须放在 `Shared/`：所有 debugger case 都会复用这套动作，而不该在每个 test 文件里重新写一次。

## 第三层：`FAngelscriptDebuggerScriptFixture` 把脚本样例固定成可复用场景夹具

前两层解决了“如何连 Runtime 调试服务”和“如何发/收协议消息”，第三层 `FAngelscriptDebuggerScriptFixture` 解决的是：**用什么脚本场景来测断点、步进、调用栈和绑定行为。**

它提供了一组很明确的工厂：

- `CreateBreakpointFixture()`
- `CreateSteppingFixture()`
- `CreateCallstackFixture()`
- `CreateBindingFixture()`

而且每个 fixture 都不是随便拼一段脚本，而是带着：

- `ModuleName`
- `Filename`
- `ScriptSource`
- `LineMarkers`
- `EvalPaths`
- `EntryFunctionDeclaration`

`AngelscriptDebuggerScriptFixture.cpp` 里最关键的设计，是会先解析脚本里的 `/*MARK:...*/` 注记，再把它们变成 `LineMarkers`。这意味着测试侧不需要硬编码“断点在第 74 行”，而是通过 marker 名稳定拿到当前源码行号。

这层设计特别像一套**可调试脚本 DSL**：

- Session 提供 runtime 调试环境
- Client 提供协议动作
- Fixture 提供可重复使用的脚本场景和定位标记

三者配在一起，才形成了当前稳定的 Debugger 测试模式。

## 一个真实 smoke test 怎么把这三层串起来

`AngelscriptDebuggerSmokeTests.cpp` 很能说明这套模式的真实使用方式。它的握手测试大致是：

1. 用 `TryGetRunningProductionDebuggerEngine()` 拿到一个可调试 production-like engine
2. 用 `FAngelscriptDebuggerTestSession` 初始化会话
3. 用 `FAngelscriptDebuggerTestClient` 连接 `127.0.0.1:Session.GetPort()`
4. `SendStartDebugging(2)`
5. 用 `Session.PumpUntil(...)` 等待 `DebugServerVersion` 消息到达
6. 反序列化并断言 `DEBUG_SERVER_VERSION`
7. 再请求 `BreakFilters`，继续用同一套 pump/wait 模式等待响应

这段 smoke test 非常能说明“协作模式”的完整闭环：

- Runtime 提供真实 debug server 和状态位 `bIsDebugging`
- Session 负责驱动 tick 和等待条件
- Client 负责协议命令和 envelope
- test case 只表达“我要握手、我要收版本、我要看过滤器”这种业务断言

因此 shared helper 的最大价值，不是封装代码行数，而是把 **调试时序复杂度** 从具体断言里抽走了。

## 为什么这些 helper 必须放在 `Shared/` 而不是 Runtime 或单个 test 文件里

这个问题其实可以从两面看：

### 不能放回 Runtime

- `FAngelscriptDebuggerTestSession` 需要测试专用超时、唯一端口、debug break 开关和 ensure 重置逻辑
- `FAngelscriptDebuggerTestClient` 是测试协议客户端，不属于生产 runtime
- `FAngelscriptDebuggerScriptFixture` 是测试脚本样例工厂，更不应下沉到 Runtime

也就是说，这些 helper 明显带着测试策略、测试超时和测试场景语义，不属于 Runtime 核心职责。

### 也不该散落到单个 `Debugger/*.cpp`

- Smoke、breakpoint、stepping 多个测试都会共享这三件套
- `TestConventions.md` 已经明确把它们列为 Debugger 场景的 shared helper
- 如果每个测试各自复制一套“建 session / 连 client / 编 fixture / pump until”，测试会迅速发散且难维护

因此把它们放在 `Shared/` 的真正原因不是“方便 include”，而是：**它们描述的是跨多个 Debugger 场景都稳定成立的 Test-Runtime 协作模式。**

## 这条协作边界应该怎么记

如果把 `Debugger Test Session` 这条链压成一句工程化判断，可以这样记：

**Runtime 持有真实的 `FAngelscriptDebugServer` 和调试状态；Test 模块通过 `Session + Client + Fixture` 三层 shared helper，把这套异步调试能力变成可建立、可驱动、可断言、可复用的自动化测试场景。**

换成更实用的阅读过滤器就是：

- 遇到真实 debug server 状态、端口、`bIsDebugging`、breakpoint count → 优先看 Runtime
- 遇到会话初始化、tick 驱动、等待条件成立 → 优先看 `FAngelscriptDebuggerTestSession`
- 遇到 typed protocol send/recv、envelope 反序列化 → 优先看 `FAngelscriptDebuggerTestClient`
- 遇到脚本场景、行标记、eval path → 优先看 `FAngelscriptDebuggerScriptFixture`

这样就不会把“运行时调试服务”和“调试测试骨架”混成一层。

## 小结

- `FAngelscriptDebuggerTestSession` 负责把 Runtime 的 `FAngelscriptDebugServer` 包装成一个测试可控的会话对象，并通过 `PumpOneTick()` / `PumpUntil()` 把异步状态机变成可断言步骤
- `FAngelscriptDebuggerTestClient` 负责把 envelope/socket 协议压成 typed 测试客户端 API
- `FAngelscriptDebuggerScriptFixture` 负责提供带 line marker 和 eval path 的稳定脚本场景
- 三者之所以放在 `Shared/`，是因为它们表达的是跨多个 Debugger 场景共同复用的 Test-Runtime 协作模式，而不是某个单一 case 的临时工具
