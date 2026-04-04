# AS 2.38 using namespace 支持回移计划

## 背景与目标

AngelScript 2.35 引入了 `using namespace` 语法，允许脚本在命名空间或块作用域中引入其他命名空间的可见性，简化对嵌套命名空间符号的引用。

语法：`using namespace A::B;`

效果：在当前作用域内，符号查找自动搜索 `A::B` 命名空间，无需每次写全限定名。

当前 ThirdParty（2.33 + UE 补丁）对此特性**零支持**：无 `ttUsing` 令牌、无 `snUsing` AST 节点、无命名空间可见性机制。这是一个**完整的新语言特性引入**，影响从词法分析到编译期符号查找的全链路。

**目标**：将 2.38 的 `using namespace` 完整回移，使得：

1. 脚本中可在顶层和块作用域内使用 `using namespace X;`
2. 符号查找优先搜索 `using` 引入的命名空间，再向上搜索父命名空间
3. 命名空间歧义时编译器给出明确错误
4. 不影响现有无 `using` 的脚本行为

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| `ttUsing` 令牌 | 不存在 | `as_tokendef.h` |
| `snUsing` AST 节点 | 不存在 | `as_scriptnode.h` |
| `ParseUsing()` | 不存在 | `as_parser.cpp:2688-2721`（顶层 + 块入口） |
| `namespaceVisibility` | 不存在 | `as_builder.h`：`asCMap<asSNameSpace*, asCArray<asSNameSpace*>>` |
| `RegisterUsingNamespace` | 不存在 | `as_builder.cpp:2258-2287` |
| `AddVisibleNamespaces` / `FindNextVisibleNamespace` | 不存在 | `as_builder.cpp:912-954`（声明期查找） |
| `m_namespaceVisibility`（编译期/块作用域） | 不存在 | `as_compiler.h:409`（函数/块内 `using`） |
| `SymbolLookup` 中 `using` 支持 | 不存在 | `as_compiler.cpp:10982-10988` |

## 分阶段执行计划

### Phase 1 — 词法与 AST

> 目标：在词法分析器和 AST 节点系统中注册 `using` 相关的基础设施，不改变任何解析或编译行为。

- [ ] **P1.1** 在 `as_tokendef.h` 中添加 `ttUsing` 令牌
  - 添加 `ttUsing` 到令牌枚举中，并注册 `asTokenDef("using", ttUsing)`
  - 这使 tokenizer 能识别 `using` 关键字
  - 需确认当前 ThirdParty 中 `using` 不被其他令牌覆盖
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add ttUsing token definition`

- [ ] **P1.2** 在 `as_scriptnode.h` 中添加 `snUsing` 节点类型
  - 添加到脚本节点枚举中
  - 此节点用于表示 `using namespace X::Y;` 声明
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: add snUsing AST node type`

### Phase 2 — Parser 层

> 目标：parser 能正确解析 `using namespace` 语法，生成对应的 AST 节点。

- [ ] **P2.1** 实现 `ParseUsing()` 方法
  - BNF：`USING ::= 'using' 'namespace' IDENTIFIER ('::' IDENTIFIER)* ';'`
  - 消费 `using`、`namespace` 关键字，解析一串 `::` 分隔的标识符，以 `;` 结束
  - 创建 `snUsing` 节点，子节点为标识符序列
  - 参考 2.38 `as_parser.cpp:2688-2721`
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement ParseUsing for using namespace syntax`

- [ ] **P2.2** 在顶层脚本块中插入 `using` 入口
  - 在 `ParseScript` 或等效的顶层解析循环中，遇到 `ttUsing` 时调用 `ParseUsing()`
  - 与 `namespace` 声明处于同级入口（约 2639-2642 行附近）
  - 参考 2.38 `as_parser.cpp` 中顶层 `using` 的入口位置
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: add using namespace entry in top-level script parsing`

- [ ] **P2.3** 在语句块中插入 `using` 入口
  - 在 `ParseStatementBlock` 或等效函数内，遇到 `ttUsing` 时调用 `ParseUsing()`
  - 这允许在函数/块内使用 `using namespace`（块作用域）
  - 参考 2.38 `as_parser.cpp:3957-3960`
