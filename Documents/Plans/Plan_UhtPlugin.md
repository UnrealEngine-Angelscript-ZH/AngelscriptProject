# UHT 插件计划：自动生成函数调用表

## 当前实现状态（2026-04-06，基于当前 `main` 工作树）

- 当前 `main` 分支保留了 `AngelscriptUHTTool` 的 5 个核心 C# 文件，包含 exporter、分片生成器、签名构建与 header 级重载解析能力；当前主干已有可工作的 UHT 工具化骨架，而不是纯计划状态。
- `AngelscriptFunctionTableExporter` 仍以 `UhtExporterOptions.Default | UhtExporterOptions.CompileOutput` 注册，并继续面向 `AngelscriptRuntime` 生成 `AS_FunctionTable_*.cpp`。
- `AngelscriptFunctionTableCodeGenerator` 仍在使用“每模块多分片”策略，并保留模块级 coverage diagnostics 输出与 editor-only 模块分流逻辑。
- `AngelscriptHeaderSignatureResolver` 与 `AngelscriptFunctionSignatureBuilder` 当前仍具备一部分 Phase 5 生成器侧增强：可基于参数/返回类型做重载候选收敛，并对少量白名单样本恢复显式 `ERASE_FUNCTION_PTR` / `ERASE_METHOD_PTR`。
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptBindConfigTests.cpp` 当前仍保留了针对生成条目填充、`AddFunctionEntry` 去重、重载恢复以及 inline/direct-bind 样本的回归测试代码。
- 使用 `Tools/RunBuild.ps1 -Label UhtPlanRefresh -SerializeByEngine` 在当前 `main` 上重新验证时，构建结果为成功；本轮取证没有复现此前的编译失败。
- 但 `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionCallers/FunctionCallers_*.cpp` 这 14 个历史文件在当前主干仍然存在，因此“死代码已删除”这一条不符合现状，`P4.1` 仍应视为待完成。
- 当前 runtime 侧关键文件 `Bind_BlueprintCallable.cpp` 与 `Helper_FunctionSignature.h` 并未体现计划顶部先前声称的全部 Phase 5 元数据增强，因此这部分不能继续按“已完成”处理。
- 本轮仅重新执行了构建验证，没有重新跑 `Angelscript.TestModule.Engine.BindConfig` 自动化测试；因此测试收口状态需要单独重跑确认，不能直接沿用旧结论。

## 基于当前主干重排后的 TODO

1. 先补回当前 `main` 缺失的 runtime 侧 Phase 5 能力，再继续扩大恢复范围：重点是 `Bind_BlueprintCallable.cpp` 与 `Helper_FunctionSignature.h` 的元数据语义与重复注册保护。
2. 在 runtime 侧逻辑补齐后，重新执行 `AngelscriptBindConfig` / `Angelscript.TestModule.Engine.BindConfig.*` 相关验证，确认现有回归测试与主干代码重新一致。
3. 在测试重新收口并确认生成链路稳定后，再推进 `P4.1` 的 `FunctionCallers_*.cpp` 清理，避免先删死代码后失去回退/对照基线。
4. 覆盖率扩展应继续建立在“生成器侧已存在的 diagnostics + 目标样本回归测试”之上，而不是沿用旧的阶段性完成结论。

## 背景与目标

### Hazelight 的 UHT 改动全景

Hazelight（UEAS2）为支持 Angelscript 对 UE 引擎做了 **两层修改**（共 33 个文件）：

| 层级 | 文件数 | 作用 |
|------|--------|------|
| UHT 层（C#） | 11 | 编译期解析/存储/生成 Angelscript 所需的 C++ 类型信息 |
| 引擎运行时层（C++） | 22 | 运行时消费 UHT 数据，支持脚本类创建、函数调用、属性访问 |

按功能域分为以下几类：

| 功能域 | 侵入程度 | 我们的替代方案 | 是否存在性能损失 |
|--------|---------|--------------|----------------|
| **方法指针生成**（UHT → `.gen.cpp` 注入 `ERASE_METHOD_PTR`） | 🔴 ABI 破坏 | `FunctionCallers.h` + 手写/生成 `AddFunctionEntry` | **是——当前覆盖率极低** |
| **属性类型信息**（`EAngelscriptPropertyFlags`，const/ref/enum 标记） | 🟡 字段新增 | 可从 UHT JSON / 运行时元数据重建 | 无直接性能影响 |
| **自定义说明符**（`ScriptCallable`、`ScriptReadOnly` 等） | 🟢 纯增量 | 不需要——直接使用 `BlueprintCallable`/`BlueprintReadWrite` 等现有说明符即可 | 无性能影响 |
| **函数默认值元数据**（`ScriptCallable` 函数也保存默认值） | 🟢 纯增量 | 不需要——`BlueprintCallable` 函数已自动保存默认值 | 无性能影响 |
| **`FUNC_RuntimeGenerated` + ScriptCore 执行路径** | 🔴 枚举新增 | 已通过 `FUNC_Native` + `UASFunctionNativeThunk` 实现等价路径 | **几乎无损失**（仅 RPC 路径多一次 FFrame 创建） |
| **UFunction 虚方法**（`RuntimeCallFunction` 等） | 🟡 vtable 扩展 | 我们已有 `UASFunction` 子类，通过 `ProcessEvent` hook | 功能等价 |
| **UClass 扩展**（`ASReflectedFunctionPointers` 等） | 🟡 字段新增 | `ClassFuncMaps` 外部映射 | 等价 |
| **对象初始化/CDO** | 🔴 深度引擎集成 | 我们已通过 `AngelscriptClassGenerator` 处理 | 功能等价 |
| **ICppStructOps 扩展** | 🔴 虚接口变更 | 无插件层替代 | 不影响调用性能 |

### 当前调用机制分析

我们的项目已经拥有与 Hazelight 完全等价的 `ASAutoCaller` 类型擦除调用基础设施（`FunctionCallers.h`），调用链为：

```
AngelScript VM → CallFunctionCaller → ASAutoCaller::RedirectMethodCaller → 直接 C++ 方法调用
```

这跳过了 UE 反射系统的 `FFrame` 参数打包/解包，性能接近原生调用。

**但关键问题是**：这套机制依赖 `ClassFuncMaps` 中存在函数的 `FFuncEntry`（函数指针 + Caller），而当前主干仍同时处于“自动生成链路已存在、runtime 收口未完成”的过渡状态：

- `FunctionCallers_*.cpp` 这批历史文件仍在仓库中，`P4.1` 尚未收口
- `Bind_BlueprintCallable` 仍会跳过 `ClassFuncMaps` 中没有条目的函数
- 当前仍同时存在手写绑定、UHT 生成链路与覆盖率扩展中的过渡逻辑，下一步重点不是“再证明链路存在”，而是把 runtime 侧规则、测试和清理顺序重新对齐

### 性能损失评估

| 场景 | 当前状态 | vs Hazelight |
|------|---------|-------------|
| 手写绑定覆盖的函数 | 使用 `ASAutoCaller` 直接调用 | **等价——无性能损失** |
| 未被手写绑定覆盖的 BlueprintCallable 函数 | **完全不可用**（不是慢，是没绑定） | **覆盖率差距** |
| 通过 `asCALL_GENERIC` 注册的函数 | 经 `CallGeneric` + `asIScriptGeneric` 间接调用 | **有额外间接调用开销** |
| Blueprint→Script 回调 | `FUNC_Native` + `UASFunctionNativeThunk` → `RuntimeCallFunction` | 维持当前插件侧等价路径；更细的性能结论需单独量化 |

**当前重评估结论**：当前主干已经具备 UHT 自动生成函数条目的主干能力，但是否已经把高价值 `BlueprintCallable` 覆盖面稳定恢复到可接受状态，仍需要 runtime 侧规则补齐与回归测试重新确认，不能仅凭历史文档结论直接视为完成。

### 本计划目标

通过 UHT Exporter 插件**自动生成 `AddFunctionEntry` 调用**，为所有 BlueprintCallable 函数填充 `ClassFuncMaps`，使 `BindBlueprintCallable` 自动发现路径恢复工作：

1. **消除手动维护负担**——不再需要手写 FunctionCallers 表
2. **实现全覆盖**——所有 BlueprintCallable 函数自动获得直接调用路径
3. **保持等价性能**——使用与 Hazelight 相同的 `ASAutoCaller` 调用机制
4. **无引擎修改**——纯 UHT 插件 + 插件侧代码

## 范围与边界

### 在范围内

- UHT Exporter 插件：遍历所有 UCLASS/UFUNCTION，生成 `AddFunctionEntry` 调用的 C++ 文件
- 生成代码编译集成：使用 `CompileOutput` 让生成的 C++ 参与编译
- 与手写绑定的兼容：手写 Bind_*.cpp 优先，自动生成作为补充
- 清理 FunctionCallers_*.cpp 死代码

### 不在范围内

- 引擎级修改（`FUNC_RuntimeGenerated`、`ScriptCore.cpp`、`ICppStructOps`）
- Blueprint→Script 回调性能优化（需引擎修改）
- 非反射类型的自动绑定（FVector、运算符等，UHT 不可见）
- `EAngelscriptPropertyFlags` 嵌入 FProperty（可通过元数据运行时重建）

## 分阶段执行计划

### Phase 1：现有 UHT 工具骨架审计与命名收口

> 目标：承认当前 `main` 已存在的 `AngelscriptUHTTool` 骨架，并把“现有实现”与“剩余待办”拆开，而不是继续按从零搭建处理。

- [x] **P1.1** 当前主干已有 UHT 工具骨架
  - `Plugins/Angelscript/Source/AngelscriptUHTTool/` 下已存在 exporter、code generator、signature builder、header resolver 等核心文件
  - 当前待办不是再创建一个全新的空白项目，而是决定是否还需要把当前布局进一步收口为更明确的插件/模块命名
- [ ] **P1.1** 📦 Git 提交：`[Plugin/UHT] Docs: reconcile existing UHT tool layout with plan terminology`

- [x] **P1.2** 最小 Exporter 骨架已存在于主干
  - 当前 `AngelscriptFunctionTableExporter.cs` 已注册 `[UhtExporter]` 并启用 `CompileOutput`
  - 重新评估后的重点变为：保留这条能力结论，并在后续验证里确认 exporter 输出仍与 runtime 侧预期一致
- [ ] **P1.2** 📦 Git 提交：`[Plugin/UHT] Docs: mark exporter skeleton landed on main`

### Phase 2：函数表生成器现状收口

> 目标：把当前主干已经存在的 generator 能力与尚未重新验证的部分分开记录。

- [x] **P2.1** 主干已具备类型签名还原与显式宏恢复能力
  - 当前 `AngelscriptFunctionSignatureBuilder` / `AngelscriptHeaderSignatureResolver` 已具备签名重建、重载候选收敛与部分显式 `ERASE_*` 恢复能力
- [ ] **P2.1** 📦 Git 提交：`[Plugin/UHT] Docs: record landed signature reconstruction capabilities`

- [x] **P2.2** 主干已具备按模块分片生成 `AS_FunctionTable_*.cpp` 的能力
  - 当前 `AngelscriptFunctionTableCodeGenerator` 已实现分片生成、`CommitOutput()` 与 editor-only 模块处理
- [ ] **P2.2** 📦 Git 提交：`[Plugin/UHT] Docs: record landed module sharding generator`

- [ ] **P2.3** 重新验证构建集成与增量更新
  - 当前重新执行的构建已通过，但本轮因为 `Target is up to date` 没有强制重放完整 exporter 输出
  - 下一轮需要补做“强制触发 exporter + 观察输出变化 + 重新采样 coverage diagnostics”的验证，避免把旧统计直接当成当前基线
- [ ] **P2.3** 📦 Git 提交：`[Plugin/UHT] Feat: refresh build integration and incremental validation on main`

### Phase 3：运行时集成与验证收口

> 目标：把当前主干已经可见的测试/运行时证据，和仍需重新执行的验证动作区分开。

- [ ] **P3.1** 重新确认运行时加载与 `ClassFuncMaps` 填充
  - 当前主干保留了相关测试代码，但本轮没有重新跑 `BindConfig` 自动化验证来确认结果仍与主干一致
  - 下一步应通过重新执行测试来确认生成条目在当前 main 上的实际填充状态
- [ ] **P3.1** 📦 Git 提交：`[Plugin/UHT] Test: revalidate runtime loading and ClassFuncMaps population on main`

- [ ] **P3.2** 重新确认与手写绑定的兼容性
  - 当前测试代码仍覆盖 `AddFunctionEntry` 去重和相关兼容场景，但需要实际重跑验证
  - runtime 侧元数据/重复注册保护逻辑仍应与这组测试一起重新收口
- [ ] **P3.2** 📦 Git 提交：`[Plugin/UHT] Test: revalidate hand-written bind compatibility on main`

- [ ] **P3.3** 重新执行功能验证测试
  - 当前已有 `BindConfig` 回归测试资产，但本轮仅完成了构建验证，没有重新完成自动化测试验收
  - 在 runtime 侧规则补齐后，再集中跑这一批测试来决定哪些子项可以重新标记为完成
- [ ] **P3.3** 📦 Git 提交：`[Plugin/UHT] Test: rerun automated function table coverage verification`

### Phase 4：清理与优化

> 目标：移除死代码，优化生成策略。

- [ ] **P4.1** 清理 FunctionCallers_*.cpp 死代码
  - 14 个 FunctionCallers_*.cpp 文件全部是注释掉的死代码（~27000 条被注释的 ERASE 条目）
  - 删除所有 FunctionCallers_*.cpp 文件
  - 从 Build.cs 移除相关引用（如有）
  - 保留 `FunctionCallers.h`（`ASAutoCaller` 命名空间、`FFuncEntry`、`ERASE_*` 宏仍被活跃使用）
- [ ] **P4.1** 📦 Git 提交：`[Plugin/UHT] Cleanup: remove dead FunctionCallers_*.cpp files`

- [ ] **P4.2** 清理编辑器中的代码生成工具
  - `AngelscriptEditorModule.cpp` 中存在生成 `AddFunctionEntry` 代码的编辑器工具函数
  - 这些工具函数是当初手动维护 FunctionCallers 的辅助工具，现已被 UHT 插件取代
  - 评估是否保留（可能仍有调试价值）或移除
- [ ] **P4.2** 📦 Git 提交：`[Plugin/UHT] Cleanup: evaluate editor code generation tools`

- [ ] **P4.3** 生成策略优化
  - 分析生成文件大小和编译时间，必要时调整分片策略
  - 考虑按 "Runtime vs Editor" 分开生成（Editor 模块的函数只在编辑器构建中生成）
  - 评估是否需要添加 `#if WITH_EDITOR` 条件编译
