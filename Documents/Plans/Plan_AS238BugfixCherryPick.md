# AS 2.38 关键 Bug 修复回移计划

## 背景与目标

AngelScript 2.34 至 2.38 跨 5 年版本累积了**数十个 Bug 修复**，涵盖崩溃、内存破坏、编译器正确性、字节码序列化、GC、平台 ABI 等多个领域。当前项目使用的 2.33.0 WIP 基线可能已命中其中部分问题。

由于整体升级到 2.38 风险大、收益分散，本计划采用**按类别分批 cherry-pick** 策略：将官方 changelog 中的 Bug 修复按风险等级和影响域分类，每批独立回移并配全量回归测试。

**重要约束**：本项目的 AngelScript fork 与 vanilla 有显著差异（无 `@` 句柄语法、全局变量须 `const`、无脚本 `interface`、无 `mixin class` 等），部分修复可能不适用。每条修复需对照 `ASSDK_Fork_Differences.md` 做适用性评审。

**目标**：

1. 消除已知的崩溃和内存安全问题
2. 提升字节码序列化/加载的健壮性
3. 修正编译器正确性问题（重载匹配、初始化顺序等）
4. 为多平台部署补齐 ABI 修复

## 范围与边界

### 在范围内

- 2.34–2.38 官方 changelog 中所有标记为 "Bug fixes" 的条目
- 相关的 ThirdParty 源文件修改
- 每个 Sprint 的回归测试

### 不在范围内

- 新语言特性（foreach、using namespace、lambda 等——由各自独立 Plan 覆盖）
- 新 API 或行为变更（非 Bug 修复性质的改动）
- 与本 fork 不相关的修复（如纯 `@` 句柄、`?&` unsafe ref 的修复）

## 分阶段执行计划

### Phase 0 — 分类与适用性评审

> 目标：对 2.34–2.38 所有 Bug 修复条目完成分类标注和适用性判定，输出 cherry-pick 候选清单。

- [ ] **P0.1** 建立 Bug 修复分类表
  - 从 `Reference/angelscript-v2.38.0/sdk/docs/articles/changes2.html` 中提取所有 Bug fixes 条目
  - 按以下类别标注：**Crash**（崩溃/内存破坏）、**Correctness**（编译器/语义正确性）、**Serialization**（字节码/序列化）、**GC**（垃圾回收/引用计数）、**Platform**（ABI/平台/构建）、**Optimizer**（字节码优化）
  - 为每条标注来源版本（2.34/2.35/2.36/2.37/2.38）
  - 输出为 `Documents/Plans/AS238_BugfixTriage.md` 或等效文档
- [ ] **P0.1** 📦 Git 提交：`[Docs] Docs: create AS 2.34-2.38 bugfix triage table`

- [ ] **P0.2** 对照 `ASSDK_Fork_Differences.md` 做适用性标注
  - 对每条修复评估：**适用**（fork 受影响）、**可能适用**（需代码验证）、**不适用**（fork 不使用相关语法/路径）
  - 重点过滤依赖以下 fork 不支持特性的修复：`@` 句柄语法、`?&` unsafe ref、脚本 `interface`、`mixin class`、全局非 `const` 变量
  - 在分类表中标注适用性列
- [ ] **P0.2** 📦 Git 提交：`[Docs] Docs: annotate bugfix applicability against fork differences`

- [ ] **P0.3** 确定 Sprint 分配
  - 将"适用"和"可能适用"的修复按以下 Sprint 分组：
    - **Sprint A**：崩溃/内存破坏（最高优先）
    - **Sprint B**：字节码/序列化
    - **Sprint C**：编译器正确性
    - **Sprint D**：平台/ABI
  - 每个 Sprint 内按版本顺序排列（2.34 → 2.38）
  - 估算每个 Sprint 的工作量和涉及文件
- [ ] **P0.3** 📦 Git 提交：`[Docs] Docs: finalize bugfix cherry-pick sprint allocation`

### Sprint A — 崩溃与内存破坏修复

> 目标：消除已知的崩溃和内存安全问题，这些是影响面最大、最不可容忍的缺陷类别。

