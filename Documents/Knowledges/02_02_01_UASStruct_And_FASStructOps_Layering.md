# `UASStruct` 与 `FASStructOps` 分层

> **所属模块**: 脚本结构体生成机制 → `UASStruct` / `FASStructOps` Layering
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASStruct.cpp`, `Documents/Hazelight/ScriptStructImplementation.md`

这一节真正要钉死的，不是 `UASStruct` 和 `FASStructOps` 共同实现了脚本结构体支持，而是它们为什么要被刻意切成两层。当前实现里，这两者的分工非常清楚：`UASStruct` 是挂在 UE 反射系统里的 **类型锚点**，负责保存脚本结构体的元数据、版本链和 `ICppStructOps` 工厂/注册边界；`FASStructOps` 则是一个挂在 `ICppStructOps` / FakeVTable 机制上的 **操作执行层**，真正负责 `Construct`、`Destruct`、`Copy`、`Identical`、`GetStructTypeHash` 这些运行时操作。也就是说，`UASStruct` 负责“这个结构体是什么”，`FASStructOps` 负责“这个结构体在 UE 生命周期里怎么被操作”。

## 先看最小结构图

把当前两层关系压成最小图，大致是：

```text
UScriptStruct
  └─ UASStruct
       ├─ ScriptType / Guid / NewerVersion / bIsScriptStruct
       ├─ CreateCppStructOps()
       ├─ PrepareCppStructOps()
       └─ UpdateScriptType()
            ↓ creates / refreshes
       FASStructOps : UASStruct::ICppStructOps
         ├─ Struct / ScriptType
         ├─ ConstructFunction / EqualsFunction / ToStrFunction / HashFunction
         └─ FakeVTable -> Construct / Destruct / Copy / Identical / GetStructTypeHash
```

这张图最重要的地方在于：当前脚本结构体支持并不是把所有逻辑塞进 `UASStruct` 里，而是明确把“反射对象”和“操作执行器”拆开。这样做的结果是：

- `UASStruct` 继续扮演一个合法的 `UScriptStruct`
- `FASStructOps` 则专注于适配 UE 的结构体操作协议

因此它们不是“谁轻谁重”的关系，而是**元数据层和执行层的分层关系**。

## `UASStruct`：反射锚点，保存结构体的长期身份

`ASStruct.h` 里的 `UASStruct` 非常紧凑，但职责很明确：

- `UASStruct* NewerVersion = nullptr;`
- `asITypeInfo* ScriptType = nullptr;`
- `FGuid Guid;`
- `bool bIsScriptStruct;`

再加上几个关键方法：

- `GetNewestVersion()`
- `SetGuid(FName FromName)`
- `UpdateScriptType()`
- `PrepareCppStructOps()`
- `CreateCppStructOps()`

这一组字段和方法说明，`UASStruct` 解决的是三类问题：

1. **脚本类型身份**：我对应哪个 AngelScript `asITypeInfo`
2. **UE 反射身份**：我作为 `UScriptStruct` 如何保持稳定 GUID 和注册身份
3. **操作层挂载点**：我如何创建并维护与自身绑定的 `ICppStructOps`

因此 `UASStruct` 更像一份脚本结构体的 **长期元数据宿主**，而不是每次构造/析构时被直接调用的那一层。

## `NewerVersion` / `ScriptType` / `Guid` 说明它承担的是“类型级稳定性”

这几个字段的组合很能说明边界：

- `NewerVersion`：服务热重载版本链，回答“新的 struct 在哪”
- `ScriptType`：服务 AngelScript VM 类型桥，回答“底层脚本类型是谁”
- `Guid`：服务序列化稳定标识，回答“这个 struct 的持久身份是谁”

这些东西都不是一次 `Construct` / `Copy` 能解决的，而是需要挂在 `UScriptStruct` 对象上长期存在。因此它们放在 `UASStruct` 上而不是 `FASStructOps` 上，是非常自然的。

换句话说，`UASStruct` 更偏 **identity / registry / versioning**，而不是 **operation execution**。

## `FASStructOps`：真正响应 UE 结构体生命周期回调的执行层

`FASStructOps` 在 `ASStruct.cpp` 里定义成：

```cpp
struct FASStructOps : UASStruct::ICppStructOps
{
    UASStruct* Struct;
    asCObjectType* ScriptType;

