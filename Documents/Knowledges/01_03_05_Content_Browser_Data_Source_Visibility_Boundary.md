# Content Browser Data Source 的脚本资产可见性边界

> **所属模块**: Editor / Test / Dump 协作边界 → Content Browser Data Source / Visibility Boundary
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptContentBrowserDataSource.h`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptContentBrowserDataSource.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`

这一节真正要钉死的，不是 Content Browser Data Source “把脚本资产显示出来了”，而是它**只负责可见性投影，不负责资产所有权和完整资产工作流**。当前设计非常克制：脚本 literal asset 的真实归属仍然在 Runtime 的 `FAngelscriptEngine::AssetsPackage`；Editor 侧的 `UAngelscriptContentBrowserDataSource` 只是把这批对象映射成 Content Browser 能理解的虚拟条目，并刻意把很多更深的资产能力留空。也正因为这种克制，它才保持了“Runtime 拥有资产，Editor 只负责看见它们”的清晰边界。

## 注册入口已经说明它是 Editor 视图层，而不是 Runtime 资产层

Data Source 的注册入口在 `OnEngineInitDone()`：

```cpp
void OnEngineInitDone()
{
    auto* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage(), "AngelscriptData", RF_MarkAsRootSet | RF_Transient);
    DataSource->Initialize();

    UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
    ContentBrowserData->ActivateDataSource("AngelscriptData");
}
```

这段代码先天就带着一层边界判断：

- Data Source 是 `Transient` 对象，不是 Runtime 状态本身；
- 它注册给的是 `ContentBrowserDataSubsystem`，也就是 Editor 的浏览器数据面；
- 它在 `OnPostEngineInit` 后才激活，说明它依赖 Runtime 已经完成初始化，而不是反过来支配 Runtime。

因此这层的正确理解是：**Editor 在引擎 ready 之后，向 Content Browser 注册一个脚本资产“观察窗口”。** 它不是在创建资产，也不是在接管资产存储。

## Runtime 真正拥有脚本资产包

这条边界在 `FAngelscriptEngine` 里写得很直白：

```cpp
/* The root angelscript UPackage everything should belong to. */
UPackage* AngelscriptPackage = nullptr;
/* The package that all literal assets are put into. */
UPackage* AssetsPackage = nullptr;
```

这意味着脚本资产的所有权并不在 Editor Data Source，而在 Runtime 的 engine 实例里。再结合 `FAngelscriptEngine::ReplaceScriptAssetContent(...)` 这类 API，可以看出 Runtime 才是：

- 资产包的拥有者；
- 脚本资产内容的权威写入者；
- 脚本资产生命周期的实际宿主。

所以 Data Source 做的事情，本质上是：**读取 Runtime 已经拥有的脚本资产包，再把其中对象投影到 Content Browser。**

## `CreateAssetItem()`：把 Runtime 对象映射成 Editor 虚拟条目

`CreateAssetItem(UObject* Asset)` 很能说明这是一层“投影”而不是“复制”：

- payload 只保存 `Path` 和 `TWeakObjectPtr<UObject> Asset`
- `Payload->Path = Asset->GetPathName()` 直接指向真实对象路径
- Content Browser 虚拟路径统一映射到 `"/All/Angelscript/" + Asset->GetName()`
- 显示名会把 `Asset_` 前缀裁掉，变成更适合浏览器显示的名字

这里的几个选择都很有意味：

- 用 `TWeakObjectPtr`，说明 Data Source 不拥有资产；
- 用虚拟路径 `/All/Angelscript/...`，说明它提供的是浏览视图，而不是原始 package 树；
- 保留底层真实 `Path` 作为 payload，说明 Editor 视图随时可以回到 Runtime 对象本体。

因此 `CreateAssetItem()` 的角色可以概括成：**把 Runtime 里的真实对象，包装成 Editor Content Browser 可消费的虚拟文件项。**

## 枚举边界：它只看 `AssetsPackage` 下的对象

真正决定“哪些脚本资产能被看见”的核心代码在 `EnumerateItemsMatchingFilter(...)`：

```cpp
TArray<UObject*> Assets;
GetObjectsWithOuter(FAngelscriptEngine::Get().AssetsPackage, Assets);

for (UObject* Object : Assets)
{
    ... class include / exclude filtering ...
    InCallback(CreateAssetItem(Object));
}
```

这段逻辑把可见性边界写得非常清楚：

- 它不会扫描整个 AssetRegistry；
- 也不会遍历项目所有 package；
- 它只看 `FAngelscriptEngine::Get().AssetsPackage` 下面的对象；
- 再根据类 include/exclude 过滤，决定哪些对象真的暴露到浏览器里。

换句话说，Content Browser Data Source 的边界不是“所有和 Angelscript 有关的东西”，而是：**Runtime 当前 engine 资产包里、并且命中过滤条件的那一批对象。**

这条边界非常重要，因为它避免了 Data Source 变成一台模糊的全局搜索器。它只服务 Runtime 明确拥有的脚本资产视图。

## 过滤边界：只响应根路径和类过滤，不做全功能浏览器

`CompileFilter(...)` 又把另一层边界补齐了：

