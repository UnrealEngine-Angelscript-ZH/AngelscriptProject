# `Angelscript.uplugin` 中模块声明与 `LoadingPhase`

> **所属模块**: 插件模块清单与装载关系 → 模块声明 / LoadingPhase
> **关键源码**: `Plugins/Angelscript/Angelscript.uplugin`, `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`, `Plugins/Angelscript/Source/AngelscriptEditor/AngelscriptEditor.Build.cs`, `Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTest.Build.cs`, `AGENTS.md`

这一节真正要钉死的，不是 `.uplugin` 里有三个模块这么表面的事实，而是这三个模块在插件里被怎样声明、为什么它们都选了现在的 `Type` 和 `LoadingPhase`，以及这些选择如何反过来印证当前仓库的“插件优先、宿主最小化”定位。当前 `Angelscript.uplugin` 的声明非常克制：只暴露 `AngelscriptRuntime`、`AngelscriptEditor`、`AngelscriptTest` 三个模块，而且三者统一使用 `LoadingPhase = PostDefault`。这套配置把插件切成了三块清晰的装载面：可随项目带起的运行时核心、仅在编辑器环境里激活的扩展层，以及同样只在编辑器环境里可用的验证层。

## `.uplugin` 是插件装载面的总清单

`Plugins/Angelscript/Angelscript.uplugin` 当前的模块清单非常短：

```json
"Modules": [
  {
    "Name": "AngelscriptRuntime",
    "Type": "Runtime",
    "LoadingPhase": "PostDefault"
  },
  {
    "Name": "AngelscriptEditor",
    "Type": "Editor",
    "LoadingPhase": "PostDefault"
  },
  {
    "Name": "AngelscriptTest",
    "Type": "Editor",
    "LoadingPhase": "PostDefault"
  }
]
```

这段配置最重要的不是字段本身，而是它把插件真正对外声明成了三种装载角色：

- `Runtime`：这是插件最小且最核心的交付面
- `Editor`：这是只在编辑器环境里才会存在的补充层
- `Test`：这是只在编辑器验证环境里启用的测试模块，而不是一个随运行时发布的产品模块

也就是说，`Angelscript.uplugin` 本身已经把“什么属于真正可复用插件能力，什么属于编辑器辅助，什么属于验证层”切开了。

## `AngelscriptRuntime`：唯一的 Runtime 模块声明

三个模块里，只有 `AngelscriptRuntime` 被声明为 `Type = Runtime`。这和根级 `AGENTS.md` 的仓库定位正好一致：

- 真正要交付和复用的是 `Plugins/Angelscript` 插件本身
- 其中最不可缺的核心能力面就是 Runtime
- Editor 和 Test 都是围绕它展开的辅助或验证层

`AngelscriptRuntime.Build.cs` 也能印证这层地位：

- 它公开依赖 `Core`、`CoreUObject`、`Engine`、`Json`、`GameplayTags`、`StructUtils`
- 私有依赖里还包含 `Networking`、`Sockets`、`EnhancedInput`、`GameplayAbilities` 等运行时互操作模块
- 并且只有在 `Target.bBuildEditor` 时才额外拉入 `UnrealEd`、`EditorSubsystem`、`UMGEditor`

这说明 Runtime 模块的默认姿态就是：**先作为插件核心能力存在，然后在编辑器构建里再按需挂 editor-only 支撑。** 它不是一个“默认必须依赖编辑器”的模块，只是为了编辑器环境额外打开了一些能力。

## `AngelscriptEditor`：Editor-only 补充层，而不是第二个核心

`AngelscriptEditor` 在 `.uplugin` 里被声明成：

- `Type = Editor`
- `LoadingPhase = PostDefault`

这说明它从插件装载层面就已经被定性为：**只在编辑器环境里可见的扩展层**。

这一点和它的 `Build.cs` 完全一致。`AngelscriptEditor.Build.cs` 公开依赖：

- `UnrealEd`
- `EditorSubsystem`
- `BlueprintGraph`
- `Kismet`
- `DirectoryWatcher`
- `AssetTools`
- 同时直接依赖 `AngelscriptRuntime`

这组依赖面很能说明问题：Editor 模块不拥有另一套脚本系统，它是围绕 Runtime 去接入 UnrealEd、Blueprint、Content Browser、DirectoryWatcher 这些编辑器设施的。换句话说，它的存在方式本来就是：**Editor 对 Runtime 的补壳层。**

因此把它声明成 `Editor` 而不是 `Runtime`，不是保守选择，而是对实际职责的一次准确命名。

## `AngelscriptTest`：Editor 型验证层，而不是发布型模块

`AngelscriptTest` 在 `.uplugin` 里同样被声明为：

- `Type = Editor`
- `LoadingPhase = PostDefault`

这点很关键，因为它明确告诉我们：测试模块不是产品能力层，而是**跟随编辑器验证流程一起装载的验证层**。

这和它的 `Build.cs` 也完全一致：

