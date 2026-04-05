# UHT 插件计划：自动生成函数调用表

## 当前实现状态（2026-04-05）

- 已在基于当前 `main` 的新 worktree `uht-tool-mainline` 上承接 `uht-plugin-mainline` 的已提交 UHT 历史，并保持当前 main 架构兼容。
- `AngelscriptUHTTool` 现可在 UHT 阶段执行，并为支持的 `BlueprintCallable` / `BlueprintPure` 函数生成 `AddFunctionEntry` 编译产物。
- 生成策略已从“每模块单文件”调整为“每模块多分片”，用于规避 `Engine` 等大模块上的 `C4883` 大函数编译失败。
- 生成过滤已收紧为“可链接 + 唯一候选”的 direct-bind 集合；对 `MinimalAPI`、未导出符号和重载/歧义候选统一回退到 `ERASE_NO_FUNCTION()`，优先保证主线可编译。
- `Tools/RunBuild.ps1` 已显式传入 worktree-local 的 `-Log=<Saved/Build/.../UnrealBuildTool.log>`，解决多 worktree 共享 `Engine/Programs/UnrealBuildTool/Log.txt` 的锁冲突；在 `uht-tool-mainline` 上已完成真实构建验证。
- `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionCallers/FunctionCallers_*.cpp` 共 14 个历史死文件已删除。
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptBindConfigTests.cpp` 已补充针对生成条目填充和 `AddFunctionEntry` 去重语义的回归测试。
- 当前已获得成功的本地构建证据；`Tools/RunTests.ps1` 已在测试前自动预热 `TargetInfo.json` 并为 `UnrealEditor-Cmd` 增加 `-BUILDMACHINE`。
- `Tools/RunTests.ps1` 现已在启动编辑器前显式等待 `Build.bat` 全局脚本锁；若锁长期被其他 worktree 占用，会快速报错并记录锁路径，而不再把整个测试超时耗在不可见的外部争用上。
- 但当前机器上的 `Angelscript.TestModule.Engine.BindConfig` 自动化验证**尚未完全收口**：测试 run 已能正常启动编辑器，但会被系统里其他 worktree 正在运行的 `Build.bat` 全局脚本锁连带阻塞，导致在进入 `Automation RunTests` 之前超时。
- 因此，目前可确认的状态是：**构建与 UHT exporter 生成链路已通过；自动化测试超时根因已定位为外部并发 `Build.bat` 锁争用，而不是当前 `uht-tool-mainline` 上的 UHT 编译链路失败。**
- 已完成 Phase 5 的首批覆盖提升：
  - `Bind_BlueprintCallable` 与 UHT generator 均已支持 `BlueprintInternalUseOnly + UsableInAngelscript` override；
  - `Helper_FunctionSignature` 已支持函数级 `ScriptMethod`；
  - `Helper_FunctionSignature` 已支持 `CallableWithoutWorldContext` 不再打 `asTRAIT_USES_WORLDCONTEXT`；
  - `AngelscriptHeaderSignatureResolver` 已支持基于参数/返回类型的重载候选判定，`AngelscriptFunctionSignatureBuilder` 对重载恢复场景已恢复显式 `ERASE_FUNCTION_PTR` / `ERASE_METHOD_PTR` 生成；
  - `Bind_BlueprintCallable` 已增加 declaration/name 级重复注册保护，避免重载恢复后与现有手写绑定冲突；
  - Exporter 构建日志已输出模块级 coverage 诊断（如 `Engine/UMG/GameplayAbilities/UnrealEd/AIModule` 的 direct/stub/shard 统计）；
  - `Angelscript.TestModule.Engine.BindConfig.` 前缀的进一步验证暂被外部 `Build.bat` 锁争用阻塞，需在无并发 Build.bat 占锁时重新执行并收口统计值。
- 已追加两条 Phase 5 白名单样本恢复：`URuntimeFloatCurveMixinLibrary::GetNumKeys` 与 `URuntimeFloatCurveMixinLibrary::GetTimeRange` 已从 `ERASE_NO_FUNCTION()` 恢复为显式 `ERASE_FUNCTION_PTR(...)` direct bind；当前 `AngelscriptRuntime` 模块覆盖统计已由 `direct=247 / stubs=161` 提升到 `direct=249 / stubs=159`。

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

**但关键问题是**：这套机制依赖 `ClassFuncMaps` 中存在函数的 `FFuncEntry`（函数指针 + Caller），而当前：

- `FunctionCallers_*.cpp`（14 个文件，曾包含 ~27000 条目）**全部被注释掉**，是死代码
- 唯一活跃的 `AddFunctionEntry` 调用仅有 5 条（GAS 库）
- `Bind_BlueprintCallable` 对 `ClassFuncMaps` 中找不到条目的函数**直接跳过**
- 实际函数绑定完全依赖手写 Bind_*.cpp 文件（~120 个）中的直接注册

### 性能损失评估

| 场景 | 当前状态 | vs Hazelight |
|------|---------|-------------|
| 手写绑定覆盖的函数 | 使用 `ASAutoCaller` 直接调用 | **等价——无性能损失** |
| 未被手写绑定覆盖的 BlueprintCallable 函数 | **完全不可用**（不是慢，是没绑定） | **覆盖率差距** |
| 通过 `asCALL_GENERIC` 注册的函数 | 经 `CallGeneric` + `asIScriptGeneric` 间接调用 | **有额外间接调用开销** |
| Blueprint→Script 回调 | `FUNC_Native` + `UASFunctionNativeThunk` → `RuntimeCallFunction` | 已等价：仅比 Hazelight 多一次 thunk 跳转和 Cast（纳秒级） |

**核心结论**：性能损失不在于"对等函数调用变慢"，而在于**大量 BlueprintCallable 函数因缺少函数指针条目而完全无法通过直接调用路径暴露给脚本**。Hazelight 通过 UHT 自动为所有 UFUNCTION 生成函数指针，实现了全自动覆盖。

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

### Phase 1：UHT 插件框架搭建

> 目标：创建可编译运行的 UHT C# 插件骨架，验证插件加载和类型遍历能力。

- [ ] **P1.1** 创建 `AngelscriptUhtPlugin` 项目结构
  - 在 `Plugins/Angelscript/Source/` 下创建 `AngelscriptUhtPlugin/` 目录，包含 `AngelscriptUhtPlugin.ubtplugin.csproj`
  - 项目引用 `EpicGames.UHT`、`EpicGames.Core`、`EpicGames.Build`、`UnrealBuildTool` 的 DLL
  - 输出路径设为 `Plugins/Angelscript/Binaries/DotNET/UnrealBuildTool/Plugins/AngelscriptUhtPlugin/`
  - 参考 `UHT-Plugin-Capabilities-Reference.md` §4.2 的 csproj 模板
- [ ] **P1.1** 📦 Git 提交：`[Plugin/UHT] Feat: scaffold AngelscriptUhtPlugin C# project`