- [ ] **P2.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: add using namespace entry in statement blocks`

### Phase 3 — Builder 层：命名空间可见性注册

> 目标：在声明注册阶段（builder）建立命名空间可见性映射，使得后续符号查找能看到 `using` 引入的命名空间。

- [ ] **P3.1** 在 `as_builder.h` 中添加 `namespaceVisibility` 数据结构
  - 类型：`asCMap<asSNameSpace*, asCArray<asSNameSpace*>>`
  - 语义：给定当前命名空间 `ns`，`namespaceVisibility[ns]` 返回该命名空间中通过 `using` 引入的所有可见命名空间列表
- [ ] **P3.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add namespaceVisibility map in builder`

- [ ] **P3.2** 实现 `RegisterNamespaceVisibility` 和 `RegisterUsingNamespace`
  - `RegisterNamespaceVisibility`：遍历 AST 子节点，对 `snNamespace` 递归、对 `snUsing` 调用 `RegisterUsingNamespace`
  - `RegisterUsingNamespace`：解析 `using namespace A::B::C` 中的标识符序列为 `asSNameSpace*`，挂到当前命名空间的 visibility 列表（去重）
  - 参考 2.38 `as_builder.cpp:883-909`（`RegisterNamespaceVisibility`）和 `2258-2287`（`RegisterUsingNamespace`）
- [ ] **P3.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement namespace visibility registration`

- [ ] **P3.3** 实现 `AddVisibleNamespaces` 和 `FindNextVisibleNamespace`
  - 这两个方法在符号查找时使用：先将 `using` 引入的命名空间加入搜索队列，再向上搜索父命名空间
  - `AddVisibleNamespaces`：将 `namespaceVisibility[ns]` 中的命名空间加入搜索数组
  - `FindNextVisibleNamespace`：从搜索数组中取出下一个待搜索的命名空间
  - 参考 2.38 `as_builder.cpp:912-954`
- [ ] **P3.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement AddVisibleNamespaces and FindNextVisibleNamespace`

### Phase 4 — Compiler 层：块作用域 using 与符号查找

> 目标：在编译器中支持块作用域的 `using namespace`，并修改 `SymbolLookup` 以在符号解析时搜索 `using` 引入的命名空间。

- [ ] **P4.1** 在 `as_compiler.h` 中添加 `m_namespaceVisibility` 成员
  - 类型：`asCArray<asSNameSpace*>`
  - 语义：函数/块内 `using namespace` 引入的命名空间列表（词法作用域）
  - 需在进入/退出块作用域时维护此列表的长度（push/pop 语义）
- [ ] **P4.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: add m_namespaceVisibility in compiler`

- [ ] **P4.2** 在语句列表编译中处理 `snUsing` 节点
  - 当编译器遇到 `snUsing` 节点时，解析出目标命名空间，校验其存在，然后追加到 `m_namespaceVisibility`
  - 维护 `visibleNamespaceCount` 以支持块退出时恢复
  - 参考 2.38 `as_compiler.cpp:1448-1466`
- [ ] **P4.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: handle snUsing nodes in compiler statement list`

- [ ] **P4.3** 修改 `SymbolLookup` 以搜索 `using` 引入的命名空间
  - 在现有的命名空间逐级向上搜索逻辑中，插入对 `namespaceVisibility`（builder 层）和 `m_namespaceVisibility`（compiler 块作用域）的查询
  - 先搜 `using` 引入的命名空间，再搜父命名空间
  - 参考 2.38 `as_compiler.cpp:10982-10988`
  - 需特别注意歧义处理：若同一符号在多个 `using` 命名空间中都找到，应报编译错误
