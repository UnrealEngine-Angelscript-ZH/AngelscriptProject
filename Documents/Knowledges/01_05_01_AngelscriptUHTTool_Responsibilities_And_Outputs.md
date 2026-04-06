# `AngelscriptUHTTool` 的职责与输出物

> **所属模块**: UHT 工具链位置与边界 → UHTTool / Responsibilities & Outputs
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptUHTTool.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableExporter.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptHeaderSignatureResolver.cs`, `Documents/Plans/Plan_UhtPlugin.md`

这一节真正要钉死的，不是 `Source/AngelscriptUHTTool/` 目录里有几份 C# 文件，而是这个工具在当前插件架构里到底承担什么职责、产出什么东西。当前主干里的 `AngelscriptUHTTool` 不是一个通用 UHT 替代品，也不是再造一套运行时绑定系统；它更准确地说是一条**UHT exporter 工具链**：在 UnrealHeaderTool 处理反射类型时，额外扫描 BlueprintCallable/Pure 函数，重建脚本侧可用的函数签名，并按模块生成 `AS_FunctionTable_*.cpp` 这类运行时函数表注册文件。也就是说，它的工作位置在“反射编译期”和“Runtime 绑定表装配”之间。

## 先看目录结构：这是一条小而专的工具链，不是大而全的独立子系统

当前 `Source/AngelscriptUHTTool/` 目录非常聚焦：

- `AngelscriptUHTTool.cs`
- `AngelscriptFunctionTableExporter.cs`
- `AngelscriptFunctionTableCodeGenerator.cs`
- `AngelscriptHeaderSignatureResolver.cs`
- `AngelscriptFunctionSignatureBuilder.cs`
- `AngelscriptUHTTool.ubtplugin.csproj`

这组文件已经把职责切得很清楚：

- 一个很薄的 tool/module 锚点
- 一个 UHT exporter 入口
- 一个生成器
- 一个 header 级签名恢复器
- 一个函数签名构建器

因此这个工具链的身份不是“又一个插件模块”，而是：**围绕函数表生成这件事组织起来的 UHT 阶段工具包。**

## `AngelscriptUHTTool.cs` 本身不是入口主逻辑，而是工具模块锚点

`AngelscriptUHTTool.cs` 的内容非常少：

```csharp
namespace AngelscriptUHTTool;

