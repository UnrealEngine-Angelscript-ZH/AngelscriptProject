# `UASClass` 核心字段与状态布局

> **所属模块**: 脚本类生成机制 → `UASClass` / Core Fields & State Layout
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`

这一节真正要钉死的，不是 `UASClass` 里“有很多字段”，而是这些字段到底把哪几类状态塞进了一起。当前 `UASClass` 不是一个普通的 `UClass` 子类，它同时承担了四类职责：记录脚本类与原生类之间的继承/热重载关系，保存脚本对象在 `UObject` 内存里的布局信息，承载脚本构造/默认值/GC/调试等运行时状态，以及把默认组件/覆盖组件这类 Actor 级声明折叠进类定义。因此读 `UASClass` 时，最重要的不是逐字段背名字，而是先把它们看成四个状态簇。

## 先看字段总图：`UASClass` 在 `UClass` 上额外挂了四层状态

从 `ASClass.h` 的字段分布可以直接看出，当前 `UASClass` 至少叠了四块数据：

- **继承 / 版本链状态**：`CodeSuperClass`、`NewerVersion`、`bHasASClassParent`、`ComposeOntoClass`
- **脚本对象布局状态**：`ContainerSize`、`ScriptPropertyOffset`、`ScriptTypePtr`、`bIsScriptClass`
- **脚本生命周期与执行状态**：`ConstructFunction`、`DefaultsFunction`、`ReferenceSchema`、`DebugValues`
- **组件声明状态**：`DefaultComponents`、`OverrideComponents`

这已经说明 `UASClass` 不是“只多加了一个 `ScriptTypePtr` 的 `UClass`”。它其实是一份**脚本类运行时描述符**：既要能被 UE 当普通 `UClass` 理解，又要能把 AngelScript 类型系统和脚本类特有状态一起装进去。

## 第一组：继承与版本链字段，回答“它从谁来、被谁替换”

这一组字段最先出现，而且很能说明 `UASClass` 的身份：

- `UClass* CodeSuperClass = nullptr;`
- `UASClass* NewerVersion = nullptr;`
- `bool bHasASClassParent = false;`
- `UClass* ComposeOntoClass = nullptr;`

它们分别回答不同的问题：

### `CodeSuperClass`

这不是普通的 `SuperStruct` 复写，而是“最近的原生父类”。`Documents/Hazelight/ScriptClassImplementation.md:69` 也明确把它解释成：

- 跳过脚本继承链后，最近的 C++ 父类

这意味着 `UASClass` 同时需要维护两种继承视角：

- UE 反射视角的 `SuperStruct`
- 脚本类构造/对象布局所需的“代码父类”视角

因此 `CodeSuperClass` 的存在，本质上是为了把 **脚本继承链** 和 **原生构造链** 拆开。

### `NewerVersion`

这个字段专门服务热重载。`UASClass::GetMostUpToDateClass()` 会沿着 `NewerVersion` 一路追到最新版本：

```cpp
if (NewerVersion == nullptr)
    return this;

UASClass* NewerClass = NewerVersion;
while (NewerClass->NewerVersion != nullptr)
    NewerClass = NewerClass->NewerVersion;
