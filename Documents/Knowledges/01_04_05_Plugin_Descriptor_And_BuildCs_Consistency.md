# `.uplugin` `Plugins` 声明与 `Build.cs` 依赖的一致性约束

> **所属模块**: 插件模块清单与装载关系 → Descriptor / Build.cs Consistency
> **关键源码**: `Plugins/Angelscript/Angelscript.uplugin`, `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`, `Plugins/Angelscript/Source/AngelscriptEditor/AngelscriptEditor.Build.cs`, `Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTest.Build.cs`, `Documents/Knowledges/01_04_03_External_Plugin_Relationships.md`

这一节真正要钉死的，不是 `.uplugin` 里列了几个插件、`Build.cs` 里又写了几个模块这么表面的事实，而是这两层配置各自回答什么问题，以及为什么它们必须保持一致。当前仓库里，`.uplugin` 的 `Plugins` 段和 `Build.cs` 的 `DependencyModuleNames` 同时存在，不是重复配置，而是两层不同的契约：前者是**插件级可用性契约**，后者是**模块级编译/链接契约**。只有两层对齐，`Angelscript` 插件的外部依赖边界才是完整的；只满足其中一层，就会出现“声明上允许，但编译/链接层没接上”或者“代码上依赖了某模块，但插件级前提没表达出来”的脱节。

## `.uplugin` `Plugins` 段回答的是插件级前提

`Angelscript.uplugin` 当前的 `Plugins` 声明很明确：

```json
"Plugins": [
  { "Name": "StructUtils", "Enabled": true },
  { "Name": "EnhancedInput", "Enabled": true },
  { "Name": "GameplayAbilities", "Enabled": true }
]
```

这层声明的意义，不是告诉编译器“请链接这些模块”，而是告诉插件管理与装载层：

- 当前 `Angelscript` 插件把这三个外部插件当作前提；
- 当 `Angelscript` 被启用时，这些插件也应处于启用/可用状态；
- 这是一条**插件对插件**的依赖关系，而不是模块对模块的链接表。

因此 `.uplugin` 里的 `Plugins` 段更像是一份“宿主环境假设清单”：它声明的是**我要依赖哪些插件存在**，而不是**我当前源文件里 include 了哪些模块头文件**。

## `Build.cs` 回答的是模块级编译与链接契约

对照来看，`Build.cs` 里的依赖列表回答的是另一层问题：

- 当前模块在编译时需要哪些模块的头文件、符号和链接结果；
- 这些依赖是 public 还是 private；
- 它们是否只在 editor 构建里条件成立。

例如 `AngelscriptRuntime.Build.cs` 当前就写了：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    ...
    "StructUtils",
});

