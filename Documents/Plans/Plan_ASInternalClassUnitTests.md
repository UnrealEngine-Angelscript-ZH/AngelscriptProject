# AS 内部类单元测试扩展计划

## 背景与目标

### 背景

当前 `Plugins/Angelscript/Source/AngelscriptTest/Internals/` 已经建立起 `Tokenizer`、`Parser`、`ScriptNode`、`Memory`、`Bytecode`、`Builder`、`Compiler`、`DataType`、`GC`、`Restore` 等内部测试骨架，但覆盖深度仍然明显偏向“可创建 / 可编译 / 可往返 / 有基本结果”的冒烟级验证：

- `AngelscriptCompilerTests.cpp` 主要验证字节码已生成、局部变量数不为零、函数能执行、基础类型转换成立，还没有把 `asCScriptFunction` 的形参/局部变量/调试信息元数据单独固化下来
- `AngelscriptBuilderTests.cpp` 已覆盖编译错误、模块重建、import 绑定，但没有把 `asCModule` 的 namespace、全局变量索引、导入解绑、类型枚举这些内部容器行为拆成细粒度断言
- `AngelscriptGCInternalTests.cpp` 对 `asCGarbageCollector` 的环检测和手动回收已经较完整，说明 `Internals/` 层适合直接 include `source/as_*.h` 来验证内部类状态
- `Native/` 测试层已经明确只允许公共 API，不允许把 `FAngelscriptEngine` 或 `source/as_*.h` 带进去；因此“AS 内部类”增强测试必须继续落在 `Internals/`，不能误放到 `Native/`

与此同时，当前仓库还缺少对以下核心 internal class 的专门单元测试文件：

- `asCContext`：状态迁移、异常快照、用户数据、嵌套调用栈信息
- `asCScriptFunction`：声明串、参数元数据、局部变量声明、脚本 section/debug line、字节码可见性
- `asCObjectType`：继承关系、属性/方法枚举、继承属性标记、factory / behaviour 元数据
- `asCModule`：默认 namespace、全局变量查找、导入函数解绑/重绑、类型与函数枚举的一致性

如果继续只在现有大而泛的文件里追加零散断言，后续很难把失败快速定位到“context / function / object type / module”哪一个 internal class 回归。因此需要先写一份明确的扩测计划，把新增文件、测试主题、验证命令和文档同步点一次性定清。

### 目标

1. 在 `Plugins/Angelscript/Source/AngelscriptTest/Internals/` 下为 `asCContext`、`asCScriptFunction`、`asCObjectType`、`asCModule` 建立更细粒度的单元测试覆盖。
2. 保持测试分层清晰：内部类测试只放 `Internals/`，继续通过 `StartAngelscriptHeaders.h` / `EndAngelscriptHeaders.h` + `source/as_*.h` 访问 internal class，不污染 `Native/` 公共 API 测试层。
3. 把“当前已有覆盖”和“本次新增覆盖”同步写进 `Documents/Guides/TestCatalog.md`，让后续执行者无需再手工梳理目录结构。
4. 将执行粒度固定为小步提交：每一类 internal class 的测试文件与对应的目录文档更新都可以独立实现、独立验证。

## 范围与边界

- **纳入范围**
  - `Plugins/Angelscript/Source/AngelscriptTest/Internals/` 下新增或扩展 internal class 单元测试
  - 以 `asCContext`、`asCScriptFunction`、`asCObjectType`、`asCModule` 为第一批目标类
  - 为新增测试补充必要的 helper（仅限 `Internals/` 层可复用的小型局部 helper，不引入新的跨层 shared abstraction）
  - `Documents/Guides/TestCatalog.md` 的条目更新
  - 必要时补充 `Documents/Guides/Test.md` 中关于 `Internals/` 层的说明，但只限本计划直接涉及的边界澄清
