# AS 2.38 上下文 Bool 转换模式回移计划

## 背景与目标

AngelScript 2.38 新增了 `asEP_BOOL_CONVERSION_MODE` 引擎属性的**实际生效逻辑**。该属性允许在条件表达式位置（`if`/`while`/`for`/三元/逻辑运算符/一元 `!`）使用更宽松的 bool 隐式转换：

- **Mode 0**（默认/旧行为）：条件位置仅对**值类型**走 `asIC_IMPLICIT_CONV`（即 `opImplConv`）
- **Mode 1**（新行为）：条件位置走 `asIC_EXPLICIT_VAL_CAST`，从而 **`opConv`** 也可参与转换，覆盖引用类型和仅定义了 `opConv` 的值类型

当前 ThirdParty 中 `asEP_BOOL_CONVERSION_MODE` 的引擎属性注册（Set/Get）已存在，但 `as_compiler.cpp` **不读取该属性**——它是一个死属性。所有条件路径统一使用 `asIC_IMPLICIT_CONV + value type` 的行为。

**目标**：让 `asEP_BOOL_CONVERSION_MODE` 实际生效，使得：

1. Mode 0 保持当前行为不变
2. Mode 1 启用时，条件位置可通过 `opConv` 隐式转 bool（减少脚本中冗余的 `!= null` / `!= 0`）
3. 不改变引擎默认值（默认 Mode 0，向后兼容）

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| `asEP_BOOL_CONVERSION_MODE` Set/Get | 存在（`as_scriptengine.cpp`） | 存在 |
| `ep.boolConversionMode` 默认值 | 0 | 0 |
| `as_compiler.cpp` 中读取 `boolConversionMode` | 无（零引用） | 8 处分支 |
| 条件位置转换策略 | 固定 `asIC_IMPLICIT_CONV` | 按 mode 0/1 分支 |

## 分阶段执行计划

### Phase 1 — 编译器分支插入

> 目标：在 `as_compiler.cpp` 的 8 个条件/逻辑判断位置插入 `boolConversionMode` 分支，使 Mode 1 时使用 `asIC_EXPLICIT_VAL_CAST`。完成后 Mode 0 行为不变，Mode 1 新增能力可用。

- [ ] **P1.1** 在 `CompileIfStatement` 中插入 bool conversion mode 分支
  - 当前 ThirdParty 的 `CompileIfStatement`（约 5300 行附近）在计算条件表达式后，对非 bool 类型调用 `ImplicitConversion(..., asIC_IMPLICIT_CONV)`
  - 需添加分支：若 `engine->ep.boolConversionMode == 1`，则改用 `ImplicitConversion(..., asIC_EXPLICIT_VAL_CAST)`
  - 注意当前 ThirdParty 可能使用 `CompileCondition` 包装函数而非直接在 `CompileIfStatement` 中做转换，需定位实际的转换调用点
  - 参考 2.38 `as_compiler.cpp:4823` 的分支模式
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in CompileIfStatement`

- [ ] **P1.2** 在 `CompileForStatement` 条件部分插入分支
  - `for` 循环的条件表达式评估路径，同样需要按 mode 选择转换策略
  - 参考 2.38 `as_compiler.cpp:5060` 的分支模式
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in CompileForStatement`

- [ ] **P1.3** 在 `CompileWhileStatement` 和 `CompileDoWhileStatement` 中插入分支
  - 两个循环语句的条件评估路径，各需要一处分支
  - 参考 2.38 `as_compiler.cpp:5705`（while）和 `5803`（do-while）的分支模式
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in while/do-while`

- [ ] **P1.4** 在三元运算符 `?:` 条件部分插入分支
  - 三元表达式的条件评估，需在条件求值后按 mode 选择转换策略
  - 参考 2.38 `as_compiler.cpp:9902` 的分支模式
- [ ] **P1.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in ternary condition`

