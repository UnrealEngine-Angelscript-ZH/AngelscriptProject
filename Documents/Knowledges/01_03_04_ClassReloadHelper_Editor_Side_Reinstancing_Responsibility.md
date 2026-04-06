# `ClassReloadHelper` 的 editor-side 重实例化责任

> **所属模块**: Editor / Test / Dump 协作边界 → ClassReloadHelper / Reinstancing
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptEditor/Private/ClassReloadHelper.h`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/ClassReloadHelper.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp`

这一节真正要讲清楚的，不是 `ClassReloadHelper` “也参与了热重载”，而是它在整个 reload 体系里承担的**editor-side 善后责任**。Runtime 和 `AngelscriptClassGenerator` 负责发现变化、重建类/结构体/委托并给出 reload 结果；`FClassReloadHelper` 则专门负责把这些结果翻译回 Unreal Editor 世界，让蓝图节点、DataTable、属性编辑器、放置面板、打开中的资产和 volume 工厂都从“旧定义”迁到“新定义”。

## 它不是决定 reload 的人，而是消费 reload 结果的人

`FClassReloadHelper::Init()` 一上来就把它的定位写清楚了：它没有主动发起编译，也没有判断是 soft reload 还是 full reload，而是订阅 `FAngelscriptClassGenerator` 暴露出来的一组事件：

- `OnStructReload`
- `OnClassReload`
- `OnDelegateReload`
- `OnLiteralAssetReload`
- `OnEnumChanged`
- `OnEnumCreated`
- `OnFullReload`
- `OnPostReload`

这意味着 `ClassReloadHelper` 的第一责任不是“产出新类”，而是**把 Runtime 侧已经产出的 reload 结果缓存在 editor-side 的 `ReloadState()` 里，并在合适时机执行重实例化与刷新。**

所以它在系统里的位置更接近“编辑器恢复器”而不是“重载决策器”。

## `ReloadState()`：把 Runtime 的变更结果先收成一份 editor-side 快照

`ClassReloadHelper.h` 里的 `FReloadState` 已经把这份快照的内容写得很明白：

- `ReloadClasses`
- `ReloadAssets`
- `NewClasses`
- `ReloadEnums`
- `NewEnums`
- `ReloadStructs`
- `ReloadDelegates`
- `NewDelegates`
- `bRefreshAllActions`
- `bReloadedVolume`

这份状态的意义不是“再复制一份 Runtime 世界”，而是只保留 Editor 做善后时真正关心的信息：

- 哪些旧对象要换成新对象；
- 哪些 Blueprint action 需要局部刷新或全量刷新；
- 哪些变化会触发 volume 几何重建；
- 哪些 enum / delegate / struct 需要额外修补图节点和编辑器缓存。

因此 `ReloadState()` 本身就是一条边界：**Runtime 输出的是 reload 结果，Editor 只缓存足够修补编辑器世界的最小映射。**

## 初始化阶段已经在做第一层 editor-side 维护

`Init()` 并不是单纯把事件记下来，它已经开始做一些轻量、实时的 editor-side 维护：

- `OnClassReload` 会根据是否碰到 interface 相关变化，决定是局部刷新 Blueprint action 还是后面全量刷新
- 对新类，如果是 `UActorComponent` 子类，会立刻 `InvalidateClass(NewClass)`，让组件类型注册表失效
- 对新 volume 类，则先置 `bReloadedVolume = true`，把几何重建推迟到后处理阶段

这说明 `ClassReloadHelper` 的职责不是“等所有东西都完了再统一收尾”，而是：

- 轻量的 editor cache 更新尽早做；
- 真正昂贵或依赖全量结果的重实例化动作留到 `OnFullReload` / `OnPostReload` 再做。

这种分层也解释了为什么它放在 Editor 模块里最合适：这些动作本来就是编辑器缓存与 UI 语义，不属于 Runtime 核心。

## `PerformReinstance()`：真正的 editor-side 重实例化主线

`FReloadState::PerformReinstance()` 是这篇文章的核心。它一上来就先卡了一条很关键的边界：

- 如果 `FAngelscriptEngine::Get().bIsInitialCompileFinished` 为假，直接返回