- **不纳入范围**
  - `Native/` 测试层的公共 API 增补
  - `FAngelscriptEngine`、`ClassGenerator`、`Bindings`、`HotReload`、`Actor/Blueprint/Interface` 等非 internal class 主题化集成测试扩张
  - 修改 `ThirdParty/angelscript/source/` 的运行时行为；本计划以“测试先行、暴露回归”为主，不默认夹带实现改动
  - 大规模重构现有 `Internals/*.cpp` 文件命名或目录布局

## 当前事实状态快照

### 测试层级与边界

- `Plugins/Angelscript/AGENTS.md` 已明确：`Source/AngelscriptTest/Native/` 只能使用 `AngelscriptInclude.h` / `angelscript.h` 公共 API，不能引入 `FAngelscriptEngine` 或 `source/as_*.h`
- 因此 internal class 测试的合法落点只有 `Plugins/Angelscript/Source/AngelscriptTest/Internals/`
- 现有 `Internals/` 文件总数为 11 个：
  - `AngelscriptBuilderTests.cpp`
  - `AngelscriptRestoreTests.cpp`
  - `AngelscriptCompilerTests.cpp`
  - `AngelscriptDataTypeTests.cpp`
  - `AngelscriptGCInternalTests.cpp`
  - `AngelscriptParserTests.cpp`
  - `AngelscriptScriptNodeTests.cpp`
  - `AngelscriptBytecodeTests.cpp`
  - `AngelscriptStructCppOpsTests.cpp`
  - `AngelscriptMemoryTests.cpp`
  - `AngelscriptTokenizerTests.cpp`

### 当前已有 internal class 覆盖

| internal class / 面向 | 当前测试文件 | 已覆盖点 | 当前缺口 |
| --- | --- | --- | --- |
| `asCTokenizer` | `Internals/AngelscriptTokenizerTests.cpp` | token 基础类型、关键字、注释/字符串、错误 token | 数字字面量边界、复合操作符、多字符 token 连续流 |
| `asCParser` + `asCScriptNode` | `Internals/AngelscriptParserTests.cpp` / `Internals/AngelscriptScriptNodeTests.cpp` | declaration、expression、control-flow、copy/traversal | 更细的 AST 结构与 node 链接关系、错误节点上下文 |
| `asCMemoryMgr` | `Internals/AngelscriptMemoryTests.cpp` | pool 复用、`FreeUnusedMemory()` | 多次复用顺序、混合池压力、释放后状态恢复 |
| `asCByteCode` | `Internals/AngelscriptBytecodeTests.cpp` | instruction append、jump resolve、output | 多 label 分支、组合输出长度、Append 后指针稳定性 |
| `asCBuilder` | `Internals/AngelscriptBuilderTests.cpp` | 单模块构建、编译错误、重建、import 绑定 | default namespace、编译后符号表稳定性、解绑路径 |
| `asCCompiler` | `Internals/AngelscriptCompilerTests.cpp` | bytecode generation、var count、函数调用、基础转换 | debug info、默认参数、局部变量声明、更多编译期元数据 |
| `asCDataType` | `Internals/AngelscriptDataTypeTests.cpp` | primitive/comparison/handle/size | const handle / ref 组合、对象子类型、模板子类型 |
| `asCGarbageCollector` | `Internals/AngelscriptGCInternalTests.cpp` | GC 统计、空回收、手动环收集、环检测 | 回收前后 engine 统计一致性、异常路径日志 |
| `asCModule` + `asCRestore` | `Internals/AngelscriptRestoreTests.cpp` | save/load bytecode roundtrip、strip debug info | namespace/global var/import/type enumeration 的内部容器一致性 |

### 第一批优先目标类与依据

1. `asCContext`
   - `source/as_context.h` 公开了 `Prepare` / `Unprepare` / `Execute` / `Suspend` / `PushState` / `PopState` / `GetState` / `GetCallstackSize` / `GetVarCount` / `GetVarName` / `GetExceptionString` / `SetUserData`
   - 当前仓库没有独立的 `Internals` 上下文测试文件，只在较高层执行测试里顺带调用 context
