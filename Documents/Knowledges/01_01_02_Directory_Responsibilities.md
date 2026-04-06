# `AngelscriptRuntime` / `AngelscriptEditor` / `AngelscriptTest` / `Dump` 目录职责

> **所属模块**: 插件总体架构 → 模块目录职责
> **关键源码**: `Plugins/Angelscript/AGENTS.md`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTestModule.cpp`

这一节不是简单把目录名翻译一遍，而是把四类最常被混淆的职责边界钉死：谁是真正承载脚本系统能力的主模块，谁负责编辑器补充层，谁负责验证体系，谁只负责状态导出而不回写业务。把这层边界看清，后面读类生成、热重载、调试器和测试体系时才不会把“能力层”和“验证层”混成一团。

## 目录职责总览

- `AngelscriptRuntime` 是插件核心能力层，负责真正的脚本引擎启动、运行时状态和核心子系统装配
- `AngelscriptEditor` 是编辑器补充层，负责把 Runtime 暴露到 Unreal Editor 工作流里，而不是重做一份脚本引擎
- `AngelscriptTest` 是验证层，负责按主题组织回归、场景、调试和导出测试
- `Dump` 不是独立模块，而是横跨 Runtime 与 Test 的一组职责拆分：Runtime 负责导出能力，Test 负责命令入口和自动化验证

```text
Host Project
└─ Plugins/Angelscript
   ├─ AngelscriptRuntime   // core runtime capability
   ├─ AngelscriptEditor    // editor-side integration
   ├─ AngelscriptTest      // validation and regression
   ├─ Runtime/Dump         // state export implementation
   └─ Test/Dump            // command + automation coverage
```

这个结构最重要的含义不是“目录很多”，而是**一层只做一类事**：Runtime 负责能力，Editor 负责编辑器接缝，Test 负责验证，Dump 负责观察而不是业务回写。

## `AngelscriptRuntime`：真正的能力主层

- 这是插件核心模块，主入口就在 `FAngelscriptRuntimeModule`
- 它不只是“提供一些工具函数”，而是负责初始化 `FAngelscriptEngine`，并持有主引擎上下文
- 目录结构也能看出它是能力收口层：`ClassGenerator`、`Preprocessor`、`StaticJIT`、`Debugging`、`Binds`、`FunctionLibraries`、`Dump` 都挂在它下面

`Plugins/Angelscript/Source/AngelscriptRuntime/` 目录本身已经说明它不是一个薄模块。它下面包含：

- `Core/`：总控与共享状态入口
- `ClassGenerator/`：脚本类 / 结构体生成
- `Preprocessor/`：脚本预处理与模块描述符装配
- `StaticJIT/`：JIT 与预编译数据链路
- `Debugging/`：调试协议与调试服务
- `Binds/` / `FunctionLibraries/` / `FunctionCallers/`：脚本与 UE 互操作层
- `Dump/`：运行时状态导出实现

下面这段入口代码是最直接的证据：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp
// 函数: FAngelscriptRuntimeModule::StartupModule
// 位置: Runtime 模块启动时接管脚本引擎初始化与后备 Tick
// ============================================================================
void FAngelscriptRuntimeModule::StartupModule()
{
    // 只有编辑器或命令行工具环境启动时，才主动初始化脚本系统
    if (GIsEditor || IsRunningCommandlet())
    {
        InitializeAngelscript(); // ★ 把 Runtime 模块和脚本引擎真正接起来
    }

    // 编辑器下再补一个 fallback tick，避免没有 tick owner 时引擎停摆
    if (GIsEditor)
    {
        FallbackTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FAngelscriptRuntimeModule::TickFallbackPrimaryEngine));
    }
}
```

这段代码说明 Runtime 不是纯静态库式的“被别人调用的工具箱”，而是模块级生命周期的真正拥有者：它决定何时初始化脚本引擎、何时托管主引擎实例、何时补齐 fallback tick。

## `AngelscriptEditor`：把 Runtime 接进编辑器工作流

- Editor 模块的职责不是复制一份 Runtime，而是把脚本系统接到编辑器交互链上
- 它处理目录监听、内容浏览器数据源、设置、菜单扩展、类重载辅助器等 editor-only 能力
- 这层的典型特征是：它大量依赖 `UnrealEd`、`DirectoryWatcher`、`ContentBrowser`、`AssetRegistry` 等编辑器设施

