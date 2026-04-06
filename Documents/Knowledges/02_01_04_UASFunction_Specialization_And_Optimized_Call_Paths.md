# `UASFunction` 特化层级与优化调用路径

> **所属模块**: 脚本类生成机制 → `UASFunction` / Specialization & Optimized Call Paths
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`

这一节真正要钉死的，不是 `UASFunction` 只是“脚本函数对应的 `UFunction` 子类”，而是当前实现为什么会把它分裂成一大组特化子类，以及这些子类到底在优化什么。当前主干里，`UASFunction` 不是只有一个通用运行时调用入口；它会根据参数/返回值形状和 JIT 可用性，选择最窄的调用器类型，把 Blueprint/ProcessEvent 入口上的解析、压栈和返回值搬运尽量压平。换句话说，`UASFunction` 在当前架构里既是“脚本函数的反射壳”，也是“脚本调用路径的微型调用器工厂”。

## 先看总图：一层基类 + 两族特化子类

`ASClass.h` 里把 `UASFunction` 体系写得非常清楚：

- 一个基类 `UASFunction`
- 一组非 JIT 特化：`NoParams`、`DWordArg`、`QWordArg`、`FloatArg`、`DoubleArg`、`ByteArg`、`ReferenceArg`、`ObjectReturn`、`DWordReturn`、`FloatReturn`、`DoubleReturn`、`ByteReturn`、`NotThreadSafe`
- 一组 JIT 对应体：`UASFunction_JIT` 和上述各类型的 `_JIT` 变体

从结构上看，这不是“一个类太大所以拆文件”，而是一种明确的调用策略编码：

```text
UASFunction (通用元数据壳)
    ├─ 非 JIT 特化子类（按参数/返回值形状分流）
    └─ JIT 特化子类（同样按形状分流，但走 Raw/Parms JIT 快路）
