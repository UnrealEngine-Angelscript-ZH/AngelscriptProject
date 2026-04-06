# Reload propagation、依赖扩散与版本链

> **所属模块**: 脚本类生成机制 → Reload Propagation / Dependency Expansion / Version Chains
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/ASClass.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`, `Documents/Knowledges/02_01_02_AngelscriptClassGenerator_Creation_And_Reload_Pipeline.md`

前几节已经分别讲了 `UASClass` 长什么样、`FAngelscriptClassGenerator` 怎么跑创建/重载主链，以及对象构造/GC/复制如何协作；这一节要把其中最容易被低估、但实际上决定整条链稳定性的那一层单独拆开：**reload requirement 是怎样沿依赖关系被放大的，旧新脚本类型和旧新 `UASClass` 又是怎样通过版本链挂在一起的。** 当前实现里，reload propagation 不是分析阶段的副产物，而是一套独立的数据结构和传播协议；而 `NewerVersion`、`ReplacedClass`、`UpdatedScriptTypeMap`、`RemovedClasses` 则把“旧版本怎么退场、新版本怎么接位”折成了一条明确的版本链。

## 先看这条链的最小闭环

把当前 reload propagation / version chain 逻辑压成最小流程图，大致是：

```text
SetupModule()
    -> Analyze()
        -> 产生局部 ReloadReq
    -> PropagateReloadRequirements()
        -> AddReloadDependency()
        -> ResolvePendingReloadDependees()
            -> 模块级 ReloadReq 被抬高
    -> PerformReload()
        -> CreateFullReloadClass / LinkSoftReloadClasses
            -> ReplacedClass / NewerVersion / UpdatedScriptTypeMap
    -> OnPostReload / CleanupRemovedClass
        -> 旧版本退场，新版本接位
