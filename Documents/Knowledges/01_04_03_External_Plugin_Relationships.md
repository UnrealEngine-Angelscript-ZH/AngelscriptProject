# `StructUtils` / `EnhancedInput` / `GameplayAbilities` 等外部插件关系

> **所属模块**: 插件模块清单与装载关系 → 外部插件关系
> **关键源码**: `Plugins/Angelscript/Angelscript.uplugin`, `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptAnyStructParameter.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptDelegateWithPayload.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FInstancedStruct.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptAbilitySystemComponent.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_UInputSettings.cpp`

这一节真正要讲清楚的，不是 `.uplugin` 里顺手启用了三个外部插件，而是当前 Angelscript 插件和这些 UE 插件之间到底是什么关系。`StructUtils`、`EnhancedInput`、`GameplayAbilities` 并不是同一种依赖：`StructUtils` 已经直接渗进了插件公开运行时表面；`GameplayAbilities` 则体现为一层明确的运行时集成扩展；`EnhancedInput` 在当前仓内更多表现为启用与私有链接边界，代码层显式包装相对轻。这三种关系放在一起，正好能看出插件处理外部依赖时的边界策略：**有些依赖进入公共脚本表面，有些依赖只作为运行时扩展桥接，有些依赖则先作为兼容/可用性前提存在。**

## 先看最外层声明：这是启用的插件级依赖，不只是模块名巧合

`Angelscript.uplugin` 的 `Plugins` 段已经把三者写成了启用依赖：

```json
"Plugins": [
  { "Name": "StructUtils", "Enabled": true },
  { "Name": "EnhancedInput", "Enabled": true },
  { "Name": "GameplayAbilities", "Enabled": true }
]
```

这意味着它们不是某个 `Build.cs` 里偶然出现的模块名，而是插件描述层就已经承认的外部前提。换句话说，当前 Angelscript 插件的装载假设里已经包含：

- `StructUtils` 可用；
- `EnhancedInput` 可用；
- `GameplayAbilities` 可用。

因此这三者的关系应该理解成**插件级依赖边界**，而不只是局部模块里的一次 include 选择。

## `StructUtils`：已经进入插件公开运行时表面的类型互操作依赖

三者里最强的一条关系是 `StructUtils`。它不仅在 `AngelscriptRuntime.Build.cs` 里被放进 `PublicDependencyModuleNames`，而且已经直接出现在公开运行时类型里：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    ...
    "GameplayTags",
    "StructUtils",
});
```

这和源码里的几个触点完全对得上：

- `FAngelscriptAnyStructParameter` 公开暴露 `FInstancedStruct InstancedStruct`
- `FAngelscriptDelegateWithPayload` 公开暴露 `FInstancedStruct Payload`
- `Bind_FInstancedStruct.cpp` 直接把 `FInstancedStruct` 绑定进脚本层，并提供 `InitializeAs`、`Get`、`Contains`、`Make` 等操作

这说明 `StructUtils` 在当前插件里的角色，不是“某个内部实现用到了一个方便容器”，而是：**为脚本层提供类型擦除的 struct 容器桥。**

从架构角度看，`FInstancedStruct` 很适合脚本插件，因为它能承载：

- 不在编译期静态确定的 `UScriptStruct`
- 需要在脚本和 UE 反射边界之间流动的异构 struct 数据
- 委托 payload 或 `AnyStruct` 这类需要保持运行时类型信息的值

也正因为它已经渗进 `ANGELSCRIPTRUNTIME_API` 的公开结构体里，当前插件把 `StructUtils` 放到 `PublicDependencyModuleNames` 是合理且必要的：**调用者看到的并不只是 Angelscript 自己的包装，还能直接接触到 `FInstancedStruct`。**

## `GameplayAbilities`：明确的运行时集成扩展，而不是基础核心

`GameplayAbilities` 在 `AngelscriptRuntime.Build.cs` 里没有放到 public，而是和 `GameplayTasks` 一起放在 private：

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    ...
    "EnhancedInput",
    "GameplayAbilities",
    "GameplayTasks",
});
```

但代码触点说明它并不是“只是为了过编译”。当前仓里已经有一条很明确的 GAS 集成面：`UAngelscriptAbilitySystemComponent`。

这份头文件直接包含：

- `GameplayEffectTypes.h`
- `AbilitySystemComponent.h`

而且公开暴露了一系列 GAS 相关包装类型和 API：

- `FAngelscriptModifiedAttribute`
- `FAngelscriptInputBindData`
- `FAngelscriptAttributeChangedData`
- `UAngelscriptAbilitySystemComponent : public UAbilitySystemComponent`
- `FAbilityGivenDelegate` / `FAbilityRemovedDelegate`
- 多组 Attribute / Ability 相关 BlueprintCallable 接口

这说明 `GameplayAbilities` 在当前插件里的角色是：**一层面向脚本/蓝图的能力系统桥接扩展。**

它不是整个插件的公共底座——否则应该像 `StructUtils` 那样直接进入 public 依赖中心——但它也绝不是“暂时开着没用”。当前做法更像是：

- Angelscript Runtime 核心并不以 GAS 为前提；
- 但一旦项目启用了 GAS，插件就提供一套运行时包装层，把 ASC、Attribute、Ability 这些概念桥接给脚本环境。

