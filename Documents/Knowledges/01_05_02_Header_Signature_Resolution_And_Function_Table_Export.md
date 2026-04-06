# Header 签名解析与函数表导出

> **所属模块**: UHT 工具链位置与边界 → Header Signature / Function Table Export
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptHeaderSignatureResolver.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionSignatureBuilder.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableExporter.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`

上一节讲的是 `AngelscriptUHTTool` 这条工具链“做什么、产出什么”；这一节要把其中最关键的一段真正拆开：它是怎么把 UHT 反射信息和真实 header 文本拼起来，最后再导出成运行时可消费的函数表条目的。当前实现并不是简单地“看到一个 BlueprintCallable 就直接拼 `ERASE_AUTO_*` 宏”，而是先尽量从 header 里恢复可链接、可见、可消歧的真实声明，再根据结果决定生成显式签名、自动签名，还是保守回退成 `ERASE_NO_FUNCTION()`。

## 先看整条导出链

当前这条链的顺序很清楚：

```text
UHT 反射函数
    -> AngelscriptFunctionSignatureBuilder.TryBuild()
        -> 先走 AngelscriptHeaderSignatureResolver.TryBuild()
        -> 失败时按规则回退到 UHT token 重建
    -> AngelscriptFunctionSignature.BuildEraseMacro()
    -> AngelscriptFunctionTableExporter 统计与导出
    -> AngelscriptFunctionTableCodeGenerator 写出 AS_FunctionTable_*.cpp