PrivateDependencyModuleNames.AddRange(new string[]
{
    ...
    "EnhancedInput",
    "GameplayAbilities",
    "GameplayTasks",
});
```

这段代码的语义是：

- `StructUtils` 模块进入 Runtime 的 public 编译/链接表面；
- `EnhancedInput`、`GameplayAbilities`、`GameplayTasks` 是 Runtime 内部实现依赖；
- 它们服务的是模块级可编译性和可链接性，而不是插件管理层的启停关系。

因此这层配置处理的是**模块对模块**的依赖，而不是插件对插件的依赖。

## 两层不是重复，而是上下游契约

把这两层对起来看，当前仓库的结构其实很清楚：

- `.uplugin` 说：我依赖 `StructUtils` / `EnhancedInput` / `GameplayAbilities` 这三个插件存在
- `AngelscriptRuntime.Build.cs` 说：在这些插件存在的前提下，我具体会链接它们暴露出来的哪些模块

这意味着两者的关注点完全不同：

### `.uplugin` 关注

- 哪些插件必须被启用
- 插件级装载关系是否闭合
- 宿主项目拿到这个插件时，需要满足什么外部前提

### `Build.cs` 关注

- 当前模块真正用到了哪些模块
- 这些模块是 public 还是 private
- 哪些依赖只在 editor build 下成立

所以这不是“双写一份同样的东西”，而是**插件级前提**和**模块级消费**两张表必须互相对得上。

## 当前仓库里最清楚的一致性例子

就这三个外部插件而言，当前仓库已经展示出一条比较干净的一致性链：

### `StructUtils`

- `.uplugin` 已声明 `StructUtils` 插件启用
- `AngelscriptRuntime.Build.cs` 把 `StructUtils` 放进 public 依赖
- 代码层明确存在 `FInstancedStruct` 触点，例如：
  - `FAngelscriptAnyStructParameter`
  - `FAngelscriptDelegateWithPayload`
  - `Bind_FInstancedStruct.cpp`

这说明这里三层是闭合的：

- 插件级前提已声明；
- 模块级依赖已接上；
- 代码层确实在消费。

### `EnhancedInput`

- `.uplugin` 已声明 `EnhancedInput` 插件启用
- `AngelscriptRuntime.Build.cs` 把 `EnhancedInput` 放进 private 依赖
- 代码层当前显式专用包装较轻，但它至少作为运行时互操作前提已经被纳入依赖表面

这说明这里虽然代码触点弱于 `StructUtils`，但“插件前提”和“模块依赖”仍然是对齐的。

### `GameplayAbilities`

- `.uplugin` 已声明 `GameplayAbilities` 插件启用
- `AngelscriptRuntime.Build.cs` 把 `GameplayAbilities` 放进 private 依赖
- 代码层存在明确的 `UAngelscriptAbilitySystemComponent` 包装层

这同样是一条插件级前提和模块级消费闭合的链。

也就是说，当前仓库并不是“随便在 `.uplugin` 里列几个插件，再在 `Build.cs` 里碰巧写几个同名模块”，而是在维持一条**声明层 → 编译层 → 代码层**的一致性闭环。

## 什么叫“不一致”

这一节最重要的不是只看当前做对了什么，还要知道什么叫没对齐。就当前架构语义来说，至少有两种典型脱节：

### 1. `.uplugin` 里启用了插件，但 `Build.cs` 根本不消费它

这种情况的含义通常是：

- 这个插件前提已经被声明为宿主要求；
- 但模块级代码并没有真正用到它暴露出来的模块；
- 最终会让依赖关系看起来比实际更重，形成“死声明”或“预留但未兑现”的边界噪音。

### 2. `Build.cs` 里消费了某插件模块，但 `.uplugin` 没有声明对应插件

这种情况的含义则相反：

- 代码层已经把某个外部插件当成事实依赖；
- 但插件级前提没有在 descriptor 中说清；
- 最终会让“宿主启用了插件但环境前提未显式表达”这件事变得隐蔽。

就架构文档来说，这两种情况都会让“依赖边界”失真：

- 前者让声明层重于事实层；
- 后者让事实层重于声明层。

所以一致性约束的核心不是形式美观，而是：**让宿主前提、构建依赖和代码事实三层互相印证。**

## 为什么 `GameplayTasks` 不需要出现在 `.uplugin` `Plugins` 段

当前 `AngelscriptRuntime.Build.cs` 里还有一个很容易误判的名字：`GameplayTasks`。它和 `GameplayAbilities` 一起出现在 private 依赖里，但 `.uplugin` 的 `Plugins` 段并没有单独列一个 `GameplayTasks` 插件。

这正好说明 `.uplugin` 和 `Build.cs` 对齐时，比较对象并不是“模块名逐字一一相等”，而是：

- `.uplugin` 处理的是**插件级前提**；
- `Build.cs` 处理的是**模块级消费**。

因此判断一致性时，要问的是：

- 这个模块属于哪个插件或哪个引擎内建模块？
- 当前是否已经在插件级把需要显式启用的外部插件声明出来了？

也就是说，一致性约束不是做字符串去重，而是做**层级对齐**。

## 这条一致性边界为什么值得单独成文

前面的 `1.4.1` 到 `1.4.4` 已经分别讲了：

- 模块声明与装载阶段
- 三模块内部依赖面
- 外部插件关系强弱
- Runtime 的条件依赖边界

但如果不再单独补上这一节，就仍然缺一层很关键的治理规则：

- 当外部依赖进入插件后，**宿主看到的 descriptor 前提** 和 **编译器看到的 module dependency** 必须同时保持一致。

这一层特别像“依赖关系的文法规则”：前面几节在描述词汇，这一节在描述这些词汇如何合法组合。也正因此，它适合作为 `1.4` 章节补充审查后追加出来的紧邻主题，而不是混进前面某一节里一笔带过。

## 这条一致性约束应该怎么记

如果把当前 `.uplugin` 与 `Build.cs` 的一致性规则压成一句工程化判断，可以这样记：

**`.uplugin` 的 `Plugins` 段负责声明“我依赖哪些外部插件存在”，`Build.cs` 负责声明“我具体消费这些插件暴露出的哪些模块”；两者必须在同一组事实依赖上对齐，才能让插件级前提、模块级链接和代码级用法形成闭环。**

换成更实用的检查器就是：

- 看到 `.uplugin` 里列出的外部插件 → 去 `Build.cs` 看是否真有对应模块消费
- 看到 `Build.cs` 里来自外部插件的模块名 → 反查 `.uplugin` 是否已声明对应插件前提
- 再往下看代码层，确认这些依赖不是死声明、死链接或隐式依赖

## 小结

- `.uplugin` 的 `Plugins` 段和 `Build.cs` 的依赖列表不是重复配置，而是插件级前提与模块级消费两层不同契约
- 当前 `Angelscript` 插件在 `StructUtils`、`EnhancedInput`、`GameplayAbilities` 三条依赖上已经形成了“descriptor 前提 + module 依赖 + 代码触点”的闭环
- 一致性问题的本质不是字符串是否完全相同，而是插件级前提、模块级链接和代码事实是否表达同一组依赖关系
- 这条规则值得单独成文，因为它决定了外部依赖关系在仓库里是清晰可维护，还是逐渐演化成隐式前提与死声明并存的模糊边界