- [ ] **P4.3** 📦 Git 提交：`[Plugin/UHT] Optimize: generation strategy tuning`

### Phase 5：覆盖率提升批次化推进（下一批）

> 目标：在保持当前“可编译、可测试、可回退”的前提下，系统性缩减 `ERASE_NO_FUNCTION()` 的数量，把下一批高价值候选恢复为 direct bind。

#### 当前覆盖率基线（按当前主干重评估）

- 当前主干的 generator 仍保留模块级 coverage diagnostics 输出能力，但本轮构建因 `Target is up to date` 未重新打印最新统计值，因此这里不再保留旧的精确计数作为“当前基线”。
- 可以确认的现状是：coverage 扩展能力仍依赖 generator 日志、白名单样本与 `BindConfig` 回归测试共同收口，而不是单独依赖文档中的历史统计数字。
- 下一轮覆盖率批次开始前，应先通过一次强制触发 exporter 的构建或定向验证重新采样最新模块统计，再决定恢复优先级。

- [x] **P5.1** 建立覆盖率基线与模块热点清单
  - 在 Exporter 日志或独立摘要中输出每个模块的 `direct-bind` / `ERASE_NO_FUNCTION()` 计数
  - 按模块生成“高价值候选清单”，优先关注 `Engine`、`UMG`、`GameplayAbilities`、`AIModule`
  - 为每个候选记录 fallback 原因：`unexported-symbol`、`overloaded-unresolved`、`non-public`、`interface` 等
  - 目标不是立刻恢复全部函数，而是先把“可恢复”和“不可恢复”边界钉死
