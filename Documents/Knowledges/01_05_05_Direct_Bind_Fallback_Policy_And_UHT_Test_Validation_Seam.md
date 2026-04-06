# Direct-Bind 回退策略与 UHT 测试/验证接缝

> **所属模块**: UHT 工具链位置与边界 → Direct-Bind Fallback / Test Validation Seam
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionSignatureBuilder.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptHeaderSignatureResolver.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`, `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptBindConfigTests.cpp`, `Documents/Plans/Plan_UhtPlugin.md`

这一节真正要钉死的，不是“有些函数会变成 `ERASE_NO_FUNCTION()`”，而是当前 UHT 工具链到底怎样决定何时恢复 direct bind、何时保守回退，以及仓库又是怎样证明这条决策链没有把 Runtime 绑定表搞坏。当前实现的核心原则非常明确：**不能稳定证明可见、可链接、可消歧，就宁可输出 stub，也不盲目生成 direct bind。** 而与这条保守策略配套的，则是一整组 `AngelscriptBindConfigTests`：它们不只测“生成器跑没跑”，而是直接检查 `ClassFuncMaps` 是否被填充、`FFuncEntry` 是否真的绑定、重复注册是不是保留首项，以及若干 fallback/override 规则是否按照预期生效。

## 先看回退决策链的总图

当前 direct-bind 决策并不是在一个点上完成，而是一条逐层收缩的链：

```text
ShouldGenerate(function)?
    -> HeaderSignatureResolver.TryBuild()
        -> public / export / overload 可判定 ?
            -> yes: 生成 direct bind 候选
            -> no: 记录 failureReason
    -> FunctionSignatureBuilder.TryBuild()
        -> 某些 failureReason 允许 fallback
        -> 某些 failureReason 直接拒绝
    -> CodeGenerator.CollectEntries()
        -> 成功: signature.BuildEraseMacro()
        -> 失败: ERASE_NO_FUNCTION()
```

也就是说，`ERASE_NO_FUNCTION()` 不是一个模糊的“失败了就这么写”，而是一个经过条件过滤、header 解析、失败原因分类之后的**保守终点**。

## 第一层：`ShouldGenerate()` 先决定某函数值不值得进入候选集

在真正谈 direct bind 之前，`AngelscriptFunctionTableCodeGenerator.ShouldGenerate(...)` 已经先做了一轮资格过滤：

- header 必须存在且属于支持范围（跳过 `/Private/`）
- 函数必须是 `BlueprintCallable` / `BlueprintPure`
- 带 `NotInAngelscript` 的直接跳过
- `BlueprintInternalUseOnly` 只有带 `UsableInAngelscript` 才放行
- `CustomThunk` 跳过
- 少量特定函数名也被直接排除

这层的意义很重要，因为它说明很多“没生成 direct bind”的原因甚至还没到签名恢复那一步，而是**压根没有进入候选集**。因此当前回退策略的第一层不是技术限制，而是脚本暴露资格控制。

## 第二层：`HeaderSignatureResolver` 把“可直接绑定”收窄成可见且可链接的声明

一旦函数进入候选集，`AngelscriptHeaderSignatureResolver.TryBuild(...)` 会先尝试最保守也最接近真实 C++ 声明的恢复路径。它会在失败时返回一组很明确的 `failureReason`：

- `header-missing`
- `class-range`
- `declaration-missing`
- `non-public`
- `unexported-symbol`
- `overloaded-unresolved`

其中最关键的两类硬拒绝是：

### `non-public`

如果候选声明全部不在 `public:` 区域，直接失败。

### `unexported-symbol`

即使声明是 public，如果：

- 函数自身没有 `*_API` 宏
- 也不是 inline / FORCEINLINE / constexpr / 内联定义
- 所在类虽有 API 宏但属于 `MinimalAPI`

那么当前实现也会拒绝 direct bind，理由不是“懒得恢复”，而是：**恢复出来的函数指针很可能在链接层根本不可安全消费。**

因此 `HeaderSignatureResolver` 的职责不只是“找签名”，更是在帮工具链守住 direct bind 的最硬边界：**只有 public 且链接可见的声明，才有资格继续往下走。**

## 第三层：`FunctionSignatureBuilder` 按失败原因决定是否允许 fallback

`AngelscriptFunctionSignatureBuilder.TryBuild(...)` 是真正把“失败原因”翻译成策略决策的地方：

```csharp
if (failureReason == "non-public" || failureReason == "unexported-symbol")
{
    return false;
}