- [ ] **P1.2** 实现最小 Exporter 骨架
  - 创建 `AngelscriptFunctionTableExporter.cs`，注册为 `[UhtExporter]`，`Options = UhtExporterOptions.Default | UhtExporterOptions.CompileOutput`
  - 遍历 `factory.Session.Packages` → `UhtClass` → `UhtFunction`，计数 BlueprintCallable 函数并输出到日志
  - 验证插件在 UBT 构建时被正确加载和执行
- [ ] **P1.2** 📦 Git 提交：`[Plugin/UHT] Feat: minimal exporter skeleton with type traversal`

### Phase 2：函数表生成器

> 目标：Exporter 能为所有 BlueprintCallable 函数生成正确的 `AddFunctionEntry` C++ 代码并参与编译。

- [ ] **P2.1** 实现 C++ 类型签名还原
  - 从 `UhtFunction` 的参数列表（`UhtProperty` Children）还原原始 C++ 函数签名
  - 处理 const/ref（从 `PropertyFlags` 和 MetaData 推断）、指针、TEnumAsByte、TSubclassOf 等
  - 处理返回值类型、void 特殊情况
  - 静态函数生成 `ERASE_FUNCTION_PTR`，实例方法生成 `ERASE_METHOD_PTR`
  - 当前 UHT 类型系统中 `UhtProperty` 的 `PropertyExportFlags` 和 `PropertyFlags` 已包含足够信息用于还原 const/ref，无需 Hazelight 的 `EAngelscriptPropertyFlags`
