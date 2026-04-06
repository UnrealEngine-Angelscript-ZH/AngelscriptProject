# Source Code Navigation 的 Editor-Runtime 桥接

> **所属模块**: Editor / Test / Dump 协作边界 → Source Navigation / Editor-Runtime Bridge
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptSourceCodeNavigation.cpp`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Plugins/Angelscript/Source/AngelscriptTest/Editor/AngelscriptSourceNavigationTests.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`

这一节真正要钉死的，不是“Editor 能跳到脚本源码”，而是这件事为什么是一个典型的 Editor-Runtime 桥接问题。导航动作发生在 Editor 里，入口挂在 `FSourceCodeNavigation` 这样的编辑器设施上；但 Editor 自己并不知道某个脚本类、属性或函数对应哪一个 `.as` 文件、哪一行，它必须向 Runtime 已经生成出来的脚本元数据借位置信息。因此这条链的本质是：**Editor 提供交互壳，Runtime 提供源码定位真相。**

## 注册入口已经说明这是 Editor 提供的导航壳

`AngelscriptEditorModule.cpp` 里并没有直接实现导航逻辑，它只在模块启动时把导航处理器注册进去：

```cpp
extern void RegisterAngelscriptSourceNavigation();

void FAngelscriptEditorModule::StartupModule()
{
    FClassReloadHelper::Init();
    RegisterAngelscriptSourceNavigation();
    ...
}
```

这段关系很重要，因为它先把边界分清了：

- `StartupModule()` 只负责把 Source Navigation 功能挂进 Editor 生命周期；
- 真正的“能不能导航、导航到哪里”逻辑不写在模块入口里；
- 具体实现则交给 `AngelscriptSourceCodeNavigation.cpp` 里的处理器。

因此这层的正确理解是：**Editor 模块负责把导航桥接能力接入编辑器，但不负责持有源码定位数据库。**

## `FAngelscriptSourceCodeNavigation`：Editor 侧的协议适配器

`AngelscriptSourceCodeNavigation.cpp` 的核心类是 `FAngelscriptSourceCodeNavigation : public ISourceCodeNavigationHandler`。它实现的不是 Angelscript 自己的一套导航协议，而是 Unreal Editor 已有的源码导航接口：

- `CanNavigateToClass` / `NavigateToClass`
- `CanNavigateToFunction` / `NavigateToFunction`
- `CanNavigateToProperty` / `NavigateToProperty`
- `CanNavigateToStruct` / `NavigateToStruct`

这说明它在架构上的角色不是“脚本系统内部工具”，而是一个**把 Runtime 元数据翻译成 Editor 源码导航语义的适配器**。

从职责看，它做两件事：

1. 在 Editor 协议层回答“当前对象能不能导航”；
2. 如果能，就把它翻译成文件路径和行号，再调用宿主环境去打开编辑器。

因此它是一座很典型的桥：上游是 Editor 的 `ISourceCodeNavigationHandler`，下游是 Runtime 里的 `FAngelscriptClassDesc` / `FAngelscriptModuleDesc` / `UASFunction` 源码定位信息。

## 类 / 结构体 / 属性导航：走 Runtime 的类描述符

对类、结构体和属性来说，导航桥接的关键在 `GetClassDesc(const UStruct* Struct, ...)`：

```cpp
TSharedPtr<FAngelscriptClassDesc> GetClassDesc(const UStruct* Struct, TSharedPtr<FAngelscriptModuleDesc>* OutModule = nullptr)
{
    auto* ASClass = Cast<const UASClass>(Struct);
    if (ASClass != nullptr)
    {
        return FAngelscriptEngine::Get().GetClass(ASClass->GetPrefixCPP() + ASClass->GetName(), OutModule);
    }

    auto* ASStruct = Cast<const UASStruct>(Struct);
    if (ASStruct != nullptr)
    {
        return FAngelscriptEngine::Get().GetClass(ASStruct->GetPrefixCPP() + ASStruct->GetName(), OutModule);
    }

    return nullptr;
}
```

这段代码把桥接逻辑写得非常清楚：

- Editor 侧拿到的是 `UClass` / `UStruct` / `UScriptStruct` 这类 Unreal 类型对象；
- 如果它们实际上是 `UASClass` / `UASStruct`，就能回到 Runtime 的脚本类描述符；
- 真正的定位数据来自 `FAngelscriptEngine::Get().GetClass(...)` 返回的 `FAngelscriptClassDesc` 和所属 `FAngelscriptModuleDesc`。

也就是说，Editor 自己不持有“这个脚本类定义在哪个文件、哪一行”；它只是把 Runtime 已经掌握的类元数据重新用来做源码导航。

对属性导航也是同样的模式：

- 先用 `InProperty->GetOwnerStruct()` 找到所属脚本类；
- 再从 `ClassDesc->GetProperty(InProperty->GetNameCPP())` 里找到属性描述符；
- 最后用属性描述符的 `LineNumber` 跳转。

所以类 / struct / property 的导航路径本质上都是：**Editor 侧对象 → Runtime 类描述符 → 模块源码位置。**

## 函数导航：直接走 `UASFunction` 的源码元数据

函数这条链比类导航更直接，因为 `UASFunction` 自己已经带了源码位置：

```cpp
virtual bool NavigateToFunction(const UFunction* InFunction) override
{
    auto* ASFunc = Cast<const UASFunction>(InFunction);
    if (ASFunc == nullptr)
        return false;

    FString Path = ASFunc->GetSourceFilePath();
    if (Path.Len() == 0)
        return false;

    OpenFile(Path, ASFunc->GetSourceLineNumber());
    return true;
}
```

这里的边界非常漂亮：