if (failureReason == "overloaded-unresolved" && !IsWhitelistedDirectBindFallback(classObj, function))
{
    return false;
}
```

这一段已经把当前 fallback policy 说得很清楚：

- **`non-public` / `unexported-symbol`**：硬拒绝，不允许 fallback 到 UHT token 重建
- **`overloaded-unresolved`**：默认拒绝，除非命中白名单
- 其他场景：允许退回到基于 UHT 属性/返回值 token 的显式签名重建

因此 builder 的职责不是“总能想办法救回来”，而是：**把不同失败原因分类成“绝不恢复”“仅白名单恢复”“可以 token fallback”三类。**

这也是当前 direct-bind 策略最关键的一层：不是所有失败都等价，失败原因本身就参与决定回退强度。

## 白名单 fallback：只为少量已知样本开后门，而不是扩大不确定性

目前 builder 里唯一的显式白名单是：

```csharp
return classObj.SourceName == "URuntimeFloatCurveMixinLibrary" &&
    (function.SourceName == "GetNumKeys" || function.SourceName == "GetTimeRange");
```

这条白名单很有代表性，因为它说明当前策略不是“重载/inline 一旦失败就放开所有 fallback”，而是：

- 先把保守规则写死；
- 再对极少数已经验证过的样本开例外；
- 并且这些例外必须有测试兜底。

后面 `BindConfigTests` 里正好就有对应的 `InlineDefinitionCoverageTest` 和 `InlineOutRefCoverageTest`。也就是说，这里的白名单不是拍脑袋，而是**先有样本验证，再给工具链开门。**

## 第四层：生成器只认两种结果——`ERASE_*` 或 `ERASE_NO_FUNCTION()`

`AngelscriptFunctionTableCodeGenerator.CollectEntries(...)` 的最终收口很克制：

- 接口类 / NativeInterface 直接 `ERASE_NO_FUNCTION()`
- `TryBuild(...)` 成功 → `signature.BuildEraseMacro()`
- `TryBuild(...)` 失败 → `ERASE_NO_FUNCTION()`

```csharp
else if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? _))
{
    eraseMacro = signature!.BuildEraseMacro();
}
else
{
    eraseMacro = "ERASE_NO_FUNCTION()";
}
```

这一层把整条 fallback policy 压成了非常明确的产物语义：

- **恢复成功**：进入 direct bind 宏路径
- **恢复失败或不允许恢复**：写成 stub

因此当前工具链的核心策略不是“尽量消灭 `ERASE_NO_FUNCTION()`”，而是：**只有可证明安全的函数才能脱离 stub。**

## `ERASE_NO_FUNCTION()` 的真正含义：不是崩溃，也不是成功，而是显式保守

从 `Plan_UhtPlugin.md` 的 P5 章节也能看出，当前仓库对 `ERASE_NO_FUNCTION()` 的态度非常明确：

- 它是 coverage 基线的一部分
- 要按模块统计
- 要记录 fallback 原因，例如 `unexported-symbol`、`overloaded-unresolved`、`non-public`、`interface`
- 下一批 direct-bind 恢复工作就是围绕这些 stub 面展开

这说明 `ERASE_NO_FUNCTION()` 在当前架构里不是“待修 bug 的临时产物”，而是一种**被允许、被统计、被治理的保守状态**。

换句话说，工具链并不把 stub 当成错误，而是把它当成：

- 当前还没有充分证据把该函数升级为 direct bind 的明确信号。

## 验证接缝的核心：不是测试 exporter 本身，而是测试 Runtime 结果

`AngelscriptBindConfigTests.cpp` 很能说明当前验证 seam 的位置。它们不是去断言：

- exporter 有没有运行
- 生成器有没有写出某个字符串

而是直接验证 Runtime 最终结果：

### 1. 生成条目是否真的进入 `ClassFuncMaps`

`FAngelscriptGeneratedFunctionEntryPopulationTest` 会：

- 创建 testing full engine
- 直接读取 `FAngelscriptBinds::GetClassFuncMaps()`
- 检查 `AActor::K2_DestroyActor`、`UGameplayStatics::GetPlayerController`、`UASClass::IsDeveloperOnly` 是否被注册进 map
- 进一步断言 `UASClass::IsDeveloperOnly` 对应的 `FFuncEntry` 真的 `IsFunctionEntryBound(...)`

也就是说，这个测试不关心 generator 具体写了什么，而是关心：**UHT 生成条目经过编译和静态注册后，是否真的被 Runtime 消费成可绑定的函数表项。**

### 2. 去重策略是否稳定

`FAngelscriptFunctionEntryDeduplicationTest` 直接手动调用两次 `AddFunctionEntry(...)`：

- 第一次给 `AActor::K2_DestroyActor` 放一个 direct bind entry
- 第二次放一个 `ERASE_NO_FUNCTION()`

最后断言：

- `StoredEntry` 仍等于第一次注册的条目
- 第二次的 stub 没有覆盖第一次的 direct bind

这条测试非常关键，因为它说明当前验证 seam 不只是“有没有写进去”，还要验证 **UHT 生成条目和手写绑定/重复注册之间的优先级契约**。这正好对应 `Plan_UhtPlugin.md` 里“手写 Bind_*.cpp 优先，自动生成作为补充”的要求。

## fallback policy 也被专门拆成多类测试样本

`AngelscriptBindConfigTests.cpp` 里还有几类和 fallback policy 直接对齐的样本：

- `FAngelscriptBlueprintInternalUseOnlyOverrideTest`
  - 验证 `UsableInAngelscript` 能否覆盖 `BlueprintInternalUseOnly` 的默认跳过规则
- `FAngelscriptOverloadResolutionCoverageTest`
  - 验证重载函数在当前规则下是否能恢复出 direct bind，而不是留成 `ERASE_NO_FUNCTION()`
- `FAngelscriptInlineDefinitionCoverageTest`
  - 验证 inline 定义函数能否被恢复为 direct bind
- `FAngelscriptInlineOutRefCoverageTest`
  - 验证 inline + out-ref 这类更敏感样本也能否恢复成功
- `FAngelscriptCallableWithoutWorldContextMetadataTest`
  - 验证运行时 trait/隐藏 world context 的语义是否和 metadata 规则一致

这些测试的共同点是：它们不测“解析器返回了哪个字符串”，而是测**最终运行时表面和 script function trait 是否符合当前策略预期。**

因此当前的测试/验证 seam 可以概括成一句话：**围绕 fallback policy 建样本，再在 Runtime 最终状态上做断言。**

## 为什么这套测试放在 `BindConfig` 而不是 UHTTool 自己内部

这个边界也很重要。当前仓里没有把这些测试放在 UHTTool C# 侧，而是放在 `AngelscriptTest/Core/AngelscriptBindConfigTests.cpp`，原因非常合理：

- 要验证的不只是“生成器逻辑”，而是 UHT → 编译 → 静态注册 → Runtime 消费这条完整链
- 只有在 Runtime 侧拿到 `ClassFuncMaps`、`FFuncEntry`、`asCScriptFunction` 时，很多结果才真正可断言
- 因此验证 seam 必须落在**Runtime 可运行的 C++ 测试环境**里，而不是停在 UHT exporter 单元层

这再次说明当前这组测试是在验证 **接口边界与最终效果**，而不是孤立地验证某个解析器内部函数。

## 这条 fallback/验证边界应该怎么记

如果把 `1.5.5` 压成一句工程化判断，可以这样记：

**当前 UHT 工具链默认采取“安全优先”的 direct-bind 回退策略：`non-public`、`unexported-symbol` 这类失败直接拒绝恢复，`overloaded-unresolved` 只对白名单样本开例外，其余无法稳定恢复的函数统一降成 `ERASE_NO_FUNCTION()`；而这套策略是否成立，不靠检查生成字符串，而是通过 `AngelscriptBindConfigTests` 在 Runtime 最终状态上验证 `ClassFuncMaps` 填表、条目绑定、去重优先级和若干关键 metadata/fallback 样本。**

换成更实用的阅读过滤器就是：

- 看到 `failureReason` / `IsWhitelistedDirectBindFallback` → 这是 fallback policy 决策层
- 看到 `ERASE_NO_FUNCTION()` → 这是保守回退的正式产物，不是临时错误
- 看到 `GeneratedFunctionEntryPopulation` / `FunctionEntryDeduplication` / `OverloadResolutionCoverage` 测试 → 这是 UHT → Runtime 最终验证接缝

## 小结

- 当前 direct-bind 回退策略不是“失败就随便 stub”，而是按失败原因做了明确分层：硬拒绝、白名单例外和普通 UHT token fallback
- `ERASE_NO_FUNCTION()` 是工具链允许且可治理的保守结果，用来标记尚未被证明安全可恢复的函数
- `AngelscriptBindConfigTests.cpp` 不是在测 exporter 文本输出，而是在 Runtime 最终状态上验证生成条目是否真的入表、是否直连、去重是否正确，以及关键 metadata/fallback 样本是否符合预期
- 因此 `1.5.5` 的真正主题不是“某个测试文件”，而是当前 UHT 生成链如何通过一套保守回退策略与 Runtime 侧验证接缝形成闭环
