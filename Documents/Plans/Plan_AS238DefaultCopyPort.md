# AS 2.38 默认拷贝语义回移计划

## 背景与目标

AngelScript 2.37 为脚本类引入了**默认拷贝语义**：自动生成 `opAssign`（赋值操作符）和拷贝构造函数，并支持 `= delete` 禁止自动拷贝。2.38 在此基础上修复了多个边界问题。

**核心能力**：
- 无用户自定义 `opAssign` 时，引擎自动为脚本类生成按成员赋值的默认 `opAssign`
- 无用户自定义单参构造函数时，引擎自动生成拷贝构造函数
- 用户可通过 `void opAssign(const T&in) = delete` 显式禁止自动拷贝
- 引擎属性 `asEP_ALWAYS_IMPL_DEFAULT_COPY` / `asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT` 控制生成策略（0=按语言规则，1=总是生成，2=从不生成）
- 后置校验：若基类/成员不可拷贝，自动移除不可行的默认拷贝构造

当前 ThirdParty 的 `as_builder.cpp` 无 `AddDefaultCopyConstructor`、无 `= delete` 支持、无 `alwaysImplDefaultCopy*` 引擎属性读取。`opAssign` → `beh.copy` 的基础注册路径存在但为旧规则。

**目标**：将 2.37/2.38 的默认拷贝语义完整回移，使得：

1. 脚本类自动获得默认赋值和拷贝构造（除非有用户自定义或 `= delete`）
2. `= delete` 语法可用于禁止拷贝
3. 引擎属性控制生成策略
4. 不可拷贝的类自动移除默认拷贝构造

## 当前事实状态

| 项目 | 2.33（当前 ThirdParty） | 2.38（参考） |
|------|----------------------|-------------|
| 默认 `opAssign` 自动生成 | 旧规则（有用户构造时可能移除） | 新规则（`AddDefaultCopy` + 后置校验） |
| 默认拷贝构造 | 不自动生成 | `AddDefaultCopyConstructor` 自动生成 |
| `= delete` 方法修饰符 | 不支持 | `ParseMethodAttributes` 中识别 `delete` |
| `asTRAIT_DELETED` 标志 | 不存在 | `as_scriptfunction.h` |
| `alwaysImplDefaultCopy*` 引擎属性 | API 已注册但 builder 不读取 | builder 中读取并应用 |
| `CompileDefaultCopyConstructor` | 不存在 | `as_compiler.cpp` |
| `CompileMemberInitializationCopy` | 不存在 | `as_compiler.cpp`（拷贝构造专用） |

## 分阶段执行计划

### Phase 1 — Parser 层：`= delete` 语法

> 目标：在脚本中支持 `void opAssign(const T&in) = delete` 语法，parser 正确识别并在 AST 中标记。不改变运行时行为。

- [ ] **P1.1** 在 `as_tokendef.h` 中确认 `delete` 标识符可用
  - 2.38 中 `delete` 不是通过新增 token 实现，而是在 `ParseMethodAttributes` 中作为上下文关键字识别（与 `override`/`final` 同级）
  - 确认当前 ThirdParty 的 tokenizer 不会将 `delete` 作为保留字拦截（它应被视为普通标识符）
  - 若当前已有 `ttDelete` 令牌用于其他用途（如 `delete obj` 语法），需评估冲突
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: verify delete identifier availability for method attribute`

- [ ] **P1.2** 在 `as_parser.cpp` 的 `ParseMethodAttributes` 中识别 `= delete`
  - 在方法声明尾部（参数列表和 `const` 之后），若遇到 `=` + `delete` 标识符，设置 `isDeleted` 标志
  - 参考 2.38 `as_parser.cpp:1305-1324` 和 `3163-3184` 的 `delete` 识别逻辑
  - 此修改仅影响脚本类方法声明的解析，不影响 C++ 注册路径
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: parse = delete method attribute`