- 只有 `IncludeFiles + IncludeAssets` 同时成立才继续；
- `InPath != "/"` 时直接返回；
- 没有 `FContentBrowserDataClassFilter` 时直接返回；
- 只把 class include / exclude 名称解析成 `UClass*` 集合。

这说明当前 Data Source 并不试图实现一个完整的多路径、多层级浏览树。它实际上依赖的是：

- Content Browser 想看的是资产文件项；
- 调用方最好已经给了类过滤范围；
- 路径视角统一从根视图进入。

因此这里的“可见性边界”不只是对象来源边界，还有**查询能力边界**：这个 Data Source 只为脚本资产可见性服务，不承担完整的通用内容浏览协议实现。

## 它明确标注“这是项目资产”，但不是插件资产也不是引擎资产

`GetItemAttribute(...)` 里还有几行很关键的属性声明：

- `ItemTypeName / ItemTypeDisplayName` → `"Script Asset"`
- `ItemIsEngineContent` → `false`
- `ItemIsProjectContent` → `true`
- `ItemIsPluginContent` → `false`

这说明 Data Source 在 Editor 语义上，把这批脚本资产定位成：

- 一种特殊资产类型；
- 对用户来说属于 project content 视图；
- 但实现来源上又不是 AssetRegistry 里的普通项目资产。

这种表述其实是另一种“投影”证据：**它在 Editor 世界里把 Runtime-owned 脚本资产伪装成了项目内容的一部分，以便浏览体验自然，但并没有改变它们真正的宿主仍然是 Runtime 资产包这一事实。**

## 很多方法故意留空，正是在守边界

这份实现里有一批显眼的“空实现”或直接 `false`：

- `EnumerateItemsAtPath()` 空着
- `EnumerateItemsForObjects()` 返回 `false`
- `CanEditItem()` / `EditItem()` / `BulkEditItems()` 返回 `false`
- `GetItemPhysicalPath()` 返回 `false`
- `TryGetCollectionId()` 返回 `false`
- `Legacy_TryConvertPackagePathToVirtualPath()` / `Legacy_TryConvertAssetDataToVirtualPath()` 返回 `false`

这些空实现不是没做完那么简单，它们恰好体现了当前边界策略：

- Data Source 提供的是**可见性**，不是完整编辑能力；
- 它可以给 Content Browser 资产项、属性和缩略图；
- 但它不承诺物理路径、集合语义、双向路径转换、批量编辑或对象到条目的完全映射。

也就是说，当前系统刻意只实现“够把脚本资产稳定显示出来”的最小面，而不是把 Runtime 资产包强行伪装成全功能 UE 资产系统。

## 缩略图和 AssetData 说明它愿意借用 Editor 表面协议，但不转移所有权

虽然很多深层方法留空，但 `UpdateThumbnail()` 和 `Legacy_TryGetAssetData()` 又说明它不是一个纯假对象层：

- `UpdateThumbnail()` 会用底层 `UObject*` 构造 `FAssetData`，让 `FAssetThumbnail` 正常工作
- `Legacy_TryGetAssetData()` 也会把 payload 里的对象包装成 `FAssetData`
- `Legacy_TryGetPackagePath()` 则把 payload 里真实对象路径回传给调用方

这说明 Data Source 的边界并不是“我只给你个显示名字”。更准确地说，它提供的是：

- **足够的 Content Browser 表面协议**，让脚本资产能被浏览、过滤、显示缩略图；
- **不足以让它被误当成完整原生资产体系** 的有限实现。

这是一种非常克制的集成方式：能看、能识别、能带缩略图，但不假装自己拥有完整 asset pipeline。

## Runtime / Editor 的职责分工可以压成一句话

如果把这条边界压成一句工程化判断，可以这样记：

**Runtime 拥有脚本 literal asset 的真实 package 和内容写回权；Editor 的 `UAngelscriptContentBrowserDataSource` 只负责把这批对象投影成 `/All/Angelscript/` 下可过滤、可显示、带缩略图的浏览器条目。**

换成更实用的阅读过滤器就是：

- 遇到资产包、脚本资产内容、调试客户端保存回写 → 优先看 `FAngelscriptEngine`
- 遇到 `/All/Angelscript/` 视图、过滤、类型属性、缩略图 → 优先看 `UAngelscriptContentBrowserDataSource`
- 遇到“为什么这里不能直接编辑/没有物理路径/不支持某些路径转换” → 把它理解成当前 Data Source 故意留下的边界，而不是单纯缺功能

## 小结

- `UAngelscriptContentBrowserDataSource` 的核心职责是脚本资产可见性投影，而不是脚本资产所有权管理
- 它只枚举 `FAngelscriptEngine::Get().AssetsPackage` 下的对象，再映射成 `/All/Angelscript/` 视图
- 类过滤、类型属性、缩略图和 `FAssetData` 支持说明它愿意接入 Editor 浏览协议；大量返回 `false` 的方法则说明它刻意不越界成完整资产系统
- 因此这条边界可以概括为：Runtime 持有资产，Editor 让资产“看得见”，但不伪装成完整的原生内容管线
