# 对象构造、GC、复制与热重载协作

> **所属模块**: 脚本类生成机制 → Construction / GC / Copy / HotReload Coordination
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`

前两节分别把 `UASClass` 的状态布局和 `FAngelscriptClassGenerator` 的重载总链路钉住了，这一节要把两者真正“串起来”的那部分单独讲清：一个脚本类对象是怎么被构造出来的，GC 怎么看见它的脚本引用，soft/full reload 时旧状态怎么被拆下来又装回去，以及这些动作为什么必须按现在的顺序协作。当前实现并不是“构造、GC、热重载各管一摊”，而是一条连续的协作链：构造时决定脚本对象如何嵌进 `UObject` 内存；GC 路径决定哪些纯脚本属性会被引用追踪；soft reload 再利用前两者的布局与类型信息，把旧对象状态迁移到新脚本类型上。

## 先看协作总图

把这条链压成最小流程图，大致是：

```text
Static*Constructor
    -> C++ 父类构造
    -> 构造 asCScriptObject
    -> CreateDefaultComponents / ExecuteConstructFunction / ExecuteDefaultsFunctions
        -> 对象进入正常运行态
            -> DetectAngelscriptReferences / AssembleReferenceTokenStream
                -> GC 能看见脚本引用
                    -> reload 时 PrepareSoftReload / DoSoftReload
                        -> 保存旧值 -> DestructScriptObject -> ReinitializeScriptObject -> 复制回新值
```

这张图最重要的地方在于：当前实现不是先把脚本对象放到独立容器里，再做一层同步，而是直接在 UE 对象壳里构造脚本对象、让 GC 看见它、再在热重载时原地拆装和迁移。也正因为如此，构造、GC、复制和热重载才必须被放在同一节讲。

## 第一段：构造路径先完成原生壳，再嵌入脚本对象

`UASClass` 为三种对象类型分别准备了静态构造入口：

- `StaticActorConstructor`
- `StaticComponentConstructor`
- `StaticObjectConstructor`

其中 `StaticActorConstructor` 最能说明整体顺序：

```cpp
ApplyOverrideComponents(Initializer, Actor, Class);
Class->CodeSuperClass->ClassConstructor(Initializer);
Actor->PrimaryActorTick.bCanEverTick = Class->bCanEverTick;
Actor->PrimaryActorTick.bStartWithTickEnabled = Class->bStartWithTickEnabled;

if (!bIsScriptAllocation && ScriptType != nullptr)
    new(Object) asCScriptObject(ScriptType);
```

这段顺序非常关键：

1. 先处理 override components
2. 再调用 `CodeSuperClass->ClassConstructor(...)` 跑完 C++ 父类构造
3. 然后才把 `asCScriptObject` 用 placement new 放进当前 `UObject` 内存

因此当前脚本类构造并不是“脚本对象拥有一个 UObject”，而是反过来：**UObject 先按 UE 规则被构造出来，脚本对象体再被嵌进这个已经有效的原生对象壳里。**

这也解释了为什么 `ScriptPropertyOffset` 和 `ContainerSize` 这类字段在上一节那么重要——它们直接决定了“脚本对象从哪开始，占多大空间”。

## `ExecuteConstructFunction()` 和 `ExecuteDefaultsFunctions()` 把脚本语义叠到原生构造之后

当前实现把脚本语义再切成两层：

- `ExecuteConstructFunction(Object, Class)`
- `ExecuteDefaultsFunctions(Object, Class)`

`ExecuteConstructFunction()` 只负责直接执行脚本构造函数：

```cpp
if (Class->ConstructFunction != nullptr)
{
    FAngelscriptContext Context(Object, Class->ConstructFunction->GetEngine());
    Context->SetObject(Object);
    Context->Execute();
}
```

而 `ExecuteDefaultsFunctions()` 则会沿脚本继承链收集 `DefaultsFunction`，再从基类到派生类逆序执行。也就是说，构造流程实际上分成了：

- **原生父类构造**
- **脚本构造函数**
- **脚本 default 链**

这条切分很重要，因为它保证：

- C++ 父类先把对象壳建稳；
- 脚本构造再做脚本对象自身初始化；
- 最后才按继承顺序应用 default 语句，确保父类默认值先于子类默认值。

因此这套实现不是一锅炖，而是非常明确地把 **构造** 和 **默认值灌入** 分开了。

## `FinishConstructObject()` 是脚本分配路径与 UE 构造路径重新汇合的接缝

`UASClass::FinishConstructObject(...)` 专门处理脚本分配路径的尾声。它的关键判断是：

- 当前是不是通过 `CurrentObjectInitializers` 识别出的脚本分配
- 当前 `ScriptType` 是否仍在这个类继承树里
- 如果到了 childmost script class，就执行 `ExecuteDefaultsFunctions(...)`

也就是说，这个函数的作用不是重复构造，而是：**让脚本侧发起的对象创建，最终在正确的继承树顶端补上 default 链执行。**

这一步很容易被忽略，但它恰恰说明当前实现不是只考虑 `NewObject<>` 从 UE 侧发起的路径，而是有意识地把 AS 发起的构造也纳入同一套对象生命周期语义里。

## 第二段：GC 不是只靠 `FProperty`，还有 `ReferenceSchema` 这条脚本侧补链

GC 协作的核心入口在 `FAngelscriptClassGenerator::DetectAngelscriptReferences(...)`。它会：

- 拿到 `UASClass` 对应的 `ScriptTypePtr`
- 先把已有 `ReferenceSchema` 追加到新的 `SchemaBuilder`
- 遍历脚本类型上的所有属性
- 跳过 primitive 和 inherited 属性
- 如果该属性没有被生成为 Unreal `FProperty`，但 `PropertyType.HasReferences()` 为真，就调用 `EmitReferenceInfo(...)`
- 最后把构建好的 schema 写回 `Class->ReferenceSchema`

```cpp
if (!bAddedAsUnrealProperty)
{
    if (PropertyType.HasReferences())
    {
        RefParams.AtOffset = PropertyOffset;
        PropertyType.EmitReferenceInfo(RefParams);
    }
}

