# 默认组件与组件覆盖的构造拓扑

> **所属模块**: 脚本类生成机制 → Default Component Composition / Override Resolution
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`

这一节真正要钉死的，不是 `UASClass` 里有 `DefaultComponents` / `OverrideComponents` 两个数组，而是当前脚本类是怎样把“组件声明”折成一套可构造、可继承、可覆盖、可热重载的拓扑规则。当前实现并不是简单地在 actor 构造时 `CreateDefaultSubobject` 几个组件，而是先在类生成阶段把带 `DefaultComponent` / `OverrideComponent` 元数据的属性抽成声明表，再在 `StaticActorConstructor` 里按“先 override、再跑 C++ 构造、再自上而下创建默认组件、最后回填 override 变量”的顺序执行。这条顺序决定了父类/子类组件、root component、attach 关系和热重载更新类版本时都能维持稳定语义。

## 先看这套拓扑的两层表示

在 `ASClass.h` 里，组件拓扑首先被压成两种声明结构：

- `FDefaultComponent`
- `FOverrideComponent`

`FDefaultComponent` 记录：

- `ComponentClass`
- `ComponentName`
- `VariableOffset`
- `bIsRoot`
- `bEditorOnly`
- `Attach`
- `AttachSocket`

`FOverrideComponent` 则记录：

- `ComponentClass`
- `OverrideComponentName`
- `VariableName`
- `VariableOffset`

这已经说明当前实现把组件语义拆成了两层：

- **默认组件声明**：我要在类实例化时主动创建哪些组件，以及它们的附着/根组件规则
- **覆盖声明**：我要把继承链上某个已有组件的实际类型替换成什么，并把它回填到哪一个脚本变量

因此组件拓扑在当前架构里不是散落在构造代码里的临时 if/else，而是类定义的一部分。

## 类生成阶段先把组件元数据抽成声明表

真正把脚本属性变成组件声明的是 `FinalizeActorClass(...)`。它会在 finalize 阶段遍历 `ClassDesc->Properties`，专门寻找：

- `NAME_Actor_DefaultComponent`
- `NAME_Actor_OverrideComponent`

对于 `DefaultComponent` 属性，它会构造 `UASClass::FDefaultComponent` 并提取：

- 组件类
- 变量偏移
- 是否 root component
- 是否 editor-only
- attach 目标与 attach socket

```cpp
Comp.ComponentClass = Property->PropertyType.GetClass();
Comp.ComponentName = *Property->PropertyName;
Comp.VariableOffset = Property->ScriptPropertyOffset;
Comp.bIsRoot = Property->Meta.Contains(NAME_Actor_RootComponent);
Comp.bEditorOnly = Property->Meta.Contains(NAME_Meta_EditorOnly);
Comp.Attach = ...;
Comp.AttachSocket = ...;
```

对于 `OverrideComponent` 属性，则会构造 `FOverrideComponent`：

```cpp
Comp.ComponentClass = Property->PropertyType.GetClass();
Comp.OverrideComponentName = *Property->Meta[NAME_Actor_OverrideComponent];
Comp.VariableName = *Property->PropertyName;
Comp.VariableOffset = Property->ScriptPropertyOffset;
```

这一步的意义很重要：**组件构造逻辑并不是在实例化时临时扫反射元数据，而是先在类 finalize 阶段把这些元数据编译成 `UASClass` 上的稳定声明表。**

## `FinalizeActorClass()` 还承担了大量组件声明合法性验证

`FinalizeActorClass(...)` 不只收集数据，它还在类还没真正变成可实例化 actor 前就先做了一轮验证：

- `DefaultComponent` 必须真的是 `UActorComponent` 子类
- 非抽象 actor 上不能声明抽象组件类
- 带 `Attach` 的组件必须是 `USceneComponent`
- `RootComponent` 也必须是 `USceneComponent`
- 继承链里只能有一个 root component

例如：

```cpp
if (Comp.ComponentClass == nullptr || !Comp.ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    ScriptCompileError(... "was marked as DefaultComponent, but is not a type of component")

if (Comp.Attach != NAME_None && !Comp.ComponentClass->IsChildOf(USceneComponent::StaticClass()))
    ScriptCompileError(... "has a component attach set, but is not a type of scene component")
```

这说明当前组件拓扑并不是“尽量创建，运行时再报错”，而是在类生成阶段就尽量把错误前置成 compile error。这也正是为什么这个主题和 `2.1.7 类最终化、默认对象初始化与验证边界` 会紧邻——组件声明验证本身就是 finalization 的一部分。

## `OverrideComponent` 解析会沿脚本父类链和 C++ 父类链双向查找

对 `OverrideComponent` 的验证尤其能说明当前实现的“拓扑感知”。它不会只在当前类里找同名组件，而是：

- 优先沿脚本父类链上溯，检查 `ParentASClass->DefaultComponents`
- 如果脚本父类链没找到，再到父 C++ 类的 CDO 组件列表里找
- 如果 CDO 组件里也找不到，还会遍历 instanced reference 属性，处理抽象组件不会出现在 `GetComponents()` 列表中的情况

而且就算找到了，还要验证：

- 新的 `OverrideComponent.ComponentClass` 必须继承自被覆盖的基类组件类型

这说明 `OverrideComponent` 不是简单的“同名替换”，而是一条严格受继承关系约束的 override 规则。也因此它被建模成单独的声明结构，而不是塞进 `DefaultComponents` 里混着处理。

## 运行时构造顺序第一步：先应用 override，再跑原生父类构造

`StaticActorConstructor(...)` 的顺序非常关键：

```cpp
// Apply override components
ApplyOverrideComponents(Initializer, Actor, Class);

// We need to run the C++ constructor first so everything is valid
Class->CodeSuperClass->ClassConstructor(Initializer);
```

`ApplyOverrideComponents(...)` 做的事情并不复杂，但它的位置很讲究：

- 先遍历当前类的 `OverrideComponents`
- 如有必要把 `ComponentClass` 更新到 `GetMostUpToDateClass()`
- 调 `Initializer.SetDefaultSubobjectClass(Override.OverrideComponentName, ComponentClass)`
- 然后再递归父脚本类，继续应用更上层的 override

这说明 override 的真实作用不是“构造完再替换组件实例”，而是：**在 C++ 父类真正创建默认子对象之前，就把对应 subobject class 绑到 `FObjectInitializer` 上。**

也就是说，当前 override 是构造期 override，不是后处理 override。

## 为什么 `ApplyOverrideComponents()` 要递归脚本父类

`ApplyOverrideComponents(...)` 在处理完当前类之后，会继续：

```cpp
if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
{
    ApplyOverrideComponents(Initializer, Actor, ParentClass);
}
```

这意味着当前 override 规则不是“只有最派生类的 override 有效”，而是整个脚本继承链上的 override 都会在构造开始前被应用到 `FObjectInitializer`。它解决的是这样一类场景：

- 父脚本类 override 了某个 C++ 组件
- 子脚本类再基于这个结果继续声明自己的默认组件

因此 override 拓扑的正确理解是：**它沿脚本继承链累积进 `FObjectInitializer`，而不是在某个单点被一次性决定。**

## 运行时构造顺序第二步：`CreateDefaultComponents()` 按父类优先原则 materialize 组件树

真正的默认组件实例化发生在 `CreateDefaultComponents(...)`，它也先做了一条很关键的递归：

```cpp
if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
{
    CreateDefaultComponents(Initializer, Actor, ParentClass);
}
```

这说明默认组件的构造拓扑是：**父脚本类先创建自己的默认组件，子脚本类后创建自己的默认组件。**

之后才遍历当前类的 `DefaultComponents`，逐个：

- 根据 `ComponentClass` 决定真实组件类（热重载时会先 `GetMostUpToDateClass()`）
- 对 editor-only 组件用 `CreateEditorOnlyDefaultSubobject(...)`
- 否则用 `CreateDefaultSubobject(...)`
- 把生成出来的组件实例写回脚本对象体中对应 `VariableOffset`

这说明脚本变量和默认子对象之间并不是通过名字延迟绑定，而是直接通过生成阶段算好的偏移量在运行时回填。组件拓扑因此既是“对象树”，也是“脚本对象内存中的字段回填图”。

## 附着规则：root、自动附着和延迟附着是三种不同路径

`CreateDefaultComponents(...)` 在创建出 `USceneComponent` 后，并不是统一地 `SetupAttachment(...)`，而是分三类情况：

### 1. `bIsRoot`

- 把当前组件设为 root
- 原先已有 root 的话，再把旧 root 挂到新 root 上

### 2. `Attach == NAME_None`

- 如果 actor 还没有 root，就让当前组件成为 root
- 否则自动附着到现有 root

### 3. 显式 `Attach`

- 先把该组件记入 `DelayedComponentAttach`
- 等本类所有组件都创建完，再按名字去 `Actor->GetComponents()` 里找 attach 目标
- 找不到目标时再退回 root 或直接成为 root

这说明当前实现非常明确地区分了：

- **根组件声明**
- **没有显式 attach 目标的默认附着**
- **依赖同批其他组件存在的延迟附着**

因此“组件构造拓扑”这个说法是准确的：它不是平铺创建，而是按 root/attach 关系组织成一张有顺序的树。

## 变量回填有两次：默认组件创建一次，override 组件再补一次

运行时回填脚本变量其实发生两轮：

### 第一轮：默认组件创建时

```cpp
UActorComponent** VariablePtr = (UActorComponent**)((SIZE_T)Actor + DefaultComponent.VariableOffset);
*VariablePtr = Component;
```

### 第二轮：所有组件都创建完后，对 override 组件变量再补一次

```cpp
for (auto* CheckComponent : Actor->GetComponents())
{
    if (CheckComponent->GetFName() == Override.OverrideComponentName)
    {
        *VariablePtr = CheckComponent;
        break;
    }
}
```

这说明 `OverrideComponent` 的变量语义不是“再创建一个组件”，而是：**把变量指向继承链上那个已被 override 后的真实 subobject 实例。**

也就是说，当前拓扑里“组件实例树”和“脚本变量引用图”虽然相关，但不是一回事；override 变量回填这一轮，就是把两者重新对齐。

## 热重载协作：组件类和 attach 语义都会追最新版本

无论是在 `ApplyOverrideComponents(...)` 还是 `CreateDefaultComponents(...)` 里，当前实现都会对 `ComponentClass` 做：

```cpp
UASClass* asClass = Cast<UASClass>(ComponentClass);
if (asClass != nullptr)
    ComponentClass = asClass->GetMostUpToDateClass();
```

这说明组件构造拓扑并不是静态写死的，而是显式接入了热重载版本链。也就是说：

- 如果组件类型本身也是脚本类
- 那么在 actor/component 构造过程中
- 会自动追到该组件类的最新版本，再用于 override 或 default component 创建

这一步非常关键，因为它保证：**热重载之后，新创建的脚本 actor 不会继续实例化旧版本脚本组件类。**

因此当前组件拓扑不是独立主题，而是 `NewerVersion` / `GetMostUpToDateClass()` 版本链在组件子树上的一个具体落点。

## `CLASS_HasInstancedReference`：组件声明还会反向影响类级 GC/引用语义

在 `FinalizeActorClass(...)` 里，无论加的是 `DefaultComponent` 还是 `OverrideComponent`，最后都会：

```cpp
ASClass->ClassFlags |= CLASS_HasInstancedReference;
```

这说明组件拓扑不仅影响构造，还会反向影响这个脚本类在 UE 世界里的引用语义。换句话说，当前实现已经把“类拥有 instanced component 引用”作为一种类级事实编码进 `ClassFlags`。这再次证明组件拓扑并不是构造细节，而是类定义的一部分。

## 这条组件构造拓扑应该怎么记

如果把 `2.1.6` 压成一句工程化判断，可以这样记：

**当前脚本 actor 的组件体系不是在运行时临时拼出来的，而是先在 `FinalizeActorClass(...)` 中把带元数据的属性编译成 `DefaultComponents` / `OverrideComponents` 声明表，再在 `StaticActorConstructor` 中按“先 override，后原生父类构造，再自上而下创建默认组件，最后回填 override 变量”的顺序 materialize 成一棵可继承、可热重载、可附着的组件子树。**

换成更实用的阅读过滤器就是：

- 看声明结构长什么样 → `FDefaultComponent` / `FOverrideComponent`
- 看这些结构何时被填充 → `FinalizeActorClass(...)`
- 看 override 何时真正生效 → `ApplyOverrideComponents(...)`
- 看默认组件何时真正实例化/附着 → `CreateDefaultComponents(...)`
- 看热重载如何影响组件类型 → `GetMostUpToDateClass()` 在组件类上的应用

## 小结

- 组件构造拓扑的第一层不是运行时实例化，而是类生成阶段：`FinalizeActorClass(...)` 先把脚本属性元数据编译成 `DefaultComponents` / `OverrideComponents` 声明表，并在此阶段完成大量合法性验证
- 运行时 `StaticActorConstructor(...)` 先通过 `ApplyOverrideComponents(...)` 把 override 规则写进 `FObjectInitializer`，再跑原生构造，最后 `CreateDefaultComponents(...)` 按父类优先、root/attach 分流和延迟附着规则 materialize 组件树
- `OverrideComponent` 并不新建组件，而是把脚本变量回填到继承链上已被 override 后的真实 subobject 实例；默认组件和 override 变量因此构成了“组件实例树”和“脚本变量引用图”的两层映射
- 组件类在构造时也会追 `GetMostUpToDateClass()`，因此这套拓扑天然接入了热重载版本链，而不是独立于重载体系之外的特殊逻辑