return NewerClass;
```

所以 `NewerVersion` 不是“缓存一下旧对象”，而是**把旧类挂到新类链上的热重载版本指针**。

### `bHasASClassParent`

这个布尔值看似简单，但它专门编码了“我的父类链里是否还有脚本类”。也就是说，它在告诉运行时：

- 当前类是不是纯原生父类之上的第一层脚本类
- 还是处于 `AS -> AS -> C++` 这样的多级脚本继承链中

这会影响构造、default 链和若干父类递归行为的处理方式。

### `ComposeOntoClass`

这个字段没有在当前 `.cpp` 主路径里大量出现，但它的命名已经暴露出一层很重要的语义：**不是所有脚本类状态都等价于“独立继承类”，还有 compose/mixin onto 某个原生类的场景。**

因此在 `UASClass` 里，它更像一条“类身份变体”的编码位，用来区分普通脚本类与 composable/mixin 型类定义。

## 第二组：布局字段，回答“脚本对象在 `UObject` 里怎么摆”

第二组字段是 `UASClass` 最核心也最底层的一层：

- `int32 ContainerSize = 0;`
- `int32 ScriptPropertyOffset = 0;`
- `void* ScriptTypePtr = nullptr;`
- `bool bIsScriptClass = false;`

这一组字段共同描述的不是逻辑继承，而是**脚本对象的物理/运行时布局**。

### `ContainerSize`

`GetContainerSize()` 直接返回它，而 `AngelscriptClassGenerator.cpp` 里也会在类布局计算中使用：

- `CastChecked<UASClass>(SuperClass)->GetContainerSize()`

这说明 `ContainerSize` 记录的是当前脚本类对应对象容器的总大小，而不是单个属性大小。它服务的是：

- 对象实例布局
- 继承链上的容器大小累积
- 运行时构造/复制/析构时对脚本对象体积的认知

### `ScriptPropertyOffset`

这个字段的职责是把“脚本属性区”从普通 UObject 内存里切出来。`Hazelight` 对照文档也明确把它解释成：

- 脚本属性在 UObject 内存中的起始偏移

也就是说，当前 `UASClass` 并不是把脚本对象另存在旁边，而是把脚本状态折进 UE 对象容器里，再靠 `ScriptPropertyOffset` 找到脚本段的起点。

### `ScriptTypePtr`

这是最关键的桥接字段之一。`ASClass.cpp` 里多个路径都依赖它：

- `ResolveScriptVirtual()` / `VerifyScriptVirtualResolved()` 会把它转回 `asCObjectType*`
- `RuntimeDestroyObject()` 用它调用脚本对象析构
- `GetConstructingASObject()` 用它判断当前正在构造的对象是不是脚本对象

因此 `ScriptTypePtr` 的真实角色不是一个“附带指针”，而是 **UE `UClass` 世界 → AngelScript `asITypeInfo/asCObjectType` 世界** 的主桥。

### `bIsScriptClass`

这个布尔位看起来像冗余标记，但它其实是非常便宜的一层“类身份判定”。有了它，很多调用点无需先解引用 `ScriptTypePtr` 或沿继承链判断，就可以快速知道：

- 这是不是真正的脚本类

因此它更多是一层 **快速分类位**，与 `ScriptTypePtr` 这种重桥接字段配合使用。

## 第三组：脚本生命周期字段，回答“怎么构造、默认化、GC 和调试”

这一组字段把 `UASClass` 从“布局描述符”变成了“可运行类描述符”：

- `asIScriptFunction* ConstructFunction;`
- `asIScriptFunction* DefaultsFunction;`
- `UE::GC::FSchemaOwner ReferenceSchema;`
- `FDebugValuePrototype DebugValues;`

### `ConstructFunction`

它保存脚本侧构造函数。`AngelscriptClassGenerator.cpp` 里有直接取用脚本对象构造行为的痕迹：

- `Manager.Engine->GetFunctionById(ObjectTypeToConstruct->beh.construct)`

也就是说，当前脚本类不是只靠原生 `ClassConstructor` 建起来，真正的脚本构造函数句柄也要挂在类上，供实例初始化和重初始化使用。

### `DefaultsFunction`

这个字段对应脚本的 `default` 语句/默认值初始化路径。它和 `ConstructFunction` 共同表达了一条设计：

- 脚本类的“构造”和“默认值建立”是两步独立的脚本语义

因此 `UASClass` 不只保存“怎么 new”，还保存“怎么灌 default state”。

### `ReferenceSchema`

这个字段专门服务 GC。它说明当前实现不是完全依赖 UE 默认反射遍历，而是允许脚本类挂自己的引用 schema。`Hazelight` 对照文档也直接把它解释成：

- 自定义 GC 引用 schema，追踪脚本属性中的 UObject 引用

因此 `ReferenceSchema` 是 **脚本对象 GC 可见性** 的关键状态，不是简单的缓存字段。

### `DebugValues`

`RuntimeDestroyObject()` 一上来就会在 `WITH_AS_DEBUGVALUES` 下释放 `Object->Debug` 对应的 `DebugValues`：

```cpp
if (Object->Debug != nullptr)
    DebugValues.Free(Object->Debug);
