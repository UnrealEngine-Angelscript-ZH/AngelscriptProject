# AS 2.38 foreach 语法对齐计划

## 背景与目标

`foreach` 是 AngelScript 中对容器迭代的语法糖。当前 ThirdParty（2.33 + UE 补丁）已通过 `//[UE++]` 实现了 `foreach` 支持，但采用**源码级 lowering**策略（将 `foreach` 生成等价的 `for` 字符串再重新解析）；而 2.38 官方实现采用**直接字节码编译**策略（`CompileForEachStatement` 一次性从 AST 生成字节码）。

两种实现存在以下差异：

| 维度 | 当前 UE 实现 | 2.38 官方实现 |
|------|-------------|-------------|
| 令牌名 | `ttForeach`（小写 e） | `ttForEach`（大写 E） |
| 解析器 | `ParseForeach` + `ParseForeachVariable`，range 用 `ParseCondition()` | `ParseForEach`，range 用 `ParseAssignment()` |
| 编译器 | 字符串生成 `for` + `opFor*`，再 `ParseStatementBlock` 二次编译 | `CompileForEachStatement` 直接发字节码 |
| Tokenizer | 静态关键字表，`asEP_FOREACH_SUPPORT` 无法运行时关闭 foreach 关键字 | `InitJumpTable()` 机制，`asEP_FOREACH_SUPPORT=false` 时从关键字表剔除 foreach |
| 多变量 | 支持 `opForKey` | 支持 `opForValue0..N`（多变量解包） |
| 错误信息 | UE 自定义（`TXT_FOREACH_VARIABLE_DECLARATION` 等） | 官方标准（`TXT_s_NOT_A_FOREACH_TYPE` 等） |

**目标**：评估两种实现的利弊，决定对齐策略，然后执行。最终使 foreach 行为健壮、与后续 AS 升级兼容、测试覆盖充分。

## 范围与边界

### 在范围内

- `as_tokenizer.cpp/h` 中 `InitJumpTable()` 机制（`asEP_FOREACH_SUPPORT` 运行时开关）
- `as_tokendef.h` 中令牌名对齐
- `as_parser.cpp/h` 中 foreach 解析逻辑
- `as_compiler.cpp/h` 中 foreach 编译逻辑
- `as_texts.h` 中 foreach 相关错误信息
- `as_scriptengine.cpp` 中 `SetEngineProperty(asEP_FOREACH_SUPPORT)` 的 tokenizer 联动

### 不在范围内

- 容器侧 `opForBegin` / `opForEnd` / `opForNext` / `opForValue` 的 Bind 实现（已在 `Binds/` 中完成）
- 其他 2.38 语言特性

## 分阶段执行计划

### Phase 0 — 评估与决策

> 目标：系统对比两种 foreach 实现，输出决策文档，确定对齐策略。此 Phase 不修改代码。

- [ ] **P0.1** 编写 foreach 实现对比分析
  - 逐文件对比当前 UE 实现与 2.38 官方实现的差异
  - 评估维度：(1) 功能完整性（多变量、`opForKey` vs `opForValueN`）；(2) 错误诊断质量；(3) 与后续 AS 升级的合并成本；(4) 对现有 Bind 的兼容性（`TArray`/`TMap`/`TSet` 已注册了哪些 `opFor*`）；(5) 二次解析的性能与维护成本
  - 输出决策：**方案 A**（对齐 2.38 官方实现，替换 parser+compiler 路径）或 **方案 B**（保留 UE 实现，仅补齐 tokenizer `InitJumpTable` 和错误信息）
  - 将决策结果记录在本文档"决策记录"小节
- [ ] **P0.1** 📦 Git 提交：`[Docs] Docs: foreach implementation comparison and alignment decision`

### Phase 1 — Tokenizer 对齐（两方案共享）

> 目标：无论采用哪种编译策略，`asEP_FOREACH_SUPPORT` 的运行时开关能力和令牌名对齐都是必要的。