- [ ] **P5.1** 📦 Git 提交：`[Plugin/UHT] Feat: add per-module coverage diagnostics`

- [ ] **P5.2** 恢复“唯一可见但当前被保守跳过”的静态/实例函数
  - 针对已导出且候选唯一的函数，验证是否可从 header 直接恢复为 `ERASE_AUTO_*`
  - 优先选取不引入新增模块依赖的类型样本，例如 `AngelscriptRuntime` 自身类、公开 `*_API` 的运行时类
  - 保持当前安全边界：`MinimalAPI`、未导出符号、interface 仍继续回退 `ERASE_NO_FUNCTION()`
  - 对恢复成功的样本补充自动化测试，验证 `ClassFuncMaps` 填充 + direct-bind 有效
- [ ] **P5.2** 📦 Git 提交：`[Plugin/UHT] Feat: recover unique exported direct binds`

  **建议优先拆成以下可插件化子批次：**

- [ ] **P5.2.a** `UsableInAngelscript` 元数据 override
    - 在 `Bind_BlueprintCallable.cpp` 中对 `BlueprintInternalUseOnly` 增加 `UsableInAngelscript` 放行逻辑
    - 目标：恢复一批当前因 `BlueprintInternalUseOnly` 被硬过滤、但脚本侧可安全暴露的节点
