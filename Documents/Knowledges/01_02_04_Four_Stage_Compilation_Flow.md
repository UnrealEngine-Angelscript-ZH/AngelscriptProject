# Types / Functions / Code / Globals 四阶段编译流

> **所属模块**: Runtime 总控与生命周期 → 四阶段编译流
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.cpp`

前一节已经把 `FAngelscriptEngine` 的主链路讲清楚了，这一节只把其中最核心、也最容易被说成“就是编译一下”的部分拆开：当前 Angelscript Runtime 的模块编译不是一次黑盒 Build，而是被明确拆成 `Types -> Functions -> Code -> Globals` 四个阶段。这个拆分不是为了代码好看，而是因为类、函数、字节码、全局变量的可见性和依赖时机根本不同，必须按阶段推进，才能同时支持首编译、预编译数据恢复和热重载。

## 先看接口层给出的阶段定义

`AngelscriptEngine.h` 直接把四个阶段函数列成了稳定接口：

```cpp
ECompileResult CompileModules(...);
void CompileModule_Types_Stage1(...);
void CompileModule_Functions_Stage2(...);
void CompileModule_Code_Stage3(...);
void CompileModule_Globals_Stage4(...);
```

这一层很重要，因为它说明四阶段不是 `CompileModules()` 里的临时实现细节，而是当前 Runtime 明确承认的一套编译协议。换句话说，后面无论是首编译还是热重载，都会被送进同一条四阶段流水线，而不是用不同模式走不同编译器。

## `CompileModules()`：四阶段的总编排器

整个四阶段编译流都由 `CompileModules()` 编排。它先做几件准备工作：

- 广播 `GetPreCompile()` delegate
- 建立 `CompilingModulesByName` / `CompilingClassesByName` 这些查找表
- 在非首编译场景下，把旧模块的类型和全局变量从当前引擎可见性里移走
- 打开 `deferValidationOfTemplateTypes` / `deferCalculatingTemplateSize`，把模板校验和模板大小计算推迟到后面统一做

这说明 `CompileModules()` 并不是简单的 `for module -> compile`，而是一台真正的**阶段调度器**：它会先准备一个允许“半成品类型先出现、后续再补函数/布局/全局”的编译环境，然后再让四个阶段依次推进。

## Stage 1：Types —— 先把模块壳和类型世界建起来

`CompileModule_Types_Stage1()` 做的事情可以概括为：**先创建脚本模块、导入依赖、装配 pre-class data，再把源码 section 加进去，让类型系统有机会先成立。**

它最关键的动作包括：

- 为当前模块创建一个临时脚本模块名；非初始编译时，会创建 `*_NEW_*` 临时模块，避免直接污染旧模块
- 把 imported modules 的可见内容导入到新模块里，并把依赖哈希折叠进当前模块的 `CombinedDependencyHash`
- 如果预编译数据完全匹配，就直接 `ApplyToModule_Stage1()`，跳过源码路径
- 为继承自 code class 的脚本类注入 `asPreClassData`
- 为 delegate / multicast delegate 注入 userdata tag
- 最后把每个代码 section 真正 `AddScriptSection(...)` 到脚本模块里

```cpp
void FAngelscriptEngine::CompileModule_Types_Stage1(...)
{
    auto* ScriptModule = (asCModule*)Engine->GetModule(..., asGM_ALWAYS_CREATE);
    ImportIntoModule(ScriptModule, ImportModule->ScriptModule);
    // ...precompiled stage1 / pre-class data / delegate tags...
    ScriptModule->AddScriptSection(...);
    Module->ScriptModule = ScriptModule;
}
```

为什么这一步必须放在第一阶段？因为后面无论是函数签名生成、类布局还是字节码生成，都建立在“模块已经存在、类型声明已经有壳、依赖模块已经导入”的前提上。没有 Stage 1，后面的函数和代码根本无从谈起。

## Stage 1 之后还有两个“半步”

`CompileModules()` 在 Stage 1 之后，并没有立刻进入 Stage 2，而是插了两个很关键的中间动作：

- `BuildParallelParseScripts()`：并行解析脚本源码
- `BuildGenerateTypes()`：在解析完成后真正生成类型

这两步不是新的“Stage 1.5”，但它们解释了 Stage 1 的真正含义：**Stage 1 更像是类型阶段的装配入口，而不是简单的“声明一下 types 就完了”。** 它先把模块壳和依赖环境搭好，再让解析和类型生成在这个环境上完成。

因此从职责上看，Stage 1 实际上覆盖了：

- 模块创建
- 依赖导入
- 预编译数据 stage1 恢复
- 脚本 section 装载
- 解析准备
- 类型生成准备与完成

## Stage 2：Functions —— 在类型存在之后生成函数世界

`CompileModule_Functions_Stage2()` 很短，但它的前置条件非常强：类型必须已经存在。它做的事也非常单纯：

- 如果模块是从预编译数据恢复的，就 `ApplyToModule_Stage2()`
- 否则调用 `ScriptModule->builder->BuildGenerateFunctions()`

```cpp
void FAngelscriptEngine::CompileModule_Functions_Stage2(...)
{
    if (Module->bLoadedPrecompiledCode)
    {
        Module->PrecompiledData->ApplyToModule_Stage2(...);
        return;
    }

    auto Result = ScriptModule->builder->BuildGenerateFunctions();
    if (Result != asSUCCESS)
        Module->bCompileError = true;
}
```

为什么函数生成必须放在类型之后？因为函数签名要引用：

- 当前模块刚生成出来的 class / struct / enum / delegate 类型
- imported modules 暴露出来的外部类型
- 热重载中刚完成映射的新旧类型引用

因此 Stage 2 的本质不是“生成函数代码”，而是**在一个已经稳定可见的类型图上，生成函数声明与签名世界。**

## Stage 2 之后为什么会插入重载分析和布局阶段

`CompileModules()` 在 Stage 2 结束后，会做一大段并不属于单个 stage 子函数、但又极其关键的工作：

- `CollectUpdatedTypeReferences()`
- `DiffForReferenceUpdate()`
- 根据依赖传播把 code-only change 升级成 structural change
- 决定哪些旧模块只需要 update references，哪些必须重新编译
- 做 `BuildLayoutClasses()` 和 `BuildLayoutFunctions()`
- 在需要时先分配 global variables，再更新旧模块字节码里的引用

这段逻辑解释了为什么四阶段编译流不是机械的四个函数顺次调用，而是一个围绕四个阶段展开的**大状态机**。尤其在热重载下，Stage 2 结束后其实才刚刚获得“当前这批改动到底是结构变化还是代码变化”的关键信息。

所以更准确地说，四阶段之间还夹着布局和引用替换逻辑，而这些逻辑的存在，正是四阶段编译流能支撑热重载的原因。

## Stage 3：Code —— 在布局之后真正生成代码并做 JIT

Stage 3 的入口是 `CompileModule_Code_Stage3()`：

- 预编译数据场景下走 `ApplyToModule_Stage3()`
- 普通场景下调用 `BuildCompileCode()`
- 编译完成后删除 builder，并立即 `JITCompile()`

```cpp
void FAngelscriptEngine::CompileModule_Code_Stage3(...)
{
    auto Result = ScriptModule->builder->BuildCompileCode();
    if (Result != asSUCCESS)
        Module->bCompileError = true;

    asDELETE(ScriptModule->builder, asCBuilder);
    ScriptModule->builder = nullptr;

    ScriptModule->JITCompile();
}
```

这里最值得注意的不是“编了代码”，而是它发生在：

- 类型已经生成完毕；
- 函数签名已经生成完毕；
- 类和函数布局已经完成；
- 必要的引用更新已经做完。

也就是说，Stage 3 的前提是**所有编译期结构都已经稳定**。这样编出来的字节码和 JIT 代码才不会立刻因为布局变化或类型替换而失效。

因此 Stage 3 的角色可以理解成：把前两阶段和布局阶段准备好的“静态结构图”，真正压成可执行代码。

## Stage 4：Globals —— 最后才重置全局变量和可执行行映射

Stage 4 是 `CompileModule_Globals_Stage4()`，它做的事更少，但时机极重要：

- `ResetGlobalVars(0)`
- 在覆盖率启用时做 `CodeCoverage->MapExecutableLines(*Module)`

```cpp
void FAngelscriptEngine::CompileModule_Globals_Stage4(...)
{
    check(!Module->bCompileError);
    ScriptModule->ResetGlobalVars(0);

    if (CodeCoverage != nullptr)
    {
        CodeCoverage->MapExecutableLines(*Module);
    }
}
```

为什么全局变量一定要放到最后？因为全局初始化可能会：

- 触发函数调用
- 读写已经编译完成的代码
- 依赖已经稳定的类布局和函数布局
- 在热重载场景下命中新旧引用替换后的最终结果

所以 Stage 4 不是“收尾小动作”，而是一个非常严格的时序承诺：**只有当前三个阶段和中间布局/引用替换都稳定后，才允许全局变量真正进入初始化状态。**

## 预编译数据为什么也要按四阶段恢复

从 Stage1/2/3 的 `ApplyToModule_StageN()` 可以看出，预编译数据并不是“一把恢复整个模块”，而是按同样的阶段拆开恢复。这一点非常重要，因为它说明：

- 四阶段不是源码路径才需要的临时流程；
- 它是 Runtime 认可的一套稳定模块装配协议；
- 无论输入来自源码还是来自缓存，最终都要遵守相同的阶段边界。

这也解释了为什么当前插件可以把首编译、预编译恢复和热重载三种路径统一到一条编译总线上：因为真正稳定的不是“输入形式”，而是**阶段语义**。

## 从架构角度该怎么理解四阶段

如果把这条流水线翻译成更工程化的话，可以这样理解：

- **Types**：建立类型可见性和模块壳
- **Functions**：建立签名和调用表面
- **Code**：生成可执行体
- **Globals**：让运行时状态真正活起来

每往后走一步，对前一步的稳定性要求就更高；也正因为这样，系统才能在热重载里细分“结构变化”和“代码变化”，并决定是否需要 full reload。

所以四阶段编译流最大的架构价值，不只是“分层清楚”，而是它给了引擎两个能力：

1. 能在首编译和热重载里复用同一套编译总线；
2. 能在真正执行全局初始化之前，把类型、函数、布局、引用替换这些前置条件全部锁稳。

## 小结

- 当前 Angelscript 模块编译不是一次黑盒 Build，而是 `Types -> Functions -> Code -> Globals` 四阶段流水线
- Stage 1 负责创建模块、导入依赖、注入 pre-class data、装载源码并完成类型生成准备
- Stage 2 负责在类型已稳定的前提下生成函数签名世界
- Stage 3 负责真正编译代码并执行 JIT；Stage 4 则在所有结构稳定后才重置全局变量并挂覆盖率映射
- 四阶段同样约束了预编译数据恢复和热重载路径，因此它本质上是一套稳定的模块装配协议，而不是单纯的源码编译细节
