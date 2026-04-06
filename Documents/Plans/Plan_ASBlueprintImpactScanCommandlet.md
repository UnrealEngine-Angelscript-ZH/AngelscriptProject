# AS 变更蓝图影响扫描 Commandlet 计划

## 背景与目标

当前 `Plugins/Angelscript` 已经具备脚本类生成、热重载、Blueprint 子类场景测试，以及 Editor 侧对 class/struct/delegate 重载后的 Blueprint 依赖重编译链路，但还没有一条独立的“变更脚本 → 扫描项目 Blueprint 资产 → 给出受影响结果”的命令行化入口。用户当前关心的是：当 `.as` 代码发生变更时，项目中继承脚本类或依赖脚本生成类型的 Blueprint 资产可能需要被重新关注、重新编译或人工复核，因此需要在 `AngelscriptEditor` 模块内新增一个可批量扫描项目 Blueprint 资产的 commandlet，同时把 commandlet 使用的核心能力拆成可单元测试的最小模块，而不是把逻辑全部塞进 `UCommandlet::Main()`。

本计划的目标是把这条链路拆成插件级、可验证、可维护的几个阶段：先冻结“什么算受影响 Blueprint”的口径与 commandlet 入参，再从 `ClassReloadHelper` 中抽出可复用的 Blueprint 依赖分析能力，随后在 `AngelscriptEditor` 模块里实现扫描 commandlet，最后为参数解析、资产枚举、依赖判断和真实 Blueprint 子类场景补齐自动化测试与执行文档。

## 范围与边界

- 本计划只覆盖 `Plugins/Angelscript` 插件内实现，不把逻辑下沉回宿主工程模块。
- commandlet 明确落在 `AngelscriptEditor` 模块内，因为它要扫描 Blueprint 资产并复用 `AssetRegistry`、`Kismet2`、Editor-only Blueprint 分析/编译辅助能力；不沿用现有 Runtime commandlet 的放置方式。
- 首批只解决“扫描项目 Blueprint 资产并报告哪些资产受给定 AS 变更影响”的能力，不把 git diff、CI 集成、JSON 报告平台化、自动修复 Blueprint 或自动保存资产打包成第一批范围。
- 首批默认支持两种输入模式：全量扫描（盘点当前项目中所有与 Angelscript 生成类型有关的 Blueprint 资产）和带变更过滤的扫描（调用方显式提供 changed script relative path / file list）。不在第一批里自动推导 git 变更。
- “受影响”优先按现有代码事实建模：Blueprint 父类是脚本生成类；Blueprint 节点/变量/pin/外部依赖引用了由 AS 变更影响到的 class/struct/delegate/enum；不额外发明一套脱离现有 reload 语义的影响定义。
- 测试重点放在 commandlet 相关核心功能的单元测试与 Blueprint 场景测试；commandlet 进程级调起本身保持薄壳，不把首批测试全部做成外部进程黑盒。

## 当前事实状态快照

