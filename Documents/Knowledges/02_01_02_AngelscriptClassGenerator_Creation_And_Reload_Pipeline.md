# `FAngelscriptClassGenerator` 创建 / 重载链路

> **所属模块**: 脚本类生成机制 → `FAngelscriptClassGenerator` / Creation & Reload Pipeline
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`, `Documents/Hazelight/ScriptClassImplementation.md`

如果说上一节的 `UASClass` 解释的是“最终生成出来的类对象长什么样”，这一节要讲的就是：谁把它们一步步造出来、分析出来、替换进去、再把旧版本清走。当前主干里真正承担这条职责链的是 `FAngelscriptClassGenerator`。它不是单纯的“类工厂”，更像一个编译后重载协调器：先把新脚本模块登记成内部数据结构，再分析类/委托/枚举和依赖关系，决定 soft/full reload，分别走不同的 materialize 路径，最后再做 finalize、default object 初始化、post-reload 广播和旧类清理。

## 先看整条主链路

把 `FAngelscriptClassGenerator` 的工作顺序压成最小流程图，大致是：

```text
AddModule() x N
    -> Setup()
        -> SetupModule()
        -> Analyze()
        -> PropagateReloadRequirements()
        -> 得出 EReloadRequirement
    -> PerformReload(full or soft)
        -> CreateFullReload* / LinkSoftReload*
        -> DoFullReload* / PrepareSoftReload / DoSoftReload
        -> FinalizeClass
        -> CallPostInitFunctions + InitDefaultObjects + VerifyClass
        -> OnClassReload/OnStructReload/OnPostReload + CleanupRemovedClass
