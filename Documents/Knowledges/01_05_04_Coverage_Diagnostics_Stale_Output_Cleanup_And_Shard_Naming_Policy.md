# Coverage Diagnostics、过期输出清理与分片命名策略

> **所属模块**: UHT 工具链位置与边界 → Coverage / Cleanup / Sharding Policy
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableExporter.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`, `Documents/Plans/Plan_UhtPlugin.md`

前面三节已经把 UHT 工具链的职责、签名恢复和 Runtime handoff 讲清楚了，这一节要单独钉死的是它的“运维语义”：当前工具链如何报告 coverage、如何给生成文件命名与分片、以及如何清理过期输出。这些机制看起来不像“主功能”，但它们其实决定了这条链能否长期稳定工作：没有 coverage diagnostics，就无法知道 direct bind 与 stub 的恢复边界；没有固定的分片和命名策略，生成输出就难以调试和比较；没有过期输出清理，旧的 `AS_FunctionTable_*.cpp` 会变成隐蔽的脏产物，反向污染 Runtime 的编译结果。

## 先看这条“运维面”在链路里的位置

当前工具链除了生成函数表源码之外，还同时承担三类操作性职责：

```text
UHT exporter/generator
    -> 输出 coverage diagnostics
    -> 按稳定规则分片并命名 AS_FunctionTable_*.cpp
    -> 删除本轮未再生成的旧 shard 文件
```

这三类动作并不改变单个函数如何被恢复成 `ERASE_*` 宏，但它们决定了：

- 你能不能看见当前覆盖率状态；
- 你能不能稳定定位某条生成注册语句属于哪个模块/分片；
- 你能不能避免历史生成文件继续参与后续编译。

因此这一节关注的不是“绑定是否正确”，而是**生成链本身如何保持可观察、可维护、可增量安全。**

## 第一类输出：Exporter 层的总体 coverage diagnostics

`AngelscriptFunctionTableExporter.Export(...)` 在触发生成后，首先会统计一轮总览性的 coverage 信息：

```csharp
int packageCount = 0;
int classCount = 0;
int functionCount = 0;
int reconstructedCount = 0;
int skippedCount = 0;
int generatedFileCount = AngelscriptFunctionTableCodeGenerator.Generate(factory);

Console.WriteLine(
    "AngelscriptUHTTool exporter visited {0} packages, {1} classes, {2} BlueprintCallable/Pure functions, reconstructed {3}, skipped {4}, wrote {5} module files.",
    packageCount,
    classCount,
    functionCount,
    reconstructedCount,
    skippedCount,
    generatedFileCount);
