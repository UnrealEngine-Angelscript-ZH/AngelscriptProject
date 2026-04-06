# 与 Runtime / Editor 生成链路的接口边界

> **所属模块**: UHT 工具链位置与边界 → UHT / Runtime / Editor Interface Boundary
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableExporter.cs`, `Plugins/Angelscript/Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintCallable.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Documents/Plans/Plan_UhtPlugin.md`

前两节已经把 UHT 工具链“做什么”和“怎么解析/导出”讲清楚了，这一节真正要钉死的是：这条工具链和 Runtime、Editor 之间的接口边界到底划在哪里。当前主干里最关键的事实是，`AngelscriptUHTTool` 并不会直接修改 Runtime 状态，也不会通过 Editor 菜单把条目即时写进引擎内存；它的职责止步于**生成可编译的函数表源码**。真正把这些条目收进运行时绑定表的是 Runtime；而 Editor 侧虽然还保留了一套旧的手工代码生成工具，但它已经退居为遗留/辅助路径，不再是当前主导的生成链路。

## 先看 handoff 的最小闭环

当前这条 handoff 链可以压成很清楚的四段：

```text
UHT exporter
    -> 生成 AS_FunctionTable_*.cpp
        -> 编译进 AngelscriptRuntime
            -> 静态注册 AddFunctionEntry(Class, Name, FFuncEntry)
                -> Bind_BlueprintCallable 在运行时按 ClassFuncMaps 消费
```

这条图最关键的不是“生成再编译”这件事，而是边界：

- **UHTTool** 只负责把条目写成源码；
- **Runtime** 才真正拥有 `ClassFuncMaps` 和绑定消费逻辑；
- **Editor** 不在这条主 handoff 链上，只保留旧的辅助生成路径。

因此理解 `1.5.3` 时，最重要的是把“产物生成”和“运行时接收”分成两层，而不是把它们想成一段连着跑的脚本。

## UHT 侧的边界：止步于 `CompileOutput`

`AngelscriptFunctionTableExporter` 的 `[UhtExporter]` 属性已经把 UHT 边界写死了：

- `Options = ... CompileOutput`
- `CppFilters = ["AS_FunctionTable_*.cpp"]`
- `ModuleName = "AngelscriptRuntime"`

这意味着 UHT 工具链的 handoff 点不是某个运行时 API 调用，而是**生成一批属于 Runtime 模块的 C++ 编译输入**。

从 `AngelscriptFunctionTableCodeGenerator.Generate(...)` 也能看出来，工具链的终点是：

- `factory.MakePath(...)`
- `factory.CommitOutput(...)`
- `DeleteStaleOutputs(...)`

也就是说，UHT 侧的责任在 `CommitOutput` 这里就结束了。它不负责：

- 把条目写进 `ClassFuncMaps`
- 主动触发 `Bind_BlueprintCallable`
- 动态更新正在运行的引擎状态

它只保证一件事：**把符合规则的函数恢复结果稳定地落成 Runtime 模块下一轮编译要消费的源码。**

## Runtime 侧的入口：生成文件通过 `FAngelscriptBinds::FBind` 静态注册接入

一旦 `AS_FunctionTable_*.cpp` 被编进 `AngelscriptRuntime`，真正的 handoff 就发生在 Runtime 里。前面 `1.5.1` 已经看到每个 shard 都会生成这样的结构：

- `AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionTable_<...>(...)`
- 里面逐条调用 `FAngelscriptBinds::AddFunctionEntry(...)`

而 `AngelscriptBinds.h` 里这层运行时接口非常明确：

```cpp
static void AddFunctionEntry(UClass* Class, FString Name, FFuncEntry Entry)
{
    auto& ClassFuncMaps = GetClassFuncMaps();
    ...
    ClassFuncMaps.Add(Class, TMap<FString, FFuncEntry>()).Add(Name, Entry);
}
```

这段代码清楚地说明：**Runtime 才是函数表状态的真正拥有者。**

换句话说，UHT 工具链和 Runtime 的接口边界可以压成一句话：

- UHT 产出的是“调用 `AddFunctionEntry` 的源码”；
- Runtime 拥有的是“`AddFunctionEntry` 写入的 `ClassFuncMaps` 状态”。

因此 UHT 不直接拥有函数表，只拥有函数表的“生成权”；真正的“保存权”和“消费权”都在 Runtime。

## Runtime 侧的消费点：`Bind_BlueprintCallable` 只认 `ClassFuncMaps`

这条边界在 `Bind_BlueprintCallable.cpp` 里又被重复钉死了一次：

```cpp
auto* map = FAngelscriptBinds::GetClassFuncMaps().Find(OwningClass);
if (map)
    Entry = map->Find(Name);

