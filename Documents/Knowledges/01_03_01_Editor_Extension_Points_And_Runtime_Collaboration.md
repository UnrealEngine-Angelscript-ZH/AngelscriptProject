# Editor 扩展点与 Runtime 协作

> **所属模块**: Editor / Test / Dump 协作边界 → Editor 与 Runtime 协作
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/ClassReloadHelper.h`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/ClassReloadHelper.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptContentBrowserDataSource.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.h`

这一节真正要钉死的不是“Editor 模块里有哪些功能”，而是它和 Runtime 的关系：`AngelscriptEditor` 并不拥有另一套脚本系统，它更像 Runtime 的编辑器接缝层。脚本引擎、编译总线、热重载决策、脚本资产包这些核心状态仍在 Runtime 一侧；Editor 负责把这些能力接进 Unreal Editor 的事件、UI、内容浏览器和蓝图重实例化工作流里。

## 先看协作总图

- Runtime 负责脚本系统状态、编译、热重载、类生成和调试服务
- Editor 负责监听编辑器事件，把这些事件整理成 Runtime 能理解的输入
- Runtime 再把阶段信号、脚本资产和重载结果暴露回来，供 Editor 侧扩展 UI 和重实例化流程使用

如果把这一层压成最小流程图，大致是：

```text
Editor event / UI / asset tools
    -> AngelscriptEditor
        -> FAngelscriptEngine / FAngelscriptRuntimeModule
            -> compile / reload / asset state / class generation
        <- runtime delegates / assets package / reload events
    -> Editor refresh / content browser / blueprint tools
```

所以这不是“Editor 调 Runtime 做一点小事”，而是一条双向协作链：**Editor 提供输入面，Runtime 提供权威状态和阶段信号。**

## `StartupModule()`：Editor 侧协作点的总挂载处

`FAngelscriptEditorModule::StartupModule()` 是当前协作面的总入口。它在一个地方把几个最关键的钩子全挂上了：

```cpp
void FAngelscriptEditorModule::StartupModule()
{
    FClassReloadHelper::Init();
    RegisterAngelscriptSourceNavigation();

    FCoreDelegates::OnPostEngineInit.AddStatic(&OnEngineInitDone);
    AngelscriptEditor::Private::RegisterStateDumpExtension(StateDumpExtensionHandle);

    // Register a directory watch on the script directory so we know when to reload
    DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(..., &OnScriptFileChanges, ...);

    FAngelscriptRuntimeModule::GetDebugListAssets().AddLambda(...);
    FAngelscriptRuntimeModule::GetEditorCreateBlueprint().AddLambda(...);

    FCoreUObjectDelegates::OnObjectPreSave.AddStatic(&OnLiteralAssetPreSave);
    UToolMenus::RegisterStartupCallback(...);
}
```

从这一段就能直接看出 Editor 的五类职责：

1. 初始化重载协作器 `FClassReloadHelper`
2. 注册内容浏览器 / 源码导航 / tools menu 这类 editor-only 入口
3. 监听脚本目录文件变化并回送 Runtime
4. 订阅 Runtime 暴露出来的 delegate 信号，驱动 editor UI 行为
5. 把 editor-specific 的状态扩展重新注册回 Runtime 观察体系

也就是说，`StartupModule()` 本身就是“Editor 如何接到 Runtime 身上”的总接线板。

## 第一类协作：目录监听把编辑器事件变成 Runtime 的重载输入

最直接的 Editor → Runtime 协作点，是脚本文件变化监听：

```cpp
void OnScriptFileChanges(const TArray<FFileChangeData>& Changes)
{
    if (!FAngelscriptEngine::IsInitialized())
        return;

    FAngelscriptEngine& AngelscriptManager = FAngelscriptEngine::Get();
    AngelscriptEditor::Private::QueueScriptFileChanges(
        Changes,
        AngelscriptManager.AllRootPaths,
        AngelscriptManager,
        IFileManager::Get(),
        [&AngelscriptManager](const FString& AbsoluteFolderPath)
        {
            return AngelscriptEditor::Private::GatherLoadedScriptsForFolder(AngelscriptManager, AbsoluteFolderPath);
        });
}
```