- Editor 侧只负责识别“这是不是一个脚本函数”；
- 一旦它是 `UASFunction`，源码路径和源码行号就直接从 Runtime 产出的函数对象里拿；
- 不需要再回头查模块描述符或手动推导函数位置。

这说明 Runtime 在函数层已经把“源码定位信息”内嵌进了生成出来的 Unreal 函数对象，而 Editor 导航器只是消费这份元数据。

因此函数导航可以看成另一种桥接形式：**Editor 协议层 -> Runtime materialized function metadata**。

## 最后的宿主动作：用 VS Code 打开真实文件

无论是 `OpenModule(...)` 还是 `OpenFile(...)`，最终执行的都是宿主层动作：

- `FPlatformMisc::OsExecute(nullptr, TEXT("code"), ...)`
- 带 `--goto "path:line"` 参数时直接跳到目标行

这一步很值得单独记住，因为它再次说明边界：

- Runtime 提供“文件和行号”；
- Editor 处理器把它变成操作系统层的源码打开命令；
- 它并不尝试自己实现源码编辑器，而是借宿主环境里已有的 `code` 命令把用户带到正确位置。

所以这条桥的完整形态其实是三段：

- Runtime 提供定位真相；
- Editor 提供导航协议适配；
- 宿主工具（VS Code）负责最终打开文件。

## `CanNavigateTo*` 和 `NavigateTo*` 的关系：先做类型守门，再做路径解析

这个处理器还有一个很清楚的设计习惯：

- `CanNavigateToClass` / `CanNavigateToProperty` / `CanNavigateToStruct` 都是先尝试取到 `ClassDesc`
- `CanNavigateToFunction` 则先检查它是不是 `UASFunction`

换句话说，导航器不会乐观地假设任何 `UClass` / `UFunction` 都能跳，而是先做“这是不是 Angelscript 产物”的守门。这也正是桥接层应有的职责：**过滤掉不属于脚本系统的普通 Unreal 对象，只为 Runtime 能解释的对象提供导航。**

如果拿不到脚本元数据，处理器就返回 `false`，让 Editor 回退到默认行为或不提供导航入口。这样可以避免把 editor-side 的通用源码导航和脚本导航混成一锅。

## 测试验证说明这不是纯 UI 便利，而是一条可验证的桥接契约

`Plugins/Angelscript/Source/AngelscriptTest/Editor/AngelscriptSourceNavigationTests.cpp` 把这条桥接契约测得很直接。测试做了几件关键事：

- 通过 `AcquireProductionLikeEngine(...)` 拿到 production-like engine
- 用内存脚本编译出一个带 `UFUNCTION` 的脚本类
- 找到生成后的 `UClass` 和 `UFunction`
- 确认生成的函数确实 materialize 成 `UASFunction`
- 检查 `GetSourceFilePath()` 和 `GetSourceLineNumber()` 是否保留正确
- 最后断言 `FSourceCodeNavigation::CanNavigateToClass(...)` 和 `CanNavigateToFunction(...)` 都为真

这说明 Source Navigation 在当前架构里并不是一个“手动点一下看看能不能跳”的 UI 小功能，而是一条**可自动化验证的 Editor-Runtime 桥接契约**：

- Runtime 必须把正确源码元数据挂到生成类型上；
- Editor 导航处理器必须能识别这些脚本类型；
- 宿主侧导航 API 至少要能判定它们可导航。

因此它很值得单独成文，而不只是作为 `StartupModule()` 里的一行注册语句略过。

## 为什么这项能力不应该写回 Runtime

Source Navigation 看起来和 Runtime 元数据绑定得很深，但它仍然不应该被下沉回 Runtime。原因是：

- Runtime 负责的是生成脚本类、函数和描述符；
- “怎么在 Editor 里打开源码”是纯 editor-side 的行为；
- `ISourceCodeNavigationHandler`、`FSourceCodeNavigation`、`OsExecute("code", ...)` 都是明显的宿主/UI 协议，不属于 Runtime 核心能力。

因此当前分层刚好合理：

- **Runtime** 负责保留脚本对象与源码位置的映射关系；
- **Editor** 负责把这份映射关系变成用户可点击、可导航的交互能力。

## 这条桥接边界应该怎么记

如果把 `Source Code Navigation` 这条链压成一句工程化判断，可以这样记：

**Runtime 负责把脚本类、结构体、属性和函数与源文件/行号绑定起来；Editor 负责把这些元数据挂到 `ISourceCodeNavigationHandler` 上，并把最终导航动作转成宿主源码编辑器调用。**

换成更实用的阅读过滤器就是：

- 遇到 `UASClass` / `UASStruct` / `UASFunction` 和 `FAngelscriptClassDesc` / `FAngelscriptModuleDesc` 的源码位置 → 优先看 Runtime 元数据
- 遇到 `CanNavigateTo*` / `NavigateTo*` / `FSourceCodeNavigation::AddNavigationHandler(...)` → 优先看 Editor 桥接层
- 遇到 `code --goto` 这类命令 → 把它理解成宿主工具集成，而不是脚本系统本体

## 小结

- `FAngelscriptSourceCodeNavigation` 是一层 Editor 协议适配器，不是 Runtime 内部工具
- 类、结构体和属性导航通过 `FAngelscriptEngine::Get().GetClass(...)` 回到 Runtime 的类描述符；函数导航则直接消费 `UASFunction` 上的源码路径与行号
- 这条桥接链的最终动作是把 Runtime 的定位元数据转成 VS Code 的 `code --goto` 调用
- `AngelscriptSourceNavigationTests.cpp` 说明这不是纯 UI 便利，而是一条可自动化验证的 Editor-Runtime 桥接契约