```

这条日志的作用不是给开发者看个热闹，而是提供一份**全局批次级摘要**：

- 这轮 UHT 实际扫描了多少包、类和 BlueprintCallable/Pure 函数；
- 其中多少函数能恢复出可用签名；
- 有多少函数被跳过；
- 最终写出了多少个 module/shard 文件。

因此 exporter 层的 coverage diagnostics 是一条**批次级仪表盘**。它告诉你这一轮整体发生了什么，但不会细化到每个模块的 direct/stub 分布。

## 第二类输出：Generator 层的 per-module coverage diagnostics

真正更有操作价值的是 `AngelscriptFunctionTableCodeGenerator.WriteCoverageDiagnostics(...)`：

```csharp
Console.WriteLine("AngelscriptUHTTool per-module coverage diagnostics:");
foreach (AngelscriptModuleGenerationSummary summary in moduleSummaries)
{
    Console.WriteLine(
        "  - {0}{1}: total={2}, direct={3}, stubs={4}, shards={5}",
        summary.ModuleName,
        summary.EditorOnly ? " [EditorOnly]" : string.Empty,
        summary.TotalEntries,
        summary.DirectBindEntries,
        summary.StubEntries,
        summary.ShardCount);
}
```

这条输出相比 exporter 总览，多了两层非常关键的信息：

- **模块粒度**：每个模块各自有多少 total/direct/stub/shard
- **EditorOnly 标识**：某模块是否只在 editor 条件下生成

也就是说，这里输出的不是简单的“生成成功/失败”，而是一份直接服务后续治理的**模块级覆盖率剖面**。这正好对应 `Plan_UhtPlugin.md` 里对 P5 的要求：

- 按模块建立 coverage 基线
- 记录 `direct-bind` / `ERASE_NO_FUNCTION()` 计数
- 优先盯住像 `Engine`、`UMG`、`GameplayAbilities` 这样的高价值模块

因此这条 diagnostics 的真正用途不是“构建日志可读性”，而是**为后续恢复 direct bind、分析 fallback 原因和决定优化优先级提供数据地基。**

## 模块摘要对象说明这不是临时日志，而是结构化策略输出

`AngelscriptModuleGenerationSummary` 这一 record 也很说明问题：

- `ModuleName`
- `EditorOnly`
- `TotalEntries`
- `DirectBindEntries`
- `StubEntries`
- `ShardCount`

这说明 per-module diagnostics 不是随手拼字符串，而是先被建成结构化摘要，再统一排序输出。也正因此，它以后很容易被扩展成：

- 写入独立摘要文件
- 进入 CI 报表
- 或参与更细粒度的增量比较。

从策略上看，这比直接在生成过程中散落 `Console.WriteLine(...)` 更稳，因为它把“统计”本身独立成了 generator 的一项正式职责。

## 排序策略：为什么先按 stub 数量再按模块名排序

`WriteCoverageDiagnostics(...)` 里还有一个很关键但容易忽略的细节：

```csharp
moduleSummaries.Sort(static (left, right) =>
{
    int stubComparison = right.StubEntries.CompareTo(left.StubEntries);
    return stubComparison != 0
        ? stubComparison
        : StringComparer.Ordinal.Compare(left.ModuleName, right.ModuleName);
});
```

这意味着日志输出顺序不是按模块原始遍历顺序，也不是按 direct-bind 数，而是：

1. **先看哪个模块 stub 最多**；
2. stub 相同再按模块名字典序稳定排序。

这个选择非常有工程味道，因为它让日志天然把“问题最重的模块”顶到最前面。也就是说，当前 coverage diagnostics 本身就已经编码了一种**优先级导向**：

- 先暴露 fallback 最严重的模块；
- 再在同等严重度下保持稳定输出顺序，便于 diff 与回归比较。

## 分片策略：`MaxEntriesPerShard = 256` 是当前主命名与切分规则的起点

当前分片策略的核心常量非常明确：

```csharp
private const int MaxEntriesPerShard = 256;
```

而具体切分逻辑则是：

```csharp
int shardCount = (entries.Count + MaxEntriesPerShard - 1) / MaxEntriesPerShard;
for (int shardIndex = 0; shardIndex < shardCount; shardIndex++)
{
    int startIndex = shardIndex * MaxEntriesPerShard;
    int entryCount = Math.Min(MaxEntriesPerShard, entries.Count - startIndex);
    string outputPath = factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".cpp");
    ...
}
```

这说明当前工具链的分片语义不是“按 package 切”“按类切”或者“文件超长时随便分”，而是：

- **先按模块聚类**；
- 再在模块内按固定上限 256 条函数表条目切 shard。

这条策略的直接好处有三点：

- 限制单个生成 `.cpp` 的体积和编译开销；
- 保证一个模块只会被拆成有限且可预测的分片；
- 让 coverage diagnostics 里的 `ShardCount` 和输出文件数量稳定对齐。

因此当前分片策略更像是一条**构建稳定性与可维护性的操作约束**，而不是随意选择的数字。

## 命名策略：`AS_FunctionTable_<Module>_<Shard>.cpp` 不是样式问题，而是定位契约

输出文件名是通过：

```csharp
factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".cpp")
```

生成的，因此命名规则很清楚：

- 固定前缀 `AS_FunctionTable_`
- 中间是 `module.ShortName`
- 结尾是三位补零的 shard 编号，例如 `000`、`001`

这条命名策略的作用，不只是“文件看起来整齐”，而是形成了一份稳定的**定位契约**：

- 一眼能看出某个生成文件属于哪个模块；
- shard 序号稳定且可排序；
- `DeleteStaleOutputs(...)` 可以用统一 glob `AS_FunctionTable_*.cpp` 处理；
- 外部构建系统和开发者都能快速从文件名反推“它为什么存在”。

因此命名策略和分片策略其实是同一套操作规则的两面：前者解决“怎么认”，后者解决“怎么切”。

## entry 顺序策略：先按类，再按函数，保证分片稳定

分片之前，generator 还会先对 `entries` 做排序：

```csharp
entries.Sort(static (left, right) =>
{
    int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
    return classComparison != 0
        ? classComparison
        : StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
});
```

这说明当前 shard 不是基于“UHT 遍历顺序”随机落盘，而是基于：

- 先 `ClassName`
- 再 `FunctionName`

做稳定排序之后再切片。

这条规则的价值很大，因为它意味着：

- 在无功能变化时，同一组条目会稳定落在同一批 shard 里；
- 小范围函数增减不会让整个输出大面积抖动；
- `git diff`、增量构建和 stale cleanup 的行为都会更可预测。

因此“分片命名策略”并不仅仅是文件名格式，还包括**切片前的稳定排序策略**。两者一起，才构成当前输出物的可维护性。

## EditorOnly 模块如何在输出层面被编码

generator 对 EditorOnly 模块还会额外挂一层边界：

- `LoadSupportedModules(...)` 解析 `AngelscriptRuntime.Build.cs`，识别 `Target.bBuildEditor` 块里的模块
- `BuildShard(...)` 对 `editorOnly = true` 的模块自动包 `#if WITH_EDITOR`
- diagnostics 输出也会给这类模块打 ` [EditorOnly]` 标记

