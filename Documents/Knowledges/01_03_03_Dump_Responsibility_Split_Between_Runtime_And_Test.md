# Dump 在 Runtime / Test 中的职责拆分

> **所属模块**: Editor / Test / Dump 协作边界 → Dump 双侧职责
> **关键源码**: `Plugins/Angelscript/AGENTS.md`, `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/AngelscriptStateDump.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Dump/AngelscriptStateDump.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/AngelscriptDumpCommand.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/Dump/AngelscriptDumpTests.cpp`, `Documents/Plans/Archives/Plan_ASEngineStateDump.md`

这一节真正要解释的，不是“Dump 功能在哪几个目录里”，而是为什么它必须拆成 Runtime 和 Test 两侧。当前仓库的设计非常明确：**Runtime 负责导出能力本身，Test 负责触发入口与自动化验证。** 这不是机械分目录，而是一个刻意的架构边界：导出器必须作为运行时公共能力存在，才能被 Editor、Test、Commandlet 复用；而控制台命令和回归测试又不应该反过来污染 Runtime 核心实现。

## 先看最硬的仓库规则

`Plugins/Angelscript/AGENTS.md` 已经把这条边界直接写成规则：

- `Source/AngelscriptRuntime/Dump/` 负责运行时状态 CSV 导出与汇总
- `Source/AngelscriptTest/Dump/` 负责状态导出控制台命令与自动化回归
- Dump 应优先通过现有 public/runtime API 做外部观察，不要为了 dump 回写或侵入原有业务类型

这三句话其实已经定义了整个子系统的职责图：

1. **实现层** 在 Runtime；
2. **触发层** 在 Test；
3. **验证层** 也在 Test；
4. **访问方式** 优先走公开状态和观察者模式，而不是侵入原有业务类型。

所以这一节的核心不是“Runtime 和 Test 都跟 dump 有关”，而是：**它们各自只拿属于自己的那一段职责。**

## Runtime 侧：`FAngelscriptStateDump` 是真正的导出总线

Runtime 侧的核心入口非常集中：`FAngelscriptStateDump::DumpAll(FAngelscriptEngine&, const FString&)`。

从 `AngelscriptStateDump.h` 可以直接看出这不是单表导出器，而是一条总线：

- `DumpAll(...)` 作为统一入口
- 多个 `Dump*` 子表函数，例如 `DumpModules`、`DumpClasses`、`DumpDiagnostics`、`DumpDebugServerState`、`DumpCodeCoverage`
- `FDumpExtensionsDelegate OnDumpExtensions` 作为扩展缝

这说明 Runtime 侧承担的是**状态采样与表格导出能力**，而不是“命令行界面”或“测试断言”。它的职责是把当前 `FAngelscriptEngine` 所拥有的可观察状态稳定地投影成 CSV 表集合。

## `DumpAll()` 证明 Runtime 负责的是导出编排，而不是测试入口

`AngelscriptStateDump.cpp` 里的 `DumpAll()` 很能说明这一点。它的核心行为是：

- 解析输出目录；
- 逐张表调用 Runtime 内部的 dump 子函数；
- 广播 `OnDumpExtensions` 让别的模块接入扩展表；
- 对扩展表结果做存在性和行数汇总；
- 最终输出 `DumpSummary.csv` 和总日志。

这里最重要的不是“它导出了很多表”，而是它的**输入和输出都很纯**：

- 输入是一个 `FAngelscriptEngine&` 和可选输出目录；
- 输出是 CSV 目录路径；
- 中间不夹带 console parsing、测试断言、Automation 前缀或 editor UI。

这说明 Runtime 的 dump 实现被刻意做成了一个**可复用的纯导出 API**。这也是为什么归档计划里会把它表述成“Runtime 公共 API，任何模块（Editor、Test、Commandlet）都可调用”。

## Runtime 侧还负责扩展总线，但不拥有 Editor/Test 行为

`FAngelscriptStateDump::OnDumpExtensions` 是 Runtime 侧很关键的一层设计。它意味着：

- Runtime 自己先导出通用运行时表；
- 然后广播一个扩展点，让 Editor 或其他模块把额外状态表挂进来；
- `DumpAll()` 只检查这些扩展表是否生成，并把它们纳入 summary。

这层设计再次说明了边界：Runtime 提供**观察总线**，但它并不直接内嵌 Editor 的重载状态或菜单扩展实现。同样，Runtime 也不会内嵌测试断言逻辑。它只保证：“如果别的模块按约定生成扩展表，我会把它们纳入导出结果。”

从这个角度看，Dump 子系统在 Runtime 侧的角色类似于：

- 一台导出编排器；
- 一组纯观察者表生成器；
- 一条允许别的模块接入的扩展总线。

## Test 侧第一职责：提供控制台触发入口

Test 侧最直接的职责体现在 `AngelscriptDumpCommand.cpp`：

```cpp
FAutoConsoleCommand GAngelscriptDumpEngineStateCommand(
    TEXT("as.DumpEngineState"),
    TEXT("Dump Angelscript engine state to CSV tables. Optional: as.DumpEngineState [OutputDir]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteDumpEngineState));
```

这里 Test 模块做的事情很清楚：

- 解析控制台参数；
- 检查当前 engine 是否已初始化；
- 调用 `FAngelscriptStateDump::DumpAll(FAngelscriptEngine::Get(), RequestedOutputDir)`；
- 用 `UE_LOG` 报告成功或失败。