- 公共依赖里直接引用 `AngelscriptRuntime`
- 只有 `Target.bBuildEditor` 时才私有依赖 `UnrealEd` 和 `AngelscriptEditor`
- 还引入 `CQTest`、`Networking`、`Sockets`

这说明 `AngelscriptTest` 的真实姿态不是“始终存在的第四个插件子系统”，而是：

- 平时围绕 Runtime 做验证
- 在 editor 构建里再接上 Editor 扩展和调试能力
- 不参与运行时发布面

因此它被声明成 `Editor` 不是偶然，而是把“测试只服务开发/验证环境”这条边界提前固化在插件装载层里。

## 三个模块都选 `PostDefault` 的含义

当前三个模块没有做阶段差异化，而是统一使用 `LoadingPhase = PostDefault`。就当前仓库结构来说，这个选择传递出的信号很清楚：

- 作者并没有把 Editor 或 Test 提前到某个更早阶段，去抢在 Runtime 之前构造状态
- 也没有把它们推迟到更晚阶段，形成复杂的分期装载关系
- 而是让三者都在常规默认阶段之后进入稳定装载序列，再由模块内部逻辑自己去决定更细的时序

这个判断可以从实际代码印证：

- Runtime 自己在 `StartupModule()` 里继续判断 `GIsEditor` / `IsRunningCommandlet()` 才决定是否初始化 Angelscript
- Editor 模块再在 `OnPostEngineInit` 之后注册 `UAngelscriptContentBrowserDataSource`
- Test 模块本身很薄，更多测试时序由 Automation 框架驱动

也就是说，`.uplugin` 里的 `PostDefault` 并没有试图解决所有模块时序问题，它只保证三者都在一个相对稳定、统一的装载阶段进入；更细的初始化控制仍然放在各模块内部完成。

从架构角度看，这是一种很克制的设计：**装载层只做粗粒度阶段划分，细粒度启动时序交给模块实现本身。**

## 为什么现在的声明能很好地匹配插件中心化目标

根级 `AGENTS.md` 已经把仓库目标说死了：

- `Plugins/Angelscript/` 是核心工作区
- `Source/AngelscriptProject/` 只是最小宿主壳

`Angelscript.uplugin` 的模块声明恰好就是这个目标在装载层的投影：

- 只有一个 Runtime 模块，说明“可交付核心”被压缩在插件主能力层里
- Editor 和 Test 都没有被伪装成 Runtime 面，说明宿主工程和编辑器工具不会反向成为插件核心的一部分
- 测试模块也不是宿主项目模块，而是插件自己的 Editor 型验证层

换句话说，如果只看 `.uplugin` 而不看整仓代码，也能看出当前插件架构的主语是谁：**Runtime 是核心，Editor 和 Test 是围绕它的装载层扩展。**

## `Build.cs` 和 `.uplugin` 的分工边界

这一节还有一个很容易混淆的点：`.uplugin` 已经声明了模块，为什么还要看 `Build.cs`？

因为它们回答的问题不同：

- `.uplugin` 回答“有哪些模块、属于哪种类型、在哪个阶段装载”
- `Build.cs` 回答“这些模块各自依赖哪些引擎/插件模块，内部又如何区分 editor-only 条件依赖”

所以现在这篇只聚焦第一层：**声明与装载阶段。** 之所以顺手带上 `Build.cs`，只是为了说明这些声明不是孤立文本，而是和模块的真实依赖面一致。真正的依赖面细节，应该放到 `1.4.2 Runtime / Editor / Test 的插件依赖面` 再展开。

## 这条装载边界应该怎么记

如果把当前 `.uplugin` 的模块声明压成一句工程化判断，可以这样记：

**`AngelscriptRuntime` 是唯一的运行时核心模块；`AngelscriptEditor` 和 `AngelscriptTest` 都是 `Editor` 型辅助/验证模块；三者统一在 `PostDefault` 进入装载序列，再由模块内部逻辑决定更细的初始化时机。**

换成更实用的阅读过滤器就是：

- 看插件真正可复用核心 → 先看 `AngelscriptRuntime`
- 看 Editor 设施接缝 → 看 `AngelscriptEditor`
- 看验证与自动化 → 看 `AngelscriptTest`
- 看“为什么它们什么时候被装载” → 先看 `.uplugin` 的 `Type` 和 `LoadingPhase`

这样读代码时，就不会把模块声明层和模块内部时序层混成一层。

## 小结

- `Angelscript.uplugin` 当前只声明了三个模块：`AngelscriptRuntime`、`AngelscriptEditor`、`AngelscriptTest`
- 其中只有 `AngelscriptRuntime` 是 `Runtime` 类型，Editor 和 Test 都被明确声明为 `Editor` 类型，体现了“核心能力 + 编辑器补充 + 验证层”的分层
- 三个模块统一使用 `LoadingPhase = PostDefault`，说明装载层只做粗粒度时序控制，细粒度初始化由模块内部逻辑继续决定
- 这种声明方式和仓库的插件中心化目标完全一致：Runtime 是核心交付面，Editor/Test 都围绕它展开，而不是反过来侵入插件主能力层
