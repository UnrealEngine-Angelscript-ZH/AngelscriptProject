# AS 2.38 Computed Goto 运行时优化回移计划

## 背景与目标

AngelScript 2.37 为 GCC/Clang 编译器启用了 **computed goto**（labels-as-values 扩展）优化 VM 字节码分发循环，2.38 修复了该特性默认未启用的问题。这一优化将 `as_context.cpp` 中 `ExecuteNext()` 的核心 `switch` 分发替换为 **`dispatch_table[256]` + `goto *(void*)dispatch_table[opcode]`** 的直接跳转，在 GCC/Clang 下可显著降低分支预测 miss 率。

当前项目 ThirdParty（2.33 + UE 补丁）的 `as_context.cpp` 仍使用经典 `switch` 分发，`as_config.h` 中无 `AS_USE_COMPUTED_GOTOS` 宏。

**目标**：将 2.38 的 computed goto 双路径机制合入当前 ThirdParty，使得：

1. GCC/Clang 编译时自动启用 computed goto 分发，提升 VM 执行性能
2. MSVC 编译时保持 `switch` 分发不变（MSVC 不支持 labels-as-values）
3. 行为完全透明，所有现有测试不受影响

## 范围与边界

### 在范围内

- `as_config.h` 中 `AS_USE_COMPUTED_GOTOS` 宏定义与平台守卫
- `as_context.cpp` 中 `ExecuteNext()` 的双路径分发机制（`INSTRUCTION`/`NEXT_INSTRUCTION`/`BEGIN` 宏 + `dispatch_table`）
- `as_context.cpp` 中 computed goto 关闭时的稀疏 opcode 补齐技巧

### 不在范围内

- VM 指令集变更
- 其他 `as_context.cpp` 中的非分发相关改动
- 性能基准测试框架（由 `Plan_PerformanceBenchmarkFramework` 覆盖）

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| `AS_USE_COMPUTED_GOTOS` 宏 | 不存在 | `as_config.h:659-667`，GCC/Clang 默认开启 |
| `dispatch_table` | 不存在 | `as_context.cpp:2208-2288`，`static const void *const dispatch_table[256]` |
| `INSTRUCTION/NEXT_INSTRUCTION/BEGIN` 宏 | 不存在 | `as_context.cpp:2208-2216`，双路径定义 |
| `ExecuteNext()` 分发 | 直接 `switch (*(asBYTE*)l_bc)` | 双路径：computed goto 或 switch |
| 稀疏 opcode 补齐 | 无 | `as_context.cpp:4887-4942`，`switch` 路径下对 201-255 补齐 `FAULT` |

## 分阶段执行计划

### Phase 1 — 配置宏与分发机制

> 目标：在 `as_config.h` 中引入 `AS_USE_COMPUTED_GOTOS` 平台守卫宏，在 `as_context.cpp` 中用宏重新包装 `ExecuteNext()` 的分发循环，使 GCC/Clang 走 computed goto 路径、MSVC 走 switch 路径。完成后所有平台编译通过，行为不变。