这里的职责边界非常清楚：

- `DirectoryWatcher` 和文件系统事件属于 Editor 环境
- 但真正的“哪些脚本已加载、哪些 root path 有效、这些变化应该如何入队”则依赖 Runtime 的 `FAngelscriptEngine`

`AngelscriptDirectoryWatcherInternal.h` 的接口又把这个边界再钉死了一次：

- `GatherLoadedScriptsForFolder(FAngelscriptEngine& Engine, ...)`
- `QueueScriptFileChanges(..., FAngelscriptEngine& Engine, ..., FEnumerateLoadedScripts)`

也就是说，Editor 负责**感知变化**，Runtime 负责**解释变化**。这恰好是一个很干净的协作切口：文件监听不进入 Runtime，脚本装载状态也不复制到 Editor。

## 第二类协作：内容浏览器数据源把 Runtime 资产暴露成 Editor 视图

第二个非常典型的协作点，是 `OnEngineInitDone()` 里注册 `UAngelscriptContentBrowserDataSource`：

```cpp
void OnEngineInitDone()
{
    auto* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(...);
    DataSource->Initialize();

    UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
    ContentBrowserData->ActivateDataSource("AngelscriptData");
}
```

这里为什么要等 `OnPostEngineInit` 之后再做，很能说明协作顺序：内容浏览器是 Editor 设施，但它要展示的数据来自 Runtime 已经初始化出来的脚本资产世界，所以必须等引擎和 Runtime 足够稳定后再激活。

`UAngelscriptContentBrowserDataSource` 本身则直接读 Runtime 状态：

- 它通过 `FAngelscriptEngine::Get().AssetsPackage` 枚举脚本资产
- 再把这些对象包装成 `/All/Angelscript/...` 下的 Content Browser item
- 过滤和属性查询仍然走 Editor 的 `ContentBrowserDataSource` 语义

也就是说，这一层的关系不是“Editor 自己维护一份脚本资产缓存”，而是：**Runtime 持有脚本资产包，Editor 只是给它加一层浏览器视图。**

## 第三类协作：`FClassReloadHelper` 订阅 Runtime 类生成结果，完成 editor-side 重实例化

`FClassReloadHelper::Init()` 是当前最关键的 Runtime → Editor 协作点之一。它没有自己发起编译，而是订阅 `FAngelscriptClassGenerator` 暴露出来的一组 reload 事件：

- `OnStructReload`
- `OnClassReload`
- `OnDelegateReload`
- `OnLiteralAssetReload`
- `OnEnumChanged`
- `OnEnumCreated`
- `OnFullReload`
- `OnPostReload`

这组订阅关系本身就说明了职责边界：**Runtime 负责判定和产出重载结果，Editor 负责把这些结果翻译成 Blueprint / Component / Volume / DataTable 世界里的重新挂接动作。**

`ClassReloadHelper.cpp` 里的 `PerformReinstance()` 又把 editor-side 处理写得很明确：

- 刷新 Blueprint pin type 和外部依赖
- 更新依赖重载 struct 的 `UDataTable::RowStruct`
- 调用 `FBlueprintCompilationManager::ReparentHierarchies(...)` 做类重实例化
- 刷新 Blueprint action database
- 当重载触及 volume 时，触发 geometry rebuild

这一层的关键不是“它做了很多 Editor 事”，而是它**只在 Runtime 已经给出 reload 结果之后**才介入。Editor 没有自己重算脚本类差异，也没有自己决定 soft/full reload；它只是消费 Runtime 的结果并把 Unreal Editor 世界修正到新状态。

## 第四类协作：Runtime delegate 暴露 editor-only 能力入口

`FAngelscriptRuntimeModule.h` 里有一组特别典型的 delegate 接缝：

- `GetDebugListAssets()`
- `GetEditorCreateBlueprint()`
- `GetEditorGetCreateBlueprintDefaultAssetPath()`

这些 delegate 的意义不在于“提供几个 helper”，而在于 Runtime 明确承认：有些行为必须由 Editor 来做，但触发时机来自 Runtime 或调试流程。

