# UHT 插件计划：自动生成函数调用表

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
| **自定义说明符**（`ScriptCallable`、`ScriptReadOnly` 等） | 🟢 纯增量 | 可通过 UHT 插件 `[UhtSpecifier]` 实现 | 无性能影响 |
| **函数默认值元数据**（`ScriptCallable` 函数也保存默认值） | 🟢 纯增量 | 可通过 UHT 插件处理 | 无性能影响 |
| **`FUNC_RuntimeGenerated` + ScriptCore 执行路径** | 🔴 枚举新增 | 无插件层替代，使用 `ProcessEvent` 路径 | **是——回调方向有开销** |
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
| Blueprint→Script 回调 | 通过 `ProcessEvent` | Hazelight 有 `FUNC_RuntimeGenerated` 快速路径 |

**核心结论**：性能损失不在于"对等函数调用变慢"，而在于**大量 BlueprintCallable 函数因缺少函数指针条目而完全无法通过直接调用路径暴露给脚本**。Hazelight 通过 UHT 自动为所有 UFUNCTION 生成函数指针，实现了全自动覆盖。

### 本计划目标

通过 UHT Exporter 插件**自动生成 `AddFunctionEntry` 调用**，为所有 BlueprintCallable（及 ScriptCallable）函数填充 `ClassFuncMaps`，使 `BindBlueprintCallable` 自动发现路径恢复工作：

1. **消除手动维护负担**——不再需要手写 FunctionCallers 表
2. **实现全覆盖**——所有 BlueprintCallable 函数自动获得直接调用路径
3. **保持等价性能**——使用与 Hazelight 相同的 `ASAutoCaller` 调用机制
4. **无引擎修改**——纯 UHT 插件 + 插件侧代码

## 范围与边界

### 在范围内

- UHT Exporter 插件：遍历所有 UCLASS/UFUNCTION，生成 `AddFunctionEntry` 调用的 C++ 文件
- UHT Specifier 插件：注册 `ScriptCallable`、`ScriptReadOnly`、`ScriptReadWrite`、`ScriptEditOnly` 自定义说明符
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

- [ ] **P1.3** 实现自定义 Specifier 注册
  - 创建 `AngelscriptSpecifiers.cs`，注册 `ScriptCallable`（UFUNCTION）、`ScriptReadOnly`/`ScriptReadWrite`/`ScriptEditOnly`（UPROPERTY）四个说明符
  - 说明符将值写入 `MetaData`，与 Hazelight 的行为一致
  - 验证在 C++ 头文件中使用 `UFUNCTION(ScriptCallable)` 不报错，且 MetaData 可在 Exporter 中读到
- [ ] **P1.3** 📦 Git 提交：`[Plugin/UHT] Feat: register ScriptCallable and ScriptReadOnly specifiers`

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
  - 过滤条件：`BlueprintCallable || BlueprintPure || MetaData["ScriptCallable"]`，排除 `NotInAngelscript`、`BlueprintInternalUseOnly`、`CustomThunk`
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

## 验收标准

1. **构建通过**：UHT 插件在完整构建和增量构建中均正常工作
2. **覆盖率**：所有 BlueprintCallable/BlueprintPure/ScriptCallable 函数自动获得 `FFuncEntry`
3. **性能等价**：自动绑定的函数使用 `ASAutoCaller` 直接调用，无反射间接开销
4. **手写兼容**：现有手写 Bind_*.cpp 不受影响，仍可覆盖自动绑定
5. **增量安全**：源文件未变时不触发重新生成和重新编译
6. **自定义说明符可用**：C++ 中可使用 `ScriptCallable` 等说明符标记函数
7. **死代码清除**：FunctionCallers_*.cpp 全部移除

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
| `FUNC_RuntimeGenerated` + ScriptCore.cpp 执行路径 | Blueprint VM 调用脚本函数的快速路径 | 当前通过 `ProcessEvent` 路径，有一定开销 |
| `ICppStructOps` 签名变更 | 脚本自定义结构体的构造/析构/复制 | 当前不支持脚本自定义结构体操作 |
| `FProperty::AngelscriptPropertyFlags` | 属性的 C++ 类型限定符运行时查询 | 可从元数据运行时重建，或由 UHT 插件导出到侧通道文件 |
| `UClass` 扩展字段 | `ScriptTypePtr`、`bIsScriptClass` | 使用外部 `TMap<UClass*, FScriptClassData>` 映射 |

### 预期收益量化

| 指标 | 当前状态 | 实施后预期 |
|------|---------|----------|
| BlueprintCallable 函数覆盖率 | ~120 个手写 Bind 文件覆盖的子集 | 所有 BlueprintCallable 函数（数千个） |
| 函数表维护工作量 | 手动添加/更新 | 零维护（自动生成） |
| 每次调用性能 | 已绑定函数等价 Hazelight | 等价 Hazelight |
| 对引擎版本的依赖 | 手写绑定需要随引擎更新 | 自动适应引擎变更 |