这说明“分片策略”和“coverage diagnostics”并不是脱离模块语义独立存在的，它们都会把 editor-only 边界编码进去。也就是说，当前输出物本身已经带着：

- 这是哪个模块的 shard
- 它是不是 editor-only 模块
- 它有多少 direct bind 和 stub

所以这套操作策略并不只是为了构建，还在承担**边界可视化**的责任。

## 过期输出清理：为什么 `DeleteStaleOutputs(...)` 是必须的，而不是锦上添花

`DeleteStaleOutputs(...)` 的实现非常直接：

```csharp
string outputDirectory = Path.GetDirectoryName(factory.MakePath("AS_FunctionTable_Stale", ".cpp"))!;
...
foreach (string existingFile in Directory.EnumerateFiles(outputDirectory, "AS_FunctionTable_*.cpp"))
{
    if (!generatedPaths.Contains(existingFile))
    {
        File.Delete(existingFile);
    }
}
```

这段逻辑的真正意义是：**本轮没再生成出来的旧 shard，必须从输出目录里移走。**

为什么这是必须的？因为当前 UHT 工具链的输出是 `CompileOutput`，也就是说这些文件会参与后续编译。如果不删旧文件，会出现几种隐蔽问题：

- 模块条目减少后，旧 shard 仍残留，导致 Runtime 继续编进过期函数表；
- 模块重命名或切分策略变化后，旧文件和新文件同时存在，覆盖率和绑定行为都可能失真；
- stale 文件继续贡献 `AddFunctionEntry(...)`，使问题看起来像 Runtime 条件分支或 UHT 重建错误，实际上只是旧产物未清理。

因此 `DeleteStaleOutputs(...)` 不是构建美化，而是**生成链增量安全**的关键一环。

## 为什么清理逻辑依赖命名策略和 generatedPaths 集合

这一点很值得记。`DeleteStaleOutputs(...)` 能成立，依赖两件事：

1. 当前所有有效输出都会被加入 `generatedPaths`
2. 所有待清理文件都遵守统一命名前缀 `AS_FunctionTable_*.cpp`

这再次说明“命名策略”和“清理策略”不是分开的：

- 如果文件名不稳定，stale cleanup 就难以可靠匹配
- 如果 shard 切分顺序不稳定，generatedPaths 就会频繁大面积变化，增量构建也会更脆弱

也就是说，**稳定命名 + 稳定排序 + stale cleanup** 其实是一套联动的运维策略。

## `Plan_UhtPlugin.md` 为什么把 diagnostics 和 cleanup 提到后续治理核心

`Plan_UhtPlugin.md` 在 P5 和风险章节里，反复把这几件事拉出来：

- 模块级 coverage diagnostics 是后续 direct-bind 恢复批次的基线
- 需要按模块统计 `direct-bind` / `ERASE_NO_FUNCTION()`
- 生成文件过大要靠分片策略控制
- 增量安全要求“源文件未变时不触发重新生成和重新编译”

这说明当前仓库已经不把这些逻辑看成“附属细节”，而是明确把它们纳入工具链治理目标：

- diagnostics 用来指导恢复优先级
- shard policy 用来控制构建成本
- stale cleanup 用来守住增量构建正确性

因此把这一节单独写出来，是合理且必要的，因为它补的正是“工具链如何长期可操作”这一层。

## 这条操作策略边界应该怎么记

如果把 `1.5.4` 压成一句工程化判断，可以这样记：

**当前 UHT 工具链不只生成函数表源码，它还同时维护一套操作性策略：用 exporter/generator 两层 coverage diagnostics 报告恢复状态，用稳定的 `AS_FunctionTable_<Module>_<Shard>.cpp` 命名和类/函数排序保证输出可比较、可增量、可定位，再用 `DeleteStaleOutputs(...)` 删除本轮未再生成的旧 shard，防止过期条目继续污染 Runtime 编译。**

换成更实用的阅读过滤器就是：

- 看到 exporter 计数日志 → 这是批次级 coverage 总览
- 看到 per-module diagnostics → 这是模块级恢复/回退剖面
- 看到 `MaxEntriesPerShard`、排序、三位编号 → 这是分片与命名策略
- 看到 `DeleteStaleOutputs(...)` → 这是增量安全边界，不是附属清理脚本

## 小结

- 当前 UHT 工具链的辅助输出不是噪声日志，而是两层 coverage diagnostics：exporter 负责批次总览，generator 负责模块级 direct/stub/shard 剖面
- 分片策略由 `MaxEntriesPerShard = 256`、按 `ClassName/FunctionName` 排序和 `AS_FunctionTable_<Module>_<Shard>.cpp` 命名共同构成，目的是保持输出稳定、可 diff、可定位
- `DeleteStaleOutputs(...)` 负责删除本轮未再生成的旧函数表 shard，是 CompileOutput 链路里保证增量安全的关键环节
- diagnostics、分片和清理并不是生成链的附属细节，而是让 UHT 工具链长期可治理、可维护、可增量运行的三根支柱