```

最关键的一点是：当前实现并不是“每个类自己决定 soft/full reload”，而是先让局部差异生成一个初始 `ReloadReq`，再通过依赖传播把这个需求扩散到所有依赖方，最后才进入执行阶段。因此这一节的核心既不是纯分析，也不是纯执行，而是两者之间那层 **requirement propagation + version handoff**。

## `FReloadPropagation`：传播本身就是一等公民，不是临时递归变量

`AngelscriptClassGenerator.h` 里最重要、也最容易被忽略的一段是：

```cpp
struct FReloadPropagation
{
    bool bStartedPropagating = false;
    bool bFinishedPropagating = false;
    bool bHasOutstandingDependencies = false;
    EReloadRequirement ReloadReq = EReloadRequirement::SoftReload;
    TArray<FReloadPropagation*> PendingDependees;
};
```

而 `FClassData` 和 `FDelegateData` 都直接继承它。这说明什么？说明当前实现从一开始就把“reload requirement 的传播状态”建模成了显式结构，而不是在 `Analyze()` 里递归几层就算了。

这几个字段分别编码了：

- **传播是否已经开始/结束**：避免递归环和重复传播
- **是否存在未决依赖**：说明当前 requirement 还可能被上游抬高
- **当前 requirement 等级**：`SoftReload` / `FullReloadSuggested` / `FullReloadRequired` / `Error`
- **待回填的下游依赖者**：允许后续 requirement 升级时把变化再推回依赖方

因此 `FReloadPropagation` 本质上是一份 **小型传播状态机**，而不是简单的“给类多挂一个 ReloadReq”。

## 初始 requirement 先来自局部差异分析

传播之前，`Analyze()` 先给每个类/委托打出一份局部 requirement。当前代码里最典型的升级条件包括：

- 父类变化 → `FullReloadRequired`
- 属性类型或定义变化 → `FullReloadRequired`
- 属性 metadata 变化 → `FullReloadSuggested`
- 新增需要 UPROPERTY 的属性 → 可能 `FullReloadRequired`
- 方法签名变化 / BlueprintEvent 新增 → `FullReloadRequired`
- 参数默认值变化 / metadata 变化 → `FullReloadSuggested`
- `DefaultsCode` 变化 → `FullReloadSuggested`
- 新脚本类型尺寸超过旧类属性区 → `FullReloadRequired`

也就是说，传播链的起点不是空白，而是已经经过一轮本地 diff 分析。这一点非常重要，因为传播层传播的不是“某类有依赖”，而是“某类已经带着某个强度的 reload 需求，而且这个需求可能会污染依赖方”。

## `AddReloadDependency()`：把脚本类型依赖转换成传播边

`AddReloadDependency` 在 header 里有两个重载：

- `AddReloadDependency(FReloadPropagation* Source, const FAngelscriptTypeUsage& Type)`
- `AddReloadDependency(FReloadPropagation* Source, asITypeInfo* TypeInfo)`

从调用点可以看出，它会被用于：

- 父类脚本类型
- 属性类型
- 方法返回值类型
- 参数类型
- 委托签名参数/返回值

也就是说，它真正做的是把“类型引用”折成一条传播图上的边：

- **当前类/委托** 依赖 **另一个脚本类型**
- 如果被依赖对象的 requirement 后续上升，当前类也必须被回填升级

这也是为什么 `FReloadPropagation` 里会有 `PendingDependees`：传播图并不是单向立即完成，而是允许先登记“谁依赖我”，后面 requirement 变高时再补发给这些下游。

换句话说，当前 generator 内部维护的不是一棵树，而更像一张 **类型依赖传播图**。

## `PropagateReloadRequirements()`：把局部差异放大成跨类型闭包

`PropagateReloadRequirements(FModuleData&, FClassData&)` 和它的 delegate 版本，是整条链真正的放大器。它会：

- 对脚本父类调用 `AddReloadDependency(...)`
- 遍历本地脚本属性，把属性类型作为传播依赖加入
- 遍历方法返回值和参数，把所有相关类型再挂进去
- 如果 `ReloadReq` 已经被抬高，就继续把变化沿 `PendingDependees` 推下去

这意味着 generator 并不是只说：

- “A 类自己改了，所以 A full reload”

而是进一步说：

- “A 改了，而且 B 的属性/返回值/参数里引用了 A，所以 B 也可能必须 full reload 或至少被建议 full reload”

因此这条函数的真正职责是把 **局部结构变化 → 全局 reload 决策** 之间的缺口补上。

## `ResolvePendingReloadDependees()`：传播的关键不是发现依赖，而是回填升级

当前传播体系里最关键的一点，其实不是“找出依赖”，而是“当 requirement 变高后，如何把这个变化推回先前登记过的依赖者”。这正是 `ResolvePendingReloadDependees(...)` 的职责：

- 某个 `Source` 的 `ReloadReq` 一旦抬高
- 就去遍历它的 `PendingDependees`
- 把新的 requirement 继续合并/提升到这些依赖者上

因此这条函数让传播链具备了真正的“扩散”性质：

- 不是一次 DFS 后固定结果
- 而是 requirement 等级可以在分析过程中逐渐升高，并被反向传播给早先已经登记的依赖者

从架构上讲，这就是当前系统能处理“先分析到子类型，后发现父类型/返回值类型的更高风险”这类场景的原因。也就是说，它让 reload propagation 从简单递归变成了**增量收敛过程**。

## `EnsureReloaded()`：传播结果最终会强制某些依赖先 materialize

传播最终不是停在一个枚举值上，而是会落到执行层的 `EnsureReloaded(...)`：

```cpp
void FAngelscriptClassGenerator::EnsureReloaded(FModuleData& Module, FClassData& Class)
{
    if (Class.bReloaded)
        return;

    if (ShouldFullReload(Class))
        DoFullReload(Module, Class);
    else if (!Class.NewClass->bIsStruct)
        DoSoftReload(Module, Class);
}
```

这说明 requirement propagation 的最终意义不是“报个建议”，而是：

- 在需要某类型被 finalize 或被别的类依赖之前，强制它先走完合适的 materialize 路径

因此传播图和执行图并不是分开的：传播图先决定谁至少要走到什么 reload 等级，`EnsureReloaded()` 再把这个决策兑现成具体动作。

## 版本链第一层：`UpdatedScriptTypeMap` 负责旧新脚本类型映射

在分析阶段，当前实现会尽早把旧脚本类型映射到新脚本类型：

- 类分析时：`UpdatedScriptTypeMap.Add(OldClass->ScriptType, NewScriptType)`
- 委托分析时：`UpdatedScriptTypeMap.Add(OldDelegate->ScriptType, ScriptType)`

这说明版本链并不只存在于 `UASClass::NewerVersion`。在脚本 VM 类型层，generator 还会维护一张 **旧 type info -> 新 type info** 的转换表。

这张表的直接消费点包括：

- `SoftReloadType(...)`：把 `FAngelscriptTypeUsage` 里的 `ScriptClass` 更新到新类型
- 各种 `UASFunction` / 参数 / 返回值 / subtype 递归软重载

因此当前版本链其实有两层：

1. **脚本类型层**：`UpdatedScriptTypeMap`
2. **Unreal 类壳层**：`NewerVersion` / `ReplacedClass`

把这两层分开看非常重要，因为前者服务 VM 类型与签名迁移，后者服务 UE 反射对象和实例重载。

## 版本链第二层：`CreateFullReloadClass()` / `NewerVersion` 负责 Unreal 类壳接位

`CreateFullReloadClass()` 的关键动作之一，就是把旧类挂到新类链上：

- 旧类重命名成 `*_REPLACED_*`
- 旧类打 `CLASS_NewerVersionExists`
- 旧 `UASClass->NewerVersion = 新 UASClass`

而 `UASClass::GetMostUpToDateClass()` 会沿着 `NewerVersion` 一路追最新版本：

```cpp
if (NewerVersion == nullptr)
    return this;