2. `asCScriptFunction`
   - `source/as_scriptfunction.h` 暴露 `GetDeclaration`、`GetParam`、`GetReturnTypeId`、`GetVarCount`、`GetVarDecl`、`FindNextLineWithCode`、`GetByteCode`
   - 现有 `Compiler` 测试只断言 bytecode 非空和局部变量数量，不足以固定函数元数据行为
3. `asCObjectType`
   - `source/as_objecttype.h` 暴露 `GetBaseType`、`DerivesFrom`、`GetMethodByDecl`、`GetProperty`、`IsPropertyInherited`、`GetFactoryCount`、`GetBehaviourByIndex`
   - 当前仓库没有专门覆盖类元数据枚举与继承属性布局的 `Internals` 测试
4. `asCModule`
   - `source/as_module.h` 暴露 `SetDefaultNamespace`、`GetDefaultNamespace`、`GetGlobalVarIndexByName`、`GetGlobalVarIndexByDecl`、`GetImportedFunctionCount`、`UnbindImportedFunction`、`GetObjectTypeCount`
   - 现有 `Builder` / `Restore` 测试用到了 module，但没有把 module 内部索引与解绑行为固化为专门断言

### 既有测试编写模式

- 统一使用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`
- `Internals/` 测试直接 include `source/as_*.h`，并用 `StartAngelscriptHeaders.h` / `EndAngelscriptHeaders.h` 包裹
- 高层脚本编译/执行优先复用 `AngelscriptTestSupport::GetResetSharedTestEngine()`、`BuildModule()`、`GetFunctionByDecl()`、`ExecuteIntFunction()`
- 当需要访问内部容器或 protected 数据时，优先使用局部 `Accessor` / `Probe` 类型，而不是把 helper 抽到 `Shared/`
- 单模块、单函数、纯元数据测试可以使用 `GetResetSharedTestEngine()`；涉及多模块 import/unbind、继承链、或容易泄露状态的 case，优先使用 `CreateCloneTestEngine()` 做隔离，避免共享引擎里的模块名和类型状态串线

## 推荐文件落点

| 目标类 | 推荐测试文件 | 说明 |
| --- | --- | --- |
| `asCContext` | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptContextTests.cpp` | 新文件，专门固化上下文状态机与异常/栈帧可见性 |
| `asCScriptFunction` | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptScriptFunctionTests.cpp` | 新文件，避免继续把函数元数据塞进 `Compiler` 冒烟测试 |
| `asCObjectType` | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptObjectTypeTests.cpp` | 新文件，聚焦继承、方法、属性、behaviour/factory 枚举 |
| `asCModule` | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptModuleTests.cpp` | 新文件，聚焦 namespace、global var、import、type/function enumeration |
| 测试目录总表 | `Documents/Guides/TestCatalog.md` | 为新增测试补目录条目和验证内容 |
| 边界说明（仅按需） | `Documents/Guides/Test.md` | 若执行阶段发现 `Internals/` 约束仍不够清晰，再补一句边界说明 |

首批执行优先级固定为：`asCContext` → `asCScriptFunction` → `asCModule` → `asCObjectType`。

- `Context`、`ScriptFunction`、`Module` 是第一批最直接、最稳定的 internal metadata / state surface，优先用于建立新增测试骨架
- `ObjectType` 放在 wave 1 末尾，只验证纯脚本类型元数据，不顺势扩到 `ClassGenerator`、生成类桥接或高层语言特性集成

## 验证命令基线

- 执行前先读取项目根目录 `AgentConfig.ini`，用其中的 `Paths.EngineRoot` 解析编辑器路径；不要在实现里写死本地绝对路径
目标自动化测试建议通过 `Tools\RunTests.ps1 -TestPrefix <TestName> -Label <TestName> -TimeoutMs 600000 -- -NullRHI` 来统一启动，保持和 `AgentConfig.ini` 中的路径/超时一致，并让脚本把日志/报告写入独立的 `Saved/Tests/<Label>/<RunId>/` 目录。

- 首批建议的验证粒度：
  - `Angelscript.TestModule.Internals.Context`
  - `Angelscript.TestModule.Internals.ScriptFunction`
  - `Angelscript.TestModule.Internals.ObjectType`
  - `Angelscript.TestModule.Internals.Module`
  - 最后再跑 `Angelscript.TestModule.Internals`

## 分阶段执行计划

### Phase 0：执行前约束确认

> 目标：在开始实现前先统一边界、文件落点和隔离规则，但不把这一步膨胀成单独的 docs-first 提交阶段。

- 进入 Phase 1 前先复核三条 preflight 约束：
  - `source/as_*.h` 只允许留在 `Internals/`，不要把 internal-class helper 带进 `Native/` 或 `Shared/`
  - 首批新增文件固定为 `AngelscriptContextTests.cpp`、`AngelscriptScriptFunctionTests.cpp`、`AngelscriptModuleTests.cpp`、`AngelscriptObjectTypeTests.cpp`
  - 多模块 import/unbind 与继承链 case 优先用 clone engine，单模块元数据 case 才复用 shared engine
- 本阶段不单独要求代码或文档提交；它是进入后续实现阶段前必须完成的约束确认清单

### Phase 1：补齐 `asCContext` 单元测试

> 目标：让上下文状态机、异常快照和调用栈可见性有独立回归，不再依赖高层执行测试顺带覆盖。

- [ ] **P1.1** 创建 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptContextTests.cpp`
  - 按现有 `Internals/` 习惯 include `StartAngelscriptHeaders.h` / `source/as_context.h` / `source/as_scriptengine.h` / `source/as_scriptfunction.h` / `EndAngelscriptHeaders.h`
  - 用 `AngelscriptTestSupport::GetResetSharedTestEngine()` + `BuildModule()` 编译至少两类脚本：一类正常执行，一类显式触发异常或非法访问，用来稳定拿到 `Prepare`/`Execute`/`GetState`/`GetExceptionString` 的观测面
  - 不要直接把这批 case 混进 `AngelscriptExecutionTests.cpp`；这里的目标是 internal context，而不是脚本语言功能本身