```

这条图最重要的地方在于：`FAngelscriptClassGenerator` 并不是把“分析”和“重载执行”混成一锅，而是先在 `Setup()` 阶段建立完整的 reload 需求图，再统一进入执行阶段。这也是它能支撑跨模块依赖传播和 soft/full reload 混合执行的根本原因。

## 第一阶段：`AddModule()` 只做登记，不急着生成类

主链路的第一跳是 `AddModule(TSharedRef<FAngelscriptModuleDesc>)`：

```cpp
void FAngelscriptClassGenerator::AddModule(TSharedRef<FAngelscriptModuleDesc> Module)
{
    FModuleData Data;
    Data.OldModule = FAngelscriptEngine::Get().GetModule(Module->ModuleName);
    Data.NewModule = Module;
    Data.ModuleIndex = Modules.Num();

    Modules.Add(Data);
    ModuleIndexByName.Add(Module->ModuleName, Data.ModuleIndex);
    ModuleIndexByNewScriptModule.Add(Module->ScriptModule, Data.ModuleIndex);
}
```

这段代码很能说明 generator 的工作风格：它不会在收到新模块时立刻创建 `UASClass`，而是先把：

- 新模块描述符
- 旧模块描述符
- 模块索引和查找映射

统一塞进内部 `FModuleData`。也就是说，`AddModule()` 的职责不是“生成”，而是**把待处理输入纳入一次统一 reload 批次**。

这层批处理思维很重要，因为后面的依赖传播、struct/class/delegate 的排序 reload，全都建立在“我已经知道本次参与生成的全部模块集合”之上。

## 第二阶段：`SetupModule()` 把模块描述符投影成内部类/委托/枚举数据图

真正开始做内部结构准备的是 `SetupModule(FModuleData&)`。它会：

- 为每个 `NewModule->Classes` 建 `FClassData`
- 通过类名和 `bIsStruct` 去旧模块里找 `OldClass`
- 为每个新类解析并挂上 `ScriptType`
- 建好 `DataRefByNewScriptType`、`DataRefByName` 这些核心查找表
- 对 delegates 做类似的 `FDelegateData` 初始化与类型标记

```cpp
ClassData.NewClass = ClassDesc;
ClassData.OldClass = ...matching old class...;
ClassData.NewClass->ScriptType = ScriptType;
DataRefByNewScriptType.Add(...);
DataRefByName.Add(...);
```

这一步的意义可以概括成：**把“脚本文本模块描述”变成 generator 可追踪的内部图结构。**

也正因为这样，后面 `Analyze()`、`PropagateReloadRequirements()` 和 `EnsureReloaded()` 才能用统一的数据引用（`FModuleData` / `FClassData` / `FDelegateData` / `FDataRef`）来工作，而不需要反复回到原始描述符里做字符串查找。

## 第三阶段：`Analyze()` 先分析差异，再决定 reload 需求

`Analyze(FModuleData&)` 会依次：

- 分析模块里的 delegates
- 分析模块里的 classes
- 初始化并分析 enums
- 检查旧模块里是否有“新模块已不存在”的类，并把它们记入 `RemovedClasses`

而更核心的 per-class 分析发生在 `Analyze(FModuleData&, FClassData&)`。它在当前主干里至少做了这些事：

- 解析 `ClassDesc->ScriptType`
- 如果旧类也有 `ScriptType`，把旧新脚本类型映射写入 `UpdatedScriptTypeMap`
- 如果父类还是脚本类，先 `EnsureClassAnalyzed(...)` 保证父类已分析完
- 枚举脚本属性，建立 `PropertyIndexMap` 和 `PropertyTypes`
- 检查与旧对象/旧类的 Unreal name 冲突
- 在需要时补充 GC 相关隐藏属性
- 逐步提高 `ClassData.ReloadReq`

这说明 `Analyze()` 的职责不是“生成一份类”，而是先回答一组更基础的问题：

- 这个类和旧版本相比变了什么？
- 它依赖哪些别的脚本类型？
- 它能 soft reload 吗，还是必须 full reload？
- 有没有命名冲突、属性问题或不可恢复错误？

因此这一阶段更像 **reload 需求推断器**，而不是 materializer。

## `Setup()`：把所有模块的分析结果汇总成一次 reload 决策

`Setup()` 是真正的“前半场总控”：

```cpp
EReloadRequirement FAngelscriptClassGenerator::Setup()
{
    for (auto& ModuleData : Modules)
        SetupModule(ModuleData);

    for (auto& ModuleData : Modules)
        Analyze(ModuleData);

    for (auto& ModuleData : Modules)
    {
        for (auto& ClassData : ModuleData.Classes)
            PropagateReloadRequirements(ModuleData, ClassData);
        for (auto& DelegateData : ModuleData.Delegates)
            PropagateReloadRequirements(ModuleData, DelegateData);
    }

    EReloadRequirement ReloadReq = EReloadRequirement::SoftReload;
    ... pick max requirement across modules ...
    return ReloadReq;
}
```

这段逻辑很关键，因为它把 generator 的前半场切成了三个明确阶段：

1. **建内部图**（`SetupModule`）
2. **局部分析差异**（`Analyze`）
3. **跨类型传播 reload 需求**（`PropagateReloadRequirements`）

也就是说，generator 在执行 reload 之前，先要把依赖传播完整跑一遍。只有这样，像“某个脚本类依赖了某个发生 structural change 的 struct”这种二阶影响，才会被抬升成同样的 full reload 要求。

## 依赖传播：`PropagateReloadRequirements()` 是整条链的关键放大器

`PropagateReloadRequirements(FModuleData&, FClassData&)` 的作用，不是简单递归父类，而是：

- 如果父类还是脚本类，把父类脚本类型作为 reload 依赖
- 遍历对象类型的本地属性、方法返回值和参数类型
- 把任何 object/subtype 依赖都递归纳入 `AddReloadDependency(...)`
- 通过 `PendingDependees` 和 `ResolvePendingReloadDependees(...)` 把更高的 reload requirement 往后续依赖方传播

这意味着 generator 并不是只看“当前类自己改没改”，而是在做一层 **依赖闭包传播**。

这也解释了为什么 header 中专门把 `FReloadPropagation` 独立成一个基类结构：

- `bStartedPropagating`
- `bFinishedPropagating`
- `bHasOutstandingDependencies`
- `ReloadReq`
- `PendingDependees`

这些字段说明 reload 需求传播本身就是 generator 内部的一等公民，而不是分析阶段顺手带一笔的递归。

## reload 决策：`ShouldFullReload()` 把“建议”和“强制”分成不同层次

头文件里的 `EReloadRequirement` 已经把强度分成：

- `SoftReload`
- `FullReloadSuggested`
- `FullReloadRequired`
- `Error`

而真正落到执行层时，又会被 `ShouldFullReload(...)` 再次解释。例如 `ShouldFullReload(FClassData&)` 里：

- 如果当前批次正在 full reload，且 `ReloadReq >= FullReloadSuggested`，就走 full reload
- interface 类、实现了接口的类，直接 full reload
- brand-new 且不是 statics class 的类，也直接 full reload

这说明 generator 并不是把 `ReloadReq` 当作“最终布尔值”，而是把它当作一个**策略等级**：

- 某些场景属于“如果这次批次已经 full reload，就顺便一起 full reload”
- 某些场景则无论如何都必须 materialize 成新类

因此当前链路不是简单的 `if (changed) full reload else soft reload`，而是一套更细的策略判断。

## 执行阶段：`PerformReload()` 统一编排 full/soft 两种路径

一旦进入执行层，无论是 `PerformFullReload()` 还是 `PerformSoftReload()`，最后都汇到 `PerformReload(bool bFullReload)`。这也是 generator 最核心的 orchestration 函数。

它的执行顺序可以概括成：

1. **先为 full reload 类/struct/delegate 创建壳**
   - `CreateFullReloadStruct()` / `CreateFullReloadClass()` / `CreateFullReloadDelegate()`
   - 非 full reload 的则先 `LinkSoftReloadClasses(...)`
2. **先 reload enums**
3. **再 full reload structs**
4. **再 full reload delegates**
5. **对 soft reload 类做预处理**（`PrepareSoftReload()`）
6. **然后 full reload 非 struct 类**（`DoFullReload(...)`）
7. **最后才执行所有 soft reload**（`DoSoftReload(...)`）
8. **再 finalize 所有 full reload 类**（`FinalizeClass(...)`）
9. **再统一初始化 default objects 和 post init**
10. **最后做 post-reload 广播、清理、GC 和 subsystem 激活**

这套顺序很说明问题：generator 不是“一边发现一边改一边 finalize”，而是按类型和依赖层次分批执行。struct 先于 class，prepare-soft 在 full reload class 前，soft reload 放到所有 full reload 之后，这些顺序都在为**类型依赖稳定性**服务。

## Full Reload 路径：先 `CreateFullReloadClass()`，再 `DoFullReloadClass()`

这条路径的拆分也很刻意。

### `CreateFullReloadClass()`

负责：

- 找到同名旧 `UASClass`
- 把旧类重命名为 `*_REPLACED_*`
- 给旧类打 `CLASS_NewerVersionExists`
- 新建一个带 `RF_Public | RF_Standalone | RF_MarkAsRootSet` 的 `UASClass`

也就是说，这一步做的是 **壳对象 materialization 和旧版本退位**。

### `DoFullReloadClass()`

负责：

- 解析脚本/原生父类关系
- 设置 `ClassFlags`、`bIsScriptClass`、`ClassConfigName`、`SuperStruct`
- 拷贝/修正 editor metadata
- 调 `AddClassProperties()` 生成 `FProperty`
- 创建每个 `UASFunction`
- 设置 native thunk / jit 指针 / function flags

换句话说，`CreateFullReloadClass()` 造的是“类壳”，`DoFullReloadClass()` 填的是“类内容”。这也是为什么 `Hazelight` 对照文档会把类创建写成：

- `CreateFullReloadClass()`
- `DoFullReloadClass()`
- `FinalizeClass()`

三步，而不是一步到位。

## Soft Reload 路径：尽量保住现有 `UASClass`，只替换脚本侧内容

和 full reload 相比，soft reload 明显更偏“保留外壳、更新绑定”：

- 先 `PrepareSoftReload(ModuleData, ClassData)` 做预处理
- 再 `DoSoftReload(...)`

虽然这一轮没有展开 `DoSoftReload()` 的所有细节，但从已有实现和 header 接口已经能看出它的目标：

- 继续沿用旧的 `UASClass`
- 更新 `ScriptTypePtr`
- 重绑定 `UASFunction::ScriptFunction`
- 重新准备 script object / 属性状态
- 不走 brand-new class materialization 路径

因此 soft reload 更像是 **脚本内容换芯**，而不是 **类壳重建**。

## Finalize 阶段：生成器在这里把“类存在”变成“类可用”

`FinalizeClass(...)` 的位置非常关键。它不是分析，也不是 reload 本身，而是把 full reload 后的类变成真正可被运行时和编辑器世界使用的类：

- `SetUpRuntimeReplicationData()`
- 解析 `ComposeOntoClass`
- 挂 implemented interfaces
- 对 interface 类做专门 finalize
- 对 Actor / Component / Object 走不同 finalize 路径
- `NotifyRegistrationEvent(...)` 把类注册到加载系统

这说明 `FinalizeClass()` 真正做的是：**把“已经被创建/装填的类”切换到“可被 UE 系统看见和使用”的状态。**

也正因此，generator 会在全部 reload 操作之后，再统一 finalize 这些 full reload 类，而不会在创建过程中立刻 finalize。

## Post-reload 尾段：广播、清旧、GC、激活新子系统

`PerformReload()` 在 2399 行之后的尾段，补上了 generator 最后那一截经常被忽略的责任：

- `OnFullReload.Broadcast()`：通知 editor-side reinstance 协调器等消费方
- 遍历 `RemovedClasses` 调 `CleanupRemovedClass(...)`
- `OnPostReload.Broadcast(bIsDoingFullReload)`
- 对 `ReplacedClass` / `ReplacedStruct` 清掉旧 `ScriptTypePtr` 和脚本函数指针
- 对所有仍引用旧脚本类型的 `UASClass` 再清一轮 `ScriptTypePtr`
- 如果有重实例化，`ForceGarbageCollection(true)`
- 如果有新 subsystem class，激活 `FSubsystemCollectionBase::ActivateExternalSubsystem(...)`

这说明 generator 的职责并不在“新类造出来”时结束，而是一直延伸到：

- 旧类语义失效
- editor/runtime 消费方收到广播
- 垃圾回收和 subsystem 生命周期重新稳定

因此它不是简单的“编译期生成器”，而是一个**跨创建、替换、后处理的重载编排器**。

## `CleanupRemovedClass()`：旧类退场的正式清算点

`CleanupRemovedClass(...)` 本身也很能说明这条链的完整性：

- 清空 `ScriptTypePtr`、`ConstructFunction`、`DefaultsFunction`
- 把类标记成 `CLASS_NotPlaceable | CLASS_HideDropDown | CLASS_Hidden`
- 对 editor 下的旧 `UASFunction` 也清空 `ScriptFunction`
- `RemoveFromRoot()` + `ClearFlags(RF_Standalone)`

这说明 generator 对“类被移除”不是放任旧壳悬空，而是有一套**正式退场语义**：

- 旧类不再被当成可实例化对象
- 旧脚本函数不再被当成有效脚本实现
- 生命周期标记也被清走。

这也是为什么 `RemovedClasses` 会在 `Analyze(ModuleData)` 阶段就先收集起来：generator 要在 reload 尾段专门清算它们。

## 这条创建 / 重载链应该怎么记

如果把 `FAngelscriptClassGenerator` 的主链路压成一句工程化判断，可以这样记：

**它先把新模块登记成内部类/委托/枚举图，再通过分析和依赖传播把每个类型抬升到合适的 reload requirement，随后在统一的 `PerformReload()` 中按“枚举 → struct → delegate → class → soft reload → finalize → post-reload cleanup”的顺序执行 materialization 与替换，最后用广播、清理、GC 和 subsystem 激活把运行时重新拉回稳定状态。**

换成更实用的阅读过滤器就是：

- 看模块和新旧描述符怎么被纳入批次 → `AddModule()` / `SetupModule()`
- 看 reload requirement 怎么产生和传播 → `Analyze()` / `PropagateReloadRequirements()` / `ShouldFullReload()`
- 看 full reload vs soft reload 怎么执行 → `CreateFullReload*` / `DoFullReload*` / `PrepareSoftReload` / `DoSoftReload`
- 看新类什么时候真正变“可用” → `FinalizeClass()`
- 看旧类和 editor/runtime 外围怎么被收尾 → `OnFullReload` / `OnPostReload` / `CleanupRemovedClass()`

## 小结

- `FAngelscriptClassGenerator` 不是单步类工厂，而是一条完整的创建 / 重载编排链：登记输入、分析差异、传播依赖、执行 reload、finalize 新类、清理旧类
- `Setup()` 的核心价值在于先做完 `SetupModule`、`Analyze` 和 `PropagateReloadRequirements`，再统一决定本轮 reload 强度
- `PerformReload()` 的执行顺序是刻意分层的：struct 先于 class，prepare-soft 先于 soft reload，finalize 和 default object 初始化又在 reload 之后统一进行
- `CleanupRemovedClass()`、`OnPostReload`、GC 和 subsystem 激活说明 generator 的责任一直延伸到 post-reload 稳定化，而不是类壳创建结束就算完成