    asIScriptFunction* EqualsFunction;
    asIScriptFunction* ConstructFunction;
    asIScriptFunction* ToStrFunction;
    asIScriptFunction* HashFunction;

    FASFakeVTable FakeVTable;
};
```

这组字段直接暴露了它的定位：

- 它持有 `UASStruct* Struct` 的反向引用
- 把 `ScriptType` 缓存成 `asCObjectType*`
- 还缓存脚本方法指针：`ConstructFunction`、`EqualsFunction`、`ToStrFunction`、`HashFunction`
- 最后再把这些操作挂到 `FakeVTable`

也就是说，`FASStructOps` 的职责不是保存“这个 struct 是谁”，而是把“这个 struct 的操作该怎么执行”准备好，并暴露给 UE。

因此它是一层 **运行时行为适配器**。

## `CreateCppStructOps()` / `PrepareCppStructOps()`：分层的真正接缝点

当前两层最关键的 handoff 就发生在：

- `UASStruct::CreateCppStructOps()`
- `UASStruct::PrepareCppStructOps()`

```cpp
UASStruct::ICppStructOps* UASStruct::CreateCppStructOps()
{
    return new FASStructOps(this, GetPropertiesSize(), GetMinAlignment());
}

void UASStruct::PrepareCppStructOps()
{
    if (CppStructOps == nullptr)
        SetCppStructOps(CreateCppStructOps());
    Super::PrepareCppStructOps();
}
```

这两段代码非常适合用来理解层次边界：

- `UASStruct` 决定何时创建 ops，以及 ops 要绑定到谁身上
- `FASStructOps` 决定创建出来以后，具体怎么响应生命周期操作

因此 handoff 很明确：**类型层创建并拥有操作层，操作层再服务类型层。**

这也是为什么 `FASStructOps` 不是一个独立全局对象，而是始终和一个具体 `UASStruct` 绑在一起。

## `SetFromStruct()`：操作层从元数据层拉取脚本方法能力

`FASStructOps::SetFromStruct(UASStruct* InStruct)` 是这套分层的另一个关键接缝。它会：

- 从 `InStruct->ScriptType` 重新解析 `ScriptType`
- 根据 `beh.construct` 找构造函数
- 查 `opEquals` / `ToString` / `Hash` 方法声明
- 更新 `EqualsFunction`、`ConstructFunction`、`ToStrFunction`、`HashFunction`

这说明 `FASStructOps` 自己并不拥有稳定元数据来源，它每次都要**从 `UASStruct` 重新拉取脚本类型信息，并把它翻译成操作级缓存**。

因此当前实现里：

- `UASStruct` 是 source of truth
- `FASStructOps` 是 cached execution facade

这也是为什么 `UpdateScriptType()` 只需要调用 `Ops->SetFromStruct(this)`，而不需要重建整套 `UASStruct`。

## FakeVTable 注入说明 `FASStructOps` 是“协议适配层”，不是语义所有者

`FASStructOps` 构造函数里会：

- `FakeVPtr = &FakeVTable;`
- 设置 `Flags` 为 `Construct` / `Destruct` / `Copy` / `Identical` / `GetStructTypeHash`
- 把静态函数地址填进 `FakeVTable`
- 再根据是否真的找到脚本方法，设置 `HasIdentical` / `HasGetTypeHash` 等 capabilities

这说明 `FASStructOps` 真正做的是：**把脚本结构体的语义，翻译成 UE 的 `ICppStructOps` 协议。**

也就是说，它并不决定“这个脚本 struct 有什么能力”，而是把 `UASStruct + ScriptType` 已有的能力映射成 UE 可以调用的生命周期接口。

因此它更像协议适配层，而不是语义所有者。

## 生命周期操作为什么留在 `FASStructOps` 而不是 `UASStruct`

`FASStructOps` 的静态方法非常直观：

- `Construct(...)`
- `Destruct(...)`
- `Copy(...)`
- `Identical(...)`
- `GetStructTypeHash(...)`

它们各自做的事情也很典型：

- `Construct`：有脚本构造函数就跑 AS VM，否则 `Memzero`
- `Destruct`：对 `asCScriptObject` 调 `CallDestructor`
- `Copy`：调 `PerformCopy`
- `Identical`：有 `opEquals` 就执行，否则返回 false
- `GetStructTypeHash`：有 `Hash()` 就执行，否则返回 0

这些操作之所以不应该挂在 `UASStruct` 上，一个根本原因是：它们本来就是 **UE 在运行时通过 `ICppStructOps` 协议回调的动作**。因此让 `FASStructOps` 去承接这套协议，是最自然的设计。

换句话说：

- `UASStruct` 回答“这个类型有什么能力”
- `FASStructOps` 回答“当 UE 调这个能力时，具体该怎么执行”

## `UpdateScriptType()`：热重载时，两层分工再次显现

`UASStruct::UpdateScriptType()` 非常能说明这套分层在热重载下的意义：

```cpp
FASStructOps* Ops = ((FASStructOps*)GetCppStructOps());
Ops->SetFromStruct(this);