- [ ] **SA.1** 回移 context 复用/栈安全相关修复
  - 2.38："Fixed memory invasion when reusing a context after a previous execution increased the stack"
  - 2.37："Fixed crash after exception where the stack pointer was not properly restored"
  - 涉及 `as_context.cpp`：栈指针管理、context 状态恢复
  - 每条修复需在 2.38 参考源中定位对应代码差异，然后在当前 ThirdParty 上适配
  - 验证方式：编写测试模拟 context 复用场景（执行→异常→重新执行），确认不崩溃
- [ ] **SA.1** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport context reuse and stack safety fixes`

- [ ] **SA.2** 回移 try/catch 栈指针相关修复
  - 2.38："Fixed try/catch corrupting the stack pointer"
  - 涉及 `as_context.cpp` 和可能的 `as_compiler.cpp`（try/catch 字节码生成）
  - 验证方式：编写包含 try/catch + 嵌套调用的脚本，确认栈不被破坏
- [ ] **SA.2** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport try/catch stack pointer fix`

- [ ] **SA.3** 回移编译期崩溃修复
  - 2.38 修复了多种编译期崩溃：`ashandle` 初始化、`null` 赋值、`auto[]`、funcdef 按值参数等
  - 涉及 `as_compiler.cpp`、`as_builder.cpp`
  - 逐条评估适用性后批量回移
  - 验证方式：编写触发对应场景的编译脚本，确认编译器不崩溃（可编译失败但不 crash）
- [ ] **SA.3** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport compiler crash fixes`

- [ ] **SA.4** 回移引擎关闭/生命周期崩溃修复
  - 2.38："Fixed crash when shutting down engine while contexts are still prepared"
  - 2.36.1："Fixed template instance being prematurely deleted when still referenced"
  - 涉及 `as_scriptengine.cpp`、`as_module.cpp`
  - 验证方式：测试引擎 shutdown 时有 prepared context 和活跃模板实例的场景
- [ ] **SA.4** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport engine shutdown and lifecycle crash fixes`

- [ ] **SA.5** Sprint A 全量回归
  - 运行全量测试套件（275/275），确认零回归
  - 若发现回归，回退对应修复并记录原因
- [ ] **SA.5** 📦 Git 提交：`[ThirdParty/AS238] Test: verify Sprint A bugfixes with full regression`

### Sprint B — 字节码与序列化修复

> 目标：提升 bytecode save/load 的健壮性，修正已知的非法字节码和格式兼容问题。

- [ ] **SB.1** 回移 LoadByteCode 健壮性修复
  - 2.35.1/2.36.0/2.38 多版本修复了 `LoadByteCode` 在各种边界条件下的崩溃或不正确行为
  - 涉及 `as_restore.cpp`：非法字节码序列检测、类型不匹配检查
  - 逐条评估后批量回移
  - 验证方式：使用 `FASSDKBytecodeStream` 做 Save/Load 往返测试
- [ ] **SB.1** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport LoadByteCode robustness fixes`

- [ ] **SB.2** 回移字节码 32/64 位兼容修复
  - 2.36.0："Fixed saving bytecode with unsafe references for cross 32/64 bit compatibility"
  - 涉及 `as_bytecode.cpp`、`as_restore.cpp`
  - 此修复对跨平台部署重要
- [ ] **SB.2** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport bytecode 32/64-bit compatibility fixes`

- [ ] **SB.3** 回移 VM 指令相关修复
  - 2.35.1："Fixed asBC_REFCPY reading uninitialized memory"
  - 涉及 `as_context.cpp` 中的 VM 指令实现
- [ ] **SB.3** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport VM instruction fixes`

- [ ] **SB.4** Sprint B 全量回归
  - 运行全量测试套件，确认零回归
- [ ] **SB.4** 📦 Git 提交：`[ThirdParty/AS238] Test: verify Sprint B bugfixes with full regression`

### Sprint C — 编译器正确性修复

> 目标：修正编译器在重载匹配、初始化顺序、类型推导等方面的已知问题。

- [ ] **SC.1** 回移重载匹配和类型推导修复
  - 2.38："Fixed `&out` overload matching"、"Fixed inherited member initialization order"
  - 2.37："Fixed `Obj@` vs `const Obj@` overload"
  - 2.36.1："Fixed lambda with `&in/out/inout`"
  - 涉及 `as_compiler.cpp`、`as_builder.cpp`
  - 逐条评估适用性（特别是依赖 `@` 句柄的条目），批量回移适用的
- [ ] **SC.1** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport compiler overload and type deduction fixes`