在 `StartupModule()` 里，Editor 侧把这几个入口接起来：

- `GetDebugListAssets()` → `ShowAssetListPopup(...)`
- `GetEditorCreateBlueprint()` → `ShowCreateBlueprintPopup(...)`

这是一种很干净的依赖方向：

- Runtime 不直接依赖具体 editor widget 或 popup 实现
- Editor 也不需要侵入 Runtime 内部逻辑，只需要把自己的 UI 行为绑定到 Runtime 暴露的 delegate 上

所以这组 delegate 可以看成 **Runtime 向 Editor 借能力的正式接口**。

## 第五类协作：编辑器保存和生成动作反向写回 Runtime

除了 watcher 和 delegate，Editor 还会把一些编辑器行为反向写回 Runtime 状态。

最明显的例子是 literal asset 保存：

- `OnLiteralAssetPreSave(...)` 只在 editor 且 engine 已初始化时生效
- 如果对象属于 `FAngelscriptEngine::Get().AssetsPackage`，就进入 `OnLiteralAssetSaved(...)`
- 最终通过 `FAngelscriptEngine::Get().ReplaceScriptAssetContent(...)` 把编辑器侧修改回写到 Runtime 脚本资产内容中

这说明 Editor 在这里也不是单纯的“可视化层”，它还承担了**把编辑器资产编辑动作重新转换回 Runtime 脚本数据**的职责。

另一个例子是 `GenerateNativeBinds()`：

- Editor 侧扫描 `UClass` 和头文件路径
- 再把结果写进 `FAngelscriptBinds::GetRuntimeClassDB()` / `GetEditorClassDB()`
- 最后用 `FAngelscriptEngine::GetScriptRootDirectory()` 输出 bind module cache 和生成文件

这其实是一个典型的 editor-driven codegen 扩展点：输入来自 Editor 可见的反射和源码导航能力，产出服务 Runtime 的 bind 模块体系。

## State Dump 扩展说明了另一种协作方式：Editor 作为 Runtime 观察面的扩展者

在 `StartupModule()` 里还有一行容易被忽略，但很能说明边界：

- `AngelscriptEditor::Private::RegisterStateDumpExtension(StateDumpExtensionHandle);`

这说明 Editor 不仅消费 Runtime，还会把 editor-specific 状态扩展注册回 Runtime 的 dump/观察体系里。也就是说，Runtime 负责提供统一的观察总线，而 Editor 可以把自己这边的重载状态、菜单状态或其他 editor-side 信息挂进去。

这种模式和前面的 watcher / delegate / content browser 不一样：它不是输入链路，而是**观察链路**。但它同样说明两边的关系不是一次性的单向调用，而是围绕 Runtime 总线做持续扩展。

## 这层协作边界应该怎么记

如果把 `AngelscriptEditor` 和 `AngelscriptRuntime` 的关系压成一句工程化判断，可以这样记：

**Runtime 拥有脚本系统的权威状态与阶段信号；Editor 负责把 Unreal Editor 的事件、视图、菜单、资产和蓝图重实例化流程接到这些状态与信号上。**

因此后面读代码时可以用一个很实用的过滤器：

- 遇到脚本目录变更、内容浏览器、popup、tool menu、Blueprint action refresh，优先回 Editor
- 遇到引擎状态、脚本根目录、资产包、类生成结果、reload requirement、delegate 信号，优先回 Runtime
- 如果两边都出现，优先看它们是通过 watcher、delegate、data source 还是 helper 在协作，而不要默认认为它们在共享同一层职责

## 小结

- `AngelscriptEditor` 不是另一套脚本系统，而是 Runtime 的 editor-side 接缝层
- `StartupModule()` 统一挂起了目录监听、内容浏览器数据源、Runtime delegate、literal asset 保存和 tools menu 这些 editor 扩展点
- `OnScriptFileChanges()` / `QueueScriptFileChanges()` 展示了“Editor 感知变化，Runtime 解释变化”的协作边界
- `FClassReloadHelper` 则展示了“Runtime 产出重载结果，Editor 完成蓝图与对象重实例化”的另一条协作链