- 现有 commandlet 已存在于 Runtime 模块：
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptTestCommandlet.h`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptTestCommandlet.cpp`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptAllScriptRootsCommandlet.h`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptAllScriptRootsCommandlet.cpp`
  它们都采用标准 `UCommandlet::Main(const FString& Params)` 模式，但不依赖 Editor 专属模块能力。
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp` 已在 `IsRunningCommandlet()` 为真时初始化 Angelscript，这意味着新的 Editor commandlet 可以依赖运行期脚本引擎已被拉起，但仍需明确自己在 Editor 模块内的资产扫描职责。
- `Plugins/Angelscript/Source/AngelscriptEditor/Private/ClassReloadHelper.cpp` 已经拥有最接近需求的 Blueprint 依赖扫描逻辑：`PerformReinstance()` 中通过 `TObjectIterator<UBlueprint>`、`HasExternalDependencies()`、pin type 检查、`FArchiveReplaceObjectRef` 等手段找出依赖被重载 class/struct/delegate 的 Blueprint，并触发 `FBlueprintCompilationManager::ReparentHierarchies()` / `QueueForCompilation()` / `FlushCompilationQueueAndReinstance()`。这说明“怎么判断 Blueprint 依赖了脚本生成类型”已有生产逻辑，只是还未被抽成可复用扫描服务。
- `Plugins/Angelscript/Source/AngelscriptEditor/Private/AngelscriptDirectoryWatcherInternal.cpp` 与 `Private/Tests/AngelscriptDirectoryWatcherTests.cpp` 已经证明 Editor 模块内部 helper + `Angelscript.Editor.*` 自动化测试是当前仓内可接受的组织模式。commandlet 相关纯 Editor 功能应沿用这条路径，而不是直接堆到 `AngelscriptTest` 模块。
- `Plugins/Angelscript/Source/AngelscriptTest/Blueprint/AngelscriptBlueprintSubclassRuntimeTests.cpp` 与 `Blueprint/AngelscriptBlueprintSubclassActorTests.cpp` 已覆盖“Blueprint 子类继承脚本父类”的真实场景，包括 transient Blueprint 创建、编译、世界内生成和行为继承。这正是后续验证 impact scanner 是否能识别“Blueprint 继承脚本类”最值得复用的测试基础。
- 当前仓内还没有“扫描整个项目 Blueprint 资产并按 AS 变更过滤结果”的公共服务，也没有现成的 `Angelscript.Editor.*` / `Angelscript.TestModule.*` 测试族专门锁住这条能力。

## 影响范围

本次实现预计涉及以下操作类型（按需组合）：

- **依赖扫描逻辑抽取**：把 `ClassReloadHelper` 中现有 Blueprint 依赖判断逻辑拆成可复用 helper / service，供 reload 与 commandlet 共用。
- **资产枚举与过滤**：通过 `AssetRegistry` 枚举 Blueprint 资产，必要时加载资产后运行深度依赖判定。
- **Editor Commandlet 外壳**：在 `AngelscriptEditor` 模块新增 commandlet，负责参数解析、调用扫描服务、输出结果和返回码。
- **测试 seam 与单元测试**：为参数解析、影响集合构建、Blueprint 依赖匹配等逻辑补 Editor 内部单元测试。
- **Blueprint 场景回归**：在 `AngelscriptTest/Blueprint/` 下补真实脚本父类 → Blueprint 子类的 impact 识别用例。
- **文档与执行入口同步**：把新增 commandlet 与测试入口登记到指南文档中。

按目录分组的预估文件清单如下（会在实施时按最小必要原则微调）：

`Plugins/Angelscript/Source/AngelscriptEditor/`（核心实现）
- `AngelscriptEditor.Build.cs` — 视实现需要补齐 commandlet / 资产扫描相关依赖或 include 路径
- `Private/ClassReloadHelper.h` — 让 reload 流程改为复用共享扫描结果/共享分析 helper
- `Private/ClassReloadHelper.cpp` — 从 `PerformReinstance()` 中抽出 Blueprint 依赖发现逻辑，避免 commandlet 重复实现
- `Private/AngelscriptEditorModule.cpp` — 若需要注册共享服务、命令入口辅助或初始化 seam，则在这里做最小同步
- `Private/BlueprintImpact/AngelscriptBlueprintImpactScanner.h` — 新增 Blueprint impact 扫描请求/结果/分析接口
- `Private/BlueprintImpact/AngelscriptBlueprintImpactScanner.cpp` — 新增扫描与过滤实现
- `Private/BlueprintImpact/AngelscriptBlueprintImpactCommandlet.h` — 新增 Editor commandlet 声明
- `Private/BlueprintImpact/AngelscriptBlueprintImpactCommandlet.cpp` — 新增 Editor commandlet 入口实现
- `Private/Tests/AngelscriptBlueprintImpactScannerTests.cpp` — 新增 Editor 内部测试，覆盖 request/filter/dependency matching

`Plugins/Angelscript/Source/AngelscriptTest/Blueprint/`（场景验证）
- `AngelscriptBlueprintSubclassRuntimeTests.cpp` — 复用或扩展现有 transient Blueprint 子类夹具
- `AngelscriptBlueprintImpactTests.cpp` — 新增 Blueprint 影响识别场景测试（如需独立文件）

`Config/` 与 `Documents/`（流程同步）
- `Config/DefaultEngine.ini` — 如需增加新的测试组或前缀聚合入口，再做最小更新
- `Documents/Guides/Test.md` — 登记 Editor 内部测试和推荐执行命令
- `Documents/Guides/TestCatalog.md` — 登记新增测试文件与定位说明
- `Documents/Tools/Tool.md` — 若新增 commandlet / 推荐执行入口，需要同步工具导航与用法索引

## 分阶段执行计划

### Phase 0：冻结影响口径、参数契约与代码分层

> 目标：先把 commandlet 要解决的问题、输入输出和复用边界固定住，避免后面一边写扫描逻辑一边反复改“什么算受影响”。

- [ ] **P0.1** 冻结首批 commandlet 的输入/输出契约
  - 明确首批 commandlet 至少支持两种模式：全量盘点项目内与 Angelscript 生成类型有关的 Blueprint；以及带变更过滤的扫描（显式提供 changed script relative paths，或从文本文件读取一组 relative paths）。
  - 返回值只覆盖流程性状态：参数错误 / 初始化失败 / 资产加载或扫描失败 / 扫描成功；不要在第一批里额外引入“发现受影响资产即失败”这类策略开关，避免把工具用途和 CI 策略混在一起。
  - 输出先以标准日志摘要 + 明细列表为准，确保人类和自动化日志都可消费；是否落 JSON/CSV 报告留作后续 sibling plan，不阻塞首批命令入口。
- [ ] **P0.1** 📦 Git 提交：`[Editor/Plan] Docs: freeze blueprint impact commandlet contract`

- [ ] **P0.2** 冻结“受影响 Blueprint”口径并对齐到现有 reload 语义
  - 明确首批 impact 口径直接复用 `ClassReloadHelper` 已有依赖判断事实，而不是创造新的图算法：Blueprint 父类链依赖脚本生成类、Blueprint 节点外部依赖触达被影响 class/struct/delegate、pin type 或变量类型引用被影响脚本生成类型、对象引用替换扫描能命中旧对象 → 新对象映射。
  - 将 enum 是否纳入首批范围一并写清：若 `ClassReloadHelper` 当前只对 enum 走 action refresh 而没有成型的 Blueprint 深度扫描逻辑，首批可先把 enum 影响标记为“只记录为保守命中或待后续增强”，不要把未落地语义直接写成强断言目标。
  - 这一步还要写清“变更输入如何映射到受影响脚本生成符号”的最小目标：以脚本 relative path / module record 为主，必要时再补类名别名，不先做 git 历史或跨版本 diff 推导。
- [ ] **P0.2** 📦 Git 提交：`[Editor/Plan] Docs: freeze blueprint impact definition`

- [ ] **P0.3** 冻结代码分层和测试分层
  - commandlet 只做参数解析、引擎/编辑器环境准备、调用 scanner、打印摘要；所有可判定逻辑都下沉到 `BlueprintImpact` helper/service 中，确保可单测。
  - Editor 内部 deterministic 逻辑放 `Plugins/Angelscript/Source/AngelscriptEditor/Private/Tests/`，Automation 前缀使用 `Angelscript.Editor.*`；真实脚本父类与 Blueprint 子类场景放 `Plugins/Angelscript/Source/AngelscriptTest/Blueprint/`，Automation 前缀沿用 `Angelscript.TestModule.*` 家族，不在 `Native/` 或 `Runtime/Tests/` 里混放。
  - 与 `ClassReloadHelper` 的关系要在本阶段锁住：扫描服务是 reload 与 commandlet 的共享底座，后续若只在 commandlet 内单独实现一份，视为偏离计划。
- [ ] **P0.3** 📦 Git 提交：`[Editor/Plan] Docs: freeze scanner and test layering`

### Phase 1：从热重载链路中抽出可复用的 Blueprint impact 扫描服务

> 目标：先把“如何识别 Blueprint 依赖了脚本生成类型”变成共享能力，再让 commandlet 和 reload 共同使用，避免两份逻辑以后漂移。

- [ ] **P1.1** 新建 Editor 内部 `BlueprintImpact` 请求/结果模型
  - 新增 `AngelscriptBlueprintImpactScanner.h/.cpp`，定义最小 request/result 结构，例如：扫描范围（全量或给定 changed script relative paths）、候选 Blueprint 资产输入、解析出的受影响 generated class/struct/delegate 集合、命中的 Blueprint 资产结果与命中原因。
  - 结果结构必须能表达“为什么这个 Blueprint 被判为受影响”，至少区分父类继承命中、节点外部依赖命中、pin/变量类型命中、对象引用替换命中，避免 commandlet 只能输出文件名而无解释。
  - 这一层不要直接依赖命令行参数字符串；参数归一化放 commandlet 或独立 parser helper，scanner 只接收结构化 request。
- [ ] **P1.1** 📦 Git 提交：`[Editor] Feat: add blueprint impact scanner request and result model`

- [ ] **P1.2** 抽取 `ClassReloadHelper` 中现有 Blueprint 依赖发现逻辑
  - 把 `ClassReloadHelper.cpp` 中目前内联在 `PerformReinstance()` 内的 Blueprint 依赖扫描步骤拆入共享 helper，包括：遍历 Blueprint、`HasExternalDependencies()`、pin type 检查、`FArchiveReplaceObjectRef` 引用扫描、对 struct/delegate 相关节点的识别。
  - `PerformReinstance()` 改为消费 scanner 结果，而不是继续保留一份私有实现；这样后续 commandlet 与 reload 的依赖口径能天然一致。
  - 抽取过程中要保留当前 reload 行为：依赖 Blueprint 集合、pin type 替换、reparent / queue compile / flush compile 等副作用仍由 `ClassReloadHelper` 驱动，scanner 本身只负责“识别与解释”，不直接执行重编译或保存资产。
- [ ] **P1.2** 📦 Git 提交：`[Editor] Refactor: share blueprint dependency discovery with reload helper`

- [ ] **P1.3** 建立“变更脚本 → 受影响脚本生成符号”解析链路
  - 基于 `FAngelscriptEngine` 当前 active modules / code sections / generated type 查询能力，把 changed script relative paths 映射为本次要关注的 generated class/struct/delegate 集合。若同一个脚本文件会产出多个生成符号，结果结构要能容纳多对多关系。
  - 首批以当前引擎内可见的模块与生成类型为准，不要求支持“脚本已被删除、当前引擎里已没有对应生成类”的历史回溯；删除类/删除文件的更激进离线分析可留到后续计划。
  - 若某类符号在现有引擎 API 下难以稳定解析，就先把该类符号列为保守命中或延后，不要为了首批计划去发明大量全新元数据缓存。
- [ ] **P1.3** 📦 Git 提交：`[Editor] Feat: resolve changed script inputs to generated symbol sets`

### Phase 2：在 Editor 模块内实现 Blueprint impact scan commandlet

> 目标：让 Editor 模块真正拥有一个可执行的 commandlet 入口，但保持其足够薄，只做 orchestration，不吞掉业务判断。

- [ ] **P2.1** 在 `AngelscriptEditor` 模块新增 commandlet 类与参数解析
  - 新建 `UAngelscriptBlueprintImpactScanCommandlet`，文件放在 `Plugins/Angelscript/Source/AngelscriptEditor/Private/BlueprintImpact/` 或等价的 Editor-only 子目录，保持与 scanner 同主题聚合。
  - 参数解析至少要覆盖：全量扫描默认模式、显式 `ChangedScript=` 多值输入、从文件读取 changed script list；并做路径标准化，统一到 plugin 现有 relative path 表达，避免命令侧大小写/分隔符噪声污染扫描结果。
  - `Main()` 内禁止重写依赖判断逻辑；只允许把参数转成 scanner request、触发资产枚举与扫描、打印摘要并返回状态码。
  - 若实现后发现 `AngelscriptEditor` 模块内 commandlet 的 discoverability 还需要 `.uplugin` / 模块描述或加载时机同步，应在这一阶段一并收口，并把真实 `-run=` 烟雾验证作为后续验收前置，而不是留到文档阶段才发现入口不可用。
- [ ] **P2.1** 📦 Git 提交：`[Editor] Feat: add blueprint impact scan commandlet shell`

- [ ] **P2.2** 接通项目 Blueprint 资产枚举与按需加载
  - 通过 `AssetRegistry` 枚举项目中的 Blueprint 资产，优先以 `UBlueprint` / Blueprint Generated Class 相关资产族为目标，不把所有 UObject 资产都纳入扫描。
  - 枚举层与深度分析层要分离：先拿轻量资产列表，再只对需要深度检查的候选资产做加载和依赖分析，避免把 commandlet 直接做成“无差别全加载所有资产”。
  - 如果首批无法仅靠 tag 精准筛出候选资产，也要保留一条“全 Blueprint 枚举 + 逐个深度检查”的可用慢路径，先保证结果正确，再后续谈性能优化。
- [ ] **P2.2** 📦 Git 提交：`[Editor] Feat: enumerate blueprint assets for impact scanning`

- [ ] **P2.3** 固化日志摘要与返回码约定
  - commandlet 输出至少包含：本次扫描模式、输入 changed script 数、解析出的受影响脚本符号数、总 Blueprint 资产数、候选数、命中数、失败加载/失败分析数、命中资产路径及命中原因摘要。
  - 返回码要和日志语义对齐：参数错误、初始化失败、资产扫描失败必须区分；“扫描成功但命中若干 Blueprint”仍视为成功扫描，不直接用失败码表达，避免把工具结果和策略判断耦合。
  - 首批先把输出留在标准日志中，不增加自动保存/自动修改资产行为，也不在 commandlet 中直接触发 Blueprint 编译或保存。
- [ ] **P2.3** 📦 Git 提交：`[Editor] Feat: finalize blueprint impact commandlet output and exit codes`

### Phase 3：为 commandlet 相关核心功能补 Editor 内部单元测试

> 目标：让 commandlet 的核心行为在不启动外部进程的情况下可稳定回归，尤其是参数、变更映射和依赖匹配逻辑。

- [ ] **P3.1** 新增 Editor 内部 scanner 单元测试文件
  - 在 `Plugins/Angelscript/Source/AngelscriptEditor/Private/Tests/` 新增 `AngelscriptBlueprintImpactScannerTests.cpp`，Automation 前缀统一使用 `Angelscript.Editor.BlueprintImpact.*`。
  - 先覆盖 deterministic 辅助逻辑：changed script 输入归一化、空输入/重复输入去重、全量扫描请求与过滤扫描请求构造、返回结果中的命中原因聚合与排序。
  - 这一步不要求一上来就构造真实项目磁盘资产；对纯参数和结果聚合逻辑优先使用最小 in-memory fixture，保证失败能快速定位。
- [ ] **P3.1** 📦 Git 提交：`[Editor/Test] Test: add blueprint impact request and result unit coverage`

- [ ] **P3.2** 为 Blueprint 依赖匹配 helper 补 Editor 内部测试
  - 针对 scanner 从 `ClassReloadHelper` 抽出的依赖识别逻辑补测试，至少覆盖：Blueprint 父类继承命中、`HasExternalDependencies()` 命中、pin type / variable type 命中，以及“无关 Blueprint 不应误报”。
  - 对 struct/delegate/enum 这类边界类型，要把当前实现真实支持到哪一层写进断言说明，禁止测试先假设未来增强已经存在。
  - 若某部分依赖分析必须依赖真实 Blueprint 编译结果，则用 transient package + transient Blueprint 夹具解决，而不要重新把逻辑塞回 commandlet 黑盒测试。
- [ ] **P3.2** 📦 Git 提交：`[Editor/Test] Test: cover blueprint dependency match reasons`

- [ ] **P3.3** 为资产枚举与候选过滤补 Editor 内部测试
  - 针对 commandlet 使用的资产枚举/过滤 helper 补测试，确保非 Blueprint 资产被忽略、Blueprint 资产路径与加载失败行为可预测、全量扫描与 changed-script 过滤路径都能稳定产生候选列表。
  - 如果实际实现引入了 `AssetRegistry` → 资产加载 → scanner 三段式管线，就分别锁住每一段的输入输出，不要只做“大而全”的单测。
  - 至少要有一条 disk-backed、`AssetRegistry` 可见的 Blueprint 资产路径验证，证明扫描链路不只对 transient Blueprint 夹具成立，而是真的覆盖项目资产发现契约；不能只靠内存 Blueprint 场景推导 commandlet 的项目扫描正确性。
  - 目标是让 commandlet 入口能保持极薄：一旦内部 helper 已被单测覆盖，`Main()` 本身只需要一个最小 smoke 层。
- [ ] **P3.3** 📦 Git 提交：`[Editor/Test] Test: cover blueprint asset enumeration and candidate filtering`

### Phase 4：补真实 Blueprint 子类与脚本变更影响场景测试

> 目标：除了 Editor 内部单元测试，还要用真实脚本父类 + Blueprint 子类场景证明 scanner 的结果不是只对假夹具成立。

- [ ] **P4.1** 复用现有 Blueprint 子类夹具，新增 impact 识别场景测试
  - 在 `Plugins/Angelscript/Source/AngelscriptTest/Blueprint/` 新增独立测试文件（建议 `AngelscriptBlueprintImpactTests.cpp`），或在现有 `AngelscriptBlueprintSubclassRuntimeTests.cpp` 中新增一组同主题 case，验证 transient Blueprint 子类会被 impact scanner 正确识别为依赖指定脚本父类。
  - 至少覆盖：Blueprint 直接继承脚本类、脚本类暴露 `UFUNCTION(BlueprintOverride)` / `UPROPERTY` 后 Blueprint 子类仍被识别、没有脚本依赖的 Blueprint 不应被误判为命中。
  - 这组测试要优先复用现有 `CreateTransientBlueprintChild()`、`CompileScriptModule()`、`FKismetEditorUtilities::CompileBlueprint()` 等夹具，避免新造第二套 Blueprint 场景测试基建。
- [ ] **P4.1** 📦 Git 提交：`[Blueprint/Test] Test: add script-parent blueprint impact scenarios`

- [ ] **P4.2** 补“变更脚本过滤”场景测试
  - 通过至少两个脚本模块 / 两个 Blueprint 子类的场景，验证给定 changed script relative path 集合时，scanner 只返回与这些脚本生成类型相关的 Blueprint，不把无关 Blueprint 一并报出。
  - 这一步特别重要，因为它验证的不是“是否能识别某个 Blueprint 是脚本子类”，而是“commandlet 是否真的能把一组 AS 变更映射成项目内受影响资产集合”。
  - 若实现中支持从 file list 输入 changed scripts，本项也要覆盖读取列表后的路径标准化与去重结果，而不是只测直接传参。
- [ ] **P4.2** 📦 Git 提交：`[Blueprint/Test] Test: verify changed-script filtered impact results`

- [ ] **P4.3** 补 `ClassReloadHelper` 与 scanner 共享逻辑的不漂移回归
  - 增加一条聚焦测试或断言，确保 `ClassReloadHelper` 使用的依赖识别入口与 commandlet scanner 是同一套 helper / service，而不是在后续重构里又偷偷分叉。
  - 这条回归可以落在 Editor 内部测试，也可以通过最小结构断言/行为回归表达，但必须有一条自动化保护“共享逻辑不再复制”这一架构约束。
  - 若实现完成后发现共享入口难以直接断言，也至少要在测试说明和代码组织上把唯一入口固定清楚，禁止再出现第二套 Blueprint 依赖扫描实现。
- [ ] **P4.3** 📦 Git 提交：`[Editor/Test] Test: lock shared scanner usage between reload and commandlet`

### Phase 5：同步文档、执行入口与首轮回归命令

> 目标：把新增能力纳入仓库现有 build/test/plan 流程，不让它变成“代码里有，没人知道怎么跑”的孤岛。

- [ ] **P5.1** 更新测试与目录导航文档
  - 更新 `Documents/Guides/Test.md`，补充 Editor 内部 `Angelscript.Editor.BlueprintImpact.*` 与 Blueprint 场景 `Angelscript.TestModule.*` 的推荐执行命令，明确它们分别保护 commandlet 的哪一层能力。
  - 更新 `Documents/Guides/TestCatalog.md`，登记新增的 Editor scanner tests / Blueprint impact tests，让后续执行者能快速定位文件与前缀。
  - 更新 `Documents/Tools/Tool.md`，把新的 commandlet 使用路径、推荐测试入口或相关工具导航纳入工具文档，满足仓内“新增推荐入口要同步工具文档”的规则。
  - 若新增测试组或需要 `Config/DefaultEngine.ini` 提供便捷聚合入口，则在这一阶段最小同步，不把更多无关测试组整理工作捎带进来。
- [ ] **P5.1** 📦 Git 提交：`[Docs/Test] Docs: register blueprint impact commandlet test entrypoints`

- [ ] **P5.2** 记录 commandlet 使用方式与首轮回归命令模板
  - 在相关文档中给出建议命令模板，至少包含：Editor commandlet 调用示例、仅跑 Editor 内部测试的 `Tools\RunTests.ps1 -TestPrefix "Angelscript.Editor.BlueprintImpact"`、仅跑 Blueprint 场景测试的推荐前缀，以及必要时的 build 命令模板。
  - 这一步要显式强调：构建使用 `Tools\RunBuild.ps1`，测试使用 `Tools\RunTests.ps1`，并继承 `AgentConfig.ini` 中的路径与超时配置；不要把旧的 `UnrealEditor-Cmd.exe` 直调方式重新写回文档。
  - 如果 commandlet 未来要接 CI，再基于这里的命令模板追加策略，而不是在首轮计划里先写死 CI 工作流。
- [ ] **P5.2** 📦 Git 提交：`[Docs/Editor] Docs: document blueprint impact commandlet usage`

- [ ] **P5.3** 执行真实 `-run=` commandlet 烟雾验证并记录 discoverability 证据
  - 在实现完成后，必须用真实 commandlet 调用链跑至少一条 `-run=` 烟雾验证，确认 `AngelscriptEditor` 模块内的 commandlet 能被发现、进入 `Main()`、输出扫描摘要并返回预期状态码，而不是只凭代码编译通过就假定入口可用。
  - 验证命令应基于 `AgentConfig.ini` 中的 `Paths.EngineRoot` 与 `Paths.ProjectFile` 组织，避免把本地绝对路径写死进计划；若需要写文档示例，也要以变量化/模板化方式表达。
  - 若烟雾验证暴露出 Editor 模块 commandlet 的加载限制，应在此阶段回收为实现缺陷（例如模块描述、加载时机或命令名映射），而不是把问题降级成“已知限制”直接放过。
- [ ] **P5.3** 📦 Git 提交：`[Editor/Test] Test: smoke-validate blueprint impact commandlet invocation`

## 验收标准

1. `AngelscriptEditor` 模块内存在一套独立的 Blueprint impact scan commandlet 实现，且 commandlet 入口只负责参数解析、调用扫描服务、输出摘要和返回状态码，没有复制 `ClassReloadHelper` 的依赖分析逻辑。
2. `ClassReloadHelper` 与 commandlet 共享同一套 Blueprint 依赖识别能力，至少覆盖脚本父类继承命中、节点/变量/pin 类型依赖命中，以及对象引用替换扫描命中等当前已有生产语义。
3. commandlet 至少支持全量扫描和显式 changed script 过滤两条路径，并能稳定输出受影响 Blueprint 资产及命中原因。
4. `Plugins/Angelscript/Source/AngelscriptEditor/Private/Tests/` 下存在 `Angelscript.Editor.BlueprintImpact.*` 自动化测试，覆盖参数/请求构造、资产枚举过滤、依赖匹配等 commandlet 相关核心功能。
5. `Plugins/Angelscript/Source/AngelscriptTest/Blueprint/` 下存在真实脚本父类 → Blueprint 子类 impact 场景回归，证明 scanner 对真实 Blueprint 资产关系成立，而不是只对伪造夹具成立。
6. `Documents/Guides/Test.md`、`Documents/Guides/TestCatalog.md` 与 `Documents/Tools/Tool.md` 已同步记录新增测试入口和 commandlet 使用方式，后续维护者无需再次全仓搜索才能复现。
7. 实现完成后已存在一条真实 `-run=` commandlet 烟雾验证记录，证明 `AngelscriptEditor` 模块内的 commandlet 在 commandlet 模式下可发现、可执行，而不是只在源码层面存在。
8. 资产枚举相关验证中至少有一条基于 disk-backed、`AssetRegistry` 可见 Blueprint 资产的回归，证明项目资产扫描链路已被覆盖，而不是只依赖 transient Blueprint 场景外推。

## 风险与注意事项

### 风险

1. **`ClassReloadHelper` 现有扫描逻辑偏向“已加载 Blueprint”而非“项目全资产扫描”**
   - 风险：热重载路径里使用 `TObjectIterator<UBlueprint>` 时默认关注已加载对象，而 commandlet 需要覆盖项目资产；如果不做资产枚举与按需加载分层，结果可能漏掉未加载 Blueprint。
   - 应对：commandlet 使用 `AssetRegistry` 先建立候选资产集合，再将需要深度分析的 Blueprint 载入并交给共享 scanner。

2. **“脚本变更 → 生成符号集合”在删除/重命名场景下可能不完整**
   - 风险：当前引擎 API 更容易解析“当前还存在的脚本文件和生成符号”，对已删除脚本的离线映射并不天然完备。
   - 应对：首批先把 changed script 输入限定为当前可解析的 relative path / active module 语义；删除脚本的深度历史映射如有需要，再拆 sibling plan。

3. **Blueprint 依赖判断若直接扩成“所有符号类型一次到位”，首批复杂度会过高**
   - 风险：class、struct、delegate、enum 四类符号的 Blueprint 影响语义并不完全等价，如果第一批要求全部做到同样强度，容易把计划拖成大而全工程。
   - 应对：优先锁住 class 继承与当前 `ClassReloadHelper` 已经成熟覆盖到的 struct/delegate 路径；enum 视现有生产语义决定首批是强识别还是保守提示。

4. **commandlet 放在 Editor 模块会与现有 Runtime commandlet 放置习惯不同**
   - 风险：仓内已有 commandlet 都在 Runtime 模块，后续实施者可能本能地继续往 Runtime 放，导致把 `AssetRegistry` / `Kismet2` / Editor-only 依赖错误地下沉进 Runtime。
   - 应对：本计划已明确 commandlet 属于 Editor 模块；若实现时仍想放 Runtime，必须先证明可以不引入任何 Editor 依赖，否则视为偏离目标；同时必须用真实 `-run=` 烟雾验证确认 Editor 模块入口在 commandlet 模式下可发现。

### 已知行为变化

1. **新增 commandlet 只做扫描，不自动重编译或保存 Blueprint**
   - 首批实现完成后，用户运行 commandlet 得到的是“受影响资产列表 + 命中原因”，而不是自动修复结果；这是有意限制范围，避免把扫描工具和批量修改工具混为一体。
2. **全量扫描模式可能比带变更过滤模式更慢**
   - 由于首批优先保证正确性，全量模式允许走“AssetRegistry 枚举 + 按需加载候选 Blueprint”的较慢路径；性能优化不阻塞首批功能落地。