UASClass* NewerClass = NewerVersion;
while (NewerClass->NewerVersion != nullptr)
    NewerClass = NewerClass->NewerVersion;
return NewerClass;
```

这说明当前类版本链不是“旧类直接被替换然后消失”，而是会保留一条 **旧壳 → 新壳 → 更新壳** 的追踪链，供：

- 组件 override 解析
- default component 热重载
- editor-side reinstance
- 外围调用点追到最新类版本

使用。

因此 `NewerVersion` 不是一个可有可无的热重载缓存字段，而是当前类版本模型的正式接口。

## `ReplacedClass` / `ReplacedStruct`：版本链里的“旧壳锚点”

`FClassData` 里还有两条很关键的旧壳锚点：

- `UASClass* ReplacedClass`
- `UStruct* ReplacedStruct`

它们和 `OldClass` 不完全一样：

- `OldClass` 是旧的脚本描述符
- `ReplacedClass` / `ReplacedStruct` 是本轮 full reload 过程中真正被从 UObject 世界里退位的旧壳对象

这也是为什么 post-reload 广播时会有这样的逻辑：

- 如果有 `OldClass`，优先拿 `OldClass->Class`
- 否则回退到 `ReplacedClass`

也就是说，版本链的手递手不是抽象的描述符关系，而是要在广播和 cleanup 阶段明确区分：

- 这个旧壳到底来自“先前脚本描述符”
- 还是来自“本轮刚被顶掉的 UObject 壳”。

## `RemovedClasses` 和 `CleanupRemovedClass()`：依赖扩散之外的退场分支

除了“旧版本被新版本替换”这条版本链，generator 还要处理另一类情况：**类直接从脚本里消失。**

这条分支通过：

- `Analyze(ModuleData)` 把已不存在的旧类收进 `RemovedClasses`
- `PerformReload()` 尾段遍历 `RemovedClasses`
- `CleanupRemovedClass(...)` 负责清空 `ScriptTypePtr` / `ConstructFunction` / `DefaultsFunction`，隐藏类并去根

这说明当前版本链不仅要处理 A→A' 的升级，还要处理 A→∅ 的退场。

因此“版本链”在这里其实有两种结局：

- 有新版：挂进 `NewerVersion` 链
- 无新版：进入 `RemovedClasses` 清算链

这也是为什么当前 `PerformReload()` 尾段要先广播 `OnClassReload` / `OnStructReload`，再清 removed classes，最后再统一 `OnPostReload`。它需要同时覆盖升级和退场两类结局。

## `ReloadReqLines`：传播不仅有强度，还有来源行号

当前 requirement 传播还有一个很有价值但容易忽略的侧面：`ReloadReqLines`。无论是：

- 属性定义变化
- 方法签名变化
- metadata 变化
- 新增/删除字段
- `DefaultsCode` 变化

当前实现都会把触发升级的行号塞进：

- `ClassData.ReloadReqLines`
- `DelegateData.ReloadReqLines`
- `ModuleData.ReloadReqLines`

这说明 propagation 不只是一个“等级枚举”，而是附带了 **诊断来源**。也就是说，当前系统不仅能说“你需要 full reload”，还能说“这几行是触发这次升级的直接原因”。

这也是为什么 `WantsFullReload(...)` / `GetFullReloadLines(...)` 这种接口值得存在：传播链最终还要反馈给工具和用户，而不只是内部使用。

## `ShouldFullReload()`：传播结果和执行策略之间的最后一道翻译

在传播结束后，`ShouldFullReload(...)` 并不会机械地把所有 `FullReloadSuggested` 都当 `true`。例如：

- 如果当前批次本来就在做 full reload，`FullReloadSuggested` 也会 materialize 成 full reload
- 接口类和实现接口的类会被强制 full reload
- brand-new 且不是 statics class 的类，在 soft reload 批次里也会被引导走 `CreateFullReloadClass()`

这说明 requirement propagation 输出的是**风险等级**，而 `ShouldFullReload()` 输出的是**当前批次里的执行策略**。

因此这两者不能混成一个概念：

- propagation 负责把风险往依赖图上扩散
- `ShouldFullReload()` 负责把这些风险在当前 reload 批次里翻译成实际动作。

## 这条传播/版本链应该怎么记

如果把 `2.1.5` 压成一句工程化判断，可以这样记：

**当前 generator 并不是每个类各自决定怎么 reload，而是先把类、委托和枚举都挂到 `FReloadPropagation` 状态图上，用 `AddReloadDependency` 和 `ResolvePendingReloadDependees` 把局部差异扩散成依赖闭包，再通过 `EnsureReloaded` / `ShouldFullReload` 把这些等级兑现成具体执行；与此同时，旧新脚本类型通过 `UpdatedScriptTypeMap` 连接，旧新 Unreal 类壳通过 `NewerVersion` / `ReplacedClass` 连接，而彻底消失的类型则进入 `RemovedClasses` / `CleanupRemovedClass` 这条退场链。**

换成更实用的阅读过滤器就是：

- 看传播状态怎么保存 → `FReloadPropagation`
- 看依赖怎么登记和扩散 → `AddReloadDependency` / `PropagateReloadRequirements` / `ResolvePendingReloadDependees`
- 看传播结果怎么变执行动作 → `ShouldFullReload` / `EnsureReloaded`
- 看旧新脚本类型映射 → `UpdatedScriptTypeMap`
- 看旧新 `UASClass` 版本链 → `CreateFullReloadClass` + `NewerVersion` + `GetMostUpToDateClass()`
- 看彻底移除的类型怎么退场 → `RemovedClasses` + `CleanupRemovedClass`

## 小结

- 当前 reload propagation 是一套显式状态机，不是分析阶段顺手递归；`FReloadPropagation` 把传播状态、requirement 等级和待回填依赖者都建模成了正式字段
- `Analyze()` 先产生局部 reload 需求，再由 `PropagateReloadRequirements()` 把这些需求沿父类、属性、参数、返回值和委托签名类型扩散成依赖闭包
- `UpdatedScriptTypeMap` 和 `NewerVersion` 分别维护脚本 VM 类型层和 Unreal 类壳层的版本链；`RemovedClasses` 则承担“直接退场”的另一类结局
- 因此当前类生成链真正稳定的关键，不只是 full/soft reload 执行顺序，而是它先用一套传播与版本链接口，把“谁应该跟着升级、谁仍然代表旧版本、谁应该被正式清理”这三件事讲清楚