- [ ] **SC.2** 回移 GC 和引用计数修复
  - 2.38："Fixed template function refcount"、"Fixed shared class factory premature release"
  - 2.35.0："Fixed funcdefs should be flagged asOBJ_GC"、"Fixed CompileGlobalVar leak"
  - 涉及 `as_scriptengine.cpp`、`as_gc.cpp`、`as_compiler.cpp`
- [ ] **SC.2** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport GC and reference count fixes`

- [ ] **SC.3** 回移优化器修复
  - 2.38："Fixed aggressive optimization of ternary operator corrupting result written to global variable"
  - 涉及 `as_compiler.cpp` 优化路径
  - 此修复直接影响正确性，优先级高
- [ ] **SC.3** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport optimizer correctness fixes`

- [ ] **SC.4** Sprint C 全量回归
  - 运行全量测试套件，确认零回归
- [ ] **SC.4** 📦 Git 提交：`[ThirdParty/AS238] Test: verify Sprint C bugfixes with full regression`

### Sprint D — 平台与 ABI 修复

> 目标：为多平台部署补齐 ABI 兼容修复，主要影响 ARM64 和 GCC/Clang 构建。

- [ ] **SD.1** 回移 ARM64 相关修复
  - 2.38："Fixed `as_callfunc_arm64_msvc`"、"Fixed 32-bit ARM hard float"
  - 2.37："Fixed arm64 `?&`"、"Fixed arm64 two double struct return"
  - 2.35.1："Fixed arm64 `asOBJ_APP_CLASS_ALLFLOATS` return reference"
  - 涉及 `as_callfunc_arm64*.cpp/asm`
  - 仅在目标平台包含 ARM64 时需要
- [ ] **SD.1** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport ARM64 ABI fixes`

- [ ] **SD.2** 回移 GCC/Clang 编译器兼容修复
  - 2.38："Fixed `as_callfunc.cpp` with gcc 12"
  - 2.36.1："Fixed `as_symboltable.h` with GCC 11"
  - 涉及对应源文件的编译器兼容调整
- [ ] **SD.2** 📦 Git 提交：`[ThirdParty/AS238] Fix: backport GCC/Clang compatibility fixes`

- [ ] **SD.3** Sprint D 全量回归
  - 运行全量测试套件（至少 Win64），确认零回归
  - 若有 ARM64/Linux 构建环境，进行交叉验证
- [ ] **SD.3** 📦 Git 提交：`[ThirdParty/AS238] Test: verify Sprint D bugfixes with full regression`

### Phase 3 — 文档

> 目标：更新变更追踪文档。

- [ ] **P3.1** 更新 `AngelscriptChange.md`
  - 登记所有回移修复的 `[UE++]` 位置、原始版本号和 changelog 条目引用
- [ ] **P3.1** 📦 Git 提交：`[Docs] Docs: document backported bugfixes in AngelscriptChange.md`

- [ ] **P3.2** 更新 `ASSDK_Fork_Differences.md`
  - 在 fork 差异文档中记录已回移的修复和仍不适用的条目
- [ ] **P3.2** 📦 Git 提交：`[Docs] Docs: update fork differences with backported fixes status`

## 验收标准

1. Phase 0 产出完整的 Bug 修复分类表，每条标注类别、版本、适用性
2. 每个 Sprint 完成后全量测试通过
3. 所有已知的崩溃/内存破坏修复（Sprint A）已回移并验证
4. 字节码 Save/Load 健壮性修复（Sprint B）已回移并验证
5. 编译器正确性修复（Sprint C）中适用的条目已回移
6. 不适用的条目在分类表中有明确标注和原因
7. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **上下文漂移**：每条修复在 2.38 源码中的上下文可能与当前 2.33 ThirdParty 不同，不能直接 patch 应用，需逐条手工适配
2. **修复间依赖**：某些修复可能依赖同版本中的其他改动（如数据结构变更、新增字段），需识别并一起回移
3. **回归风险**：修复可能改变已有的（虽然不正确的）行为，导致依赖该行为的脚本出错。每个 Sprint 的全量回归是关键安全网
4. **工作量大**：2.34-2.38 累计数十个修复，完整回移可能需要多个迭代周期。建议 Sprint A 优先完成，B-D 可按需推进
5. **fork 特有问题**：部分 `//[UE++]` 修改可能已修复了与官方修复相同的问题（通过不同方式），需要交叉检查避免重复或冲突