- [ ] **P5.2.b** `ScriptMethod` 按函数级 mixin 支持
    - 当前本地仅支持类级 `ScriptMixin`；补齐单函数级 `ScriptMethod` 逻辑
    - 目标：恢复一批静态帮助函数的实例风格调用能力，和 Hazelight/UE 约定对齐
- [ ] **P5.2.c** `ScriptAllowTemporaryThis` / `CallableWithoutWorldContext` 元数据
  - 对齐 Hazelight 已支持但本地尚未覆盖的元数据语义
  - 目标：先扩展脚本侧行为能力，再决定这些函数是否进入 direct bind 恢复集合
  - 当前进度：当前 `main` 未确认 `CallableWithoutWorldContext` 的 runtime 侧 trait 抑制逻辑仍在；`ScriptAllowTemporaryThis` 在当前代码库、UEAS2 参考和 AngelScript traits 中仍无现成语义/trait，对应能力继续记为“待设计”而非强行落地
  - [x] **P5.2.d** direct-bind 候选白名单样本
    - 先在 `AngelscriptRuntime` 自身类型和少量公开运行时类中恢复一小批 direct bind
    - 每恢复一类样本，必须补对应自动化测试，避免再次引入链接错误

- [x] **P5.3** 缩小“重载歧义”导致的 fallback 面
  - 为重载类函数建立更细粒度的候选筛选规则，而不是一律 `overloaded-unresolved`
  - 优先只处理“同名重载中 blueprint 暴露签名可唯一映射”的场景
  - 对无法稳定判定的重载，继续保守回退，避免重新引入链接/签名错误
  - 为每条新增规则配套失败测试，确保不会把先前已稳定的 direct bind 再打回编译失败