if (Entry == nullptr)
    return;
```

这里最关键的地方在于：Runtime 的绑定消费端根本不关心条目是手写绑定来的，还是 UHT 生成来的。它只看 `ClassFuncMaps` 里有没有 `FFuncEntry`。

这说明当前 handoff 的接口做得非常干净：

- 上游可以是手写 `Bind_*.cpp`
- 也可以是 UHT 生成的 `AS_FunctionTable_*.cpp`
- 只要最后都落到 `AddFunctionEntry(...)` → `ClassFuncMaps`
- `Bind_BlueprintCallable` 就按统一路径消费

也就是说，Runtime 消费端看到的是**统一函数表接口**，而不是“这是 UHT 条目 / 那是手写条目”的分裂世界。

## `ShouldSkipBlueprintCallableFunction` 说明过滤边界也留在 Runtime

`Bind_BlueprintCallable.cpp` 里另一个很重要的点是：

```cpp
if (FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(Function))
    return;
```

这说明就算 UHT 工具链已经生成了某些条目，Runtime 侧仍然保留自己的最终过滤语义，例如：

- `NotInAngelscript`
- `BlueprintInternalUseOnly` 且没有 `UsableInAngelscript`

这条边界特别重要，因为它说明当前链路不是“UHT 说能导就一定进脚本”，而是：

- UHT 负责尽量生成可用条目；
- Runtime 仍然保留最后的绑定语义裁决权。

因此 handoff 关系不是“单向绝对控制”，而是**UHT 负责生成候选，Runtime 负责最终接纳与消费。**

## Runtime 还保留了旧的 bind module 持久化接口，但不等于主链路仍在 Editor

`AngelscriptBinds.h` 里还有一组容易被误读的接口：

- `GetBindModuleNames()`
- `SaveBindModules(...)`
- `LoadBindModules(...)`

这说明 Runtime 侧曾经支持或仍支持一套 bind module 名称持久化/读取机制。但关键是：这条接口和 UHT 生成链并不是同一个 handoff 面。

当前 UHT 主链路更直接：

- 不是先生成模块名清单，再让 Runtime 二次解释；
- 而是直接生成 `AddFunctionEntry(...)` 代码并编译进 Runtime。

因此这些 `BindModuleNames` 接口更像历史兼容面或辅助面，而不是当前 `1.5` 这条 UHT 主生成链的中心接口。

## Editor 侧的角色：保留旧的手工代码生成器，但已经退居边缘

`AngelscriptEditorModule.cpp` 里确实还保留着一套旧的生成工具：

- `GenerateSourceFiles(...)`
- `GenerateFunctionEntries(...)`
- 里面会遍历 `GetRuntimeClassDB()` / `GetEditorClassDB()`
- 通过 `FSourceCodeNavigation::FindClassHeaderPath(...)`、`FindClassModuleName(...)` 去找头文件和模块名
- 再拼接 `FAngelscriptBinds::RegisterBinds(...)` 这类注册代码

这说明 Editor 并不是完全与“代码生成”无关。但这里要特别注意边界：

- 这条路径是 **editor-side 手工/辅助生成工具链**；
- 它依赖 `FSourceCodeNavigation`、ClassDB、头文件路径等 editor 信息；
- 而 `Plan_UhtPlugin.md` 已经把它明确标成需要评估/清理的旧工具：
  - “这些工具函数是当初手动维护 FunctionCallers 的辅助工具，现已被 UHT 插件取代”

因此当前架构判断应该是：**Editor 仍保留旧生成器遗留，但它不再是主 handoff 链的正式一环。**

## `Plan_UhtPlugin.md` 给出的边界结论非常直接

计划文档里有两段和这一节高度相关：

### 范围内

- “UHT Exporter 插件：遍历所有 UCLASS/UFUNCTION，生成 `AddFunctionEntry` 调用的 C++ 文件”
- “生成代码编译集成：使用 `CompileOutput` 让生成的 C++ 参与编译”
- “与手写绑定的兼容：手写 Bind_*.cpp 优先，自动生成作为补充”

### 清理项

- `P4.2` 明确点名 `AngelscriptEditorModule.cpp` 中仍有旧的代码生成工具
- 并且直接写明：这些工具函数“现已被 UHT 插件取代”

这两段组合起来，几乎已经把接口边界写成了结论：

- **主链路**：UHT → Runtime
- **遗留辅助链**：Editor 旧生成工具
- **消费/裁决权**：仍在 Runtime

因此本章里把 Editor 只写成“遗留/辅助生成者，而不是当前主链路主体”，并不是推断，而是和计划文档当前状态一致。

## Hand-off 的最核心契约：`FFuncEntry`

把 UHT 和 Runtime 之间真正连接起来的，不是某个模块名，而是 `FFuncEntry` 这一层运行时契约。无论条目是从：

- 手写 `Bind_*.cpp`
- 还是 UHT 生成的 `AS_FunctionTable_*.cpp`

最终都要变成：

- `FGenericFuncPtr`
- `ASAutoCaller::FunctionCaller`

组成的 `FFuncEntry`，再放进 `ClassFuncMaps`。

因此 `1.5.3` 这一节真正的接口边界不该只描述“谁生成文件”，还应该记住：**跨过 UHT / Runtime 边界之后，统一语言就是 `AddFunctionEntry(Class, Name, FFuncEntry)`。**

## 这条接口边界应该怎么记

如果把 `1.5.3` 的 handoff 压成一句工程化判断，可以这样记：

**UHT 工具链的职责止步于生成会调用 `FAngelscriptBinds::AddFunctionEntry(...)` 的编译产物；Runtime 负责真正拥有 `ClassFuncMaps`、执行最后过滤并消费这些条目；Editor 侧旧生成工具仍存在，但在当前主干中已经退居遗留/辅助位置，而不是这条主链路的权威入口。**

换成更实用的阅读过滤器就是：

- 看到 `factory.CommitOutput(...)` / `AS_FunctionTable_*.cpp` → 这是 UHT handoff 终点
- 看到 `AddFunctionEntry(...)` / `GetClassFuncMaps()` / `Bind_BlueprintCallable` → 这是 Runtime handoff 起点与消费面
- 看到 `GenerateSourceFiles(...)` / `GenerateFunctionEntries(...)` in `AngelscriptEditorModule.cpp` → 把它理解成旧 editor-side 生成遗留，而不是当前主链路中心

## 小结

- 当前 `AngelscriptUHTTool` 和 Runtime 的接口边界非常清楚：UHT 生成编译产物，Runtime 拥有函数表状态并执行最终消费
- `FAngelscriptBinds::AddFunctionEntry(...)` 与 `ClassFuncMaps` 是这条 handoff 的核心契约面
- `Bind_BlueprintCallable.cpp` 只认统一函数表接口，不关心条目来自手写绑定还是 UHT 生成
- Editor 侧仍保留旧的生成工具函数，但根据当前计划与代码状态，它们已经属于遗留/辅助链，而不是当前主导的 UHT → Runtime 生成链路