- [ ] **P4.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: integrate namespace visibility into SymbolLookup`

### Phase 5 — 测试与文档

> 目标：编写测试覆盖 `using namespace` 的各种场景，包括正例、歧义、块作用域、嵌套等。

- [ ] **P5.1** 编写 using namespace 测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptUsingNamespaceTests.cpp`，遵循 Native Core 层规则
  - 测试用例清单：
    - **BasicUsing**：`namespace A { int x = 42; }` + `using namespace A;` → `int r = x;` 断言 r == 42
    - **NestedNamespace**：`namespace A::B { int x = 10; }` + `using namespace A::B;` → 可直接引用 x
    - **MultipleUsing**：同时 `using namespace A;` 和 `using namespace B;`，各自的符号都可见
    - **AmbiguityError**：A 和 B 中有同名符号，同时 `using` 后引用该符号编译失败
    - **BlockScoped**：函数内 `{ using namespace A; /* A 可见 */ }` 退出块后 A 不可见
    - **UsingInNamespace**：在 `namespace X { using namespace A; }` 中，X 内可见 A 的符号
    - **NonExistentNamespaceError**：`using namespace NonExistent;` 编译失败
    - **WithoutUsing_Unchanged**：不使用 `using namespace` 时，行为与当前完全一致
  - 参考 2.38 `sdk/tests/test_feature/source/test_namespace.cpp:65-163`
- [ ] **P5.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add using namespace verification tests`

- [ ] **P5.2** 全量回归验证
  - 运行全量测试套件，确认零回归
  - 重点关注：`using` 关键字是否与现有脚本中的标识符冲突
- [ ] **P5.2** 📦 Git 提交：`[ThirdParty/AS238] Test: verify using namespace port with full regression`

- [ ] **P5.3** 更新 `AngelscriptChange.md`
  - 登记所有 `[UE++]` 修改位置和原因
- [ ] **P5.3** 📦 Git 提交：`[Docs] Docs: document using namespace backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/.../as_tokendef.h` | 修改 | 添加 `ttUsing` 令牌 |
| `ThirdParty/.../as_scriptnode.h` | 修改 | 添加 `snUsing` 节点类型 |
| `ThirdParty/.../as_parser.cpp` | 修改 | `ParseUsing()` + 顶层/块入口 |
| `ThirdParty/.../as_parser.h` | 修改 | `ParseUsing()` 声明 |
| `ThirdParty/.../as_builder.cpp` | 修改 | `namespaceVisibility`、`RegisterUsingNamespace`、`AddVisibleNamespaces`、`FindNextVisibleNamespace` |
| `ThirdParty/.../as_builder.h` | 修改 | 数据结构与方法声明 |
| `ThirdParty/.../as_compiler.cpp` | 修改 | `m_namespaceVisibility`、`snUsing` 处理、`SymbolLookup` 修改 |
| `ThirdParty/.../as_compiler.h` | 修改 | `m_namespaceVisibility` 成员 |
| `AngelscriptTest/Native/AngelscriptUsingNamespaceTests.cpp` | 新增 | using namespace 测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. `using namespace A::B;` 语法在顶层和块作用域中正确工作
2. `using` 引入的命名空间中的符号在当前作用域可直接引用
3. 块作用域退出后 `using` 效果消失
4. 多个 `using` 导致歧义时编译器给出明确错误
5. 引用不存在的命名空间时编译器给出明确错误
6. 不使用 `using namespace` 的现有脚本行为不变
7. 所有现有测试通过
8. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **`using` 关键字冲突**：引入 `ttUsing` 后，现有脚本中以 `using` 为变量名/函数名的代码会编译失败。需评估现有脚本中是否使用了 `using` 作为标识符
2. **符号查找性能**：`SymbolLookup` 是编译期高频路径，额外的 `using` 命名空间搜索可能增加编译时间。在大量 `using` 场景下需关注
3. **`SymbolLookup` 是大函数**：2.38 的 `SymbolLookup` 非常大（10000+ 行附近），与当前 ThirdParty 可能有大量上下文差异，需仔细手工适配而非直接 copy
4. **与 UE 命名空间的交互**：UE 绑定中注册的命名空间（如 `System`、`UE` 等）应能通过 `using namespace` 引入，需验证
5. **递归 using**：`namespace A { using namespace B; } namespace C { using namespace A; }` 的传递可见性需与 2.38 行为一致