- [ ] **P1.1** 📦 Git 提交：`[Test/Internals] Feat: add asCContext internal test file`

- [ ] **P1.2** 为 `asCContext` 固化状态迁移与用户数据断言
  - 覆盖“新建 context → Prepare 后进入 Prepared → Execute 完成后进入 Finished → Unprepare 后回到 Uninitialized/可复用态”的基本状态迁移
  - 覆盖 `SetUserData` / `GetUserData` 的 round-trip，确保不同 `type` 槽位不会串写
  - 如果 `PushState` / `PopState` 在当前分支能稳定使用，则增加一条嵌套调用或保存/恢复状态的正例；若执行中发现该接口在当前引擎路径上不稳定，则明确降级为后续条目，不在本阶段硬凑覆盖，也不要把断言写成依赖具体内部栈布局的脆弱快照
- [ ] **P1.2** 📦 Git 提交：`[Test/Internals] Test: cover asCContext state and user-data flow`

- [ ] **P1.3** 为 `asCContext` 固化异常与栈帧元数据断言
  - 构造一个包含嵌套函数调用和局部变量的脚本，执行失败后验证 `GetExceptionString()`、`GetExceptionFunction()`、`GetExceptionLineNumber()`、`GetCallstackSize()`、`GetVarCount()`、`GetVarName()` 至少能给出稳定的非空结果
  - 重点不是比较具体报错文案全文，而是固定“异常函数存在、section/line 合法、至少一个局部变量可见”这种对回归最稳的边界
  - 若当前实现支持 `GetAddressOfVar()` / `IsVarInScope()` 的稳定观测，可把它作为同文件里的第二层断言，但不要在第一版就依赖脆弱的地址值比较
