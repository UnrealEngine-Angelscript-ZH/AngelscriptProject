# Runtime / Editor / Test 的插件依赖面

> **所属模块**: 插件模块清单与装载关系 → 模块依赖面
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`, `Plugins/Angelscript/Source/AngelscriptEditor/AngelscriptEditor.Build.cs`, `Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTest.Build.cs`, `Documents/Knowledges/01_04_01_Module_Declarations_And_Loading_Phase.md`

如果说上一节的 `.uplugin` 模块声明回答的是“谁会被装载”，这一节要回答的就是“它们各自真正依赖谁”。当前插件的依赖面其实非常有层次：`AngelscriptRuntime` 是唯一的基础能力层，自己不依赖 `AngelscriptEditor` 或 `AngelscriptTest`；`AngelscriptEditor` 在此之上公开依赖 Runtime，并把大量 editor-only 设施接进来；`AngelscriptTest` 则直接公开依赖 Runtime，在 editor 构建里再私有依赖 Editor。把这条依赖方向读清楚之后，很多“为什么这个逻辑该放哪一层”的判断就不再靠感觉，而是能直接从 `Build.cs` 的依赖图里读出来。

## 先把三模块之间的方向画出来

只看插件内部三模块，当前依赖方向很干净：

```text
AngelscriptRuntime
    ↑
AngelscriptEditor
    ↑