UE::GC::FSchemaView View(Schema.Build(GetARO(Class)), UE::GC::EOrigin::Other);
Class->ReferenceSchema.Set(View);
```

这段逻辑说明当前 GC 协作不是单通道，而是双通道：

- **Unreal property 通道**：已经 materialize 成 `FProperty` 的脚本属性，走 UE 原生 token stream
- **脚本 schema 通道**：纯脚本属性但持有 UObject 引用的部分，走 `ReferenceSchema`

也就是说，当前脚本类的 GC 可见性并不完全建立在“都变成 UPROPERTY”这个前提上，而是允许一部分纯脚本状态通过 schema 补链进入 GC 世界。

## `AssembleReferenceTokenStream(true)` 和 `ReferenceSchema` 一起形成 GC 可见性闭环

在 `DoSoftReload()` 里，generator 会在更新属性和函数之后再次执行：

- `DestroyAngelscriptUnversionedSchema(Class)`
- `Class->AssembleReferenceTokenStream(true)`
- `DetectAngelscriptReferences(ClassDesc)`

这说明 GC 协作也不是一锤子买卖，而是随着 soft reload 重新计算的：

- `AssembleReferenceTokenStream(true)` 刷新 Unreal property 层的 GC token
- `DetectAngelscriptReferences(...)` 刷新脚本 schema 层的引用信息

因此当前实现真正维护的是一套 **reload-aware GC visibility**：类定义一变，GC 视图也要重建，而不是沿用旧的 token/schema。

## 第三段：复制与热重载协作依赖“先拆旧脚本对象，再把值搬回新布局”

`DoSoftReload()` 里最关键的一大段逻辑其实是状态迁移。它会：

- 先通过 `FLocalPropertyContext` 把旧脚本对象和新脚本对象的可复制属性扁平化出来
- 只收集那些 `CanCopy()`、`CanConstruct()`、`CanDestruct()` 的类型
- 对旧实例逐个比较 CDO 值，决定哪些属性需要真正迁移
- 把这些属性先复制到临时 buffer

```cpp
bool bShouldCopy = Copy.bModifiedByDefaults || !Copy.bCanCompare || !Copy.Type.IsValueEqual(CDOPtr, OriginalPtr);
if (bShouldCopy)
{
    if (Copy.bNeedConstruct)
        Copy.Type.ConstructValue(NewPtr);
    Copy.Type.CopyValue(OriginalPtr, NewPtr);
}
```

这说明当前的复制策略并不是“把整个脚本对象内存块硬拷贝”，而是：

- 先按类型能力判断能不能迁移
- 再按属性级 diff 判断需不需要迁移
- 只迁移真正需要保留的状态

因此它更像一套 **语义级状态搬运**，而不是字节块级复制。

## `DestructScriptObject()` 与 `ReinitializeScriptObject()`：热重载的拆旧/装新原子对

真正的热重载原子对是：

- `DestructScriptObject(...)`
- `ReinitializeScriptObject(...)`

前者做两件事：

- 对旧 `asCScriptObject` 调用析构
- 把脚本属性区对应的内存清零

```cpp
if (ObjectTypeToDestruct != nullptr)
    ScriptObject->CallDestructor(ObjectTypeToDestruct);