也就是说，**重实例化只在“已经存在旧世界需要修补”时才有意义**。初始编译没有旧对象世界，自然也没有 editor-side 重实例化责任。

然后它会初始化一个 `UAngelscriptReferenceReplacementHelper`，后面给 Unreal 的重实例化系统做开放资产引用替换。这说明它不只是操作脚本类映射，还要照顾编辑器里已经打开的资产编辑器状态。

## 默认路径：自定义重实例化链，而不是完全依赖 Unreal 原生 reload

当前默认路径受 `angelscript.UseUnrealReload` 控制，默认值是 `0`。在这个默认分支里，`PerformReinstance()` 做的是一条相当完整的 editor-side 修补链：

1. 扫描所有 `UBlueprint`，找出依赖被重载 class/struct/delegate/enum 的蓝图；
2. 更新 Blueprint pin type、用户自定义 pin、macro wildcard、事件签名等引用；
3. 用 `FArchiveReplaceObjectRef` 检查 Blueprint 是否引用了待替换类；
4. 对 `ReloadStructs` 做 `BroadcastPreChange`，并修正依赖该 struct 的 `UDataTable::RowStruct`；
5. 做一次 GC，清掉旧对象；
6. 调 `FBlueprintCompilationManager::ReparentHierarchies(ReloadClasses)` 重建对象层级；
7. 再做一次 GC；
8. 重建相关 Blueprint 节点并把依赖 Blueprint 排入编译队列；
9. `FlushCompilationQueueAndReinstance()`；
10. 对新 struct 做 `BroadcastPostChange`。

这条链说明了一个关键事实：`ClassReloadHelper` 的责任不是只有“换类指针”。它真正做的是把**类、结构体、委托、蓝图图节点、DataTable、对象层级和编辑器缓存**一起拉回到同一个新定义世界里。

## Blueprint 与图节点修补是它最重的一部分责任

在 `PerformReinstance()` 里，最复杂的逻辑几乎都围绕 Blueprint 展开：

- 遍历全部 Blueprint 和全部节点；
- 通过 `HasExternalDependencies()` 找出外部依赖；
- 对 `UK2Node` 的 pins、`UK2Node_EditablePinBase` 的 `UserDefinedPins`、`UK2Node_Event` 的事件签名函数、`UK2Node_MacroInstance` 的 wildcard type 做替换/检查；
- 发现依赖后再调用 schema 的 `ReconstructNode(*Node, true)` 重建节点。

这里的核心责任可以概括为一句话：**Runtime 负责生成新类，`ClassReloadHelper` 负责让 Blueprint 图上的旧类型语义重新指回这些新类。**

如果没有这一层，很多 reload 之后的错误不会立刻出现在运行时，而是以“节点变红”“pin 类型失效”“事件签名不匹配”的形式滞留在 Editor 图面上，直到开发者手工刷新。

## DataTable、Enum、Delegate 也在 editor-side 善后范围内

`ClassReloadHelper` 的责任并不止于 Blueprint class：

- 对 `ReloadStructs`，它会修正依赖 struct 的 `UDataTable::RowStruct`
- 对 enum 变化，会在后处理里 `RefreshAssetActions(ChangedEnum)`
- 对 delegate 重载，会在 Blueprint 节点检查和刷新里作为依赖源参与判断

这说明它处理的是一层更宽的 editor-side 语义一致性，而不是单纯的“类重实例化”。严格来说，它在做的是：**把所有编辑器侧对脚本类型系统的引用面，重新对齐到 Runtime 新生成的定义。**

## `UAngelscriptReferenceReplacementHelper` 负责打开中的资产引用修复

这部分很容易被忽略，但其实特别关键。`UAngelscriptReferenceReplacementHelper` 做了两件事：

- `AddReferencedObjects()` 把 `UAssetEditorSubsystem` 当前打开的资产加入引用收集，避免它们在替换过程中丢失追踪
- `Serialize()` 在对象引用收集阶段把 `OriginalAsset` 替换成 `ReplacedAsset`，并对已打开的 editor instance 发 `NotifyAssetClosed/Opened`

也就是说，它的责任不是编译或 reload 本身，而是保证：**编辑器里那些已经打开着的资产编辑器窗口，不会因为对象替换后还握着旧指针而变成悬空状态。**