- [ ] **P5.3** 📦 Git 提交：`[Plugin/UHT] Feat: reduce overload fallback cases`

  **下一批先不碰的重载类型：**

  - 带复杂 delegate / interface / container typedef 的重载
  - 仅靠 header 文本无法稳定区分的 `const&` / by-value 双重载
  - 任何需要引擎侧 `ASReflectedFunctionPointers` / UHT 注入才能彻底解决的场景

- [x] **P5.4** 拆分 runtime/editor 覆盖策略
  - 将 `Editor` 模块的 direct-bind 恢复策略与 `Runtime` 模块分开评估，避免 editor-only 规则污染 runtime 覆盖率
  - 补齐 `WITH_EDITOR` 条件下的统计和测试入口
  - 对 `UnrealEd` / `UMGEditor` 等 editor-only 模块单独维护 fallback 原因和覆盖目标
- [ ] **P5.4** 📦 Git 提交：`[Plugin/UHT] Optimize: split runtime and editor coverage policy`

- [x] **P5.5** 下一批覆盖率验收测试
  - 为每个恢复批次至少补 1 条“表已填充”测试 + 1 条“direct bind 有效”测试
  - 当前已存在的 `BindConfig` 测试资产作为回归基线；是否“已验证通过”需以下一轮重跑结果为准
  - 新增测试优先覆盖：
    - 公开 runtime 类 direct bind 样本
    - 静态帮助类样本
    - 重载函数恢复样本
    - editor-only 样本（若该批次涉及）
