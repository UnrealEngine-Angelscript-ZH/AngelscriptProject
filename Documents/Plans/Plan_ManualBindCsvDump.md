# 手动 Bind 函数与成员 CSV 导出计划

## 背景与目标

当前 `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/` 下已经有 100 个 `Bind_*.cpp` 文件，手动 bind 的全局函数、全局变量、成员函数、成员属性、构造/析构/工厂等注册语句分散在这些文件里。仓库当前虽然已经有：

- `FAngelscriptBinds::GetAllRegisteredBindNames()` / `GetBindInfoList()` 这样的 **bind 级** 查询；
- `FAngelscriptBindDatabase` 这样的 **自动绑定缓存**；
- `FAngelscriptDocs::DumpDocumentation()` 这样的 **运行时枚举** 样板；

但还没有一条稳定、可重复、面向审计/对账的链路，把**手动 bind 的函数与成员**统一导出成一份可直接在 Excel / Sheets / Python 中消费的 CSV。

本计划的目标是新增一套**仓库级导出工具**，把显式写在 `Bind_*.cpp` 中的手动 bind 符号整理成单一 CSV，并补齐自测、烟雾验证和文档入口，最终形成一条“拉源码 → 生成 CSV → 自测/抽样验证”的固定工作流。

## 范围与边界

- **导出对象**：`Bind_*.cpp` 中显式写出的手动注册语句，至少覆盖：
  - `FAngelscriptBinds::BindGlobalFunction(...)`
  - `FAngelscriptBinds::BindGlobalVariable(...)`
  - `Binder.Method(...)`
  - `Binder.Property(...)`
  - `Binder.Constructor(...)`
  - `Binder.Destructor(...)`
  - `Binder.Factory(...)`
- **输出形式**：单文件 CSV，UTF-8 BOM，稳定排序，可重复生成。
- **主落点**：`Tools/`。这是一个**源码审计/导出工具**，主输入是仓库里的 `Bind_*.cpp`，不是 packaged runtime 里的运行时对象，因此 canonical implementation 不放在 host project，也不强行塞进 runtime 公共 API。
- **验证落点**：
  - 解析/CSV 自测放在 `Tools/Tests/`；
  - 插件侧只补最小 smoke，用于确认若干代表性手动 bind 与导出结果没有明显漂移。
- **明确排除**：
  - `Bind_BlueprintCallable.cpp` 这类 **UFunction 反射驱动自动绑定**；
  - `FAngelscriptBindDatabase` 中缓存的自动 class/struct/property/method 记录；
  - 枚举值、delegate type info、文档字符串等**不属于“函数与成员”主体**的记录；
  - 一切需要启动 UE Editor 或依赖 `AgentConfig.ini` 才能完成的核心导出步骤。
- **与其他计划的关系**：如果后续需要把“运行时最终绑定表”也导出为 CSV，应并入 `Documents/Plans/Plan_ASEngineStateDump.md`；本计划只解决**源码里的手动 bind 清单**。

## 当前事实状态快照

- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptBinds.h` / `AngelscriptBinds.cpp`
  已提供 `FAngelscriptBinds::FBind`、`GetAllRegisteredBindNames()`、`GetBindInfoList()`、`CallBinds()` 等能力，但这些 API 暴露的是 **bind 单元级** 信息，而不是“这个 bind 单元内部到底注册了哪些函数/成员”。
- 大量手动 bind 文件采用 `AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Xxx(..., [] { ... });` 这种写法，常用的是**无显式 `FName` 的 `FBind` 构造重载**。这意味着运行时经常只能看到 `UnnamedBind_n`，无法把 `Bind_FApp`、`Bind_FIntPoint` 这类源文件级 bind 单元名稳定映射回来，因此 **runtime bind name 不能作为本任务的主主键**。
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptDocs.cpp` 的 `DumpDocumentation()` 已证明：运行时可以枚举 object type / method / property / global function / global variable，但它解决的是“引擎里最后有什么”，不是“这些东西是不是来自手写 bind 文件”。
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintCallable.cpp` 通过 `FAngelscriptFunctionSignature::WriteToDB()` 把自动 UFunction 绑定写入 `FAngelscriptMethodBind`；`FAngelscriptBindDatabase` 也持有 class/struct/property/method 缓存。这些结构更适合当作**自动绑定基线/排除源**，不适合作为手动 bind 的唯一来源。
- `Plugins/Angelscript/Source/AngelscriptRuntime/Testing/AngelscriptBindExecutionObservation.*` 只记录执行过哪些 bind 名称和耗时，不记录符号级条目，而且仍会受到 unnamed bind 的限制，无法直接完成本任务。
- 代表性手动 bind 文件已经足够说明模式覆盖面：
  - `Bind_FApp.cpp`：全局函数；
  - `Bind_FIntPoint.cpp`：值类型属性 + 成员函数；
  - `Bind_FCollisionQueryParams.cpp`：多 bind block、成员属性、成员函数、全局函数、全局变量混合；
  - `Bind_UEnum.cpp`：自动枚举扫描 + 手动成员 bind 混合，是需要在 Phase 0 明确边界的典型复杂文件。
- `Tools/Tests/` 已有 `AutomationToolSelfTests.ps1`、`RunToolingSmokeTests.ps1`、fixture 目录与 helper 目录，说明本仓库已经接受“PowerShell 工具 + 自带 fixture/self-test”的工具交付形态。

## 影响范围

本次计划预估涉及以下操作（按需组合）：

- **工具脚本新增**：新增手动 bind 扫描与 CSV 写出入口。
- **工具自测新增**：新增 fixture、自测脚本，并接入现有 tooling smoke runner。
- **插件烟雾验证新增**：新增最小化 automation smoke，锁住代表性手动 bind 没有和导出规则脱节。
- **文档同步**：新增工具说明、命令示例、输出路径说明，并把本计划挂到机会索引。

按目录分组的文件清单如下：

`Tools/`（6 个）：
- `Tools/ExportManualBinds.ps1` — 主导出工具，负责枚举 `Bind_*.cpp`、解析 bind block、输出 CSV。
- `Tools/Tests/ManualBindExportSelfTests.ps1` — 解析器与 CSV writer 自测入口。
- `Tools/Tests/Fixtures/ManualBindExport/SimpleGlobalBind.cpp` — 最小全局函数/变量 fixture。
- `Tools/Tests/Fixtures/ManualBindExport/SimpleMemberBind.cpp` — 最小成员函数/成员属性 fixture。
- `Tools/Tests/Fixtures/ManualBindExport/MixedNamespaceBind.cpp` — namespace、多 bind block、混合语句 fixture。
- `Tools/Tests/RunToolingSmokeTests.ps1` — 接入新的手动 bind 导出自测。

`Plugins/Angelscript/Source/AngelscriptTest/Core/`（1 个）：
- `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptManualBindInventoryTests.cpp` — 代表性手动 bind smoke，确认导出目标与真实绑定面没有明显漂移。

`Documents/`（3 个）：
- `Documents/Tools/Tool.md` — 新工具说明、参数、输出路径、示例命令。
- `Documents/Guides/TestCatalog.md` — 若新增 automation smoke，则登记测试入口。
- `Documents/Plans/Plan_OpportunityIndex.md` — 把本计划登记为活跃 plan。

## 分阶段执行计划

### Phase 0：冻结“手动 bind”口径、CSV 列设计与复杂文件边界

> 目标：先把“什么算手动 bind”“什么要排除”“一行 CSV 到底表达什么”固定下来，避免实现到一半又回头改口径。

- [ ] **P0.1** 冻结手动 bind 的纳入/排除矩阵
  - 先把本计划的核心定义写清楚：我们导出的是 **源文件中显式出现的手动注册语句**，而不是“运行时所有 app-bound 符号”。这一步必须把 `Bind_BlueprintCallable.cpp`、`Bind_BlueprintEvent.cpp`、`Bind_BlueprintType.cpp`、`Bind_UEnum.cpp` 这类包含自动/混合逻辑的文件逐个归类，明确是“整体排除”“部分纳入”还是“只取显式写死 declaration 的语句”。
  - 这一步还要明确：`Bind_FApp` 这种 static variable identifier 才是本任务的 canonical bind unit name；运行时 `UnnamedBind_n` 只可作为可选 debug 信息，不能反客为主变成 CSV 主键。
  - 若某个混合文件需要拆成“显式手动语句纳入、反射循环生成语句排除”，要在这一项里先把判定规则写进脚本注释和 fixture，再进入实现；不要把口径藏在若干 `if` 分支里。
- [ ] **P0.1** 📦 Git 提交：`[Tools/BindDump] Docs: freeze manual bind inclusion matrix`

- [ ] **P0.2** 冻结 CSV schema、排序规则与输出路径
  - 先确定 CSV 至少包含这些列：`BindFile`、`BindVariable`、`BindOrderExpr`、`OwnerKind`、`OwnerName`、`Namespace`、`RecordKind`、`RegistrationApi`、`Declaration`、`SourceLine`、`Notes`。其中 `RecordKind` 至少要区分 `GlobalFunction`、`GlobalVariable`、`Method`、`Property`、`Constructor`、`Destructor`、`Factory`；`BindOrderExpr` 第一版固定保存源码里 `FBind(...)` 构造参数中的**原始顺序表达式文本**（例如 `FAngelscriptBinds::EOrder::Late`、`(int32)FAngelscriptBinds::EOrder::Early`、`0`），不在脚本层尝试求值归一化。
  - 输出路径建议固定为 `Saved/Analysis/ManualBinds/ManualBinds.csv`，默认每次覆盖同一路径；如需保留历史，再另加可选 `-TimestampedOutput` 参数，但不要在第一版就把工具做成多模式矩阵。
  - 排序规则必须稳定：先按 `BindFile`，再按 `BindVariable`，再按 `SourceLine`。这样 diff 才有意义，也便于以后把 CSV 纳入 audit/对账流程。
- [ ] **P0.2** 📦 Git 提交：`[Tools/BindDump] Docs: freeze CSV schema and output contract`

### Phase 1：实现源码扫描器，先把 bind block 和符号语句稳定抽出来

> 目标：先稳定解析 `Bind_*.cpp` 里的结构化模式，不急着做花哨功能；第一版宁可严格失败，也不要静默漏行。

- [ ] **P1.1** 实现 `Bind_*.cpp` 枚举与 bind block 发现
  - 在 `Tools/ExportManualBinds.ps1` 中先完成文件枚举和 bind block 定位：识别 `AS_FORCE_LINK const FAngelscriptBinds::FBind ...` 起始行、变量名、构造参数和 lambda body 范围。这里不要只靠单行正则；至少要支持跨行构造参数和 lambda body 的括号/花括号计数，否则 `Bind_FCollisionQueryParams.cpp` 这类文件会立刻打穿脚本。
  - 这一项只解决“找到 block”和“拿到 bind 级元数据”，不急着一口气抽所有注册语句。输出可以先是内部对象模型，例如 `BindFile + BindVariable + BindOrderExpr + BodyText + StartLine/EndLine`，让后续解析器有稳定输入。
  - 对无法闭合括号或无法定位 lambda body 的 block，脚本必须报错并退出非 0，而不是跳过；否则第一版 CSV 会在没人知道的情况下默默漏 bind。
- [ ] **P1.1** 📦 Git 提交：`[Tools/BindDump] Feat: discover bind blocks from source files`

- [ ] **P1.2** 实现显式注册语句提取与 owner 解析
  - 在 bind block 内识别并抽取显式注册语句：静态全局调用（`BindGlobalFunction` / `BindGlobalVariable`）和 binder 变量调用（`.Method` / `.Property` / `.Constructor` / `.Destructor` / `.Factory`）。
  - 同时建立最小 owner 解析：像 `auto FIntPoint_ = FAngelscriptBinds::ValueClass<FIntPoint>("FIntPoint", ...)`、`auto UEnum_ = FAngelscriptBinds::ExistingClass("UEnum")`、`FAngelscriptBinds::FNamespace ns("FApp")` 这类语句要被记录成 alias/namespace 上下文，使后续 CSV 行可以写出 `OwnerKind` / `OwnerName` / `Namespace`，而不是只剩下一段无法消费的原始源码。
  - 第一版不需要做完整 C++ 语法树，但必须支持：跨行 declaration、lambda 作为第二参数、`METHODPR_TRIVIAL` / `FUNC_TRIVIAL` 这类宏包裹的参数列表，以及一个 bind block 中多个 alias 变量并存的情况。若出现 unsupported pattern，必须把原始源码片段写进错误信息，方便补 fixture 和规则。
- [ ] **P1.2** 📦 Git 提交：`[Tools/BindDump] Feat: extract manual registration rows and owner context`

- [ ] **P1.3** 实现 deterministic CSV writer 与最小 CLI 参数面
  - 给脚本补 CSV writer：统一做逗号、双引号、换行转义，写出 UTF-8 BOM，并确保空值、布尔值和多行 declaration 的落盘格式固定。不要依赖 Excel 专有格式，也不要把一堆调试信息混进主 CSV。
  - CLI 第一版只保留最小参数：`-OutputPath`、`-Root`、`-FailOnUnsupportedPattern`。默认从仓库根目录跑，不依赖 `AgentConfig.ini`、引擎路径或 UE 进程。
  - 生成结束后在 stdout 打印文件数、bind block 数、行数、unsupported pattern 数；这些摘要也要进入自测断言，防止脚本“成功退出但实际空表”。
- [ ] **P1.3** 📦 Git 提交：`[Tools/BindDump] Feat: write deterministic manual-bind CSV output`

### Phase 2：用 fixture 和真实文件把解析器钉住，避免后续回归

> 目标：先把解析器测试做扎实；这种脚本一旦没有 fixture，很快就会在某个跨行 lambda 或宏参数上悄悄坏掉。

- [ ] **P2.1** 在 `Tools/Tests/Fixtures/ManualBindExport/` 建立最小 fixture 组
  - 新增三类 fixture：纯全局注册、纯成员注册、混合 namespace + 多 bind block 注册。fixture 内容要尽量贴近仓内真实写法，而不是发明一套比真实代码更整齐的 toy syntax。
  - 自测脚本 `Tools/Tests/ManualBindExportSelfTests.ps1` 应直接断言：发现了多少期望 block、抽出了多少期望 row、关键 declaration 和 owner 信息是否正确、CSV 是否包含 BOM 与正确转义。
  - fixture 不是为了“证明脚本能跑”，而是为了给未来扩规则时一个不会误伤既有模式的回归基线；每当发现真实 bind 文件里的新花样，都应该先加 fixture 再调解析逻辑。
- [ ] **P2.1** 📦 Git 提交：`[Tools/Test] Test: add manual bind export fixtures and parser self-tests`

- [ ] **P2.2** 用真实 bind 文件做 repo 级 smoke，自测代表性输出
  - 除了 synthetic fixture，还要在自测里直接跑仓内代表性文件：至少覆盖 `Bind_FApp.cpp`、`Bind_FIntPoint.cpp`、`Bind_FCollisionQueryParams.cpp`。断言重点不是总行数，而是代表性 row 是否存在：例如 `FApp::CanEverRender`、`FApp::GetProjectName`、`FIntPoint` 的 `X` / `Y` / `opAdd`、`FCollisionQueryParams` 的成员属性和相关全局函数。
  - 这一步还要显式断言自动路径没有混进来：例如 `Bind_BlueprintCallable.cpp` 不能作为正常手动 bind CSV 的来源；如果 Phase 0 最终把 `Bind_BlueprintType.cpp` 列为部分纳入，就要写清楚哪些显式行应该保留、哪些反射驱动项必须被排除。
  - 真实文件 smoke 既是“这个脚本能处理真实仓库写法”的证明，也是在没有启动 UE 的前提下给未来 refactor 提供最廉价的回归层。
- [ ] **P2.2** 📦 Git 提交：`[Tools/Test] Test: add repo-level manual bind export smoke coverage`

- [ ] **P2.3** 把新自测接入 `RunToolingSmokeTests.ps1`
  - 现有 `Tools/Tests/RunToolingSmokeTests.ps1` 已经是 tooling 自测入口，新脚本不能变成一个只能靠人记忆单独执行的孤立测试。需要把 `ManualBindExportSelfTests.ps1` 接进去，并保持失败时返回非 0。
  - 这一步还要确认输出目录清理、fixture 路径解析、临时 CSV 路径不会污染仓库；所有工具自测都应把临时产物放到可清理的测试目录，不要直接写到固定生产输出路径。
  - 如果现有 tooling smoke runner 有统一 PASS/FAIL 打印风格，新增脚本要跟它保持一致，不要另起一套报告口径。
- [ ] **P2.3** 📦 Git 提交：`[Tools/Test] Chore: wire manual bind export checks into tooling smoke runner`

### Phase 3：补插件侧最小 smoke，锁住“导出的代表行”与真实绑定面一致

> 目标：工具本体不依赖 UE，但仍需要一层最小 runtime smoke，防止源文件导出的 declaration 和真实绑定结果长期漂移。

- [ ] **P3.1** 新增 `AngelscriptManualBindInventoryTests.cpp`，锁住代表性手动 bind
  - 在 `Plugins/Angelscript/Source/AngelscriptTest/Core/AngelscriptManualBindInventoryTests.cpp` 新增最小 automation smoke，测试名前缀使用 `Angelscript.TestModule.Engine.ManualBindInventory.*`。测试只覆盖若干代表性条目，不把整张 CSV 和整套 runtime 绑定表强行做一一比对。
  - 至少覆盖三类代表：
    - 全局函数：`FApp::CanEverRender()` / `GetProjectName()`；
    - 值类型成员：`FIntPoint` 的 `X` / `Y` / `opAdd`；
    - 混合型 bind：`FCollisionQueryParams` 或 `FComponentQueryParams` 中的成员属性/成员函数/全局函数组合。
  - 这层 smoke 的目标是“快速发现导出规则和真实绑定面已经明显脱节”，而不是复刻完整 exporter。若实现时发现某类断言更适合已有 `Bindings/` 测试目录，也要保持 `Engine.ManualBindInventory` 只做最小代表性真值层。
- [ ] **P3.1** 📦 Git 提交：`[Test/Core] Test: add representative manual bind inventory smoke`

### Phase 4：同步工具文档、测试目录登记与机会索引

> 目标：让这套导出能力真正可被发现、可被复跑、可被后续计划引用，而不是“源码里有脚本，但没人知道怎么用”。

- [ ] **P4.1** 更新 `Documents/Tools/Tool.md`
  - 给 `Tools/ExportManualBinds.ps1` 增加工具条目：用途、输入目录、输出路径、参数、典型命令、是否依赖引擎、与 `Plan_ASEngineStateDump.md` 的边界关系都要写清楚。
  - 同时补一条自测说明：如何单独运行 `Tools/Tests/ManualBindExportSelfTests.ps1`，以及它和 `RunToolingSmokeTests.ps1` 的关系。不要让执行者只能从脚本源码里猜参数。
  - 如果工具输出摘要里有 `UnsupportedPatternCount`、`SkippedFileCount` 之类字段，也应在文档里说明其含义，防止后续看到非 0 就误判成 bug。
- [ ] **P4.1** 📦 Git 提交：`[Docs/Tools] Docs: add manual bind CSV export tool guide`

- [ ] **P4.2** 更新测试目录文档与机会索引
  - 若新增了 `Angelscript.TestModule.Engine.ManualBindInventory.*` automation smoke，则同步更新 `Documents/Guides/TestCatalog.md`，登记测试文件、用途与推荐前缀。
  - 同步更新 `Documents/Plans/Plan_OpportunityIndex.md`：把本计划登记为活跃 plan，并修正活跃 plan 数量，避免全景索引和实际目录状态不一致。
  - 这一项还应在相关计划/文档中补最小交叉引用：后续若要做运行时全量状态 dump 或脚本 API 文档生成，应能快速发现本计划已经提供了“手动 bind 源码清单”这块基础能力。
- [ ] **P4.2** 📦 Git 提交：`[Docs/Plan] Docs: register manual bind CSV dump plan and test entrypoints`

## 验收标准

1. `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\ExportManualBinds.ps1` 在仓库根目录可直接运行，并输出单一 CSV 到默认路径。
2. CSV 使用 UTF-8 BOM，列头固定，排序稳定；连续两次无源码变更运行，结果只允许时间戳/临时路径外的 0 diff。
3. CSV 至少包含以下代表性条目：
   - `Bind_FApp.cpp` 中的 `CanEverRender`、`GetProjectName`；
   - `Bind_FIntPoint.cpp` 中的 `X`、`Y`、`opAdd`；
   - `Bind_FCollisionQueryParams.cpp` 中至少一条成员属性和一条全局函数记录。
4. `Tools/Tests/ManualBindExportSelfTests.ps1` 与 `Tools/Tests/RunToolingSmokeTests.ps1` 全部通过；当解析器遇到 unsupported pattern 时，失败信息能指出具体文件与源码片段。
5. 若落地了 `AngelscriptManualBindInventoryTests.cpp`，对应 `Angelscript.TestModule.Engine.ManualBindInventory.*` smoke 通过，并能证明至少一组导出代表项在真实引擎绑定面中可见。
6. `Documents/Tools/Tool.md`、`Documents/Guides/TestCatalog.md`（若涉及）和 `Documents/Plans/Plan_OpportunityIndex.md` 已同步更新。

## 风险与注意事项

### 风险

1. **C++ 源码解析脆弱**：`Bind_*.cpp` 中广泛存在跨行 lambda、宏包裹参数、多个 bind block 混写的情况，单纯依赖一条大正则很容易漏行。
   - **缓解**：以“bind block 级括号/花括号扫描 + 显式注册语句模式识别”为主；新增规则前先补 fixture，再改脚本。

2. **自动/手动边界在混合文件中不清晰**：`Bind_BlueprintType.cpp`、`Bind_UEnum.cpp` 等文件同时包含显式注册和反射驱动逻辑。
   - **缓解**：在 Phase 0 明确 inclusion matrix；第一版宁可保守排除整类混合自动路径，也不要把自动绑定大量误纳入手动 CSV。

3. **runtime bind name 与源码 bind unit 名不稳定映射**：大量 bind 使用无显式 `FName` 的 `FBind` 构造，运行时只会生成 `UnnamedBind_n`。
   - **缓解**：把 `BindVariable` 和 `BindFile` 作为 CSV 的主识别键；runtime 信息只做可选 smoke，不反向主导 schema。

4. **声明文本与最终运行时 declaration 可能存在少量偏差**：某些语句在绑定后还会通过 `SetPreviousBindNoDiscard`、`ModifyScriptFunction` 等路径再补 trait 或默认参数行为。
   - **缓解**：本计划导出的是“源码里的手动 bind 清单”，不是运行时最终 ABI dump；用 Phase 3 的代表性 smoke 检查高价值条目是否明显漂移即可，不追求第一版做全量双向对账。

### 已知行为变化

1. **第一版 CSV 将以源码可读性优先，不保证还原所有宏展开后的 native 符号名**。
   - 影响文件：`Tools/ExportManualBinds.ps1`
   - 说明：像 `METHODPR_TRIVIAL`、`FUNC_TRIVIAL` 这类宏包裹的 native symbol hint，可先保留原始参数片段，后续如确有需要再追加更细的字段。

2. **CSV 主键将从“runtime bind name”切换为“源码 bind variable + source line”**。
   - 影响文件：`Tools/ExportManualBinds.ps1`、`Tools/Tests/ManualBindExportSelfTests.ps1`
   - 说明：这是为了规避 `UnnamedBind_n` 不稳定、不可审计的问题；任何基于旧 bind name 的消费逻辑都不应直接复用到这份 CSV 上。