AngelscriptTest   (editor build only also depends on AngelscriptEditor)
```

更准确地说是：

- `AngelscriptRuntime` **不依赖** `AngelscriptEditor` / `AngelscriptTest`
- `AngelscriptEditor` **公开依赖** `AngelscriptRuntime`
- `AngelscriptTest` **公开依赖** `AngelscriptRuntime`
- `AngelscriptTest` 在 `Target.bBuildEditor` 时 **私有依赖** `AngelscriptEditor`

这条方向很关键，因为它说明当前插件并没有形成“Editor 反向侵入 Runtime”或“Test 反向侵入产品层”的循环依赖。整个方向仍然是：**能力在 Runtime，Editor 和 Test 往上叠。**

## `AngelscriptRuntime`：基础能力层，自己不反向依赖 Editor / Test

`AngelscriptRuntime.Build.cs` 的依赖面可以先分成两组看：

### PublicDependencyModuleNames

- `ApplicationCore`
- `Core`
- `CoreUObject`
- `Engine`
- `EngineSettings`
- `DeveloperSettings`
- `Json`
- `JsonUtilities`
- `GameplayTags`
- `StructUtils`

### PrivateDependencyModuleNames

- `AIModule`
- `NavigationSystem`
- `NetCore`
- `Landscape`
- `Networking`
- `Sockets`
- `InputCore`
- `SlateCore`
- `Slate`
- `UMG`
- `TraceLog`
- `AssetRegistry`
- `Projects`
- `PhysicsCore`
- `CoreOnline`
- `EnhancedInput`
- `GameplayAbilities`
- `GameplayTasks`

最值得注意的不是它依赖很多，而是：**它依赖的都是引擎/功能模块，没有反向依赖插件自己的 Editor/Test 子模块。**

这说明 Runtime 在当前架构里的角色就是基础能力面：

- 它确实很重，因为要把脚本、绑定、调试、JIT、网络、资产和 UI 互操作面都接进来；
- 但它仍然保持了“向上不看 Editor/Test”的方向性。

这条边界非常重要，因为它确保 Runtime 仍然可以被理解为插件真正的底座，而不是夹带 editor/test 语义的混合层。

## Runtime 也有 editor-only 条件依赖，但这不等于它变成 Editor 模块

`AngelscriptRuntime.Build.cs` 里还有一段很值得单独讲：

```csharp
if (Target.bBuildEditor)
{
    PublicDependencyModuleNames.AddRange(new string[]
    {
        "UnrealEd",
        "EditorSubsystem",
    });

    PrivateDependencyModuleNames.AddRange(new string[]
    {
        "UMGEditor",
    });
}
```

这段代码最容易被误读成“Runtime 其实也依赖 Editor，所以分层不纯”。但它真正表达的是另一件事：

- Runtime 是**同一份代码库**，要同时支持 game/runtime build 和 editor build；
- 当目标是 editor build 时，它会额外挂上一些 editor-only 支持能力；
- 这些依赖是**条件性的**，而不是 Runtime 的常驻定义。

所以正确理解不是“Runtime = Editor 模块”，而是：**Runtime 仍然是底座，只是在 editor 构建下被允许感知少量 editor 支撑模块。**

这也解释了为什么 `AngelscriptRuntime` 在 `.uplugin` 里仍然是 `Runtime` 类型：它的默认身份没有变，只是为了在编辑器里更完整地工作而开了条件依赖。

## `AngelscriptEditor`：对 Runtime 的补壳层，依赖方向最明确

`AngelscriptEditor.Build.cs` 的 public 依赖里，有一条最关键的线：

- `AngelscriptRuntime`

而且它周围围着的几乎全是典型 editor-only 模块：

- `UnrealEd`
- `EditorSubsystem`
- `BlueprintGraph`
- `Kismet`
- `DirectoryWatcher`
- `AssetTools`
- `Slate` / `SlateCore`

这很准确地定义了 Editor 模块的依赖面：

- 它不是从零开始提供脚本系统；
- 它是拿着 Runtime 已经存在的类、引擎状态、脚本资产包和 reload 结果，再接上编辑器的图形界面、蓝图、目录监听、内容浏览器和资产工具设施。

再看它的 private 依赖：

- `LevelEditor`
- `PlacementMode`
- `PropertyEditor`
- `ContentBrowser`
- `ContentBrowserData`
- `ToolMenus`
- `ToolWidgets`

这和我们前面写过的 `1.3` 小节完全对应：`AngelscriptEditor` 的工作几乎全部是在 Editor 世界里，把 Runtime 的权威状态转成菜单、面板、Data Source、重实例化和源码导航这些可交互表面。

所以这条依赖面本身就已经证明：**Editor 是 Runtime 的补壳层，而不是平行的第二核心。**

## `AngelscriptTest`：验证层直接依赖 Runtime，在 editor 构建里再借 Editor 能力

`AngelscriptTest.Build.cs` 的关键点有两个：

### 常驻 public 依赖

- `Core`
- `CoreUObject`
- `Engine`
- `GameplayTags`
- `Json`
- `JsonUtilities`
- `AngelscriptRuntime`

### editor 条件 private 依赖

- `CQTest`
- `Networking`
- `Sockets`
- `UnrealEd`
- `AngelscriptEditor`

这里最值得注意的是：`AngelscriptTest` 并没有把 `AngelscriptEditor` 放进 public 依赖，而只是在 editor build 时私有依赖它。这表达了非常清晰的层次判断：

- 测试模块真正长期要验证的对象是 Runtime
- 但在 editor 环境下，它又需要借用 Editor 提供的一些设施来做更完整的验证

这和我们前面写过的测试分层也完全一致：

- 很多 `Angelscript.TestModule.*` case 本质上是在验证 Runtime 行为
- 但像 `Debugger`、`SourceNavigation`、`Dump command` 这种场景，又必须借 editor process / UnrealEd / editor helper 才能成立

因此 `AngelscriptTest` 的依赖面可以概括成：**默认面向 Runtime，按需借 Editor。**

## Public vs Private 也在表达边界

这一节还不能只看“依赖了谁”，还要看依赖是 public 还是 private。因为这意味着模块想对外暴露什么耦合。

当前三模块有一个很有意思的结构：

- `AngelscriptEditor` 把 `AngelscriptRuntime` 放在 public 依赖里
- `AngelscriptTest` 也把 `AngelscriptRuntime` 放在 public 依赖里
- 但 `AngelscriptTest` 对 `AngelscriptEditor` 只保留 private 依赖

这说明：

- Runtime 的存在是 Editor/Test 对外可见接口的一部分；
- Editor 对 Test 来说只是内部实现辅助，而不是 Test 对外语义的一部分。

换句话说，`Test` 不想把 “我依赖 Editor” 暴露成自己的公共面，它只是为了实现而在内部借用 Editor。

这是一种很值得注意的边界控制：**真正稳定的公共中心是 Runtime，不是 Editor。**

## 依赖面反推出的架构层次

如果只看三份 `Build.cs`，其实已经能反推出当前插件的层次结构：

### 第一层：Runtime 基础层

- 能单独成立
- 没有对 Editor/Test 的反向依赖
- 依赖范围最广，承担真正的能力聚合

### 第二层：Editor 接缝层

- 明确公开依赖 Runtime
- 自己额外挂接一批 editor-only 设施
- 把 Runtime 状态翻译成 Editor 行为

### 第三层：Test 验证层

- 明确公开依赖 Runtime
- 在 editor build 下私有依赖 Editor
- 说明验证层主要围绕 Runtime，但会借 Editor 设施做更真实的场景验证

也就是说，这三份 `Build.cs` 的依赖方向和上一节 `.uplugin` 的装载角色是彼此强化的，而不是各说各话。

## 为什么这一节要和 `1.4.3` 分开

当前 Runtime 的 private 依赖里已经出现了：

- `StructUtils`
- `EnhancedInput`
- `GameplayAbilities`

但这一节不应该把这些外部插件关系展开太多，因为它回答的是：**插件内部三模块彼此如何耦合。**

外部插件依赖是下一节 `1.4.3` 的主题。这里最多只需要看出一件事：

- 三个插件子模块的内部依赖方向已经清楚；
- 至于它们共同依赖哪些外部插件，再下一节单独拆，会更清楚。

因此这一节的重点始终是 **Runtime / Editor / Test 三者之间的依赖面**，而不是把所有引擎/插件依赖一口气讲完。

## 这条依赖边界应该怎么记

如果把当前三模块的依赖面压成一句工程化判断，可以这样记：

**`AngelscriptRuntime` 是唯一的依赖中心；`AngelscriptEditor` 直接公开依赖 Runtime，再把 editor-only 设施挂上去；`AngelscriptTest` 直接公开依赖 Runtime，并在 editor 构建里私有借用 Editor，形成“Runtime 为底、Editor 为壳、Test 为验证层”的单向依赖图。**

换成更实用的阅读过滤器就是：

- 看到 Editor/Test 都依赖 Runtime → 说明 Runtime 是公共能力中心
- 看到 Test 只私有依赖 Editor → 说明 Editor 只是验证层的辅助，不是它的公共中心
- 看到 Runtime 的 editor 条件依赖 → 把它理解成 editor build 兼容面，而不是 Runtime 身份变化

## 小结

- `AngelscriptRuntime` 不反向依赖 `AngelscriptEditor` 或 `AngelscriptTest`，是当前插件唯一的基础能力层
- `AngelscriptEditor` 公开依赖 `AngelscriptRuntime`，并叠加 UnrealEd、BlueprintGraph、DirectoryWatcher、ContentBrowser 等 editor-only 能力，体现“对 Runtime 的补壳”角色
- `AngelscriptTest` 公开依赖 `AngelscriptRuntime`，并在 editor build 下私有依赖 `AngelscriptEditor`，体现“围绕 Runtime 做验证，按需借 Editor” 的分层
- Public/Private 与条件依赖一起构成了一条很清晰的单向依赖图：Runtime 为中心，Editor/Test 向上叠，而不是彼此交叉反向耦合