它**没有重新实现任何 dump 表逻辑**，也没有去拼接 CSV 内容。它只是把 Runtime 已经存在的导出能力包装成一个适合开发时人工触发的控制台入口。

因此 Test 侧在这里扮演的角色不是“第二套实现者”，而是**驱动器**。

## Test 侧第二职责：把 Runtime 导出能力变成自动化回归

第二个职责体现在 `AngelscriptDumpTests.cpp`。这个文件没有扩展 dump 总线，也没有改写 CSV writer；它主要做两件事：

- 测 `FCSVWriter` 的基础行为与特殊字符转义；
- 测 `FAngelscriptStateDump::DumpAll()` 的端到端产物和 summary 状态。

它的 Automation 前缀也非常刻意：

- `Angelscript.TestModule.Dump.CSVWriter.Basic`
- `Angelscript.TestModule.Dump.CSVWriter.SpecialCharacters`
- `Angelscript.TestModule.Dump.DumpAll.EndToEnd`
- `Angelscript.TestModule.Dump.DumpAll.Summary`

这恰好和 `Plugins/Angelscript/AGENTS.md` 里的规则对齐：`Source/AngelscriptTest/Dump/` 负责状态导出控制台命令与自动化回归，Automation 前缀统一用 `Angelscript.TestModule.Dump.*`。

也就是说，Test 侧不是用来“顺带放几个 dump 示例”的，而是专门承担：

- **可执行入口**；
- **产物级回归验证**。

## 为什么控制台命令和自动化测试不应该放回 Runtime

这一点看起来像组织习惯，其实是关键边界：

- 如果把 `as.DumpEngineState` 放进 Runtime，Runtime 就会直接承担 editor/test 工具型入口职责；
- 如果把端到端 dump 断言放进 Runtime，Runtime 私有实现层就会和验证层缠在一起；
- 这样会让导出 API 和导出使用方式耦合，后面无论是换触发入口、换测试 runner，还是在 Commandlet/Editor 中复用，都会更难拆。

当前设计反过来做，正好把边界保持得很干净：

- **Runtime**：只关心“能不能正确导出”
- **Test**：关心“怎么触发导出”和“导出结果是不是符合预期”

这也是归档计划里把“核心导出逻辑放在 Runtime，CVar 触发和测试放在 Test”列成单独目标的原因。

## 归档计划把这条边界讲得更彻底

`Documents/Plans/Archives/Plan_ASEngineStateDump.md` 其实把这个架构判断讲得比代码更直白：

- 核心导出逻辑放在 `AngelscriptRuntime`
- CVar 触发和测试放在 `AngelscriptTest`
- 导出器作为 Runtime 公共 API，任何模块都可调用
- 所有 dump 逻辑都在 `AngelscriptRuntime/Dump/` 目录内，不修改已有业务文件

这份归档计划还有一个很关键的原则：**纯外部观察者模式**。这意味着 dump 实现不该靠在原有类里塞 `Dump()` / `ToString()` 方法来完成，而是尽量只用已经公开的 runtime/public API 来观测现有状态。

从这个角度看，Runtime/Test 的职责拆分并不是孤立规则，而是整个 dump 架构原则的一部分：

- Runtime 通过外部观察者方式读取状态并导出；
- Test 通过控制台和自动化方式验证这些导出是否稳定。

## Editor 为什么没有单独占据这一节的主角

虽然 dump 最终还能导出 `EditorReloadState.csv`、`EditorMenuExtensions.csv` 这类 Editor 侧扩展表，但在这一小节里，Editor 不是主角。原因很简单：

- Runtime 是 dump 总线和主导出器；
- Test 是驱动和回归层；
- Editor 只是通过 `OnDumpExtensions` 接入额外观察表。

所以这一节讨论的是**Runtime / Test 的职责拆分**，而不是整个 dump 生态的所有参与方。Editor 的位置更像“挂在 Runtime 总线上的扩展者”，不是和 Test 一样承担驱动/验证职责的一侧。

## 这条边界应该怎么记

如果把当前 dump 子系统压成一句工程化判断，可以这样记：

**Runtime 负责把引擎状态投影成结构化 CSV 快照；Test 负责把这项能力包装成开发入口，并用 `Angelscript.TestModule.Dump.*` 回归验证导出结果。**

换成更实用的判断器就是：

- 这是导出表结构、汇总逻辑、扩展总线的问题 → 看 `AngelscriptRuntime/Dump/`
- 这是控制台命令、人工触发或自动化断言的问题 → 看 `AngelscriptTest/Dump/`

这样读代码时，就不会把“导出器实现”和“导出器使用方式”混在一起。

## 小结

- `FAngelscriptStateDump::DumpAll()` 和各张 CSV 表生成逻辑属于 Runtime，这是 dump 子系统的实现层
- `as.DumpEngineState` 控制台命令与 `Angelscript.TestModule.Dump.*` 自动化测试属于 Test，这是 dump 子系统的驱动层与验证层
- `OnDumpExtensions` 说明 Runtime 提供了扩展总线，但扩展表和测试断言并没有反向侵入 Runtime 实现
- 这种拆分让 dump 既能作为公共运行时 API 被复用，又能通过 Test 模块获得稳定的人工入口和自动化回归覆盖