- [ ] **P1.3** 在 `as_scriptfunction.h` 中添加 `asTRAIT_DELETED` 标志
  - 添加到函数 trait 枚举中，用于标记被 `= delete` 的方法
  - 参考 2.38 `as_scriptfunction.h` 中的定义
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: add asTRAIT_DELETED flag to script function`

### Phase 2 — Builder 层：默认拷贝生成与删除

> 目标：在 `as_builder.cpp` 中实现默认 `opAssign` 和默认拷贝构造函数的自动生成逻辑，以及 `= delete` 标记的处理和后置校验。

- [ ] **P2.1** 在 `as_builder.cpp` 中实现 `AddDefaultCopyConstructor`
  - 在类方法注册阶段之后，对非 shared 类检查：若无用户定义的单参拷贝构造函数（且未被 `= delete`），自动注入一个默认拷贝构造
  - 考虑 `asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT` 引擎属性（0=按规则，1=总是，2=从不）
  - 参考 2.38 `as_builder.cpp:756-815`
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement AddDefaultCopyConstructor in builder`

- [ ] **P2.2** 实现 `= delete` 对默认拷贝的禁止
  - 在 builder 中，若脚本声明了 `void opAssign(const T&in) = delete`，设置 `isDefaultCopyDeleted` 标志
  - 同理处理默认构造和默认拷贝构造的 `= delete`
  - 限制 `= delete` 仅可用于自动提供的三种函数（默认构造、默认拷贝、默认赋值）
  - 参考 2.38 `as_builder.cpp:5340-5364` 的 `asTRAIT_DELETED` 处理
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement = delete for default copy functions`

- [ ] **P2.3** 实现默认拷贝的后置校验
  - 在自动生成的拷贝构造函数编译之后，若基类或成员不可拷贝（基类无 `opAssign`、成员类型无拷贝构造等），自动移除该拷贝构造
  - 这避免生成无法正确执行的默认拷贝
  - 参考 2.38 `as_builder.cpp:3898+` 的后置校验逻辑
- [ ] **P2.3** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement post-validation for default copy constructors`

- [ ] **P2.4** 接通 `asEP_ALWAYS_IMPL_DEFAULT_COPY*` 引擎属性
  - 当前这些属性已在引擎中注册（Set/Get 可用），但 builder 不读取
  - 在 `AddDefaultCopy`/`AddDefaultCopyConstructor` 的生成逻辑中读取对应属性，按 0/1/2 策略决定是否生成
  - 参考 2.38 `as_scriptengine.cpp:469-478` 和 `as_builder.cpp` 中的属性读取
- [ ] **P2.4** 📦 Git 提交：`[ThirdParty/AS238] Feat: wire up alwaysImplDefaultCopy engine properties in builder`

### Phase 3 — Compiler 层：默认拷贝构造编译

> 目标：在 `as_compiler.cpp` 中实现 `CompileDefaultCopyConstructor` 和 `CompileMemberInitializationCopy`，为自动生成的拷贝构造函数产生正确的字节码。

- [ ] **P3.1** 实现 `CompileDefaultCopyConstructor`
  - 此函数为自动生成的拷贝构造函数编译字节码
  - 逻辑：按成员声明顺序，对每个可拷贝成员生成赋值/拷贝构造调用
  - 需处理值类型成员、引用类型成员、句柄成员的不同拷贝策略
  - 参考 2.38 `as_compiler.cpp` 中 `CompileDefaultCopyConstructor` 实现
- [ ] **P3.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement CompileDefaultCopyConstructor`

- [ ] **P3.2** 实现 `CompileMemberInitializationCopy`
  - 与 `CompileMemberInitialization` 类似，但专用于拷贝构造场景
  - 从源对象中读取成员值，而非使用声明处默认初始化表达式
  - 参考 2.38 `as_compiler.cpp` 中的对应实现
- [ ] **P3.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: implement CompileMemberInitializationCopy`

### Phase 4 — 测试与文档

> 目标：编写测试覆盖默认拷贝生成、`= delete`、后置校验、引擎属性控制等场景。