- [ ] **P1.1** 在 `as_tokenizer.cpp/h` 中引入 `InitJumpTable()` 机制
  - 当前 tokenizer 在构造函数中静态遍历 `tokenWords` 建表，无法在 `SetEngineProperty` 后动态增减关键字
  - 2.38 的 `InitJumpTable()` 将建表逻辑抽为独立方法，`SetEngineProperty(asEP_FOREACH_SUPPORT, 0/1)` 时调用 `tok.InitJumpTable()` 重建关键字表
  - 参考 2.38 `as_tokenizer.cpp:83-85` 的 `foreach` 条件跳过逻辑
  - 需在引擎的 `SetEngineProperty(asEP_FOREACH_SUPPORT)` 分支中添加 `tok.InitJumpTable()` 调用
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: introduce InitJumpTable for runtime foreach keyword toggle`

- [ ] **P1.2** 统一令牌名 `ttForeach` → `ttForEach`
  - 当前 `as_tokendef.h` 中为 `ttForeach`（小写 e），2.38 为 `ttForEach`（大写 E）
  - 全局搜索替换所有引用（parser、compiler、tokendef、scriptnode 等）
  - 这是纯命名对齐，不改变行为
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Refactor: rename ttForeach to ttForEach for upstream alignment`

### Phase 2 — 编译策略实施（根据 Phase 0 决策）

> 目标：根据 Phase 0 决策结果实施 parser/compiler 层的对齐或修补。

#### 方案 A：对齐 2.38 官方实现

- [ ] **P2A.1** 替换 `as_parser.cpp` 中的 `ParseForEach` 实现
  - 用 2.38 的 `ParseForEach()`（约 4569-4634 行）替换当前 `ParseForeach` + `ParseForeachVariable`
  - range 表达式从 `ParseCondition()` 改为 `ParseAssignment()`
  - 需保持 `snForEach` 节点结构与 2.38 一致
- [ ] **P2A.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: replace ParseForeach with upstream ParseForEach`

- [ ] **P2A.2** 替换 `as_compiler.cpp` 中的 foreach 编译逻辑
  - 用 2.38 的 `CompileForEachStatement`（约 5190-5678 行）替换当前字符串 lowering 逻辑
  - 直接从 AST 生成字节码：解析 `snDataType + 标识符`、编译 range、查找 `opForBegin`/`opForEnd`/`opForNext`/`opForValue*`
  - 若当前 Bind 中注册了 `opForKey`（UE 特有），需评估是否在 `CompileForEachStatement` 中保留对 `opForKey` 的识别或迁移到 `opForValue0`/`opForValue1` 模式
- [ ] **P2A.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: replace foreach string lowering with direct bytecode compilation`

- [ ] **P2A.3** 对齐 `as_texts.h` 中的 foreach 错误信息
  - 替换 UE 自定义的 `TXT_FOREACH_VARIABLE_DECLARATION` 等为 2.38 标准的 `TXT_s_NOT_A_FOREACH_TYPE` 等
  - 保留 UE 特有的错误信息仅在确认无官方对应时
- [ ] **P2A.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: align foreach error messages with upstream`

#### 方案 B：保留 UE 实现，仅补齐

- [ ] **P2B.1** 补齐 `SetEngineProperty` 中 `asEP_FOREACH_SUPPORT` 的 tokenizer 联动
  - Phase 1.1 已引入 `InitJumpTable()`，此步确认 `asEP_FOREACH_SUPPORT = false` 时 `foreach` 可作为普通标识符使用
- [ ] **P2B.1** 📦 Git 提交：`[ThirdParty/AS238] Fix: ensure asEP_FOREACH_SUPPORT runtime toggle works`

- [ ] **P2B.2** 审查并修补现有 UE foreach 实现的已知问题
  - 检查当前字符串 lowering 是否有边界问题（如嵌套 foreach、foreach 内 break/continue、range 表达式含逗号等）
  - 对比 2.38 测试中的 foreach 用例，找出当前实现未覆盖的场景
- [ ] **P2B.2** 📦 Git 提交：`[ThirdParty/AS238] Fix: address known issues in UE foreach implementation`

### Phase 3 — 测试与文档

> 目标：编写测试覆盖 foreach 的各种场景，包括正例、错误路径、容器类型、`asEP_FOREACH_SUPPORT` 开关。

- [ ] **P3.1** 编写 foreach 测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptForeachTests.cpp`，遵循 Native Core 层规则
  - 测试需要先注册一个支持 `opForBegin`/`opForEnd`/`opForNext`/`opForValue` 的测试容器类型
  - 测试用例清单：
    - **BasicIteration**：foreach 遍历容器，断言每个元素按顺序访问
    - **AutoTypeDeduction**：`foreach (auto x : container)` 语法（若支持）
    - **MultiVariable**：多变量解包（key-value 或 value0-value1，取决于方案）
    - **NestedForeach**：嵌套 foreach 循环
    - **BreakInForeach**：foreach 内 break 正确退出
    - **ContinueInForeach**：foreach 内 continue 跳过当前迭代
    - **EmptyContainer**：空容器上 foreach 不执行循环体
    - **ForeachDisabled**：`asEP_FOREACH_SUPPORT = false` 时 `foreach` 为普通标识符，可用作变量名
    - **NonIterableTypeError**：对不支持 `opFor*` 的类型使用 foreach，断言编译失败
  - 参考 2.38 `sdk/tests/test_feature/source/test_foreach.cpp`
