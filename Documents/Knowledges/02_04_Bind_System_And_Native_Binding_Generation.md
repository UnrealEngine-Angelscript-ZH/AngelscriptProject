# Bind 系统与 Native 绑定生成

> **所属模块**: 类型系统与生成链路 → Bind System / Native Binding Generation
> **关键源码**: `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.h`, `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.cpp`, `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`, `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptEditorModule.cpp`, `Documents/Plans/Plan_HazelightBindModuleMigration.md`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`

这一节要回答的核心问题是：插件怎么把 UE 原生 API 变成脚本世界可见的绑定面。当前实现并不是“运行时扫描反射后一把梭注册”，而是**手写 Bind、生成产物、运行时注册态和 cooked 缓存职责共同协作**。

## 2.4.1 手写 Bind 体系如何接入 UE 反射与脚本层

- `FAngelscriptBinds` 是核心注册入口，负责声明类、方法、属性、全局函数与附加元数据
- `FBind` 是静态注册单元，通常通过 `AS_FORCE_LINK const FAngelscriptBinds::FBind ...` 在模块装载时挂入 bind 列表
- `ReferenceClass`、`ExistingClass`、`ValueClass<T>` 把 UE 反射对象、已有脚本类和值类型带进 AngelScript 类型系统

这说明手写 Bind 的真实角色是：**在反射信息之上补脚本调用语义和约束元数据。**

## 2.4.2 `GenerateNativeBinds()` 输出链路与产物边界

- 当前仓库里大量 `Bind_*.cpp` 是已经落地的绑定产物与手写绑定协作层
- 生成链的职责不是取代运行时注册，而是为大面积 UE API 暴露提供规模化输入
- UHT/生成器负责产出绑定代码，Runtime 负责真正把这些产物注册进 AngelScript 引擎

因此这里的边界是：**生成器产出代码，运行时消费产物，Bind 系统维护注册时序与能力元数据。**

## 2.4.3 Runtime / Editor Bind 模块分工

- Runtime Bind 是主线，负责脚本运行时真正会用到的类型、方法、参数封送和元数据
- Editor 只补 editor-only 暴露、工具型类型或需要编辑器上下文的绑定项
- 这和插件模块边界一致：运行时不反向依赖 editor-only 绑定能力

## 2.4.4 `FAngelscriptBindState` 注册时序与执行生命周期

- Bind 注册不是随用随建，而是先把 `FBind` 聚合，再按 `EOrder` 排序执行
- `Early / Normal / Late` 的顺序控制允许基础类型、核心类和高层 mixin/扩展按依赖顺序装配
- `SetPreviousBindIsEditorOnly`、`SetPreviousBindRequiresWorldContext`、`SetPreviousBindNoDiscard` 这类接口，说明 bind 生命周期里还夹带着调用规则与可见性元数据

也就是说 `BindState` 不只是“注册过没有”，还保存了脚本层如何理解这些绑定的策略信息。

## 2.4.5 `FAngelscriptBindDatabase` 在 cooked 场景中的缓存职责

- bind 数据库的目标不是替代源码生成，而是为 cooked/运行时场景保留稳定、可查找的绑定描述
- 这类缓存职责通常围绕类名、方法名、签名、世界上下文与脚本可见性展开
- 在 cooked 场景里，数据库更像“绑定可用性快照”，避免每次都从 editor 生成链重推导一遍

## 当前设计最关键的边界

- **Binds/**：注册内容与适配逻辑
- **UHT / 生成器**：批量产出绑定源
- **Runtime BindState / Database**：运行时注册顺序、元数据、缓存与可用性边界
- **Editor Bind**：只处理 editor-only 暴露，不反向吞并 Runtime 主线

## 小结

- Bind 系统是反射、生成器和脚本运行时之间的桥，不只是简单注册函数
- `FBind + EOrder + metadata setters` 共同定义了绑定执行顺序和调用语义
- 生成链、Runtime 注册态和 cooked 缓存各司其职，构成当前 Native 绑定生成与消费的完整闭环

## 2.4.6 direct bind 与 reflective fallback 的职责边界

当前仓库已经不再适合把“能从脚本调用”与“能恢复为原始 native ABI 直绑”混成同一个概念。对 `BlueprintCallable` / `BlueprintPure` 函数来说，现在至少有两条本质不同的调用后端：

- **direct bind**：`FFuncEntry` 里持有 `FGenericFuncPtr + ASAutoCaller::FunctionCaller`，脚本调用直接进入原始 native ABI。
- **reflective fallback**：`FFuncEntry` 没有可用 native pointer 时，以 `UFunction*` 为调用描述符，通过 `ProcessEvent` 和 `FProperty` 封送完成调用。

两者都能把 UE 能力暴露给 AngelScript，但语义和成本模型不同：

- direct bind 面向热路径，目标是尽量避开反射封送。
- reflective fallback 面向 coverage 补面，目标是把一部分 `ERASE_NO_FUNCTION()` 从“完全不可调用”提升为“可正确调用”。

因此当前推荐的解释模型是：

```text
Tier 1: direct native bind
Tier 2: plugin-owned generated bridge（后续可选增强）
Tier 3: reflective fallback
```

第一阶段真正落地的是 Tier 1 + Tier 3：保留 direct bind 主路径，同时为部分 unresolved 条目补出 reflective fallback 后路。Tier 2 仍然属于后续增强闸门，不应被偷渡成当前阶段承诺。

## 2.4.7 为什么 `UFunction::Func` 不能替代 native method pointer

一个容易误判的点是：既然蓝图能调用 `BlueprintCallable`，那是否意味着当前绑定系统也能直接拿 `UFunction::Func` 来充当 `FGenericFuncPtr`？答案是否定的。

- `UFunction::Func` 存的是 `FNativeFuncPtr` thunk，签名是 `void (*)(UObject*, FFrame&, RESULT_DECL)`。
- 当前 direct bind 体系依赖的是原始 C++ 成员函数 / 全局函数指针，以及与之配套的 `ASAutoCaller` 签名调用器。
- thunk 指针服务的是反射执行链，不是原始 native ABI。

所以 `UFunction*` 很适合作为 reflective fallback 的元数据中心键，但不能直接塞回 Tier 1 payload 中冒充 direct bind 成功。对数据结构的正确理解应该是：

```text
call descriptor
├─ native payload（可选）
├─ reflective metadata（可选）
└─ runtime eligibility / statistics state
```

也就是说，`UFunction*` 是调用描述符的一部分，而不是原始 method pointer 的同义替代品。

## 2.4.8 `ProcessEvent` 路径的成本与热路径保护

当前仓库里已经有明确的热路径基线：`UASFunctionNativeThunk` 只是薄跳板，随后由 `RuntimeCallFunction()` 与 `UASFunction` 特化子类/JIT 分流接管。这条路径存在的根本原因，就是为了避免每次调用都重新走完整反射封送。

相比之下，reflective fallback 至少会承担这些固定成本：

- 基于 `ParmsSize` 分配并初始化参数 buffer
- 逐项按 `FProperty` 复制输入参数
- 通过 `ProcessEvent` / `Invoke` 落入 `FFrame` 驱动的执行链
- 调用后回写返回值与 out/ref 参数
- 销毁局部值与 buffer 内的属性状态

这意味着 reflective fallback 的正确定位必须是：

- **coverage fallback / cold path**
- 不是 hot path 的等价替代
- 更不能反向吞掉已有 direct bind 的函数

因此，只要某个函数已经有可用的 direct bind，binder 就必须显式阻止 reflective fallback 介入。这个优先级约束不应该只存在于计划或文档里，而应该体现在 runtime 路由和测试断言中。

## 2.4.9 统计口径：从 direct/stub 升级到 direct/reflective/unresolved

在只有 direct bind 的旧阶段，很多统计都会把非 direct 的条目粗略算成 stub。但一旦 reflective fallback 落地，这种二分类就不再准确，因为：

- 一部分函数仍然没有 native pointer
- 但它们已经不再是“不可调用”
- 同时它们也绝不是 direct bind 覆盖提升

因此当前正确的口径必须升级为三分类：

1. **direct**：已有 `FGenericFuncPtr + ASAutoCaller::FunctionCaller`，走 native ABI。
2. **reflective**：native pointer 缺失，但 runtime eligibility 通过并成功注册了 reflective fallback。
3. **unresolved**：仍被接口类、`CustomThunk`、参数形态或其他边界明确排除。

这一口径的实际意义有三点：

- 让覆盖率提升不再误报为 direct bind 提升。
- 让模块收益（例如 `AIModule`、`GameplayTags`、`UMG`）可以被单独观察。
- 让后续是否值得做 bridge thunk / cache 强化有稳定的数据基础。

从验证层面说，`AngelscriptGeneratedFunctionTableTests.cpp` 不再只是检查“有没有 direct bind entry”，还需要能够断言 reflective fallback 和 handwritten direct bind 的优先级关系，避免 generated reflective path 侵入已有手写覆盖面。

为了让这套统计不只存在于运行时测试里，当前 UHT 导出链还会在每次生成 `AS_FunctionTable_*.cpp` 的同时写出 `AS_FunctionTable_Summary.json`。这份 summary 至少固定包含：

- `totalGeneratedEntries`
- `totalDirectBindEntries`
- `totalStubEntries`
- `directBindRate`
- `stubRate`
- `totalShardCount`
- `moduleCount`
- `modules[]`

除此之外，UHT 现在还会同步写出两份 CSV，作为“可查询产物”而不是仅供肉眼阅读的日志：

- `AS_FunctionTable_ModuleSummary.csv`：逐模块聚合统计
- `AS_FunctionTable_Entries.csv`：逐条 `ClassName + FunctionName + EntryKind + EraseMacro + ShardIndex` 明细

这意味着查询链路已经从“看控制台日志 / grep shard cpp”升级为：

1. 看 `AS_FunctionTable_Summary.json` 获取总量与 rate
2. 看 `AS_FunctionTable_ModuleSummary.csv` 获取模块聚合分布
3. 看 `AS_FunctionTable_Entries.csv` 追查具体函数为什么被归类为 direct 或 stub

因此现在既可以从自动化测试验证“三分类统计逻辑仍然成立”，也可以直接从 UHT 产物目录读取“本次实际生成了多少条格式绑定、各模块分布如何”。

## 2.4.10 第一阶段 reflective fallback 资格矩阵

当前 reflective fallback 并不是对所有 `ERASE_NO_FUNCTION()` 一视同仁地放开，而是要按资格矩阵做保守分层。第一阶段推荐的最小矩阵如下：

| 类别 | 第一阶段策略 | 理由 |
|---|---|---|
| `FUNC_Native` + 非接口类 + 非 `CustomThunk` + 当前 `FAngelscriptType`/`FProperty` 可表达的参数 | **允许 reflective fallback** | 反射调用链清晰，语义可控 |
| `UInterface` / `NativeInterface` | **明确排除** | 当前 direct bind 与 reflective helper 都不应假设接口类实例布局等价于普通 UObject class |
| `CustomThunk` | **明确排除** | 其真实调用语义依赖自定义 K2 thunk，不适合作为第一阶段通用 fallback 样本 |
| latent / custom K2 thunk / 强依赖 Blueprint VM 特殊流程的函数 | **明确排除** | 仅靠通用 `ProcessEvent` 封送不足以保证语义正确 |
| 参数或返回值当前无法由 `FAngelscriptTypeUsage::FromProperty` 稳定表达 | **明确排除** | fallback 目标是 correctness，不应牺牲类型安全去追覆盖率 |
| header 解析失败但 runtime 反射面完整的普通 native UFunction | **优先纳入 reflective 候选** | 这是当前 plan 最主要的 coverage 补面对象 |

这个矩阵背后的工程原则是：**第一阶段只承诺“反射路径清晰且风险可控”的 unresolved 条目**。其余函数仍然应该被统计为 unresolved，并在日志/测试中明确说明原因，而不是 silent skip。

## 2.4.11 手写绑定优先于 generated reflective fallback

当前仓库早已有一个重要契约：generated function table 是对 hand-written binds 的补充，而不是覆盖层。这个契约在 reflective fallback 阶段仍然必须保留，而且比以前更重要。

原因很直接：

- 手写 bind 往往代表已经验证过的 direct bind 或脚本语义定制。
- generated reflective fallback 只是 coverage 补面，不应该把已有 hot path 或人工调优后的 API 表面改写掉。

因此 binder 层至少要满足两条规则：

1. 如果当前函数已有 direct bind，则 reflective fallback **不得** 再注册第二份脚本表面。
2. 如果脚本引擎里已经存在同名/同声明的 hand-written API，则 generated reflective fallback **不得** 因为 runtime reflection 可行就重复注册。

这条规则既是性能保护，也是兼容性保护。对测试来说，它意味着控制样本不能只看 reflective 正例，还要持续验证 hand-written GAS、native actor/component 等既有 direct path 没有被回退路径侵蚀。