- [ ] **P4.1** 编写默认拷贝语义测试
  - 在 `AngelscriptTest/Native/` 下创建 `AngelscriptDefaultCopyTests.cpp`，遵循 Native Core 层规则
  - 测试用例清单：
    - **DefaultOpAssignGenerated**：脚本类无自定义 `opAssign`，创建两个实例后赋值 `a = b`，断言成员值正确拷贝
    - **DefaultCopyConstructGenerated**：脚本类无自定义拷贝构造，通过 `T copy(original)` 创建拷贝，断言成员值正确
    - **UserDefinedOpAssignPreserved**：脚本类有自定义 `opAssign`，断言使用用户版本而非默认版本
    - **DeletedOpAssignFails**：`void opAssign(const T&in) = delete` 后 `a = b` 编译失败
    - **DeletedCopyConstructFails**：拷贝构造被 delete 后 `T copy(original)` 编译失败
    - **DeleteOnlyAllowedForDefaults**：对非默认方法使用 `= delete` 编译失败
    - **UncopyableBaseRemovesCopy**：基类不可拷贝时，派生类的默认拷贝构造自动移除
    - **UncopyableMemberRemovesCopy**：成员类型不可拷贝时，包含该成员的类的默认拷贝构造自动移除
    - **AlwaysImplDefaultCopy_Mode1**：`asEP_ALWAYS_IMPL_DEFAULT_COPY = 1` 时，即使有用户构造也生成默认拷贝
    - **AlwaysImplDefaultCopy_Mode2**：`asEP_ALWAYS_IMPL_DEFAULT_COPY = 2` 时，不生成默认拷贝
  - 参考 2.38 `sdk/tests/test_feature/source/test_scriptstruct.cpp` 中相关用例
- [ ] **P4.1** 📦 Git 提交：`[ThirdParty/AS238] Test: add default copy semantics verification tests`

- [ ] **P4.2** 全量回归验证
  - 运行全量测试套件，确认默认生成的拷贝不破坏现有脚本行为
  - 重点关注：现有脚本中赋值操作是否因新增默认 `opAssign` 而改变语义
- [ ] **P4.2** 📦 Git 提交：`[ThirdParty/AS238] Test: verify default copy port with full regression`

- [ ] **P4.3** 更新 `AngelscriptChange.md`
  - 登记所有 `[UE++]` 修改位置和原因
- [ ] **P4.3** 📦 Git 提交：`[Docs] Docs: document default copy semantics backport changes`

## 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `ThirdParty/.../as_tokendef.h` | 审查 | 确认 `delete` 标识符可用性 |
| `ThirdParty/.../as_parser.cpp` | 修改 | `ParseMethodAttributes` 中 `= delete` 识别 |
| `ThirdParty/.../as_scriptfunction.h` | 修改 | `asTRAIT_DELETED` 标志 |
| `ThirdParty/.../as_builder.cpp` | 修改 | `AddDefaultCopyConstructor`、`= delete` 处理、后置校验、引擎属性 |
| `ThirdParty/.../as_builder.h` | 修改 | 新方法声明、`sClassDeclaration` 标志位 |
| `ThirdParty/.../as_compiler.cpp` | 修改 | `CompileDefaultCopyConstructor`、`CompileMemberInitializationCopy` |
| `ThirdParty/.../as_compiler.h` | 修改 | 新方法声明 |
| `ThirdParty/.../as_scriptengine.cpp` | 修改 | 引擎属性与 builder 联动（若需要） |
| `AngelscriptTest/Native/AngelscriptDefaultCopyTests.cpp` | 新增 | 默认拷贝语义测试 |
| `AngelscriptChange.md` | 修改 | 登记变更 |

## 验收标准

1. 无自定义 `opAssign` 的脚本类自动获得按成员赋值
2. 无自定义拷贝构造的脚本类自动获得拷贝构造
3. `= delete` 可用于禁止默认赋值/拷贝构造/默认构造
4. `= delete` 仅可用于自动提供的三种函数
5. 基类/成员不可拷贝时，默认拷贝构造自动移除
6. `asEP_ALWAYS_IMPL_DEFAULT_COPY*` 引擎属性正确控制生成策略
7. 所有现有测试通过
8. 所有第三方修改用 `//[UE++]` 标注并在 `AngelscriptChange.md` 中登记

## 风险与注意事项

1. **语义变化**：引入默认拷贝后，之前不可赋值的脚本类变为可赋值，可能导致意外的浅拷贝行为。若脚本类持有外部资源（如 UObject 引用），默认按成员赋值可能不安全
2. **与 UE 绑定的交互**：UE 侧注册的类型（通过 `RegisterObjectType`）不受此改动影响，仅脚本定义的类受影响
3. **编译器复杂度**：`CompileDefaultCopyConstructor` 需正确处理所有成员类型（值类型、引用、句柄、嵌套脚本类型等），边界情况多
4. **`= delete` 与 `override`/`final` 的优先级**：需确认这三个方法修饰符的解析优先级和互斥规则
5. **后置校验性能**：在大量类的项目中，后置校验需遍历所有类的所有成员，可能影响编译时间（但通常可忽略）