if (Ops->EqualsFunction != nullptr)
    StructFlags |= STRUCT_IdenticalNative;
else
    StructFlags &= ~STRUCT_IdenticalNative;
```

这段逻辑里：

- `UASStruct` 先保持自己的 `ScriptType` / `NewerVersion` / `Guid` 等元数据身份
- 然后让 `FASStructOps` 重新发现脚本方法
- 再由 `UASStruct` 回过头根据新的 `Ops` 能力调整 `StructFlags`

这就是一个非常标准的“元数据层 ↔ 操作层”双向协作：

- 元数据层负责触发刷新
- 操作层负责重新探测能力
- 元数据层再把这些能力写回 UE 类型标志。

也就是说，热重载不是打破这套分层，而是恰好利用了这套分层。

## `GetToStringFunction()` 暴露了一个有趣的边界：UASStruct 负责提供“能力查询”，FASStructOps 负责持有实际缓存

`GetToStringFunction()` 的实现是：

```cpp
if (ICppStructOps* StructOps = GetCppStructOps())
    return ((FASStructOps*)StructOps)->ToStrFunction;
return nullptr;
```

这说明即便是一个看似“元数据查询”性质的接口，当前实现也没有把缓存再复制一份到 `UASStruct` 上，而是：

- 由 `FASStructOps` 持有实际缓存
- `UASStruct` 只提供一个向上暴露的查询接口

因此分层非常克制：

- `UASStruct` 不试图镜像所有 ops 内部字段
- 它只在需要时通过公开方法去问 ops

这也减少了热重载时需要双写/双同步的状态量。

## 这条分层边界应该怎么记

如果把 `2.2.1` 压成一句工程化判断，可以这样记：

**`UASStruct` 是脚本结构体在 UE 反射系统里的长期身份锚点，负责保存脚本类型、版本链、GUID 和 ops 工厂/刷新边界；`FASStructOps` 则是紧贴 `ICppStructOps` / FakeVTable 协议的执行层，负责缓存脚本方法并真正响应 Construct/Destruct/Copy/Identical/Hash 这些生命周期操作。**

换成更实用的阅读过滤器就是：

- 看 `ScriptType` / `Guid` / `NewerVersion` / `PrepareCppStructOps` → 优先想到 `UASStruct`
- 看 `Construct` / `Destruct` / `Copy` / `Identical` / `GetStructTypeHash` → 优先想到 `FASStructOps`
- 看热重载时两层怎么重新同步 → 看 `UpdateScriptType()` + `SetFromStruct()`

## 小结

- `UASStruct` 和 `FASStructOps` 的分层不是形式拆分，而是“反射锚点 vs 生命周期执行层”的职责分离
- `UASStruct` 保存类型级长期身份（脚本类型、版本链、GUID）并负责创建/刷新 ops；`FASStructOps` 则把这些元数据转译成 UE `ICppStructOps` 可调用的运行时操作
- `CreateCppStructOps()` / `PrepareCppStructOps()` / `UpdateScriptType()` 是这套分层最关键的接缝点：类型层创建并驱动操作层，操作层回馈能力状态给类型层
- 也正因为有这层分工，热重载时不需要重造整套 `UASStruct` 语义，只需刷新 `ScriptType` 和 ops 缓存，再把结果重新同步回结构体标志即可