```

也就是说，当前“签名解析”与“函数表导出”不是两套分开的系统，而是一条连续的加工链：

- 上游负责把函数变成可擦除签名；
- 下游负责把这些签名变成真正的 C++ 注册代码。

因此理解这一节时，最重要的不是盯某一个 helper，而是看清四个层次如何首尾相接。

## 第一层：`AngelscriptFunctionSignatureBuilder` 是总调度器，不是唯一算法实现者

`AngelscriptFunctionSignatureBuilder.TryBuild(...)` 是这条链的总入口：

```csharp
public static bool TryBuild(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? failureReason)
{
    if (AngelscriptHeaderSignatureResolver.TryBuild(classObj, function, out signature, out failureReason))
    {
        return true;
    }

    if (failureReason == "non-public" || failureReason == "unexported-symbol")
    {
        return false;
    }

    if (failureReason == "overloaded-unresolved" && !IsWhitelistedDirectBindFallback(classObj, function))
    {
        return false;
    }

    ... fallback to UHT-based reconstruction ...
}
```

这段逻辑把整个策略说得很清楚：

1. **优先走 header 解析**；
2. 如果失败原因属于“非 public”或“不可链接导出符号”，直接终止；
3. 如果是“重载无法稳定消歧”，只有命中白名单才允许继续尝试 fallback；
4. 其他场景再退回到基于 UHT 类型信息的显式签名重建。

因此 `FunctionSignatureBuilder` 的真正职责不是“自己解析一切”，而是作为一个**签名恢复策略调度器**：先尝试最接近真实 C++ 声明的路径，再按失败原因决定是否允许退化方案介入。

## 第二层：`HeaderSignatureResolver` 先做 header 文本净化，再找类体和候选声明

`AngelscriptHeaderSignatureResolver.TryBuild(...)` 的第一步不是解析参数，而是先确认能不能在真实 header 里定位到目标类和目标声明：

- header 文件必须存在，否则 `failureReason = "header-missing"`
- 先通过 `GetSanitizedHeader(...)` 去掉行注释、块注释和噪声宏调用的影响
- 再用 `TryFindClassBody(...)` 找到 `UCLASS(` / `UINTERFACE(` 对应的类体范围
- 然后用 `FindCandidates(...)` 在类体内查找目标函数名对应的所有候选声明

```csharp
string header = GetSanitizedHeader(classObj.HeaderFile.FilePath);
if (!TryFindClassBody(header, classObj.SourceName, out int classBodyStart, out int classBodyEnd, out string classDeclaration))
{
    failureReason = "class-range";
    return false;
}

List<CandidateDeclaration> candidates = FindCandidates(header, classBodyStart, classBodyEnd, function.SourceName);
```

这说明当前实现不是把 UHT 信息当成“最终真相”，而是把它当作定位索引，再回到原始 header 文本里找最接近真实 ABI/声明面的线索。也正因此，这一层特别适合解决：

- API 宏可见性
- public/protected/private
- inline/constexpr/FORCEINLINE
- 同名重载的真实声明差异

这些东西靠纯 UHT token 并不总能稳定还原。

## `FindCandidates()` 的意义：先找全量候选，再做访问级和签名级筛选

`FindCandidates(...)` 的工作方式非常典型：

- 在类体里搜索 `functionName + "("`
- 回溯找到声明起点，前进找到声明终点
- 用 `FindAccessSpecifier(...)` 判断该候选当前落在 `public` / `protected` / `private` 哪个区域
- 用 `HashSet<string>` 去重，得到唯一候选列表

这意味着解析器一开始并不会假设“只有一个对的声明”，而是先承认：

- 可能有多个同名重载；
- 可能有非 public 候选；
- 可能有声明文本重复或噪声。

然后再逐步筛掉：

- 非 public → `non-public`
- 不可链接 → `unexported-symbol`
- 多个 public 但无法唯一匹配 → `overloaded-unresolved`

也就是说，`HeaderSignatureResolver` 本质上是在做一套**保守的候选收敛**，而不是激进地猜一个“看起来最像”的声明。

## `IsLinkVisible()`：为什么有些 public 函数仍然不能 direct bind

`IsLinkVisible(...)` 是当前解析器里最容易被忽略、但对架构最关键的一层。它看的是：

- 函数声明前缀是否带 `*_API`
- 类声明是否带 `*_API`
- 类是否 `MinimalAPI`
- 函数是否是 `inline` / `FORCEINLINE` / `constexpr` / 内联定义

```csharp
bool functionHasApiMacro = ApiMacroPattern.IsMatch(declarationPrefix);
bool classHasApiMacro = ApiMacroPattern.IsMatch(classDeclaration);
bool classIsMinimalApi = classDeclaration.Contains("MinimalAPI", StringComparison.Ordinal);
bool isInlineDefinition = ...;

if (functionHasApiMacro || isInlineDefinition)
{
    return true;
}

return classHasApiMacro && !classIsMinimalApi;
```

这条规则非常重要，因为它解释了当前工具链为什么会故意跳过一部分函数：**不是能解析出声明就一定能安全 direct bind，还必须确认这个符号在链接层面真的可见。**

也就是说，签名解析这条链的目标从来不是“尽量多生成”，而是“只为那些既能解析又能安全链接的函数生成 direct bind 条目”。

## 单候选 vs 多候选：两条不同的恢复路径

`TryBuild(...)` 里对候选数有一个很关键的分流：

### 单候选且 public 唯一

如果只有一个候选而且也是唯一的 public 候选：

- 只要 `IsLinkVisible(...)` 通过
- 就直接返回一个 `UseExplicitSignature = false` 的 `AngelscriptFunctionSignature`

这意味着后面会优先走：

- `ERASE_AUTO_METHOD_PTR(...)`
- 或 `ERASE_AUTO_FUNCTION_PTR(...)`

### 多候选或需要消歧

如果候选不止一个：

- 先根据 UHT 反射信息构造 `expectedParameterTypes` 和 `expectedReturnType`
- 再对每个 public 候选执行 `TryParseDeclaration(...)`
- 用 `AreTypesEquivalent(...)` / `NormalizeTypeText(...)` 做参数和返回值匹配
- 只有恰好得到一个 `exactMatch` 才算成功

这意味着当前策略不是“多候选就放弃”，而是**只在真的能靠 UHT 参数/返回类型把某一个候选唯一钉住时，才恢复成显式签名。**

## `TryParseDeclaration()`：把声明文本降成 `AngelscriptFunctionSignature`

一旦进入解析候选声明这一步，`TryParseDeclaration(...)` 的目标就很明确：

- 找到函数名和参数括号范围
- 解析返回类型（优先用 UHT 的 `ReturnProperty` token，否则清洗声明前缀）
- 用 `ParseParameterTypes(...)` 把参数列表拆成类型集合
- 再把这些信息装配成 `AngelscriptFunctionSignature`

这一层还有几个很值得注意的清洗动作：

- `CleanReturnType(...)` 会去掉 `virtual`、`static`、`inline`、`FORCEINLINE`、API 宏等噪声 token
- `ParseParameterTypes(...)` 会 strip 默认值、`UPARAM` 包装和尾部参数名
- `NormalizeTypeText(...)` 会在比较时消掉 `const`、`&` 和多余空白

这说明这条解析链并不是做语法树级别的完整 C++ 解析，而是做一套**足够保守、足够稳定的签名降维**：

- 保住 direct bind 真正关心的类型信息；
- 清掉不影响函数擦除签名的声明噪声。

## `BuildEraseMacro()`：签名对象如何决定最终导出宏

一旦拿到 `AngelscriptFunctionSignature`，后面的导出语义就很清楚了。`BuildEraseMacro()` 只有两类路径：

- `UseExplicitSignature = true` → 生成 `ERASE_METHOD_PTR` / `ERASE_FUNCTION_PTR`
- `UseExplicitSignature = false` → 生成 `ERASE_AUTO_METHOD_PTR` / `ERASE_AUTO_FUNCTION_PTR`

```csharp
return IsStatic
    ? $"ERASE_FUNCTION_PTR({OwningType}::{FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))"
    : $"ERASE_METHOD_PTR({OwningType}, {FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))";
```

这说明前面整条“header 解析 + UHT fallback”链的最终目的，就是决定：

- 能不能安全走 auto 形式；
- 还是必须降成显式签名形式；
- 还是应该直接放弃 direct bind。

也就是说，签名解析的终点不是“得出一个漂亮的字符串”，而是**为运行时函数表生成准确选择 `ERASE_*` 宏。**

## 导出层：`FunctionTableExporter` 负责遍历与统计，不直接决定细节签名

`AngelscriptFunctionTableExporter` 的角色比较克制：

- 用 `IsBlueprintCallable(...)` 过滤出目标函数
- 调 `AngelscriptFunctionTableCodeGenerator.Generate(factory)` 触发真正的生成
- 再统计访问了多少 package/class/function，多少 reconstructed / skipped / wrote files

因此 exporter 更像**UHT 阶段的协调器和统计出口**，而不是签名算法本体。它的职责是把：

- `TryBuild(...)` 的成败
- 生成器写出的模块文件数

变成 UHT 阶段可观察的结果。

## 生成层：`CollectEntries()` 把签名恢复结果真正落成函数表条目

真正把签名恢复结果接到导出文件上的，是 `AngelscriptFunctionTableCodeGenerator.CollectEntries(...)`。它的关键逻辑是：

- 只处理 `ShouldGenerate(...)` 认可的 class/function
- 给每个 class header 添加 external dependency 和 include
- 接口类 / NativeInterface 直接使用 `ERASE_NO_FUNCTION()`
- 普通类则调用 `AngelscriptFunctionSignatureBuilder.TryBuild(...)`
- 成功就 `signature.BuildEraseMacro()`；失败就 `ERASE_NO_FUNCTION()`

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

这段代码很好地体现了当前导出策略的保守性：**宁可输出 stub，也不盲目生成一个可能链接失败或签名错误的 direct bind。**

因此这条链的最终导出语义可以概括成：

- 可安全恢复 → 导出 `ERASE_*`
- 不可安全恢复 → 导出 `ERASE_NO_FUNCTION()`

这也正是 `Plan_UhtPlugin.md` 里反复强调“覆盖率提升要建立在 diagnostics + 回归测试之上”的原因。

## `ShouldGenerate()`：导出前还有一层功能语义过滤

这条链在真正生成前还会先过一层功能语义过滤：

- 只处理支持的 header（跳过 `/Private/`）
- 只处理 `BlueprintCallable` / `BlueprintPure`
- 跳过 `NotInAngelscript`
- `BlueprintInternalUseOnly` 只有带 `UsableInAngelscript` 才放行
- 跳过 `CustomThunk`
- 对特定白名单函数做额外例外处理

这说明函数表导出也不是“任何反射函数都进表”，而是先经过一层 **脚本暴露资格过滤**，再进入签名恢复与宏生成链。

因此这一节的导出流程应理解成：

1. 先决定某函数是否值得进入表；
2. 再决定它能否恢复出安全签名；
3. 最后决定生成 direct bind 还是 stub。

## 这条解析/导出边界应该怎么记

如果把 `1.5.2` 的整条链压成一句工程化判断，可以这样记：

**UHT 反射信息负责给出“有哪些 BlueprintCallable 函数值得进表”，header 解析负责确认“这些函数的真实 C++ 声明能否安全恢复与链接”，签名对象负责选择 `ERASE_AUTO_*` 还是显式 `ERASE_*`，生成器最终再把结果写成 `AS_FunctionTable_*.cpp` 或保守回退到 `ERASE_NO_FUNCTION()`。**

换成更实用的阅读过滤器就是：

- 看到 `TryBuild(...)` / `failureReason` → 这是签名恢复策略层
- 看到 `GetSanitizedHeader` / `FindCandidates` / `IsLinkVisible` → 这是 header 解析与可链接性判断层
- 看到 `BuildEraseMacro()` → 这是导出宏选择层
- 看到 `CollectEntries(...)` / `ShouldGenerate(...)` → 这是函数表装配层

## 小结

- 当前函数表导出链不是单靠 UHT token 拼接，而是“UHT 反射过滤 + header 解析恢复 + 签名对象宏选择 + 生成器装配”四层联动
- `HeaderSignatureResolver` 负责在真实 header 文本里恢复候选声明、访问级和链接可见性，并对重载做保守收敛
- `FunctionSignatureBuilder` 负责按失败原因决定是采用 header 结果、白名单回退，还是直接放弃生成 direct bind
- `FunctionTableCodeGenerator` 最终把成功恢复的签名落成 `ERASE_*` 宏，把失败路径统一降成 `ERASE_NO_FUNCTION()`，因此导出链的核心目标不是“尽量多生成”，而是“只为可安全恢复的函数生成可链接条目”