- [ ] **P1.3** 📦 Git 提交：`[Test/Internals] Test: cover asCContext exception and callstack metadata`

- [ ] **P1.4** 运行 `Angelscript.TestModule.Internals.Context` 相关测试并修正命名/断言颗粒度
  - 用 `Automation RunTests Angelscript.TestModule.Internals.Context` 验证新文件中的测试名分组是否合理
  - 如果有 case 名称过于贴实现细节，先在本阶段重命名，避免后续目录总表频繁改名
- [ ] **P1.4** 📦 Git 提交：`[Test/Internals] Test: verify asCContext automation group`

### Phase 2：补齐 `asCScriptFunction` 与 `asCModule` 元数据测试

> 目标：把函数签名/局部变量/调试信息和模块 namespace/import/global 索引从现有 compiler/builder/restore 冒烟测试里剥离成独立回归。

- [ ] **P2.1** 创建 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptScriptFunctionTests.cpp`
  - 通过一段包含默认参数、多个局部变量、嵌套调用或多行函数体的脚本，拿到 `asCScriptFunction*`
  - 固化 `GetDeclaration()`、`GetParam()`、`GetReturnTypeId()`、`GetVarCount()`、`GetVarDecl()`、`FindNextLineWithCode()`、`GetScriptSectionName()`、`GetByteCode()` 的观测面
  - 优先比对“参数个数、参数名、默认参数是否存在、bytecode 长度大于 0”这类稳定结果，不要把容易随着编译器微调波动的整段声明串格式全部写死；若需要验证声明串，只锁关键片段而不是全文逐字符匹配
- [ ] **P2.1** 📦 Git 提交：`[Test/Internals] Feat: add asCScriptFunction internal tests`

- [ ] **P2.2** 扩展 `asCScriptFunction` 到调试/局部变量边界
  - 用多行函数体验证 `FindNextLineWithCode()` 在首行、跳过空行/注释后仍能返回有效行号
  - 用至少两个局部变量和一个参数，验证 `GetVar()` / `GetVarDecl()` 能区分局部变量与签名参数，避免未来 restore/debug 信息改动时悄悄丢变量声明
  - 若当前分支的调试信息行号受预处理强影响，则在断言中只要求“非负且落在脚本 section 合理范围内”，不强锁精确行号
- [ ] **P2.2** 📦 Git 提交：`[Test/Internals] Test: cover asCScriptFunction debug metadata`

- [ ] **P2.3** 创建 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptModuleTests.cpp`
  - 延续 `Builder`/`Restore` 里的脚本构建方式，但单独聚焦 `asCModule`
  - 覆盖 `SetDefaultNamespace()` / `GetDefaultNamespace()` round-trip、`GetGlobalVarIndexByName()` / `GetGlobalVarIndexByDecl()`、`GetFunctionCount()` / `GetFunctionByIndex()`、`GetObjectTypeCount()` / `GetObjectTypeByIndex()` 的一致性
  - 不把 save/load bytecode roundtrip 继续塞到这个文件；module 的 restore 仍留在 `AngelscriptRestoreTests.cpp`
- [ ] **P2.3** 📦 Git 提交：`[Test/Internals] Feat: add asCModule internal tests`

- [ ] **P2.4** 为 `asCModule` 增加 import 解绑与恢复断言
  - 复用现有 `Builder.ImportBinding` 的双模块模式，新增 `UnbindImportedFunction()` / `UnbindAllImportedFunctions()` / 再绑定后的行为验证
  - 目标是把“导入计数存在”扩成“解绑后调用失败或未绑定、重绑后恢复可执行”的完整边界
  - 如果当前实现对未绑定导入的执行表现依赖错误消息字符串，优先验证返回码或导入计数变化，而不是全文匹配日志
- [ ] **P2.4** 📦 Git 提交：`[Test/Internals] Test: cover asCModule import unbind and rebind flow`