- [ ] **P2.1** 📦 Git 提交：`[Plugin/UHT] Feat: C++ type signature reconstruction from UHT types`

- [ ] **P2.2** 实现函数表 C++ 文件生成
  - 按模块分片生成 `AS_FunctionTable_<ModuleName>.cpp` 文件（避免单文件过大）
  - 每个文件包含：必要的 `#include`、`FAngelscriptBinds::AddFunctionEntry(...)` 调用
  - include 列表从 `UhtHeaderFile` 路径生成
  - 使用 `factory.CommitOutput()` 写入生成目录，由 `CompileOutput` 参与编译
  - 过滤条件：`BlueprintCallable || BlueprintPure`，排除 `NotInAngelscript`、`BlueprintInternalUseOnly`、`CustomThunk`
  - 对于 `NativeInterface` 类或包含静态数组/不支持参数的函数，生成 `ERASE_NO_FUNCTION()`
- [ ] **P2.2** 📦 Git 提交：`[Plugin/UHT] Feat: generate AddFunctionEntry C++ files per module`

- [ ] **P2.3** 构建集成与增量更新验证
  - 确保生成的 C++ 文件被 AngelscriptRuntime 模块编译（可能需要在 Build.cs 中添加 generated 目录）
  - 验证增量构建：头文件未变时不重新生成、生成内容未变时不重写文件
  - 验证完整构建：从零开始构建时生成所有文件并编译通过
  - 测量生成时间对总构建时间的影响
- [ ] **P2.3** 📦 Git 提交：`[Plugin/UHT] Feat: build integration and incremental update`

### Phase 3：运行时集成与验证

> 目标：自动生成的函数表在运行时正确填充 `ClassFuncMaps`，`BindBlueprintCallable` 路径恢复工作。

- [ ] **P3.1** 运行时加载验证
  - 确保 UHT 生成的 `AddFunctionEntry` 调用在模块加载时执行（可能需要 `FAngelscriptBinds::FBind` 包装，或利用全局静态初始化）
  - 验证 `ClassFuncMaps` 在绑定阶段前已填充
  - 打印统计：自动填充条目数 vs 手写条目数
- [ ] **P3.1** 📦 Git 提交：`[Plugin/UHT] Feat: runtime loading and ClassFuncMaps population`

- [ ] **P3.2** 与手写绑定的兼容性处理
  - `AddFunctionEntry` 已有去重逻辑（`ClassFuncMaps.Contains` 检查），手写在先的条目不会被覆盖
  - 确保 UHT 生成的条目在手写 Bind_*.cpp 之前注册（利用 `FAngelscriptBinds::EOrder` 或静态初始化顺序）
  - 验证手写绑定仍能覆盖自动生成的绑定（对于需要自定义 wrapper 的函数）
- [ ] **P3.2** 📦 Git 提交：`[Plugin/UHT] Feat: hand-written bind compatibility`

- [ ] **P3.3** 功能验证测试
  - 选取 10 个有代表性的 UE 类（`AActor`、`UWorld`、`UGameplayStatics`、`APlayerController` 等），验证其 BlueprintCallable 函数自动可用
  - 对比 Hazelight 的覆盖列表，统计覆盖率差异
  - 添加自动化测试：调用若干自动绑定的函数，验证参数传递和返回值正确