`AngelscriptEditorModule.cpp` 的 include 面本身就是职责证据：它同时拉进了 `AngelscriptDirectoryWatcherInternal.h`、`ClassReloadHelper.h`、`AngelscriptContentBrowserDataSource.h`、`ContentBrowserModule.h`、`AssetRegistryModule.h`。这说明它的主工作不是“执行脚本逻辑”，而是把 Runtime 的脚本能力嵌进编辑器体验里。

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp
// 函数: OnScriptFileChanges
// 位置: Editor 侧文件变化进入 Runtime 热重载队列的接缝点
// ============================================================================
void OnScriptFileChanges(const TArray<FFileChangeData>& Changes)
{
    // 编辑器初始化没完成前，先不接受脚本变更事件
    if (!FAngelscriptEngine::IsInitialized())
        return;

    FAngelscriptEngine& AngelscriptManager = FAngelscriptEngine::Get();
    AngelscriptEditor::Private::QueueScriptFileChanges(
        Changes,
        AngelscriptManager.AllRootPaths,
        AngelscriptManager,
        IFileManager::Get(),
        [&AngelscriptManager](const FString& AbsoluteFolderPath)
        {
            // ★ 把编辑器侧目录变化映射回 Runtime 已加载脚本集合
            return AngelscriptEditor::Private::GatherLoadedScriptsForFolder(AngelscriptManager, AbsoluteFolderPath);
        });
}
```

这段逻辑的重点不在“收到文件变化”这么表面的事，而在于它展示了 Editor 的真实定位：**Editor 负责感知编辑器事件，Runtime 负责持有脚本状态和执行重载策略**。两者在这里交汇，但职责并不混同。

## `AngelscriptTest`：按主题组织的验证层

- Test 模块不是 Runtime 的附属目录，而是插件验证体系的独立承载层
- `Plugins/Angelscript/AGENTS.md` 明确要求这里按具体主题组织，例如 `Actor`、`Component`、`Delegate`、`Debugger`、`Dump`、`HotReload`
- 这意味着它不是“随便堆测试”的杂物间，而是和运行时子系统一一对照的验证地图

当前 `Plugins/Angelscript/Source/AngelscriptTest/` 目录下面已经存在大量主题目录：`ClassGenerator/`、`Preprocessor/`、`HotReload/`、`Debugger/`、`Dump/`、`Interface/`、`GC/`、`Subsystem/`、`Validation/` 等。这种布局直接体现了文档规则里强调的“按主题组织验证，而不是按大杂烩场景堆积”。

模块入口虽然很薄，但它证明了 `AngelscriptTest` 是一个真正可加载的模块，而不是零散测试文件的集合：

```cpp
// ============================================================================
// 文件: Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTestModule.cpp
// 函数: FAngelscriptTestModule::StartupModule / ShutdownModule
// 位置: Test 模块作为独立验证层被加载与卸载
// ============================================================================
void FAngelscriptTestModule::StartupModule()
{
    UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest module started."));
}

void FAngelscriptTestModule::ShutdownModule()
{
    UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest module shut down."));
}
```

真正重要的是它背后的目录规则：

- `Source/AngelscriptTest/` 使用 `Angelscript.TestModule.*` 前缀
- `Native/` 是 Native Core 测试层，只使用公共 API
- `Dump/` 承担导出命令和自动化回归
- `Shared/` 提供跨专题共享 fixture 和 helper

所以 Test 模块的职责应该理解成：**围绕插件子系统做分层验证**，而不是“只在最后跑一批自动化用例”。

## `Dump`：导出实现与验证入口分拆

- Runtime 侧 `Dump/` 负责状态导出实现和汇总
- Test 侧 `Dump/` 负责控制台命令与自动化回归
- 这是一种很刻意的观察者式拆分：导出能力放在 Runtime，可测试入口放在 Test

`Plugins/Angelscript/AGENTS.md` 已经把这条边界说得非常明确：

1. `Source/AngelscriptRuntime/Dump/` 负责运行时状态 CSV 导出与汇总；
2. `Source/AngelscriptTest/Dump/` 负责状态导出控制台命令与自动化回归。

这条规则的架构意义非常大，因为它避免了两个常见退化：

- 为了导出而把业务逻辑硬塞进 Runtime 各处；
- 为了方便测试而让 Test 反向掌控 Runtime 内部状态。

换句话说，Dump 的正确读法不是“一个导出目录”，而是“**导出实现层 + 验证触发层**”的两段式设计。

## 为什么这个拆分值得先记住

- 后面分析类生成、热重载、调试器时，经常会同时碰到 Runtime、Editor、Test 三层代码
- 如果不先把目录职责钉住，很容易把 editor event、runtime state、test harness 混成一个系统
- 而 Dump 的例子又进一步说明：即使是同一个功能域，也可能按“实现 / 触发 / 回归”拆在不同目录里

所以这一节的目的不是把目录名背下来，而是建立一个阅读过滤器：

- 看到核心能力，优先回 `AngelscriptRuntime`
- 看到编辑器事件、菜单、内容浏览器、目录监听，优先回 `AngelscriptEditor`
- 看到验证入口、fixture、专题回归，优先回 `AngelscriptTest`
- 看到状态导出时，再进一步区分它是在 Runtime 实现，还是在 Test 暴露入口

## 小结

- `AngelscriptRuntime` 是能力主层，负责真正的脚本系统运行时能力
- `AngelscriptEditor` 是编辑器接缝层，负责把 Runtime 接进 Unreal Editor 工作流
- `AngelscriptTest` 是按主题组织的验证层，承担插件能力的回归和验证地图
- `Dump` 不是独立王国，而是 Runtime 导出实现与 Test 验证入口的两段式拆分