- [ ] **P1.5** 在一元 `!` 操作数中插入分支
  - 逻辑非操作的操作数需要隐式转 bool，需按 mode 选择
  - 参考 2.38 `as_compiler.cpp:13347` 的分支模式
- [ ] **P1.5** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in unary not`

- [ ] **P1.6** 在二元 `&&` / `||` 操作数中插入分支（2 处）
  - 逻辑与/或的左右操作数各需要按 mode 选择转换策略
  - 参考 2.38 `as_compiler.cpp:16889` 和 `16896` 的分支模式
- [ ] **P1.6** 📦 Git 提交：`[ThirdParty/AS238] Feat: add boolConversionMode branch in logical and/or`

### Phase 2 — 测试与文档

> 目标：编写测试验证 Mode 0 不退化、Mode 1 新增能力正确，更新变更追踪文档。

- [ ] **P2.1** 编写 bool conversion mode 测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptBoolConversionTests.cpp`，遵循 Native Core 层规则
  - 使用 `CreateNativeEngine()` 创建独立引擎，分别在 Mode 0 和 Mode 1 下执行相同脚本
  - 测试用例清单：
    - **Mode0_ValueTypeImplConvWorks**：注册值类型含 `bool opImplConv()`，Mode 0 下 `if(obj)` 编译通过并正确调用
    - **Mode0_OnlyOpConvFails**：注册值类型仅含 `bool opConv()`（无 `opImplConv`），Mode 0 下 `if(obj)` 编译失败
    - **Mode1_OpConvInIfWorks**：同上类型，Mode 1 下 `if(obj)` 编译通过并正确调用 `opConv`
    - **Mode1_OpConvInWhileWorks**：Mode 1 下 `while(obj)` 正确
    - **Mode1_OpConvInTernaryWorks**：Mode 1 下 `obj ? 1 : 0` 正确
    - **Mode1_OpConvInNotWorks**：Mode 1 下 `!obj` 正确
    - **Mode1_OpConvInLogicalAndWorks**：Mode 1 下 `obj && true` 正确
    - **Mode0_UnchangedBehavior**：验证 Mode 0 下现有测试套件的条件路径行为不变（回归保护）
  - 参考 2.38 `sdk/tests/test_feature/source/test_bool.cpp:105-204`
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add bool conversion mode verification tests`

- [ ] **P2.2** 全量回归验证
  - 运行全量测试套件，确认 Mode 0（默认）下零回归
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Test: verify bool conversion port with full regression`

- [ ] **P2.3** 更新 `AngelscriptChange.md`
  - 登记 `as_compiler.cpp` 中 8 处 `[UE++]` 修改位置和原因
- [ ] **P2.3** 📦 Git 提交：`[Docs] Docs: document bool conversion mode backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/angelscript/source/as_compiler.cpp` | 修改 | 8 处 `boolConversionMode` 分支插入 |
| `AngelscriptTest/Native/AngelscriptBoolConversionTests.cpp` | 新增 | Mode 0/1 验证测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. `asEP_BOOL_CONVERSION_MODE = 0`（默认）时行为与当前完全一致，所有现有测试通过
2. `asEP_BOOL_CONVERSION_MODE = 1` 时，`if`/`while`/`for`/`do-while`/三元/`!`/`&&`/`||` 位置均可通过 `opConv` 转 bool
3. Mode 1 下仅含 `opConv`（无 `opImplConv`）的类型在条件位置可编译通过
4. Mode 0 下仅含 `opConv` 的类型在条件位置编译失败
5. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **UE fork 中 `CompileCondition` 等包装函数**：当前 ThirdParty 的条件编译路径可能与 2.38 不同（如使用了 `CompileCondition` 中间函数），需定位实际的 `ImplicitConversion` 调用点而非机械对照 2.38 行号
2. **`FStopCondition` 等 UE 特有结构**：当前编译器可能有 UE 特有的条件评估逻辑，分支插入点需避开这些区域
3. **Mode 0 默认不变**：引擎默认值已是 0，此回移不改变默认行为，仅为需要宽松转换的用户提供选项