- [ ] **P3.3** 📦 Git 提交：`[Plugin/UHT] Test: automated function table coverage verification`

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

#### 当前覆盖率基线（基于 2026-04-05 主线稳定版）

- 当前生成结果中，`ERASE_NO_FUNCTION()` 仍约有 **5619** 个，direct bind（`ERASE_AUTO_FUNCTION_PTR` / `ERASE_AUTO_METHOD_PTR`）约有 **423** 个。
- `ERASE_NO_FUNCTION()` 主要集中在：`Engine`、`UMG`、`GameplayAbilities`、`UnrealEd`、`AIModule`、`AssetRegistry`。
- 这些 fallback 目前混合了三类原因：
  1. **明确不可插件化 direct bind**：`MinimalAPI` / 未导出符号 / 非 public / interface；
  2. **当前策略保守跳过但理论可恢复**：多重重载、静态帮助类、部分 editor-only API；
  3. **需要更细粒度签名/可见性判定**：函数级 `*_API`、类级 `*_API`、模板/typedef 包装、重载候选精确选型。

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

  - [x] **P5.2.a** `UsableInAngelscript` 元数据 override
    - 在 `Bind_BlueprintCallable.cpp` 中对 `BlueprintInternalUseOnly` 增加 `UsableInAngelscript` 放行逻辑
    - 目标：恢复一批当前因 `BlueprintInternalUseOnly` 被硬过滤、但脚本侧可安全暴露的节点
  - [x] **P5.2.b** `ScriptMethod` 按函数级 mixin 支持
    - 当前本地仅支持类级 `ScriptMixin`；补齐单函数级 `ScriptMethod` 逻辑
    - 目标：恢复一批静态帮助函数的实例风格调用能力，和 Hazelight/UE 约定对齐
  - [ ] **P5.2.c** `ScriptAllowTemporaryThis` / `CallableWithoutWorldContext` 元数据
    - 对齐 Hazelight 已支持但本地尚未覆盖的元数据语义
    - 目标：先扩展脚本侧行为能力，再决定这些函数是否进入 direct bind 恢复集合
    - 当前进度：`CallableWithoutWorldContext` 已完成；`ScriptAllowTemporaryThis` 在当前代码库、UEAS2 参考和 AngelScript traits 中均无现成语义/trait，对应能力暂记为“待设计”而非强行落地
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
  - 当前已验证通过的 `BindConfig` 测试作为基线，不得回归
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
| `FUNC_RuntimeGenerated` + ScriptCore.cpp 执行路径 | Blueprint VM 调用脚本函数的快速路径 | 已通过 `FUNC_Native` + `UASFunctionNativeThunk` 实现等价快速路径；仅 RPC 场景多一次 FFrame 栈分配（纳秒级） |
| `ICppStructOps` 签名变更 | 脚本自定义结构体的构造/析构/复制 | 已通过 `FASStructOps` + FakeVTable 机制实现等价功能，无需引擎修改 |
| `FProperty::AngelscriptPropertyFlags` | 属性的 C++ 类型限定符运行时查询 | 可从元数据运行时重建，或由 UHT 插件导出到侧通道文件 |
| `UClass` 扩展字段 | `ScriptTypePtr`、`bIsScriptClass` | 使用外部 `TMap<UClass*, FScriptClassData>` 映射 |

### 预期收益量化

| 指标 | 当前状态 | 实施后预期 |
|------|---------|----------|
| BlueprintCallable 函数覆盖率 | ~120 个手写 Bind 文件覆盖的子集 | 所有 BlueprintCallable 函数（数千个） |
| 函数表维护工作量 | 手动添加/更新 | 零维护（自动生成） |
| 每次调用性能 | 已绑定函数等价 Hazelight | 等价 Hazelight |
| 对引擎版本的依赖 | 手写绑定需要随引擎更新 | 自动适应引擎变更 |