- [ ] **P2.5** 运行 `Angelscript.TestModule.Internals.ScriptFunction` 与 `Angelscript.TestModule.Internals.Module` 分组测试
  - 先跑两个新分组，再跑 `Angelscript.TestModule.Internals.Compiler` / `Builder` / `Restore` 邻近旧用例，确保新旧测试对同一 helper 的使用没有互相污染
  - 如发现 `BuildModule()` 共享状态导致测试串线，优先在测试里改用 clone engine 或显式 `DiscardModule()`，不要在本阶段顺手重构 shared helper；import/unbind 类 case 默认按 clone engine 处理
- [ ] **P2.5** 📦 Git 提交：`[Test/Internals] Test: verify script-function and module groups`

### Phase 3：补齐 `asCObjectType` 类元数据测试

> 目标：为脚本类的继承、方法、属性、behaviour/factory 元数据建立独立回归，避免未来 object type 改动只在高层执行用例中被动暴露。

- [ ] **P3.1** 创建 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptObjectTypeTests.cpp`
  - 通过一段包含基类/派生类、成员属性、普通方法、构造函数的脚本，拿到 `asCObjectType*`
  - 首批覆盖 `GetBaseType()`、`DerivesFrom()`、`GetMethodCount()`、`GetMethodByName()`、`GetMethodByDecl()`、`GetPropertyCount()`、`GetPropertyDeclaration()`
  - 若当前分支的脚本类 factory 生成路径稳定，则顺带验证 `GetFactoryCount()` / `GetFactoryByIndex()`；若 factory 在当前 APV2 集成里不稳定，则先把 property/method/derives 链路固化下来
- [ ] **P3.1** 📦 Git 提交：`[Test/Internals] Feat: add asCObjectType internal tests`

- [ ] **P3.2** 固化继承属性与访问标记断言
  - 用基类 + 派生类脚本验证 `IsPropertyInherited()` 与 `GetProperty(..., outIsPrivate, outIsProtected, outOffset, ..., outIsConst)` 返回值组合
  - 重点观察“基类属性在派生类型中的继承标记”和“派生类自有属性 offset/可见性”是否稳定；避免只断言属性数量这种过宽边界
  - 如当前脚本语法不稳定支持 `private/protected` 细节，则最少也要把 inherited / non-inherited 两类属性区分开
- [ ] **P3.2** 📦 Git 提交：`[Test/Internals] Test: cover asCObjectType inherited property metadata`

- [ ] **P3.3** 固化方法枚举与 behaviour 枚举断言
  - 对同名方法查找、`GetMethodByIndex(index, true)` / `GetMethodByDecl(..., true)` 之类虚方法重载 surface 做存在性验证，确认当前 APV2 暴露的 2.38 兼容重载没有退化
  - 至少加入一条 `GetBehaviourByIndex()` 断言，验证构造/析构/引用行为枚举能返回稳定的 behaviour 类型；不要只停留在 `GetMethodCount() > 0`，也不要把 behaviour 在数组中的精确索引顺序写死
  - 若执行阶段证明 behaviour 顺序并不稳定，则改为“遍历中存在构造或析构 behaviour”而不是写死索引位置
- [ ] **P3.3** 📦 Git 提交：`[Test/Internals] Test: cover asCObjectType methods and behaviours`

- [ ] **P3.4** 运行 `Angelscript.TestModule.Internals.ObjectType` 及相邻 `StructCppOps` 回归
  - 新 object type 测试会和 `StructCppOps` 的脚本结构体元数据边界相邻，至少要补跑 `Angelscript.TestModule.Internals.StructCppOps` 与 `Angelscript.TestModule.Internals.ObjectType`
  - 不把 `ClassGenerator` 强行列为必跑项；它属于 Runtime Integration 层，只在本阶段实现实际触及生成类桥接时才补跑
- [ ] **P3.4** 📦 Git 提交：`[Test/Internals] Test: verify object-type adjacent regressions`

### Phase 4：同步测试目录文档并完成回归基线

> 目标：把新增 internal-class 测试正式纳入目录与回归命令基线，避免实现完以后只有代码没有导航。

- [ ] **P4.1** 更新 `Documents/Guides/TestCatalog.md`
  - 在 `## 6. Internals — 内部机制` 下新增 `Context`、`ScriptFunction`、`ObjectType`、`Module` 四个小节或合适的归类段落
  - 每个测试名都写明“验证内容”，延续现有目录总表风格，而不是只堆文件名
  - 若某个新增文件最终承载多个测试主题，也要在目录总表中按主题拆条，不把整文件概括成一句大而空的描述；如果仓库要求目录顶部 PASS 基线计数与总测试数保持同步，也在同一提交里一并更新