FMemory::Memzero((void*)((SIZE_T)Object + ASClass->ScriptPropertyOffset),
    ASClass->GetPropertiesSize() - ASClass->ScriptPropertyOffset);
```

后者则做两件事：

- 用新的 `asCObjectType` 在同一块对象内存上重新 placement new 一个脚本对象
- 如果该类型有构造函数，就执行新的脚本构造函数

```cpp
new(ScriptObject) asCScriptObject(ObjectTypeToConstruct);
...
Context->SetObject(ScriptObject);
Context->Execute();
```

因此当前热重载并不是“给旧脚本对象打补丁”，而是**在同一块 UObject 内存上，拆掉旧脚本对象、原地重建一个新脚本对象，再把允许迁移的状态搬回来。**

这也是为什么构造、复制和热重载必须放在一节里讲：热重载本质上就是“构造协议”在存量对象上的再执行。

## `PrepareSoftReload()` / `DoSoftReload()` 的配合：先冻结 default 语义，再迁移实例

`DoSoftReload()` 一上来就有一条很关键的语义：

- `ClassDesc->DefaultsCode = ClassData.OldClass->DefaultsCode;`

它说明 soft reload 默认会**保留旧的 defaults code 语义**，不立即接管新的 defaults 行为。也就是说，soft reload 不是“所有新脚本语义都立刻完全替换”，而是先尽量保证实例状态平稳迁移。

这也解释了为什么 generator 会先 `PrepareSoftReload(...)`，再等所有 full reload 路径执行完，最后才做 soft reload：

- 它要先把 struct/class/delegate 的新定义准备好
- 再让 soft reload 用这些稳定的新定义去搬旧实例状态

所以当前实现里，soft reload 并不是 cheap path，而是一条很讲究顺序的 **稳定迁移路径**。

## CDO 与普通实例被区别对待，这保证了 default 语义能单独重建

在 `DoSoftReload()` 里，普通实例和 CDO 不是一视同仁处理的：

- 普通实例会被立即纳入临时 buffer + destruct/reinitialize + copy back 的流程
- 带 `RF_ClassDefaultObject` 的实例先放进 `CDOInstances`，不在同一轮按普通实例方式迁移

这说明当前实现有意识地把：

- **存量实例状态迁移**
- **默认对象语义重建**

拆成了两条处理线。这样可以避免把 default 语义和运行中实例状态混成一锅，从而更稳地处理“哪些值来自旧实例修改，哪些值应来自新的默认定义”这类问题。

## Debug prototype 也会随着 soft reload 更新

在 `DoSoftReload()` 尾部，当前实现还会在 `WITH_AS_DEBUGVALUES` 下调用：

- `CreateDebugValuePrototype(ClassDesc)`

这意味着热重载之后，调试值原型也会跟着脚本对象布局一起更新。它再次说明热重载协作并不只包含“类能不能继续跑”，还要包含：

- GC 是否还能看见引用
- 调试视图是否还能理解新布局

也就是说，热重载在当前实现里是一条 **全栈对象协作链**，不只是运行时方法替换。

## 这条协作链应该怎么记

如果把 `2.1.3` 压成一句工程化判断，可以这样记：

**当前脚本类的构造、GC、复制和热重载并不是四个独立功能：`Static*Constructor` 先把脚本对象嵌进 `UObject` 内存并跑构造/default 链，`DetectAngelscriptReferences` 再把纯脚本引用补进 GC 视图，soft reload 则依赖这些布局与类型信息先把旧状态抽到临时 buffer，再原地析构/重建脚本对象并把允许迁移的属性复制回去。**

换成更实用的阅读过滤器就是：

- 看对象出生顺序 → `StaticActorConstructor` / `StaticComponentConstructor` / `StaticObjectConstructor`
- 看脚本语义何时叠上去 → `ExecuteConstructFunction` / `ExecuteDefaultsFunctions` / `FinishConstructObject`
- 看 GC 何时看见纯脚本引用 → `DetectAngelscriptReferences` + `ReferenceSchema`
- 看热重载怎么保状态 → `DoSoftReload` + `DestructScriptObject` + `ReinitializeScriptObject`

## 小结

- 当前脚本对象构造采用“原生壳先构造、脚本对象后 placement new、defaults 最后执行”的顺序，确保 UE 生命周期与脚本语义正确叠加
- GC 不是只靠 `FProperty`，还通过 `ReferenceSchema` 补充纯脚本属性中的 UObject 引用可见性
- soft reload 不做整块内存盲拷贝，而是按可复制/可比较属性构建迁移集，先拆旧脚本对象，再原地重建新脚本对象并把允许迁移的状态复制回去
- 因此对象构造、GC、复制和热重载在当前实现里构成了一条连续的协作链：前面的对象布局与引用追踪规则，正是后面稳定热重载迁移的前提