- [ ] **P3.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add foreach syntax verification tests`

- [ ] **P3.2** 编写容器绑定 foreach 集成测试
  - 在 `AngelscriptTest/Bindings/` 下创建 `AngelscriptForeachBindingsTests.cpp`，验证 UE 容器绑定的 foreach 支持
  - 测试用例（需要 `FAngelscriptEngine`）：
    - **TArrayForeach**：`foreach (int x : myArray)` 遍历 `TArray<int>`
    - **TMapForeach**：`foreach (auto key, auto value : myMap)` 遍历 `TMap`
    - **TSetForeach**：`foreach (auto x : mySet)` 遍历 `TSet`
  - 参考现有 `AngelscriptContainerBindingsTests.cpp` 的模式
- [ ] **P3.2** 📦 Git 提交：`[ThirdParty/AS238] Test: add foreach container bindings integration tests`

- [ ] **P3.3** 全量回归验证
  - 运行全量测试套件，确认零回归
- [ ] **P3.3** 📦 Git 提交：`[ThirdParty/AS238] Test: verify foreach port with full regression`

- [ ] **P3.4** 更新 `AngelscriptChange.md`
  - 登记所有 `[UE++]` 修改位置和原因
  - 若方案 A，记录被替换的 UE 自定义 foreach 实现
- [ ] **P3.4** 📦 Git 提交：`[Docs] Docs: document foreach alignment changes`

## 决策记录

> 此区域在 Phase 0 完成后填写。

**决策结果**：（待评估后填写）
**选择理由**：（待评估后填写）
**opForKey 处理方案**：（待评估后填写）

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/angelscript/source/as_tokenizer.cpp` | 修改 | `InitJumpTable()` 机制 |
| `ThirdParty/angelscript/source/as_tokenizer.h` | 修改 | `InitJumpTable()` 声明 |
| `ThirdParty/angelscript/source/as_tokendef.h` | 修改 | 令牌名对齐 |
| `ThirdParty/angelscript/source/as_parser.cpp` | 修改 | foreach 解析逻辑 |
| `ThirdParty/angelscript/source/as_parser.h` | 修改 | 方法声明 |
| `ThirdParty/angelscript/source/as_compiler.cpp` | 修改 | foreach 编译逻辑 |
| `ThirdParty/angelscript/source/as_compiler.h` | 修改 | 方法声明 |
| `ThirdParty/angelscript/source/as_texts.h` | 修改 | 错误信息对齐（方案 A） |
| `ThirdParty/angelscript/source/as_scriptengine.cpp` | 修改 | `SetEngineProperty` tokenizer 联动 |
| `AngelscriptTest/Native/AngelscriptForeachTests.cpp` | 新增 | foreach 语法测试 |
| `AngelscriptTest/Bindings/AngelscriptForeachBindingsTests.cpp` | 新增 | 容器 foreach 集成测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. `asEP_FOREACH_SUPPORT = true`（默认）时 foreach 语法正常工作
2. `asEP_FOREACH_SUPPORT = false` 时 `foreach` 可作为普通标识符使用
3. foreach 对 UE 容器绑定（TArray/TMap/TSet）正确工作
4. 嵌套 foreach、break、continue 行为正确
5. 对不支持 `opFor*` 的类型使用 foreach 时编译器给出明确错误
6. 所有现有测试通过
7. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **opForKey 兼容性**：当前 UE 实现支持 `opForKey`（用于 TMap 的 key 迭代），2.38 官方不使用此操作符而是用 `opForValue0`/`opForValue1` 多变量模式。若选方案 A，需确认 TMap 绑定是否已注册 `opForValue0`/`opForValue1`，或需要同步调整 Bind 代码
2. **字符串 lowering 的隐式依赖**：当前实现通过二次解析依赖了完整的 parser/compiler 路径，其错误报告可能与直接编译方式不同，测试需覆盖错误路径
3. **`ParseAssignment` vs `ParseCondition`**：2.38 用 `ParseAssignment` 作为 range 表达式解析，允许范围更广的表达式作为 range；当前用 `ParseCondition` 范围更窄，替换后可能接受之前拒绝的语法