- [ ] **P4.1** 📦 Git 提交：`[Test/Internals] Docs: catalog new internal class tests`

- [ ] **P4.2** 按需更新 `Documents/Guides/Test.md`
  - 只有当执行阶段真的新增了 `Internals/` 局部 helper 或发现当前文档没有明确说明 `source/as_*.h` 只能留在 `Internals/` 时，才补一句边界说明
  - 如果没有新增规则，就不要为了“看起来完整”强行改文档，避免无谓噪音
- [ ] **P4.2** 📦 Git 提交：`[Test/Internals] Docs: clarify internal test header usage`

- [ ] **P4.3** 跑最终回归基线并记录结果
  - 先跑新增四组：`Context` / `ScriptFunction` / `ObjectType` / `Module`
  - 再跑 `Angelscript.TestModule.Internals`
  - 如时间允许，再补跑 `Angelscript.TestModule.Shared`，确认新增 helper 使用方式没有影响现有共享测试基础设施
  - 任何失败都先确认是新断言暴露真实回归，还是测试假设过窄；不要为了通过而删除失败断言
- [ ] **P4.3** 📦 Git 提交：`[Test/Internals] Test: verify expanded internal class regression suite`

## 验收标准

1. `Internals/` 下至少新增 4 个以 internal class 为中心命名的测试文件：`Context`、`ScriptFunction`、`ObjectType`、`Module`。
2. 每个新增文件至少覆盖一个“元数据/内部状态”类断言，而不只是“脚本编译成功 / 返回值正确”。
3. `Native/` 层没有新增任何 `source/as_*.h` include，也没有把 internal-class helper 抽到 `Shared/`。
4. `Documents/Guides/TestCatalog.md` 能列出新增测试名与验证内容。
5. 目标自动化分组和 `Angelscript.TestModule.Internals` 总分组可在命令行下运行。

## 风险与注意事项

### 风险 1：把 internal class 断言误写成语言功能测试

高层脚本行为测试更容易写，但那会掩盖真正需要回归的内部类元数据。执行时必须优先断言 `GetDeclaration()`、`GetProperty()`、`GetState()`、`GetImportedFunctionCount()` 这一类 internal surface，而不是只看脚本返回 42。

### 风险 2：断言过度依赖易波动的实现细节

像完整错误文案、精确 bytecode opcode 序列、某些 behaviour 索引顺序，未来可能因为上游合并或 UE++ 调整而变化。新增测试优先锁住“非空 / 数量 / 名称 / 关系 / 返回码”这种稳定边界，减少脆断。

### 风险 3：共享测试引擎导致用例串线

`GetResetSharedTestEngine()` 足够方便，但模块名复用、导入状态和全局变量初始化容易让 internal tests 互相污染。每个新增文件都应优先使用唯一 module name；必要时改用 clone engine，而不是把不稳定状态留给后续排查。

### 风险 4：文档与代码不同步

`TestCatalog.md` 已经是当前测试导航入口。若实现新增了 `Context` / `Module` 等测试分组却不更新目录，后续再做计划或回归时还会重复搜索同一批文件。