- [ ] **P1.1** 在 `as_config.h` 中添加 `AS_USE_COMPUTED_GOTOS` 宏定义
  - 当前 `as_config.h` 在 GNU 风格编译器分支（`__GNUC__` 且非 `_MSC_VER`）中有其他平台配置，在同一区域末尾添加 `#define AS_USE_COMPUTED_GOTOS 1`
  - MSVC 分支不定义此宏（MSVC 不支持 labels-as-values 扩展，`&&label` 语法会编译失败）
  - 参考 2.38 `as_config.h:659-667` 的守卫条件；需注意当前 ThirdParty 的 `as_config.h` 已有 `//[UE++]` 区域，新增宏应避免与已有修改冲突
  - 使用 `//[UE++]: Backport computed goto support from AS 2.38` 标注
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add AS_USE_COMPUTED_GOTOS platform guard macro`

- [ ] **P1.2** 在 `as_context.cpp` 的 `ExecuteNext()` 中引入双路径分发宏
  - 在 `ExecuteNext()` 函数内、主循环开始前，添加宏定义块：
    - `AS_USE_COMPUTED_GOTOS == 1` 时：`INSTRUCTION(x)` → `case_##x`（label）、`NEXT_INSTRUCTION()` → `goto *(void*)dispatch_table[*(asBYTE*)l_bc]`、`BEGIN()` → `NEXT_INSTRUCTION()`
    - 否则：`INSTRUCTION(x)` → `case x`、`NEXT_INSTRUCTION()` → `break`、`BEGIN()` → `switch(*(asBYTE*)l_bc)`
  - 将现有 `switch(*(asBYTE*)l_bc)` 替换为 `BEGIN()`，每个 `case asBC_*:` 替换为 `INSTRUCTION(asBC_*)`，每个 `break;`（分发回主循环处）替换为 `NEXT_INSTRUCTION()`
  - 这是 `ExecuteNext()` 中最大的机械替换工作，约涉及 200+ 个 case 分支
  - 参考 2.38 `as_context.cpp:2208-2216` 的宏定义和 `2217+` 的使用模式
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: replace switch dispatch with dual-path INSTRUCTION macros in ExecuteNext`

- [ ] **P1.3** 添加 `dispatch_table[256]` 静态跳转表（computed goto 路径）
  - 在 `ExecuteNext()` 内、`BEGIN()` 之前，添加 `#if AS_USE_COMPUTED_GOTOS` 守卫的 `static const void *const dispatch_table[256]`
  - 表中每个条目对应一个 opcode 的 label 地址（`&&case_asBC_PopPtr` 等），未使用的 opcode 指向 `&&case_FAULT`
  - 此表与宏定义配合实现 computed goto 分发
  - 参考 2.38 `as_context.cpp:2222-2288` 的完整表内容
  - 需要与当前 ThirdParty 的 opcode 枚举逐一核对，确保不遗漏已有的 `//[UE++]` opcode 修改
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: add dispatch_table for computed goto jump targets`

- [ ] **P1.4** 添加稀疏 opcode 补齐（switch 路径）
  - 在 `ExecuteNext()` 的 switch 语句末尾、`#if !AS_USE_COMPUTED_GOTOS` 守卫下，为 opcode 201-255 添加 fall-through 到 `FAULT` 的空 case
  - 这使得编译器在 switch 路径下不生成低效的稀疏跳转表
  - 参考 2.38 `as_context.cpp:4887-4942`
- [ ] **P1.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: add sparse opcode fallback for switch dispatch path`

### Phase 2 — 验证与文档

> 目标：确认所有平台编译通过，行为完全不变，更新变更追踪文档。

- [ ] **P2.1** 全量编译验证
  - Win64 MSVC（Development Editor）编译通过，确认 `AS_USE_COMPUTED_GOTOS` 未定义、走 switch 路径
  - 若本地有 Clang/GCC 工具链，验证 computed goto 路径编译通过
  - 运行全量测试套件（275/275），确认零回归
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Test: verify computed goto port with full regression`

- [ ] **P2.2** 更新 `AngelscriptChange.md`
  - 登记 `as_config.h` 和 `as_context.cpp` 中所有 `[UE++]` 修改位置和原因
  - 若文件不存在则创建
- [ ] **P2.2** 📦 Git 提交：`[Docs] Docs: document computed goto backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/angelscript/source/as_config.h` | 修改 | 添加 `AS_USE_COMPUTED_GOTOS` 宏定义与平台守卫 |
| `ThirdParty/angelscript/source/as_context.cpp` | 修改 | 双路径分发宏、`dispatch_table`、稀疏 opcode 补齐 |
| `AngelscriptChange.md` | 新增/修改 | 登记变更 |

## 验收标准

1. Win64 MSVC 编译通过，走 switch 分发路径，行为不变
2. 所有现有测试（275/275）仍然通过
3. `as_config.h` 中 `AS_USE_COMPUTED_GOTOS` 仅在 GCC/Clang 非 MSVC 时定义为 1
4. `as_context.cpp` 中 `ExecuteNext()` 使用 `INSTRUCTION`/`NEXT_INSTRUCTION`/`BEGIN` 宏，两路径逻辑等价
5. `dispatch_table[256]` 的 opcode 条目与当前引擎枚举完全对齐
6. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **Opcode 枚举差异**：2.33 和 2.38 的 opcode 枚举可能有增删，`dispatch_table` 必须基于当前 ThirdParty 的实际枚举生成，不能直接复制 2.38 的表
2. **机械替换风险**：200+ 个 case 分支替换为 `INSTRUCTION` 宏时，需确保不遗漏条件跳转内的 `break`（这些 break 是跳出 switch，computed goto 路径需等价处理）
3. **调试友好性**：computed goto 路径下断点设置不如 switch 直观，这是已知权衡
4. **Win64 为主要开发平台**：当前项目以 MSVC 为主，computed goto 实际生效需在 Linux/Mac/Console 构建时验证；Win64 下此改动是纯粹的代码重构（switch 路径不变）
