# AS 2.38 成员初始化模式回移计划

## 背景与目标

AngelScript 2.38 新增了 `asEP_MEMBER_INIT_MODE` 引擎属性的**实际生效逻辑**。该属性控制脚本类构造函数中成员初始化的行为：

- **Mode 0**（旧行为）：声明处的默认初始化表达式始终执行，即使构造函数体内对同一成员进行了显式初始化
- **Mode 1**（默认/新行为）：若构造函数体内对某成员进行了显式初始化，则**跳过**该成员的声明处默认初始化，与 C++ 的成员初始化语义更一致

当前 ThirdParty 中 `asEP_MEMBER_INIT_MODE` 已注册且默认为 1，但 `as_compiler.cpp` 中 `CompileMemberInitialization` **不读取 `memberInitMode`**，也没有 `m_initializedProperties` 跟踪机制——这又是一个死属性。

**目标**：让 `asEP_MEMBER_INIT_MODE` 实际生效，使得：

1. Mode 1 下，构造函数体内的显式初始化覆盖声明处默认值（消除冗余初始化）
2. Mode 0 保持旧行为不变
3. 与 UE 特有的 `FinConstruct` 等初始化路径无冲突

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| `asEP_MEMBER_INIT_MODE` Set/Get | 存在 | 存在 |
| `ep.memberInitMode` 默认值 | 1 | 1 |
| `CompileMemberInitialization` 中读取 `memberInitMode` | 无 | 约 650-752 行，mode 1 跳过已初始化成员 |
| `m_initializedProperties` | 不存在 | `as_compiler.h` 中声明，构造函数体内赋值时填充 |
| `AS_NO_MEMBER_INIT` 条件编译 | 不存在 | `as_config.h` + `as_compiler.cpp` + `as_scriptobject.cpp` |

## 分阶段执行计划

### Phase 1 — 编译器跟踪机制

> 目标：在 `as_compiler.h/cpp` 中引入 `m_initializedProperties` 集合，在构造函数体编译过程中跟踪哪些成员被显式初始化，并在 `CompileMemberInitialization` 中按 `memberInitMode` 决定是否跳过这些成员的声明处初始化。

- [ ] **P1.1** 在 `as_compiler.h` 中添加 `m_initializedProperties` 成员
  - 类型为 `asCArray<int>` 或等效集合，存储在构造函数体内被显式赋值的成员属性索引
  - 此集合在每个构造函数编译开始时清空
  - 参考 2.38 `as_compiler.h` 中的对应声明
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add m_initializedProperties tracking in compiler`

- [ ] **P1.2** 在构造函数体编译中记录显式初始化的成员
  - 在 `as_compiler.cpp` 中，当编译构造函数体内的赋值语句时，若赋值目标是 `this` 的成员属性，将其索引加入 `m_initializedProperties`
  - 需要在赋值表达式编译路径中添加检测逻辑，识别 `this.member = expr` 模式
  - 参考 2.38 `as_compiler.cpp:9512+` 附近的赋值检测逻辑
  - 注意仅在构造函数上下文中启用此检测，普通函数不受影响
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: detect member assignments in constructor body`

- [ ] **P1.3** 在 `CompileMemberInitialization` 中添加 mode 0/1 分支
  - 当 `engine->ep.memberInitMode == 1` 时，遍历待初始化成员列表，跳过已在 `m_initializedProperties` 中记录的成员
  - 当 `engine->ep.memberInitMode == 0` 时，保持当前行为（所有声明处初始化表达式都执行）
  - 当前 ThirdParty 的 `CompileMemberInitialization`（约 832 行起）使用两阶段 `onlyDefaults` true/false 调用，且含 UE 特有的 `FinConstruct` 路径，需确保新分支与这些路径兼容
  - 参考 2.38 `as_compiler.cpp:650-752` 的分支结构
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement memberInitMode 0/1 branch in CompileMemberInitialization`

### Phase 2 — 测试与文档

> 目标：编写测试验证 Mode 0 不退化、Mode 1 跳过已初始化成员行为正确，更新变更追踪文档。

- [ ] **P2.1** 编写 member init mode 测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptMemberInitModeTests.cpp`，遵循 Native Core 层规则
  - 使用 `CreateNativeEngine()` 创建独立引擎，分别在 Mode 0 和 Mode 1 下执行相同脚本
  - 测试用例清单：
    - **Mode1_ConstructorOverridesDefault**：声明 `int x = 10`，构造函数体 `x = 20`，Mode 1 下断言 x == 20 且声明处初始化未被执行（通过副作用或调用计数验证）
    - **Mode1_UnassignedMemberKeepsDefault**：声明 `int x = 10`，构造函数体不赋值 x，Mode 1 下断言 x == 10
    - **Mode0_BothInitAndAssignExecute**：声明 `int x = 10`，构造函数体 `x = 20`，Mode 0 下两次初始化都执行（最终值 20，但声明处初始化也执行过）
    - **Mode1_MultipleMembers**：多个成员，部分在构造函数体中赋值，部分不赋值，验证各自行为正确
    - **Mode0_UnchangedBehavior**：验证 Mode 0 下现有测试行为不变
  - 参考 2.38 `sdk/tests/test_feature/source/test_scriptstruct.cpp:1719-1742`
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add member init mode verification tests`

- [ ] **P2.2** 全量回归验证
  - 运行全量测试套件，确认默认 Mode 1 下现有测试无回归（当前 ThirdParty 默认也是 1，只是未生效）
  - 若发现回归，说明现有脚本依赖"重复初始化"行为，需评估是否调整默认值或修复脚本
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Test: verify member init mode port with full regression`

- [ ] **P2.3** 更新 `AngelscriptChange.md`
  - 登记 `as_compiler.h` 和 `as_compiler.cpp` 中 `[UE++]` 修改位置和原因
- [ ] **P2.3** 📦 Git 提交：`[Docs] Docs: document member init mode backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/angelscript/source/as_compiler.h` | 修改 | 添加 `m_initializedProperties` 成员 |
| `ThirdParty/angelscript/source/as_compiler.cpp` | 修改 | 构造函数体赋值检测 + `CompileMemberInitialization` mode 分支 |
| `AngelscriptTest/Native/AngelscriptMemberInitModeTests.cpp` | 新增 | Mode 0/1 验证测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. `asEP_MEMBER_INIT_MODE = 0` 时行为与当前完全一致
2. `asEP_MEMBER_INIT_MODE = 1` 时，构造函数体内显式初始化的成员跳过声明处默认初始化
3. Mode 1 下未在构造函数体中赋值的成员仍执行声明处初始化
4. UE 特有的 `FinConstruct` 路径不受影响
5. 默认值（Mode 1）下所有现有测试通过
6. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **`FinConstruct` 交互**：当前 ThirdParty 在 `CompileMemberInitialization` 中有 UE 特有的 `FinConstruct` 调用，这是一种"在所有成员初始化之后执行的回调"，新分支必须在 `FinConstruct` 之前完成跳过逻辑
2. **两阶段 `onlyDefaults` 调用**：当前编译器分两阶段调用 `CompileMemberInitialization`（先默认值、后非默认值），跳过逻辑需在两个阶段中都正确工作
3. **默认值已是 1**：当前引擎属性默认值已是 1 但未生效；启用后如果有脚本依赖"重复初始化"的副作用，可能出现行为变化
4. **检测精度**：构造函数体内 `this.member = expr` 的检测需要准确，不能误将局部变量赋值当作成员初始化
