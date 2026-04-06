# 类最终化、默认对象初始化与验证边界

> **所属模块**: 脚本类生成机制 → Class Finalization / Default Object Initialization / Verification Boundaries
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Documents/Knowledges/02_01_02_AngelscriptClassGenerator_Creation_And_Reload_Pipeline.md`, `Documents/Knowledges/02_01_06_Default_Component_Composition_And_Override_Resolution.md`

这一节真正要钉死的，不是 `FinalizeClass()` 之后还有几个辅助函数，而是当前类生成链在“创建/重载完成”与“类真正可用”之间其实还切了三道边界：**最终化（finalization）**、**默认对象初始化（default object initialization）**、**验证（verification）**。当前实现并不是在 `DoFullReloadClass()` 结束后就把类扔给 UE 世界，而是先通过 `FinalizeClass()` 补齐类级接口、组件/接口/注册信息，再通过 `CallPostInitFunctions()` 与 `InitDefaultObjects()` 统一建立后置初始化和 CDO，最后再由 `VerifyClass()` 做 actor/component 级的收尾验证。也就是说，这一段更像“类落地稳定化”而不是“再跑几步杂项逻辑”。

## 先看这三层边界在主链路里的位置

当前 `PerformReload()` 里，这三层边界的顺序是明确分开的：

```text
DoFullReload / DoSoftReload
    -> FinalizeClass
        -> FinalizeActorClass / FinalizeComponentClass / FinalizeObjectClass
    -> CallPostInitFunctions
    -> InitDefaultObjects
        -> InitDefaultObject
    -> VerifyClass
```

这条顺序非常重要，因为它意味着：

- 先把类定义本身补齐到“可注册/可反射/可构造”状态
- 再让需要 post-init 的脚本/字面量资源跑起来
- 再创建 CDO 和默认对象树
- 最后才基于真实默认对象状态去做验证

因此这三层边界不是重复劳动，而是一种严格分工：**finalize 决定“类是什么”，init default object 决定“类的默认实例长什么样”，verify 决定“这个默认实例与类声明是否一致”。**

## 第一层：`FinalizeClass()` 负责把“类壳”变成“可接入 UE 世界的类”

`FinalizeClass(FModuleData&, FClassData&)` 是真正的总入口。它先做几件类级收束动作：

- `ClassData.bFinalized = true;`
- `NewClass->SetUpRuntimeReplicationData();`
- 解析 `ComposeOntoClass`
- 解析并补齐 implemented interfaces
- 对 interface class 走特殊 finalize 路径
- 对 actor/component/object 走三条不同 finalize 分支
- 最后 `NotifyRegistrationEvent(...)` 正式告诉加载系统这个类已经存在

```cpp
ClassData.bFinalized = true;
NewClass->SetUpRuntimeReplicationData();
...
NotifyRegistrationEvent(TEXT("/Script/Angelscript"), *NewClass->GetName(),
    ENotifyRegistrationType::NRT_Class,
    ENotifyRegistrationPhase::NRP_Finished, nullptr, false, NewClass);