```

因此当前 `UASFunction` 体系的第一原则就是：**函数签名形状会影响运行时调度形状。**

## 基类 `UASFunction`：既保存脚本元数据，也保存调用编排信息

`UASFunction` 本体已经比普通 `UFunction` 多挂了完整的调用元数据：

- `asIScriptFunction* ScriptFunction`
- `UFunction* ValidateFunction`
- `TArray<FArgument> Arguments`
- `TArray<FArgument> DestroyArguments`
- `FArgument ReturnArgument`
- `asJITFunction JitFunction`
- `asJITFunction_ParmsEntry JitFunction_ParmsEntry`
- `asJITFunction_Raw JitFunction_Raw`
- `bIsWorldContextGenerated` / `WorldContextOffsetInParms` / `WorldContextIndex`
- `bIsNoOp`

这组字段其实分成了三层：

### 1. 脚本函数身份层

- `ScriptFunction`
- `ValidateFunction`

决定它在脚本 VM 里真正对应哪个函数，以及 RPC 校验路径是否存在。

### 2. 参数/返回值布局层

- `Arguments`
- `DestroyArguments`
- `ReturnArgument`
- `ArgStackSize`

决定函数调用时，如何从 Blueprint VM / `Parms` 结构里取参、压到临时栈、以及哪些参数在调用后需要显式析构。

### 3. 优化调用层

- `JitFunction`
- `JitFunction_ParmsEntry`
- `JitFunction_Raw`
- 特化子类覆盖的 `RuntimeCallFunction` / `RuntimeCallEvent`

决定它最后是走 generic context 执行，还是走 raw JIT 直跳。

因此 `UASFunction` 本质上不是“挂了个脚本函数指针的 `UFunction`”，而是**运行时函数调度描述符**。

## `FinalizeArguments()`：优化路径建立之前的统一规范化步骤

在进入具体特化子类之前，所有函数都会先执行 `FinalizeArguments()`。它会为每个参数和返回值确定：

- `ValueBytes`
- `StackOffset`
- `PosInParmStruct`
- `ParmBehavior`
- `VMBehavior`

例如参数会被分类成：

- `Reference`
- `ReferencePOD`
- `Value1Byte/2Byte/4Byte/8Byte`
- `FloatExtendedToDouble`
- `ObjectPointer`
- `WorldContextObject`

而返回值会被分类成：

- `ReturnObjectPointer`
- `Reference`
- `ReferencePOD`
- `Value1Byte/2Byte/4Byte/8Byte`
- `FloatExtendedToDouble`
- `ReturnObjectValue`
- `ReturnObjectPOD`

这一步的意义特别关键，因为它把“复杂的 Unreal/AS 类型组合”压成了后面调用模板可以直接 switch 的行为枚举。也就是说，特化优化不是绕过类型系统，而是建立在 `FinalizeArguments()` 已经做完行为规范化的前提上。

## 入口统一：`UASFunctionNativeThunk` 只是极轻的跳板

当前 Blueprint / C++ 调用脚本函数的统一入口不是某个巨大的解释器，而是：

```cpp
void UASFunctionNativeThunk(UObject* Object, FFrame& Stack, RESULT_DECL)
{
    UASFunction* Function = Cast<UASFunction>(Stack.Node);
    check(Function != nullptr);
    Function->RuntimeCallFunction(Object, Stack, RESULT_PARAM);
}
```

这里的意义很清楚：

- UE 仍然把这些脚本函数当 `FUNC_Native` 来走原生快速路径
- Thunk 本身只做一件事：从 `Stack.Node` 拿到真正的 `UASFunction` 对象，再交给虚分派

因此当前设计不是修改 UE 的整个执行框架，而是借 `FUNC_Native + NativeFunc` 把脚本函数接进 UE 既有的原生函数调用入口。这也是为什么这条路径性能上很划算：入口跳板极薄，真正的分流发生在 `UASFunction` 自己的虚方法里。

## 两条主调用模板：`AngelscriptCallFromBPVM` 和 `AngelscriptCallFromParms`

当前体系里几乎所有 `RuntimeCallFunction` / `RuntimeCallEvent` 最终都汇向两条模板：

- `AngelscriptCallFromBPVM<TThreadSafe, TNonVirtual>(...)`
- `AngelscriptCallFromParms<TThreadSafe, TNonVirtual>(...)`

它们分别服务：

- **BPVM/FFrame 路径**：Blueprint 或反射栈驱动的调用
- **Parms 路径**：`ProcessEvent` / 事件参数包驱动的调用

这两条模板又都共享同样的分流逻辑：

1. 检查 hot reload 下 `ScriptFunction == nullptr` 的早退
2. 检查 thread-safety / game thread 执行要求
3. 根据 `TNonVirtual` 决定是直接用 `ScriptFunction` 还是通过 `ResolveScriptVirtual(...)` 解析虚调用
4. 决定是走 JIT 快路，还是走 context 执行慢路

因此当前 `UASFunction` 的“真正调用核心”其实是这两条模板，而各个特化子类只是把最适合的参数/返回值搬运方式塞进同一套框架里。

## JIT 与非 JIT 的分流：优化的第一道大岔路

在 `AngelscriptCallFromBPVM` 和 `AngelscriptCallFromParms` 里，最核心的第一道分流就是：

- 如果有合适的 `JitFunction` / `jitFunction_Raw` / `jitFunction_ParmsEntry`
  - 走 `MakeRawJITCall_*` 或直接 `JitFunction(...)`
- 否则
  - 建 `FAngelscriptPooledContextBase` / `FAngelscriptGameThreadContext`
  - `PrepareAngelscriptContext(...)`
  - 逐参数 `SetArg*`
  - `Execute()`

也就是说，当前“优化调用路径”的第一层不是 `NoParams` 还是 `DWordArg`，而是：**有没有 JIT 快路可走。**

这也解释了为什么头文件里几乎每个非 JIT 特化都会再有一个 `_JIT` 兄弟类：同一个参数/返回值形状，在 JIT 和非 JIT 下需要的是两条完全不同的搬运策略。

## 为什么要有这么多特化子类

`AllocateFunctionFor(...)` 的选择逻辑已经把特化存在的理由写得很直白：

- 无参无返回值 → `UASFunction_NoParams` / `_JIT`
- 单个 4 字节参数 → `UASFunction_DWordArg` / `_JIT`
- 单个 float 参数 → `UASFunction_FloatArg` / `_JIT`
- 单个引用参数 → `UASFunction_ReferenceArg` / `_JIT`
- 返回对象 / 各种标量返回值 → 各自的 `*Return` / `*Return_JIT`
- 线程不安全场景则落到 `UASFunction_NotThreadSafe` / `_JIT`

这意味着当前特化策略的目标非常明确：**把最常见、最可压平的函数形状单独拿出来，避免走通用参数循环和动态类型分派。**

例如 `UASFunction_NoParams::RuntimeCallFunction(...)`：

- 如果有 `jitFunction_Raw`，直接 `MakeRawJITCall_NoParam(...)`
- 否则只做最小上下文准备和一次 `Execute()`

而 `UASFunction_DWordArg::RuntimeCallFunction(...)` 则可以：

- 直接 `Stack.StepCompiledIn<FProperty>(&ArgumentValue)` 取一个 `asDWORD`
- JIT 路径下直接 `MakeRawJITCall_Arg<asDWORD>(...)`
- 非 JIT 路径下直接把值写到 `stackFramePointer[AS_PTR_SIZE]`

这说明特化不是为了“代码分类好看”，而是为了把高频调用路径上的：

- 参数提取
- 返回值搬运
- stack frame 布局

从通用逻辑里剪出来。

## 虚调用解析：优化路径还要处理 override 语义

当前脚本函数并不总是静态绑定的。`ResolveScriptVirtual(...)` 会：

- 看 `ScriptFunction->vfTableIdx`
- 根据对象当前 `UASClass` 的 `ScriptTypePtr` 找到真实 `virtualFunctionTable`
- 取出真正应该调用的 `asCScriptFunction`

这意味着优化路径不是“只要 direct 就不管 override”，而是：**即使在优化模板里，也先要解决虚分派问题。**

因此 `TNonVirtual` 这个模板参数很重要：

- 某些路径可以直接用缓存的 `JitFunction`
- 某些路径则必须先 resolve virtual，再决定用哪条 JIT/非 JIT 分支

这也是为什么当前体系能同时支持：

- 静态优化调用
- override/virtual 正确语义

而不是二选一。

## Thread-safe 与 world context：优化不只是快，还要保住语义

`UASFunction` 体系里另一个很重要的切分是：

- `UASFunction_NotThreadSafe`
- 普通 `UASFunction`（默认走 thread-safe 模板）

`CheckGameThreadExecution(...)` 会在非 thread-safe 路径里：

- 检查是不是 game thread
- 检查是不是构造/默认值等特殊场景

同时，调用模板里还会处理：

- 非静态函数时自动把 `Object` 作为 world context
- `bIsWorldContextGenerated` 时从额外 world context 参数取对象
- 或按 `WorldContextIndex` 从参数地址里恢复

这说明当前优化路径不仅在做性能优化，还在显式维护：

- 线程执行语义
- world context 语义
- hot reload 下 `ScriptFunction == nullptr` 的安全早退

也就是说，`UASFunction` 的特化层级本质上是在做 **“保住语义前提下的调用路径压平”**，而不是单纯追求速度。

## 返回值搬运：特化子类的另一半价值

从 `FinalizeArguments()` 和 `AngelscriptCallFromBPVM/Parms` 可以看出，优化不仅体现在参数提取，也体现在返回值搬运：

- `Value1Byte/2Byte/4Byte/8Byte`
- `Reference` / `ReferencePOD`
- `ReturnObjectPointer`
- `ReturnObjectValue` / `ReturnObjectPOD`
- `FloatExtendedToDouble`

这也是为什么除了 `*Arg` 特化，还有整套 `*Return` 特化：

- 某些返回值可以直接走寄存器或简单 memcpy
- 某些需要 `CopyValue(...)`
- 某些对象返回甚至要先析构 Blueprint VM 预先初始化的返回槽，再让 AS VM 重建

因此“特化层级”并不只是围绕参数，而是围绕整个调用 ABI 形状。

## `AllocateFunctionFor(...)`：把元信息压成具体调用器类型

这一点特别重要：当前 `UASFunction` 体系不是运行时再去“猜”走哪条优化路径，而是在函数生成阶段就由 `AllocateFunctionFor(...)` 选好最合适的子类。

例如当前实现会明确分流：

- 单参数 4 字节 → `UASFunction_DWordArg`
- 单参数 float → `UASFunction_FloatArg`
- 无参返回对象 → `UASFunction_ObjectReturn`
- 如果有 non-virtual JIT 函数 → 对应选择 `_JIT` 变体
- 其他不匹配高频模式的 → 回落 `UASFunction_NotThreadSafe(_JIT)` 等更通用路径

这说明 `UASFunction` 体系本质上是一套 **在类生成时就确定好的调用器选择器**。生成阶段决定“你是什么形状的函数”，运行阶段只负责走这条预先选好的路径。

## 这条特化与优化边界应该怎么记

如果把 `2.1.4` 压成一句工程化判断，可以这样记：

**`UASFunction` 不是单一的脚本 `UFunction` 壳，而是一套按参数/返回值 ABI 形状与 JIT 可用性分层的调用器体系：基类保存脚本函数元数据和参数布局，`AllocateFunctionFor(...)` 在生成阶段挑出最合适的特化子类，`UASFunctionNativeThunk` 只做极薄跳板，真正的运行时调用则在 `AngelscriptCallFromBPVM/Parms` 两条模板里按 JIT、虚调用、thread-safe/world context 等语义继续分流。**

换成更实用的阅读过滤器就是：

- 看字段和参数布局 → `UASFunction` 本体 + `FinalizeArguments()`
- 看为什么有这么多子类 → `AllocateFunctionFor(...)`
- 看 Blueprint/ProcessEvent 入口 → `UASFunctionNativeThunk`
- 看真正调用核心 → `AngelscriptCallFromBPVM` / `AngelscriptCallFromParms`
- 看 JIT 和非 JIT 的差别 → `MakeRawJITCall_*` 与 context 执行分支

## 小结

- `UASFunction` 体系的核心不是“脚本函数能被 UE 调用”，而是“脚本函数能按最合适的 ABI 形状与 JIT 条件被高效调用”
- 基类 `UASFunction` 挂住了脚本函数指针、验证函数、参数/返回值描述和多种 JIT 指针；`FinalizeArguments()` 负责把复杂类型语义规范化成统一的 Parm/VM 行为
- 头文件中大量 `*Arg` / `*Return` / `_JIT` 特化子类，对应的是不同调用形状与快路径，而不是单纯的代码分类
- 当前运行时入口非常克制：`UASFunctionNativeThunk` 只做 cast + 虚分派，真正的性能分流发生在 `AngelscriptCallFromBPVM/Parms` 以及各特化子类对参数/返回值搬运的优化上
