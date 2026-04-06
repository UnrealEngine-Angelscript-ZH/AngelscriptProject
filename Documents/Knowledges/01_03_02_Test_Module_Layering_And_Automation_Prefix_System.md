# Test 模块分层与 Automation 前缀体系

> **所属模块**: Editor / Test / Dump 协作边界 → Test 模块分层 / Automation Prefix
> **关键源码**: `Plugins/Angelscript/AGENTS.md`, `Documents/Guides/Test.md`, `Documents/Guides/TestConventions.md`, `Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTestModule.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/`

这一节真正要讲清楚的，不是 `AngelscriptTest` 目录下有哪些文件夹，而是为什么这个测试模块必须按层级和前缀被刻意拆开。当前仓库里同时存在 `AngelscriptRuntime/Tests/`、`AngelscriptEditor/Private/Tests/`、`AngelscriptTest/Native/`、`AngelscriptTest/Debugger/`、`Actor/`、`HotReload/` 等多条测试线；如果不先把“它们分别验证哪一层、为什么用不同前缀、为什么不能混放”说明白，后续任何新增测试都会迅速失去边界。

## 先把总规则钉死

`Plugins/Angelscript/AGENTS.md` 和 `Documents/Guides/TestConventions.md` 给出的规则可以压成三句话：

- **先定层级，再定目录，再定 Automation 前缀**
- `CppTests`、`Editor`、`TestModule` 三条线必须严格区分，不允许混用前缀
- `AngelscriptTest` 内部再继续按层级优先或主题优先细分，而不是把所有回归都堆成一层“大场景测试”

这说明当前测试体系不是“一个模块里放很多 case”，而是一套明确的**测试分层架构**。目录只是表面，真正稳定的是三件事：

1. 这类测试依赖什么运行环境；
2. 这类测试验证哪一层语义；
3. 这类测试应该暴露成什么 Automation 前缀。

## 顶层三线：`CppTests` / `Editor` / `TestModule`

当前测试体系最上面的切分，不是在 `AngelscriptTest/` 目录内部，而是在整个插件源码层面：

- `Plugins/Angelscript/Source/AngelscriptRuntime/Tests/` → `Angelscript.CppTests.*`
- `Plugins/Angelscript/Source/AngelscriptEditor/Private/Tests/` → `Angelscript.Editor.*`
- `Plugins/Angelscript/Source/AngelscriptTest/` → `Angelscript.TestModule.*`

`Plugins/Angelscript/AGENTS.md` 已经把这三条边界写成硬规则：

- Runtime 内部测试只放在 `Runtime/Tests/`，前缀统一 `Angelscript.CppTests.*`
- Editor 内部测试只放在 `Editor/Private/Tests/`，前缀统一 `Angelscript.Editor.*`
- `Source/AngelscriptTest/` 使用 `Angelscript.TestModule.*` 前缀

这层分割非常关键，因为它实际表达的是三种不同的依赖面：

- **CppTests**：可以碰 Runtime 私有实现和内部状态
- **Editor**：可以碰 Editor 私有实现与 editor-only helper
- **TestModule**：是插件对外的综合验证层，通常围绕 `FAngelscriptEngine`、UObject/World/Actor 语义、脚本行为和调试器场景组织

所以顶层前缀不是随便起的命名风格，而是**测试权限边界**。

## `AngelscriptTest` 不是杂物间，而是主题化验证层

`AngelscriptTestModule.cpp` 本身很薄：

```cpp
IMPLEMENT_MODULE(FAngelscriptTestModule, AngelscriptTest);

void FAngelscriptTestModule::StartupModule()
{
    UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest module started."));
}
```

这恰好说明它的职责不是在模块入口塞复杂逻辑，而是作为一个**承载大量主题测试目录的验证容器**。真正重要的结构体现在目录布局上。当前 `Source/AngelscriptTest/` 下已经存在：

- `Native/`
- `Core/`、`Bindings/`、`Internals/`、`Compiler/`、`Preprocessor/`、`FileSystem/`、`ClassGenerator/`
- `Debugger/`
- `Actor/`、`Blueprint/`、`Component/`、`Delegate/`、`GC/`、`HotReload/`、`Interface/`、`Subsystem/`
- `Learning/`
- `Examples/`
- `Dump/`
- `Shared/`