- [ ] **P5.5** 📦 Git 提交：`[Plugin/UHT] Test: extend coverage batch verification`

#### 下一批明确不做的内容

- 不尝试通过插件层强行恢复 `MinimalAPI` / 未导出符号的 direct bind
- 不引入引擎 patch 来复制 Hazelight 的 `ASReflectedFunctionPointers` / UHT 注入方案
- 不在没有失败测试的前提下继续放宽签名恢复规则
- 不把“能填表”误当成“能 direct bind”，两者必须分别验证

#### 需要引擎 patch 才能进入后续阶段的事项

- `UClass::ASReflectedFunctionPointers` 一类的 CoreUObject 结构扩展
- `FClassNativeFunction` 携带 Angelscript 反射函数指针并在 `RegisterFunctions()` 自动写入
- UHT 直接向 `.gen.cpp` / 原生注册表注入 `ERASE_METHOD_PTR` / `ERASE_FUNCTION_PTR`
- 任何依赖引擎私有注册表而非插件侧 `ClassFuncMaps` 的全覆盖方案

## 验收标准

1. **构建通过**：UHT 插件在完整构建和增量构建中均正常工作
2. **覆盖率**：所有 BlueprintCallable/BlueprintPure 函数自动获得 `FFuncEntry`
3. **性能等价**：自动绑定的函数使用 `ASAutoCaller` 直接调用，无反射间接开销
4. **手写兼容**：现有手写 Bind_*.cpp 不受影响，仍可覆盖自动绑定
5. **增量安全**：源文件未变时不触发重新生成和重新编译
6. **死代码清除**：FunctionCallers_*.cpp 全部移除

## 风险与注意事项

### 技术风险

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| UHT 类型信息不足以完全还原 C++ 签名 | 部分函数生成 `ERASE_NO_FUNCTION` | 逐步扩展签名还原能力；对不可还原的函数记录日志 |
| `CompileOutput` 生成文件的 include 路径问题 | 编译失败 | 参考 UnrealSharp 的做法；必要时在 Build.cs 中手动添加 include 路径 |
| 模块加载顺序：UHT 生成代码 vs 手写绑定 | 手写绑定被覆盖 | `AddFunctionEntry` 已有去重，确保手写在先 |
| 生成文件过大导致编译慢 | 增量构建时间增加 | 按模块分片；使用 `#pragma once` + 最小 include |

### 不可通过 UHT 插件解决的问题

以下 Hazelight 引擎修改无法通过插件层替代，需要引擎修改或接受功能差异：

| Hazelight 改动 | 影响 | 我们的应对 |
|---------------|------|----------|
| `FUNC_RuntimeGenerated` + ScriptCore.cpp 执行路径 | Blueprint VM 调用脚本函数的快速路径 | 当前继续依赖 `FUNC_Native` + `UASFunctionNativeThunk` 路径；更细的性能/语义对比需单独验证 |
| `ICppStructOps` 签名变更 | 脚本自定义结构体的构造/析构/复制 | 当前仍依赖插件侧 `FASStructOps` / FakeVTable 方案；与引擎改动的细节差异不在本轮重评估范围内 |
| `FProperty::AngelscriptPropertyFlags` | 属性的 C++ 类型限定符运行时查询 | 可从元数据运行时重建，或由 UHT 插件导出到侧通道文件 |
| `UClass` 扩展字段 | `ScriptTypePtr`、`bIsScriptClass` | 使用外部 `TMap<UClass*, FScriptClassData>` 映射 |

### 预期收益量化

| 指标 | 当前状态 | 实施后预期 |
|------|---------|----------|
| BlueprintCallable 函数覆盖率 | 当前仍是手写绑定 + UHT 生成链路并存的过渡状态 | 逐步扩大到尽可能高的 BlueprintCallable 覆盖面 |
| 函数表维护工作量 | 手动添加/更新 | 零维护（自动生成） |
| 每次调用性能 | 已绑定函数仍以当前 `ASAutoCaller` 路径为基础 | 继续维持接近原生的直接调用路径，并在后续需要时单独量化 |
| 对引擎版本的依赖 | 手写绑定需要随引擎更新 | 自动适应引擎变更 |