internal static class AngelscriptUHTToolModule
{
}
```

这反而很有说明力。它告诉我们：

- 当前工具链的真正“工作入口”并不在这个文件里；
- 它更像一个用于组织命名空间/模块边界的最小锚点；
- 真正的 UHT 集成点在 exporter 类上。

所以如果只看 `AngelscriptUHTTool.cs`，很容易误以为这个工具“还没做什么”；但目录里真正重要的并不是这个壳，而是带 `[UnrealHeaderTool]` / `[UhtExporter]` 的导出器和后续生成链。

## 真正的入口：`AngelscriptFunctionTableExporter`

当前主干里最关键的入口其实是 `AngelscriptFunctionTableExporter.cs`：

```csharp
[UnrealHeaderTool]
internal static class AngelscriptFunctionTableExporter
{
    [UhtExporter(
        Name = "AngelscriptFunctionTable",
        Description = "Exports Angelscript function table data",
        Options = UhtExporterOptions.Default | UhtExporterOptions.CompileOutput,
        CppFilters = ["AS_FunctionTable_*.cpp"],
        ModuleName = "AngelscriptRuntime")]
    private static void Export(IUhtExportFactory factory)
    {
        int generatedFileCount = AngelscriptFunctionTableCodeGenerator.Generate(factory);
        ...
    }
}
```

这段属性声明已经把职责和输出物都说得很明确：

- 它是一条 `UhtExporter`
- 名字就叫 `AngelscriptFunctionTable`
- 输出物是 `CompileOutput`，也就是生成出来的 C++ 要直接参与编译
- 过滤规则明确锁定 `AS_FunctionTable_*.cpp`
- 面向的宿主模块是 `AngelscriptRuntime`

因此这条工具链的第一职责可以压成一句话：**在 UHT 过程中，额外导出一批供 Runtime 直接编译进来的函数表注册源码。**

这也解释了为什么它不属于 Editor 或 Test：它的输出目标是 Runtime 的函数表装配，而不是编辑器工具 UI 或测试断言。

## 它真正扫描的不是所有函数，而是 BlueprintCallable/Pure 函数

`AngelscriptFunctionTableExporter` 自己还定义了 `IsBlueprintCallable(UhtFunction function)`，并在遍历 `factory.Session.Modules` 时专门统计：

- package 数量
- class 数量
- `BlueprintCallable` / `BlueprintPure` 函数数量
- reconstructed / skipped 数量

这说明工具链的第二职责不是“导出所有反射函数信息”，而是更窄的一条：

- 找出当前反射系统里对脚本侧最有意义的一批函数；
- 尝试重建它们的可擦除签名（erase macro）；
- 把成功恢复的函数转成可编译的运行时注册代码。

所以它不是泛用反射导出器，而是一条**面向 BlueprintCallable 函数表恢复**的定向工具链。

## 主要输出物：按模块分片生成的 `AS_FunctionTable_*.cpp`

真正决定输出物形态的是 `AngelscriptFunctionTableCodeGenerator.Generate(factory)`。它做了几件很关键的事：

- 根据 `AngelscriptRuntime.Build.cs` 解析“支持哪些模块、哪些是 editor-only 模块”
- 遍历 UHT session 里的模块，只处理被纳入支持集的模块
- 为每个模块收集 `AngelscriptGeneratedFunctionEntry`
- 按 `MaxEntriesPerShard = 256` 进行分片
- 用 `factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".cpp")` 生成输出路径
- 用 `factory.CommitOutput(...)` 把每个分片真正写成编译输入

这说明当前工具链最核心的输出物不是一个 JSON、不是一份报告，而是一批**会被编译进 Runtime 的 C++ 分片文件**。

输出物的命名规则也非常明确：

- `AS_FunctionTable_<Module>_<Shard>.cpp`

这意味着工具链输出物在仓库里的定位是：

- **中间生成物**，但不是只看不编译的中间报告；
- 而是被 Runtime 当成真实代码消费的生成源码。

## 输出物里实际写进去的是什么

`BuildShard(...)` 把每个分片文件怎么长出来写得很清楚：

- 先 include `CoreMinimal.h`、`AngelscriptBinds.h`、`AngelscriptEngine.h`、`FunctionCallers.h`
- 再 include 当前模块涉及到的头文件集合
- 然后生成一个 `AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionTable_<Module>_<Shard>(...)`
- 里面逐条调用：

```cpp
FAngelscriptBinds::AddFunctionEntry(Class::StaticClass(), "FunctionName", { ERASE_* ... });
```

因此这些分片文件的职责很纯：**把 UHT 阶段识别出来的 BlueprintCallable 函数，转成 Runtime 能直接装配进 `ClassFuncMaps`/函数表的注册语句。**

也就是说，工具链最终产出的并不是高层“描述信息”，而是低层“可直接执行的注册代码”。

## 第二类输出：覆盖率与诊断信息

除了 C++ 文件，当前工具链还会输出一类更轻的产物：**控制台 coverage diagnostics**。

在 exporter 层它会打印：

- visited packages / classes / BlueprintCallable functions
- reconstructed / skipped / wrote module files

在 code generator 层，它还会按模块输出：

- total entries
- direct bind entries
- stub entries
- shard count
- editor-only 模块标记

这些信息不是会被编译进 Runtime 的正式产物，但它们仍然是工具链的重要输出面。因为当前 `Plan_UhtPlugin.md` 也明确把“module-level coverage diagnostics”和后续 direct-bind 恢复工作挂钩。

所以更完整地说，这条工具链有两类输出：

1. **主输出**：`AS_FunctionTable_*.cpp` 编译产物
2. **辅助输出**：module/package/function 级覆盖率与恢复统计日志

## `AngelscriptHeaderSignatureResolver` 和 `FunctionSignatureBuilder` 是支撑，不是最终产物

这一节不深入讲签名恢复算法本身，但至少要把边界说清：

- `AngelscriptHeaderSignatureResolver` 负责从 header 文本里恢复候选声明、可见性和重载收敛
- `AngelscriptFunctionSignatureBuilder` 则把 `UhtFunction` 转成可生成 `ERASE_*` 宏的签名对象

它们是工具链的**中间加工层**，不是输出物本身。

这点很重要，因为它帮助区分：

- `1.5.1` 讲的是“工具的职责与输出物”
- 后面的 `1.5.2` 再深入“Header 签名解析与函数表导出”

所以在这一节里，这两者的正确位置是：**解释工具为什么能产出当前那些文件，但不把细节算法展开。**

## 它如何嵌回 Runtime 主链路

从 `UhtExporter(... ModuleName = "AngelscriptRuntime")` 和生成文件内容可以看出，这条工具链虽然是 C# / UHT 阶段的工具，但最终服务对象非常明确：

- 不是 Editor 菜单
- 不是 Test 辅助器
- 而是 Runtime 的函数表注册链

这也和 `Plan_UhtPlugin.md` 当前重排后的目标一致：

- 让 UHT exporter 自动生成 `AddFunctionEntry` 调用
- 为所有 BlueprintCallable 函数填充 `ClassFuncMaps`
- 让 `BindBlueprintCallable` 自动发现路径恢复工作

因此 `AngelscriptUHTTool` 在整体架构里的位置可以理解成：**UHT 阶段的代码生成前哨，为 Runtime 函数绑定表提供补充代码。**

## 这条工具链边界应该怎么记

如果把 `AngelscriptUHTTool` 的职责和输出物压成一句工程化判断，可以这样记：

**它不是另一个运行时模块，而是一条挂在 UnrealHeaderTool 上的 exporter 工具链：输入是 UHT 看见的 BlueprintCallable/Pure 反射函数，输出是按模块分片生成、会直接参与 Runtime 编译的 `AS_FunctionTable_*.cpp` 注册文件，以及辅助性的覆盖率/恢复诊断日志。**

换成更实用的阅读过滤器就是：

- 看到 `[UhtExporter]` / `CompileOutput` / `factory.CommitOutput(...)` → 这是工具链入口与生成产物层
- 看到 `AS_FunctionTable_<Module>_<Shard>.cpp` → 这是工具链主输出物
- 看到签名恢复和 header 解析 → 把它理解成支撑层，不是这一节的最终关注点

## 小结

- `AngelscriptUHTTool` 当前是一条围绕函数表生成组织起来的 UHT exporter 工具链，而不是独立的产品模块
- 真正的入口是 `AngelscriptFunctionTableExporter`，它在 UHT 阶段扫描 BlueprintCallable/Pure 函数，并面向 `AngelscriptRuntime` 生成 `CompileOutput`
- 这条工具链的主输出物是按模块分片的 `AS_FunctionTable_*.cpp`，它们会直接参与 Runtime 编译并注册函数表条目
- 辅助输出则是 package/module/function 级覆盖率与恢复统计日志，用来支撑后续 direct-bind 覆盖率治理