这一层是非常典型的 editor-side 责任，因为 Runtime 根本不应该知道资产编辑器窗口现在开着什么。

## 后处理阶段：刷新 UI、工厂和放置面板

`PerformReinstance()` 后半段还有一串很“编辑器”的收尾动作：

- `PropertyEditorModule->NotifyCustomizationModuleChanged()`：强制属性编辑 UI 刷新
- 为新 volume 类重新生成 `UActorFactoryVolume` 工厂实例，并加入 `GEditor->ActorFactories`
- 对新增或变化的 enum 刷新 `FBlueprintActionDatabase` 的 asset actions
- 如果有新类，广播 `IPlacementModeModule::OnAllPlaceableAssetsChanged()` 与 `OnPlaceableItemFilteringChanged()`

这几步非常能说明 `ClassReloadHelper` 的身份：它处理的不是 Runtime 内部状态，而是**Editor 各类缓存、面板、放置系统和工厂注册表**。这些东西只有在 Editor 世界里才存在，所以这份责任天然应该留在 Editor 模块。

## `OnPostReload`：重实例化之后还要做 editor-side 清理和刷新

`OnPostReload` 回调把另一条责任链也补齐了：

- 如果需要，`FBlueprintActionDatabase::RefreshAll()`
- full reload 时 `GEditor->BroadcastBlueprintCompiled()`
- 初编译未完成时，`FComponentTypeRegistry::Get().Invalidate()`
- 如果重载触及 volume，执行 `MAP REBUILD ALLVISIBLE` 并恢复当前 level
- 最后把 `ReloadState()` 重置为一个新的空状态

这说明 `ClassReloadHelper` 的责任不是在 `PerformReinstance()` 结束就停止，而是要一路负责到：

- 编辑器动作列表恢复；
- Blueprint 编译状态广播；
- 特殊几何副作用修复；
- 临时重载状态清空。

所以它更准确的定义是：**editor-side reload reconciliation coordinator**，而不只是一个“调用了 ReparentHierarchies 的 helper”。

## 还保留了一个 Unreal 原生 reload 兼容分支

`ClassReloadHelper.cpp` 开头有一个显式的 CVar：

- `angelscript.UseUnrealReload`

默认值是 `0`，说明当前主路径仍然认为 Unreal 原生 reload 系统还不足以完整覆盖 Angelscript reload 的复杂度，尤其是 `UFunction` 定义变化这类场景。只有打开这个 CVar，才会走 `FReload` 的另一条分支。

这条兼容分支本身也说明了 `ClassReloadHelper` 的责任边界：它不执着于某个具体底层 reload 机制，它的职责是**保证 editor-side 的结果被修补正确**。只要 Unreal 原生 reload 还不能稳定做到这一点，`ClassReloadHelper` 就必须保留自己的善后逻辑。

## 这条责任边界应该怎么记

如果把 `ClassReloadHelper` 的职责压成一句工程化判断，可以这样记：

**Runtime 负责产出“旧定义 → 新定义”的 reload 结果，`ClassReloadHelper` 负责把 Unreal Editor 世界里所有依赖这些定义的蓝图、资产、表格、面板、工厂和缓存重新对齐。**

换成更实用的阅读过滤器就是：

- 遇到 reload 决策、类生成、struct/enum/delegate 新旧映射的产生 → 优先看 Runtime / ClassGenerator
- 遇到 Blueprint 重构、DataTable 行结构替换、属性面板刷新、放置面板广播、volume 几何 rebuild → 优先看 `ClassReloadHelper`

这样就不会把“生成新类型”和“修补编辑器世界”混成同一层职责。

## 小结

- `FClassReloadHelper` 的核心责任不是决定 reload，而是消费 Runtime 已经产出的 reload 结果，完成 editor-side 重实例化与缓存修补
- `ReloadState()` 只缓存 Editor 真正需要的旧新映射、刷新标志和特殊类型集合
- `PerformReinstance()` 覆盖 Blueprint 节点、DataTable、对象层级、打开资产、属性面板、volume 工厂和放置面板等多条 editor-side 善后链
- `OnPostReload` 负责统一收尾和状态清空，因此 `ClassReloadHelper` 更像 reload 之后的 editor reconciliation coordinator，而不只是一个简单 helper