这个目录树本身已经验证了 `TestConventions.md` 的中心判断：`AngelscriptTest` 不是一层宽泛的“集成测试”，而是围绕不同验证层和主题域拆出来的多簇测试体系。

## `TestConventions.md` 给出的层级矩阵

`Documents/Guides/TestConventions.md` 已经把当前测试分层整理成一张非常清晰的矩阵。核心几层可以概括成：

- **Runtime 内部 C++**：`Angelscript.CppTests.*`
- **Editor 内部**：`Angelscript.Editor.*`
- **Native Core**：`Angelscript.TestModule.Native.*`
- **Runtime 集成**：`Angelscript.TestModule.*`
- **Debugger 场景**：`Angelscript.TestModule.Debugger.*`
- **UE 场景层**：`Angelscript.TestModule.<Theme>.*`
- **Learning**：`Angelscript.TestModule.Learning.<Layer>.*`
- **Examples**：`Angelscript.TestModule.ScriptExamples.*`

这张矩阵真正重要的地方不在于列了很多类，而在于它回答了新增测试最先该问的三个问题：

1. 它是否需要 `FAngelscriptEngine`？
2. 它是否需要真实 `UObject` / `World` / `Actor` 生命周期？
3. 它是否只是 Editor 内部行为？

也就是说，测试分类不是按“我想测什么功能”直接命名，而是先按**依赖层和运行环境**来分层，再按主题命名。

## 层级优先 vs 主题优先

这套体系里还有一个很容易忽略，但极重要的命名原则：什么时候层级优先，什么时候主题优先。

`Plugins/Angelscript/AGENTS.md` 和 `TestConventions.md` 都强调：

- `Native/` 和 `Learning/` 采用**层级优先**命名
- `Actor/`、`Component/`、`Delegate/`、`Interface/`、`HotReload/` 等主题目录采用**主题优先**命名
- 当目录已经表达“场景层”时，Automation 路径里**不要再重复追加 `Scenario`**

因此：

- `Angelscript.TestModule.Native.Execute.*` 是合理的
- `Angelscript.TestModule.Learning.Runtime.*` 是合理的
- `Angelscript.TestModule.Actor.*`、`Angelscript.TestModule.Component.*` 是合理的
- 但像 `Angelscript.TestModule.Actor.Scenario.*` 这种就属于重复表达层级，当前规则明确反对

这条规则的价值在于：Automation 前缀不是写作文，它的首要目标是让人一眼看出“这是哪一层、哪一类测试”。

## `Native/` 为什么被单独保护

`Plugins/Angelscript/AGENTS.md` 第一条就把 `Source/AngelscriptTest/Native/` 单独拉出来强调：

- 它是 Native Core 测试层
- 只使用 `AngelscriptInclude.h` / `angelscript.h` 暴露的公共 API
- 不要把 `FAngelscriptEngine` 或 `source/as_*.h` 带进这个目录

这说明 `Native/` 不是普通的主题目录，而是一个**边界保护层**。它的职责是验证公共 AngelScript API / ASSDK 适配层，而不是借助 Runtime 私有实现把一切都测通。

所以 `Native/` 的特殊性不在于前缀更长，而在于它明确被用来维护“公共 API 视角”的独立验证面。这也是为什么 `TestConventions.md` 还要求：

- 纯原生公共 API 用 `AngelscriptNative*Tests.cpp`
- ASSDK 适配层显式带 `ASSDK`

这类命名其实是在把“验证公共 API”与“验证引擎内部集成”刻意拉开。

## `Debugger/`、`UE 场景层`、`Learning/` 各自解决不同问题

在 `AngelscriptTest` 内部，最容易被误解的是这几层看上去都像“集成测试”，但其实目标完全不同：

