# Diagnostics、错误收集与调试输出面

> **所属模块**: Runtime 总控与生命周期 → Diagnostics / Error Output
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`

前面几节讲的是 Runtime 怎么启动、怎么编译、怎么热重载；这一节讲的是另一个同样关键、但很容易被忽略的面：当编译或运行时发现问题时，这些错误、警告和信息到底是怎么被收集、缓存、格式化，再发到日志或 debug server 的。当前实现并不是“随手 `UE_LOG` 一下”，而是把 Diagnostics 当成一块独立的状态面：既要服务本地日志，也要服务远端调试客户端，还要在多线程编译和热重载过程中避免信息乱序或重复轰炸。

## 先看 Diagnostics 在 engine 里的状态形状

`AngelscriptEngine.h` 已经把这层状态直接挂在 `FAngelscriptEngine` 里：

- `struct FDiagnostics`：按文件聚合诊断信息
- `TMap<FString, FDiagnostics> Diagnostics`：当前诊断缓存
- `TMap<FString, FDiagnostics> LastEmittedDiagnostics`：上次已发出的诊断快照
- `bool bDiagnosticsDirty`：当前诊断是否有未发送更新
- `bool bIgnoreCompileErrorDiagnostics`：热重载失败期间是否暂时抑制后续噪声
- `FormatDiagnostics()` / `ResetDiagnostics()` / `EmitDiagnostics()`：三类对外输出动作

从这里就能看出一个很重要的设计判断：**Diagnostics 不只是日志副产品，而是 engine 生命周期中的正式状态。** 它会被编译线程写入，会被调试输出面读取，也会在某些阶段被显式 reset 或 suppress。

## `FDiagnostics` 的聚合维度：按文件，而不是按事件流

`FDiagnostics` 的字段很少，但已经足够说明设计意图：

- `Filename`
- `TArray<FDiagnostic> Diagnostics`
- `bHasEmittedAny`
- `bIsCompiling`

这说明当前 Diagnostics 面不是“纯事件流”，而是**按文件维护一份可反复更新的诊断快照**。一个编译中的脚本文件，会不断往自己对应的 `FDiagnostics` 槽里追加错误、警告或信息；而输出层在发消息时，也不是简单追加日志，而是围绕“某个文件当前有哪些诊断”来组织消息。

这种按文件聚合的模型非常适合两种场景：

1. **Editor / IDE 型消费**：远端客户端更关心某个文件当前的 diagnostics 集合，而不是原始事件流；
2. **热重载重编译**：文件重新编译时，可以直接重置并覆盖该文件的诊断，而不需要从全局日志里再去反向清理旧记录。

和它配套的原子条目是 `FDiagnostic`。从当前写入路径可以看出，它至少稳定携带五个字段：

- `Message`
- `Row`
- `Column`
- `bIsError`
- `bIsInfo`

也就是说，Runtime 缓存的并不是一段纯文本，而是足够同时支撑日志显示、IDE 定位和远端调试协议的结构化诊断记录。

## 第一条入口：编译器消息回调 `LogAngelscriptError`

真正最基础的 diagnostics 入口是 `LogAngelscriptError(asSMessageInfo*, void*)`。它接住 Angelscript 编译器吐出来的 message，然后同时做三件事：

- 先根据 `Message->type` 把内容打到 `UE_LOG`
- 再按 `section` 找到对应文件的 `FDiagnostics`
- 最后把结构化诊断追加进去，并把 `bDiagnosticsDirty` 置脏

```cpp
void LogAngelscriptError(asSMessageInfo* Message, void* DataPtr)
{
    auto& Manager = FAngelscriptEngine::Get();
    if (Manager.bIgnoreCompileErrorDiagnostics)
        return;

    FScopeLock MessageLock(&Manager.CompilationLock);
    // ...格式化 section / row / col / message...
    UE_LOG(...);

    auto* FileDiagnostics = Manager.Diagnostics.Find(Section);
    if (FileDiagnostics != nullptr)
    {
        FileDiagnostics->Diagnostics.Add({ ... });
        Manager.bDiagnosticsDirty = true;
    }
}
```

这段代码有三个架构意义：

1. **编译器 message 是 diagnostics 的底层源头之一**；
2. **日志输出和 diagnostics 缓存同步发生，而不是二选一**；
3. **这里显式加了 `CompilationLock`，说明编译消息可能来自并行阶段，不能把 diagnostics 写入当成单线程行为。**

也就是说，`LogAngelscriptError` 不是单纯的打印函数，它其实是“编译器消息 -> Runtime diagnostics 状态”的转换层。

## 第二条入口：Runtime 主动注入的 `ScriptCompileError(...)`

并不是所有 diagnostics 都来自底层编译器。Runtime 自己也会主动制造结构化编译错误，例如：

- import 模块找不到
- usage restriction 校验失败
- class generator / reload requirement 触发的合成错误
- 某些基于 Unreal 语义的额外校验失败

这一类错误统一走 `ScriptCompileError(...)` 系列重载：

- `ScriptCompileError(const FString& AbsoluteFilename, const FDiagnostic& Diagnostic)`
- `ScriptCompileError(TSharedPtr<FAngelscriptModuleDesc> Module, int32 LineNumber, ...)`
- `ScriptCompileError(UClass* InsideClass, const FString& FunctionName, ...)`

其中最底层的版本会：

- 把 `bDiagnosticsDirty` 置为 true
- 根据文件名拿到对应 `FDiagnostics`
- 追加结构化 diagnostic
- 同时用 `UE_LOG(Error/Warning)` 打到日志

这说明当前 Runtime 的错误面不仅接底层编译器，也允许高层逻辑主动往 diagnostics 里写“合成错误”。因此这套系统更准确的名称不是 compile error log，而是**统一诊断汇聚面**。

## 为什么要有 `bIgnoreCompileErrorDiagnostics`

`bIgnoreCompileErrorDiagnostics` 的用途在热重载和并发编译场景里非常关键。`LogAngelscriptError()` 一上来就会检查它：

- 如果为 true，就直接忽略当前编译器 message

这和前面四阶段编译流里的逻辑是连起来的：当热重载中某些解析或结构步骤先失败后，系统会临时抑制后续一大串“依赖模块连带报错”的噪声，避免把真正有价值的第一层错误淹没。

也就是说，这个标志不是偷懒，而是一个很明确的 **diagnostic noise gate**：当 Runtime 已经知道当前轮编译会产生大量派生噪声时，它宁可先压住这些 message，等真正稳定后再发结构化结果。

## `FormatDiagnostics()`：本地字符串视图

`FormatDiagnostics()` 做的事很直白：

- 遍历 `Diagnostics`
- 跳过空列表
- 以“文件名 + 行列号 + message”的形式，把当前 diagnostics 拼成一段字符串

这层的作用不是网络协议，而是**把当前缓存的结构化 diagnostics 快照压平为本地可显示文本**。它本身不负责清空状态，也不负责发给客户端，所以更像一个“本地查看面”或调试辅助函数。

## `EmitDiagnostics()`：真正的对外输出面

`EmitDiagnostics()` 才是 Diagnostics 面真正往外发消息的出口。它有两层：

1. `EmitDiagnostics(FSocket* Client = nullptr)`：遍历所有文件 diagnostics，并决定哪些要发、哪些要删除
2. `EmitDiagnostics(FDiagnostics& Diag, FSocket* Client = nullptr)`：把单文件 diagnostics 打包成 `FAngelscriptDiagnostics` 消息，再送给 debug server

它的行为可以概括成：

- 对没有 diagnostic 条目的文件，如果之前发过或当前仍处于编译中，也会再发一次空诊断，用来告诉客户端“这个文件现在干净了”
- 如果有真正的 diagnostics，就发送结构化 message，并标记 `bHasEmittedAny = true`
- 发完一轮后，把 `bDiagnosticsDirty = false`

这说明 `EmitDiagnostics()` 不是“有错才报”，而是一个**状态同步面**：它既会发送新增错误，也会发送“错误已清空”的事实。对于 IDE / 调试器来说，这一点比单纯打印一条日志要重要得多。

## debug server 在这里的角色：传输层，而不是诊断所有者

`EmitDiagnostics(FDiagnostics&, FSocket*)` 里可以看到：

- 先把 `FDiagnostics` 转成 `FAngelscriptDiagnostics`
- 再调用 `DebugServer->SendMessageToAll(...)` 或 `SendMessageToClient(...)`

这说明 debug server 在 diagnostics 体系里的角色是**传输层**，不是诊断的拥有者。真正拥有 diagnostics 状态的是 `FAngelscriptEngine`；debug server 只负责把它序列化并发出去。

这层边界很重要，因为它让 diagnostics 体系不会被调试协议反向绑死：

- 没有 debug server 时，Runtime 仍然可以本地收集 diagnostics 并打日志；
- 有 debug server 时，只是多了一条结构化远端输出通道。

从协议名上看，这一层通常会把 engine 内部的 `FDiagnostics` / `FDiagnostic` 转成 debug server 侧的 `FAngelscriptDiagnostics` / `FAngelscriptDiagnostic`。这再次说明 debug server 不拥有诊断状态，它只负责把 Runtime 内部的结构化结果翻译成可传输消息。

## 日志面、缓存面、远端面是同时存在的

当前设计最值得注意的一点，是它没有在“打印日志”与“保留结构化 diagnostics”之间二选一，而是三层并存：

- **日志面**：`UE_LOG(Log/Warning/Error)`，服务本地控制台与开发者即时观察
- **缓存面**：`Diagnostics` / `LastEmittedDiagnostics` / `bDiagnosticsDirty`，服务状态同步与去重
- **远端面**：`EmitDiagnostics()` + debug server，服务调试客户端或 IDE 诊断显示

这种三层并存的好处是：

- 本地看日志的人不会因为远端协议存在而丢掉即时反馈；
- 远端客户端不会被迫从文本日志里反向解析结构化诊断；
- Runtime 还能用 `bHasEmittedAny` / `bIsCompiling` / dirty flag 控制增量同步，而不是每次都全量刷屏。

## Diagnostics 和编译主链路是怎么接起来的

虽然这一节不再重复编译主链路本身，但仍然需要记住两个接缝：

- `CompileModules()` 在开始新一轮编译前，会把参与编译模块对应文件的 diagnostics 重置为“正在编译”状态
- 各种编译/校验路径再通过 `LogAngelscriptError()` 或 `ScriptCompileError()` 把错误重新灌回 `Diagnostics`

因此 Diagnostics 面并不是编译完才回头总结，而是**与编译过程同步推进**的。它的生命周期更像一个 per-file 状态机：

1. 标记当前文件正在编译；
2. 编译和校验过程中不断灌入诊断；
3. 最后由 `EmitDiagnostics()` 把当前状态推给外部；
4. 干净文件在后续轮次中也能被显式“清空同步”。

## 小结

- 当前 Runtime 的 diagnostics 不是零散 `UE_LOG`，而是一套挂在 `FAngelscriptEngine` 上的正式状态面
- 诊断入口至少有两条：编译器消息回调 `LogAngelscriptError()`，以及 Runtime 主动注入的 `ScriptCompileError(...)`
- `FormatDiagnostics()` 提供本地文本视图，`EmitDiagnostics()` 提供结构化远端同步面，debug server 只是传输层
- `bDiagnosticsDirty`、`bHasEmittedAny`、`bIsCompiling` 与 `bIgnoreCompileErrorDiagnostics` 共同决定了 diagnostics 的节流、同步和去噪行为