```

这说明 `DebugValues` 的角色是：

- 为脚本类运行时实例提供一层调试值原型/存储管理

它不参与对象身份或继承关系，但确实是 `UASClass` 负责托管的一部分实例态辅助资源。

## 第四组：组件声明字段，回答“脚本类把哪些默认组件和覆盖组件折进类定义里”

这组字段让 `UASClass` 不只是一般 UObject 类描述符，而能承载 Actor/Component 级类声明：

- `TArray<FDefaultComponent> DefaultComponents;`
- `TArray<FOverrideComponent> OverrideComponents;`

其中 `FDefaultComponent` 记录：

- `ComponentClass`
- `ComponentName`
- `VariableOffset`
- `bIsRoot`
- `bEditorOnly`
- `Attach` / `AttachSocket`

而 `FOverrideComponent` 记录：

- 覆盖哪个组件名
- 变量名与偏移
- 替换后的组件类

这说明 `UASClass` 的类描述里不只保存“属性和函数”，还保存了**默认组件拓扑声明**。也正因为如此，它才能支撑脚本 Actor/Component 类的默认组件构造与覆盖行为。

## 静态/线程局部辅助状态说明它还承担了构造期环境管理

除了上述四组主字段，`ASClass.cpp` 里还有几条很值得单独记住的静态/线程局部状态：

- `UASClass::OverrideConstructingObject`
- `thread_local UObject* GASDefaultConstructorOuter`
- `FScopeSetDefaultConstructorOuter`

它们说明 `UASClass` 不只是保存类描述，还参与了**构造期环境管理**：

- 当前是不是在脚本对象构造过程中
- 默认构造的 outer 应该是谁

`GetConstructingASObject()` 也正是沿着这条状态链去判断当前线程里是否存在正在构造的脚本对象。因此这一层更像 `UASClass` 的“构造期上下文辅助面”。

## 运行时方法正好映射回这些字段分组

`UASClass` 自带的几个运行时方法，正好可以反推前面这些字段为什么会被放在类上：

- `GetMostUpToDateClass()` ↔ `NewerVersion`
- `RuntimeDestroyObject()` ↔ `DebugValues` + `ScriptTypePtr`
- `GetLifetimeScriptReplicationList()` ↔ 脚本属性/继承链上的 net 状态
- `IsFunctionImplementedInScript()` ↔ 脚本函数身份判断
- `GetContainerSize()` ↔ `ContainerSize`

也就是说，这些字段并不是“先放上去以后可能会用”，而是已经和类级运行时 API 构成了一套直接映射关系。

## `UASClass` 的布局其实是在把多种视角压缩到一个类上

如果把 `UASClass` 的状态布局翻译成更抽象的话，它是在同一个 `UClass` 子类对象上压缩四种视角：

- **UE 反射视角**：我是一个合法的 `UClass`
- **脚本类型视角**：我对应一个 `asITypeInfo/asCObjectType`
- **对象布局视角**：我知道脚本对象体和属性区怎么放进 `UObject`
- **实例生命周期视角**：我知道怎么构造、默认化、GC、热重载和销毁

这也是为什么它看上去比普通 `UClass` 子类“字段更杂”。这些字段不是杂乱堆积，而是在把多种运行时视角压进同一个类描述符。

## 这层状态布局应该怎么记

如果把 `UASClass` 的核心字段压成一句工程化判断，可以这样记：

**`UASClass` 是脚本类在 UE 反射系统里的主承载体：它用 `CodeSuperClass/NewerVersion` 维护继承与版本链，用 `ContainerSize/ScriptPropertyOffset/ScriptTypePtr` 维护脚本对象布局桥，用 `ConstructFunction/DefaultsFunction/ReferenceSchema/DebugValues` 维护脚本生命周期状态，再用 `DefaultComponents/OverrideComponents` 把 Actor 级组件声明折进类定义。**

换成更实用的阅读过滤器就是：

- 看到父类链和热重载版本 → 看 `CodeSuperClass` / `NewerVersion`
- 看到脚本对象布局和 VM 类型桥 → 看 `ContainerSize` / `ScriptPropertyOffset` / `ScriptTypePtr`
- 看到构造、default、GC、调试 → 看 `ConstructFunction` / `DefaultsFunction` / `ReferenceSchema` / `DebugValues`
- 看到默认组件和覆盖组件 → 看 `DefaultComponents` / `OverrideComponents`

## 小结

- `UASClass` 不是只给 `UClass` 多挂一个 `ScriptTypePtr`，而是把继承链、对象布局、生命周期状态和组件声明四类信息一起压进了脚本类描述符
- `CodeSuperClass` / `NewerVersion` 处理的是“从谁继承、被谁替换”的问题；`ContainerSize` / `ScriptPropertyOffset` / `ScriptTypePtr` 处理的是“脚本对象怎么在 UE 对象里摆放”的问题
- `ConstructFunction` / `DefaultsFunction` / `ReferenceSchema` / `DebugValues` 让类描述符同时携带脚本构造、默认值、GC 和调试语义
- `DefaultComponents` / `OverrideComponents` 则让 `UASClass` 能够承载 Actor 级组件拓扑声明，因此它本质上是一份多视角的脚本类运行时总描述符