- `Debugger/`：验证调试协议、握手、断点、步进、附着等 production-like 场景，前缀 `Angelscript.TestModule.Debugger.*`
- `Actor/`、`Component/`、`HotReload/`、`Interface/` 等 UE 主题目录：验证最终的 UObject / World / Actor 语义，前缀 `Angelscript.TestModule.<Theme>.*`
- `Learning/`：偏结构化 trace 和教学型可观测测试，前缀 `Angelscript.TestModule.Learning.<Layer>.*`

这几层的共同点是都在 `AngelscriptTest` 模块里，但差异在于：

- **Debugger** 强调协议与会话；
- **UE 场景层** 强调最终行为语义；
- **Learning** 强调解释性和可观测性。

所以新增 case 时，不能因为它“看起来像集成测试”就全塞进同一个目录；必须先判断它到底验证的是调试链、最终游戏语义，还是教学型 trace。

## `Shared/` 不是新的测试层，而是 helper 层

`TestConventions.md` 还把 helper 复用规则写得很明确：

- Native → `AngelscriptNativeTestSupport.h` / `AngelscriptTestAdapter.h`
- Runtime 集成 → `Shared/AngelscriptTestEngineHelper.*`
- Debugger → `Shared/AngelscriptDebuggerTestSession.*` / `Shared/AngelscriptDebuggerTestClient.*` / `Shared/AngelscriptDebuggerScriptFixture.*`
- UE 场景 → `Shared/AngelscriptScenarioTestUtils.h`
- Learning → `Shared/AngelscriptLearningTrace.*`

这说明 `Shared/` 是**辅助层**，不是新的 Automation 分类层。也就是说，helper 的位置不能反向决定测试该落在哪个主题目录；测试仍应优先落在“最终行为发生处”，而不是 helper 所在处。

## Automation 前缀同时决定测试入口和运行方式

前缀体系之所以必须稳定，还因为 `Documents/Guides/Test.md` 已经把标准 runner 和前缀入口绑定在一起了。例如：

- `Tools\RunTests.ps1 -TestPrefix "Angelscript.CppTests."`
- `Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Dump"`
- `Tools\RunTestSuite.ps1 -Suite Debugger`
- `Tools\RunTestSuite.ps1 -Suite ScenarioSamples`

这意味着前缀不是纯文档标签，它直接关系到：

- 具名 suite 如何收拢一批测试
- group / prefix 命令怎样匹配目标集
- CI 和本地回归怎样稳定选中同一簇测试

因此把 `Angelscript.CppTests.*` 和 `Angelscript.TestModule.*` 混用，不只是“命名不整齐”，而是会直接破坏测试入口的可调度性。

## 这套体系的最小记忆法

如果把当前 Test 模块分层和前缀体系压成一句工程化规则，可以这样记：

**先按依赖层切开：Runtime 内部、Editor 内部、TestModule 综合验证；再在 TestModule 里按 Native / Runtime 集成 / Debugger / UE 主题 / Learning / Examples 继续细分；最后让 Automation 前缀与目录层级一一对齐。**

换成更实用的判断器就是：

- 只测 Runtime 私有实现 → `Angelscript.CppTests.*`
- 只测 Editor 私有行为 → `Angelscript.Editor.*`
- 测公共 API / ASSDK → `Angelscript.TestModule.Native.*`
- 测一般引擎集成 → `Angelscript.TestModule.*`
- 测 UE 场景最终行为 → `Angelscript.TestModule.<Theme>.*`
- 测调试协议 → `Angelscript.TestModule.Debugger.*`

这样分类之后，文件名、目录、前缀、运行入口就会自动对齐，不需要靠事后补文档来兜底。

## 小结

- `AngelscriptTest` 不是宽泛的“大集成测试目录”，而是一套带明确层级矩阵的验证模块
- 顶层必须先区分 `Angelscript.CppTests.*`、`Angelscript.Editor.*`、`Angelscript.TestModule.*` 三条线
- 在 `AngelscriptTest` 内部，`Native` / `Learning` 采用层级优先命名，`Actor` / `Component` / `HotReload` 等采用主题优先命名
- Automation 前缀不仅表达分类，还直接决定标准 runner、suite 和回归入口，因此必须与目录层级严格对齐