因此它的边界可以理解成：**可选但很深的运行时扩展依赖。**

## `EnhancedInput`：当前更像启用/兼容边界，而不是大规模专用包装层

`EnhancedInput` 也被启用在 `.uplugin` 里，并且出现在 `AngelscriptRuntime.Build.cs` 的 private 依赖里：

- `.uplugin`：`"Name": "EnhancedInput", "Enabled": true`
- `AngelscriptRuntime.Build.cs`：`"EnhancedInput"` in `PrivateDependencyModuleNames`

但和 `StructUtils`、`GameplayAbilities` 相比，当前仓内可直接看到的显式触点要轻得多。在本轮代码扫描里：

- 没有像 `UAngelscriptAbilitySystemComponent` 这样明确的专用 `EnhancedInput` 包装类浮出来；
- 输入侧更直接暴露出来的仍是 `Bind_UInputSettings.cpp` 里对 `UInputSettings`、`FInputActionKeyMapping`、`FInputAxisKeyMapping` 这类传统输入设置的绑定；
- 没有出现大块 `UEnhancedInput...` / `FInputActionValue` 的专用桥接层。

这并不意味着 `EnhancedInput` 没用，而更像是在传递另一种边界信号：**当前插件希望在运行时环境里允许或兼容 EnhancedInput 相关类型/模块存在，但本轮源码可见触点还没有像 GAS 那样形成一条厚重的独立包装面。**

因此就当前仓库状态来说，`EnhancedInput` 更适合被理解成：

- 一个已声明的运行时互操作前提；
- 一个为输入相关绑定/反射互操作预留的依赖面；
- 而不是已经在插件内部长出大块专用适配层的独立子系统。

换句话说，它的关系比 `StructUtils` 更浅，比 `GameplayAbilities` 也更偏“边界可用性”而不是“专用包装类”。

## 三者关系的层次并不相同

把这三个依赖并排看，当前插件其实在用三种不同强度的外部关系：

### 1. `StructUtils`：公共类型表面依赖

- `.uplugin` 启用
- Runtime public 依赖
- 已进入公开 `USTRUCT` 表面
- 已有专门 binding 桥 `Bind_FInstancedStruct.cpp`

### 2. `GameplayAbilities`：深度运行时扩展依赖

- `.uplugin` 启用
- Runtime private 依赖
- 已有专门包装类 `UAngelscriptAbilitySystemComponent`
- 当前更像“可选集成扩展层”而不是插件公共底座

### 3. `EnhancedInput`：运行时兼容/预留依赖

- `.uplugin` 启用
- Runtime private 依赖
- 当前仓内显式专用包装层较少
- 更像为输入互操作和环境兼容保留的边界

这三层强度差异本身就很值得记，因为它说明：**并不是所有外部插件依赖都应该被同样对待。**

## 为什么只有 `StructUtils` 进了 public，而另外两个留在 private

这一点是当前外部依赖边界最值得单独记的一层。

从仓内证据看，当前分法很一致：

- `StructUtils` 进 public，是因为 `FInstancedStruct` 已经直接暴露在插件公开结构体和脚本绑定表面里
- `GameplayAbilities` 和 `EnhancedInput` 留在 private，是因为它们当前更多是 Runtime 内部的扩展集成前提，而不是整个插件向外暴露的公共中心

也就是说，当前作者的判断标准不是“这个插件重不重要”，而是：

- 这个依赖是否已经渗进插件的公共运行时类型面？
- 还是它主要只是 Runtime 内部为了某类集成能力而拉进来的支持模块？

从边界控制角度看，这是很合理的：**公共依赖应该只留给真正已经进入公共表面的类型系统，其他依赖尽量压在 Runtime 内部。**

## 这条外部依赖边界应该怎么记

如果把当前外部插件关系压成一句工程化判断，可以这样记：

**`StructUtils` 是已经进入 Angelscript 公共类型表面的 struct 互操作桥；`GameplayAbilities` 是一条明确的运行时能力系统扩展桥；`EnhancedInput` 则更多体现为运行时兼容与输入互操作前提，而不是当前仓内一条厚重的专用包装层。**

换成更实用的阅读过滤器就是：

- 看 `FInstancedStruct`、`AnyStruct`、payload struct 这类类型擦除桥 → 优先想到 `StructUtils`
- 看 `UAngelscriptAbilitySystemComponent`、Attribute / Ability / Spec 这些 GAS 语义桥 → 优先想到 `GameplayAbilities`
- 看输入相关依赖边界但缺少厚包装层 → 把 `EnhancedInput` 理解成当前环境/互操作前提，而不是独立子系统主线

## 小结

- 这三个外部插件都在 `.uplugin` 里被明确启用，因此它们是插件级前提，不是局部偶发依赖
- `StructUtils` 是三者里关系最强的一条：它进入了 Runtime public 依赖，也直接出现在公开 struct 和脚本绑定表面里
- `GameplayAbilities` 通过 `UAngelscriptAbilitySystemComponent` 形成了明确的运行时能力系统桥接，但当前仍被压在 Runtime private 依赖层
- `EnhancedInput` 当前更多体现为已启用的运行时兼容/互操作边界，显式专用包装层相对轻，因此它的关系强度弱于前两者