```

这说明 `FinalizeClass()` 的核心职责不是再改类定义，而是：**把已经 materialize 好的类切换到“运行时/编辑器都能认它”的状态。**

尤其是 `NotifyRegistrationEvent(...)` 很能说明这一点——这是类正式进入加载/注册系统的边界，而不是类内部某个字段赋值。

## implemented interfaces 的补齐说明 finalize 还要解决“接口世界”

`FinalizeClass()` 的一大块逻辑都在处理 `ImplementedInterfaces`：

- 先通过脚本类描述、AS type system 或 `TObjectIterator<UClass>` 找到 `UInterface`
- 再递归加入 base interfaces 和 interface 自己实现的 interfaces
- 最终把它们放进 `NewClass->Interfaces`
- 然后检查实现类是否真的提供了接口要求的全部方法

这说明 interface 相关工作并不是在类创建时就完全解决，而是属于 finalization 边界的一部分：**类壳已经有了，但它是不是一个语义完整的“实现了这些接口的 UE 类”，要到 `FinalizeClass()` 才最终成立。**

因此这一步更多是在补“行为契约”，而不是“内存布局”。

## 第二层：`FinalizeActorClass` / `FinalizeComponentClass` / `FinalizeObjectClass` 把最终化再按类族切开

`FinalizeClass()` 本身并不直接处理所有具体类，而是再切成三条 finalize 分支：

- `FinalizeActorClass(...)`
- `FinalizeComponentClass(...)`
- `FinalizeObjectClass(...)`

这说明当前系统认为“类最终化”不是单一动作，而是至少要按 UObject 家族语义分层。

### `FinalizeActorClass(...)`

它做的事最重，前一节已经展开了一大半：

- 指定 `ClassConstructor = &UASClass::StaticActorConstructor`
- 扫描并验证 `DefaultComponents` / `OverrideComponents`
- 赋值 `CLASS_HasInstancedReference`

也就是说，actor class 的 finalization 包含了**组件拓扑编译**，而这一步必须发生在默认对象创建前。

### `FinalizeComponentClass(...)`

这条路径相对轻，但同样非常明确：

- 把 `ClassConstructor` 指向 `UASClass::StaticComponentConstructor`
- editor 下对可放置组件标记 `BlueprintSpawnableComponent`

因此 component finalization 的重点是：**把组件类接到 UE 组件构造和 editor 可放置语义上。**

### `FinalizeObjectClass(...)`

对象类的 finalize 最简单：

- `ClassConstructor = &UASClass::StaticObjectConstructor`

它的意义在于：object 类同样需要一个稳定的 script-aware 构造入口，但不需要 actor/component 那些额外拓扑或 editor 语义。

这三条分支一起说明：`FinalizeClass()` 的“边界”其实不是一个函数，而是一套按类族分发的**最终化协议**。

## interface class 有一条提前返回的 finalize 特例

`FinalizeClass()` 里对 `ClassDesc->bIsInterface` 还有一条很关键的特例：

- interface class 不走 actor/component finalization
- 直接 `FinalizeObjectClass(ClassDesc)`
- 立刻 `NotifyRegistrationEvent(...)`
- 然后 `return`

这说明当前 finalization 还会处理一种“中途收束”的边界：**并不是所有类都要走完整的 actor/component/object 分发树，interface class 在完成接口类最小 finalization 后就可以直接结束。**

因此当前最终化体系并不是单纯按继承树划分，而是同时按“类的语义类别”划分。

## `CallPostInitFunctions()`：在 CDO 创建前执行全局 post-init 脚本

finalization 之后并不会立刻创建 CDO，当前实现先跑了一段 `CallPostInitFunctions()`：

- 遍历每个 `ModuleData.NewModule->PostInitFunctions`
- 在对应 `ScriptModule->globalFunctionList` 里找到同名全局函数
- `PrepareAngelscriptContextWithLog(...)`
- `Context->Execute()`

这段逻辑的注释也很关键：

- “Ensure that all literal assets have been created now that we can”

这说明 `CallPostInitFunctions()` 的定位不是“普通默认值逻辑”，而是：**在创建所有类的默认对象之前，先完成模块级 post-init 脚本和 literal asset 准备。**

也就是说，这是一条位于“类已经 finalized”与“默认对象开始 materialize”之间的**模块级初始化接缝**。

因此当前体系把“类本身 ready”和“模块级资源 ready”区分开了。

## 第三层：`InitDefaultObjects()` 把 finalized 类真正 materialize 成 CDO

`InitDefaultObjects()` 的逻辑虽然短，但边界很清楚：

1. 先做一轮 `InitClassTickSettings(...)` 预处理
2. 再遍历 full reload 类，逐个 `InitDefaultObject(...)`

注释已经说明了为什么要这么分：

- tick 设置必须在 CDO 创建前统一算好
- 否则 Blueprint CDO 创建时可能会连带触发其他类的 CDO 创建，而那些类的 tick 语义还没稳定

这说明默认对象初始化并不是单纯的 `GetDefaultObject(true)`，而是一条**需要先稳定 class-level 行为配置，再允许 UE 正式构造 CDO** 的流程。

所以 `InitDefaultObjects()` 的真正职责是：**把 finalize 完成但尚未 materialize 的类，安全地推进到拥有正确 CDO 的状态。**

## `InitDefaultObject()` 本身很薄，但它是一个非常明确的边界点

`InitDefaultObject(...)` 几乎只做一件事：

```cpp
NewClass->GetDefaultObject(true);
```

这恰恰说明：当前系统刻意没有把默认对象创建散落到各处，而是把“真正触发 CDO 构造”单独包进了一个函数。这样做的价值不是代码量，而是边界清晰：

- 只要进入 `InitDefaultObject(...)`
- 就说明前面的 finalize、post-init、tick 预处理都已经完成
- 现在可以正式把类 materialize 成默认对象树

因此 `InitDefaultObject()` 很像一条 **last-mile materialization boundary**。

## 为什么 `VerifyClass()` 必须放在默认对象创建之后

`PerformReload()` 里，`VerifyClass(...)` 的调用顺序在：

- `CallPostInitFunctions()`
- `InitDefaultObjects()`

之后。这不是偶然，因为 `VerifyClass()` 的很多检查都依赖真实默认对象状态。例如当前它会：

- 对 actor class：检查抽象父类中的 abstract component 是否都被 override 了
- 通过 `AActor* ActorDefaultObject = CastChecked<AActor>(ASClass->GetDefaultObject(false));` 读取真实 CDO
- 验证 `DefaultComponents` 的 attach parent 是否存在
- 验证 attach parent 是不是 `USceneComponent`
- 验证 editor-only attach parent / root component 是否与 actor 的 editor-only 语义匹配
- 验证 `CLASSMETA_NotAngelscriptSpawnable`
- 对 deprecated component 发 warning

这些检查如果放在 CDO 创建前根本做不了，因为：

- `GetDefaultObject(false)` 还不存在
- 组件实例也还没被 `StaticActorConstructor` 建出来

因此 `VerifyClass()` 的角色很明确：**它不是“再分析一次类描述符”，而是在真实默认对象树已经建立后，做一轮基于实际实例状态的收尾验证。**

## `VerifyClass()` 和 `FinalizeActorClass()` 的边界：前者检查“声明是否成立”，后者检查“实例是否真的长成那样”

这个边界特别值得单独记住：

- `FinalizeActorClass(...)` 更偏 **声明级编译**
  - 收集/校验 `DefaultComponents`、`OverrideComponents`
  - 检查组件类类型、metadata、继承约束
- `VerifyClass(...)` 更偏 **实例级验证**
  - attach 目标在真实 CDO 上是否存在
  - attach parent 是否是 `USceneComponent`
  - editor-only 根组件/附着关系在真实对象树里是否仍然成立

因此它们不是重复校验，而是前后两级：

- finalize 负责“这类声明理论上合法吗”
- verify 负责“CDO 真的按这些声明被正确建出来了吗”

这也是为什么当前 `2.1.7` 值得独立成文，而不是把 `FinalizeClass()` 顺手塞进 `2.1.2` 结尾就算讲完。

## `GetFullReloadLines()` / `WantsFullReload()` / `NeedsFullReload()` 把验证边界再向外暴露了一层

在 `InitDefaultObject()` 之后，generator 还提供了几条对外可见的 reload 诊断接口：

- `GetFullReloadLines(...)`
- `WantsFullReload(...)`
- `NeedsFullReload(...)`

它们说明当前“最终化和验证边界”并不完全关在 generator 内部。reload 的行号来源、建议级 full reload、强制级 full reload 这几层结论，还会被向外暴露给 editor/tooling。也就是说，当前最终化阶段的输出不仅是“类能用了”，还有一层**可诊断的边界信息**。

## 这条最终化/默认对象/验证边界应该怎么记

如果把 `2.1.7` 压成一句工程化判断，可以这样记：

**当前类生成链在 `DoFullReload/DoSoftReload` 之后并没有结束，而是先通过 `FinalizeClass()` 把类的接口、继承、组件和注册状态补齐，再通过 `CallPostInitFunctions()` 与 `InitDefaultObjects()` 把模块级资源和 CDO materialize 完整，最后由 `VerifyClass()` 基于真实默认对象树做 actor/component 级收尾验证。**

换成更实用的阅读过滤器就是：

- 看类什么时候真正“接入 UE 世界” → `FinalizeClass()` + `NotifyRegistrationEvent(...)`
- 看类族分支怎么不同 → `FinalizeActorClass` / `FinalizeComponentClass` / `FinalizeObjectClass`
- 看默认对象何时真正被建立 → `CallPostInitFunctions()` + `InitDefaultObjects()` + `InitDefaultObject()`
- 看哪些检查必须等 CDO 存在后才能做 → `VerifyClass()`

## 小结

- 当前类生成链在重载完成后又切成了三道边界：finalize 负责让类定义可注册、post-init/default object 初始化负责让模块级资源和 CDO 真正 materialize、verify 负责基于真实默认对象树做收尾检查
- `FinalizeClass()` 自身又按 actor/component/object/interface 分化出不同的最终化路径，因此“类最终化”本身就是一套协议，而不是单函数动作
- `InitDefaultObjects()` 之所以独立存在，是为了先稳定 tick 和 post-init 语义，再安全地触发 `GetDefaultObject(true)`
- `VerifyClass()` 不是重复分析类描述符，而是利用真实 CDO 去验证 attach 父节点、abstract component override、editor-only 组件边界等只有实例化后才能看见的问题
