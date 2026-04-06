# Script 示例恢复与扩展计划

## 背景与目标

### 背景

当前仓库已经具备插件级脚本运行时、编辑器与测试骨架，但 **Script 示例资产本身仍明显弱于 Hazelight 基线**：

- 当前仓库实际落盘的插件消费级脚本示例几乎只有 `Script/Example_Actor.as` 与少量 `Script/Tests/*.as`。
- 与之相对，`AgentConfig.ini` 中 `References.HazelightAngelscriptEngineRoot` 指向的 Hazelight 基线在 `Script-Examples/` 下提供了 `Examples/`、`EnhancedInputExamples/`、`GASExamples/`、`EditorExamples/` 四类共 26 个 `.as` 示例；但其中 `GASExamples` 与 `EditorExamples` 不应直接成为当前计划的必做范围。
- 当前仓库 `Plugins/Angelscript/Source/AngelscriptTest/Examples/` 已经存在大量 `AngelscriptScriptExample*Test.cpp`，并直接引用 `Example_Array.as`、`Example_Timers.as`、`Example_Widget_UMG.as` 等 Hazelight 风格示例文件名；但这些测试当前主要通过内联脚本文本编译，`Script/` 目录并没有对应的示例资产落盘。
- `AngelscriptScriptExampleTestSupport.cpp` 当前把示例源码当作测试内联数据，使用 `ScriptExamples/<ExampleFileName>` 作为虚拟文件名编译；这说明仓库里已经有“示例测试”概念，但 **还没有把示例脚本升级为正式交付资产与文档入口**。

当前插件相对 Hazelight 的差距盘点也已经把“独立脚本示例与上手资产”明确列为明显差距，见 `Documents/Plans/Plan_HazelightCapabilityGap.md`。

### 目标

本计划的目标不是做一轮泛化的 Hazelight 全量对齐，而是围绕 **Script 示例资产** 建立一条可执行、可验证、可扩展的主线：

1. **先补齐 Hazelight 原有 Script-Examples 的核心资产面**，让当前仓库具备与示例测试、文档入口、插件交付一致的真实 `.as` 文件集合。
2. **再在其基础上扩展当前项目更需要的示例主题**，突出本仓库已经具备或正在补齐的能力面，而不是只做一份静态镜像。
3. **把“示例脚本”从测试内联字符串提升为正式资产**，让示例目录、测试入口、文档说明与插件定位形成闭环。

额外约束：**本计划中的示例资产统一放在项目目录下**，即以仓库根 `Script/` 作为示例承载位置；不把这些 `.as` 示例转移到 `Plugins/Angelscript/` 内部目录，也不在宿主工程之外另建平行示例仓。后续所有目录设计、测试映射与文档入口都以“项目目录中的示例资产”为前提。

## 范围与边界

- **范围内**
  - 项目目录 `Script/` 下示例目录结构与 `.as` 示例资产
  - 与示例资产直接相关的最小测试支撑适配（仅在现有专门测试无法复用时）
  - 示例目录的 README / Guide / 索引挂接
  - 与示例直接相关的最小测试/文档同步
- **范围外**
  - 以补示例为借口顺手扩 Runtime 新功能
  - 全量 Hazelight parity 总盘点、引擎侧补丁、Loader/GAS/EnhancedInput 模块拆分本身
  - 把所有 `Script/Tests/*.as` 全部重构成示例；测试脚本与示例脚本仍需保持职责分离
  - 与示例无关的 README / CI / 发布工程硬化主线（这些由 `Plan_PluginEngineeringHardening.md` 承接）
- **执行边界**
  - 先走“**先补齐再扩展**”路线：优先恢复 Hazelight 对位示例，再追加本仓库专属扩展示例。
  - 示例资产统一留在项目目录 `Script/Examples/` 下，不把它们迁入 `Plugins/Angelscript/` 或其他非项目脚本目录。
  - 不要求首轮就覆盖 Hazelight 全部 26 个示例的行为深度一致，但必须先形成真实资产面、稳定目录与测试映射。
  - `Plugins/Angelscript/Source/AngelscriptTest/Examples/` 是专门测试层，不默认作为大批量改动面；只有当现有测试无法复用真实示例资产时，才允许做最小支撑层调整。
  - 每个扩展示例都必须回答“它展示了当前插件什么独特能力或当前缺口收口结果”，避免为凑数量而扩展。

## 当前事实状态快照

1. 当前仓库 `Script/` 下仅可直接命中 `Script/Example_Actor.as` 与 `Script/Tests/*.as`，尚无插件级 `Script-Examples` 目录。
2. 本地 Hazelight 基线 `References.HazelightAngelscriptEngineRoot/Script-Examples/` 下可直接命中 26 个 `.as` 示例，覆盖 `Examples/`、`GASExamples`、`EnhancedInputExamples/`、`EditorExamples/`；但本计划不要求把这些分类逐项原样照搬为当前仓库必做范围。
3. 当前仓库 `Plugins/Angelscript/Source/AngelscriptTest/Examples/` 已存在 20+ 个 `AngelscriptScriptExample*Test.cpp`，测试名与 Hazelight 示例文件名高度一致，例如：
   - `AngelscriptScriptExampleArrayTest.cpp` → `Example_Array.as`
   - `AngelscriptScriptExampleTimersTest.cpp` → `Example_Timers.as`
   - `AngelscriptScriptExampleWidgetUmgTest.cpp` → `Example_Widget_UMG.as`
4. 这些 Example 测试本质上属于专门测试层，当前计划不应默认把它们视为 20+ 个待批量修改文件；它们首先是现有验证入口，而不是示例资产主承载位置。
5. `Plugins/Angelscript/Source/AngelscriptTest/Examples/AngelscriptScriptExampleTestSupport.cpp` 当前仍以内联字符串为示例源码来源，并通过 `CompileAnnotatedModuleFromMemory()` 使用虚拟路径 `ScriptExamples/<ExampleFileName>` 编译。
6. `Documents/Plans/Plan_HazelightCapabilityGap.md` 已把“独立脚本示例与上手资产差距”列为 `P2.3` 明显差距，因此本计划应视为其后续执行面，而不是平行重复盘点。

## 影响范围

本计划涉及以下操作（按需组合）：

- **示例资产落盘**：把 Hazelight 对位示例与本仓库新增示例落为真实 `.as` 文件。
- **目录重组**：在 `Script/` 下建立稳定的示例分组目录与 README 入口，优先围绕 Core / EnhancedInput / Extended；不强制保留 GAS 或 Editor 示例分组。
- **测试适配（按需）**：仅在现有专门测试无法复用真实示例资产时，对示例测试支撑层做最小适配。
- **示例索引补全**：为示例目录补 README / Guide / 索引入口，说明每个示例展示什么、对应哪些测试。
- **扩展示例新增**：新增当前仓库专属的能力示例，如 `Subsystem`、`Networking`、`Console/CVar`、`Interface`、`BlueprintSubclass`。

### 按目录分组的文件清单

Script 示例资产（首批 26+ 个）：
- `Script/Examples/Core/Example_AccessSpecifiers.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Actor.as` — 从现有 `Script/Example_Actor.as` 迁移/复制为正式示例目录资产
- `Script/Examples/Core/Example_Array.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_BehaviorTreeNodes.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_CharacterInput.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_ConstructionScript.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Delegates.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Enum.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_FormatString.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Functions.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_FunctionSpecifiers.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Map.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Math.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_MixinMethods.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_MovingObject.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Overlaps.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_PropertySpecifiers.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Struct.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Timers.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/Core/Example_Widget_UMG.as` — 新建，对位 Hazelight 基础示例
- `Script/Examples/EnhancedInput/Example_EI_Component.as` — 新建，对位 Hazelight EnhancedInput 示例
- `Script/Examples/EnhancedInput/Example_EI_PlayerController.as` — 新建，对位 Hazelight EnhancedInput 示例
- `Script/Examples/Extended/Example_SubsystemLifecycle.as` — 新建，当前仓库专属扩展示例
- `Script/Examples/Extended/Example_NetworkReplication.as` — 新建，当前仓库专属扩展示例
- `Script/Examples/Extended/Example_ConsoleWorkflow.as` — 新建，当前仓库专属扩展示例
- `Script/Examples/Extended/Example_InterfaceDispatch.as` — 新建，当前仓库专属扩展示例
- `Script/Examples/Extended/Example_BlueprintSubclass.as` — 新建，当前仓库专属扩展示例
- `Script/Examples/README.md` — 新建，示例总入口与分类说明

示例测试支撑（仅在确有必要时）：
- `Plugins/Angelscript/Source/AngelscriptTest/Examples/AngelscriptScriptExampleTestSupport.h` — 按需修改，仅在现有专门测试无法直接复用真实示例资产时增加最小接口
- `Plugins/Angelscript/Source/AngelscriptTest/Examples/AngelscriptScriptExampleTestSupport.cpp` — 按需修改，仅在需要读取项目目录示例文件时做最小支撑层适配

文档与索引：
- `Documents/Guides/TestCatalog.md` — 修改，新增/更新 ScriptExamples 条目与示例目录说明
- `Documents/Guides/Build.md` 或 `Documents/Guides/Test.md` — 修改，按需补充示例运行/验证入口
- `Documents/Plans/Plan_HazelightCapabilityGap.md` — 修改，按需把“示例差距”挂接到本计划
- `Documents/Plans/Plan_OpportunityIndex.md` — 修改，登记本计划入口与状态
- `Documents/Plans/Plan_ScriptExamplesExpansion.md` — 当前计划本体

## 分阶段执行计划

### Phase 0：冻结示例资产策略与目录规则

> 目标：先固定“示例资产到底以什么目录、什么命名、什么测试映射为准”，避免后续一边补文件一边改约定。

- [ ] **P0.1** 固定 Script 示例的 source-of-truth 语义
  - 当前 Example 测试主要以内联字符串作为示例源码，真实 `.as` 资产并不是测试或文档的单一真相来源；这会导致“测试知道示例存在，但仓库用户找不到示例文件”。
  - 这一步要明确：后续示例脚本的单一真相来源应当是 `Script/Examples/**.as` 真实文件，而不是 `AngelscriptScriptExample*Test.cpp` 中的内联字符串。
  - 同时定义过渡策略：为避免一次性改动过大，可允许测试短期保留 inline fallback，但最终目标必须是 file-backed example first。
- [ ] **P0.1** 📦 Git 提交：`[Docs/Examples] Docs: freeze script example source-of-truth policy`

- [ ] **P0.2** 固定示例目录、命名与测试映射规则
  - 这一步要明确 `Core / EnhancedInput / Extended` 三类目录的职责，并在规则中显式记录 `GASExamples` / `EditorExamples` 为当前排除项，避免把当前仓库自定义扩展示例重新混回 Hazelight 对位目录。
  - 文件命名保持 `Example_*.as` 风格，以便复用现有 Example 测试命名与 Hazelight 迁移语义。
  - 同时规定：每个新示例必须有对应的 Example 测试、README 条目或索引说明，至少满足三者中的两项，不能只落脚本文件不挂接验证入口。
- [ ] **P0.2** 📦 Git 提交：`[Docs/Examples] Docs: define script examples layout and naming rules`

### Phase 1：恢复 Hazelight 对位示例资产

> 目标：先把 Hazelight 原始 Script-Examples 的核心资产面补回当前仓库，消除“测试名已经存在但真实示例文件不存在”的状态。

- [ ] **P1.1** 盘点 Hazelight 26 个示例与当前仓库现状，冻结首批恢复矩阵
  - 先建立一个对位矩阵，列出 Hazelight 26 个示例、当前仓库是否已有真实 `.as` 文件、是否已有 Example 测试、是否需要依赖额外插件。
  - 这一步必须显式区分“首批就恢复”和“明确排除/后移”的项，避免后续反复争论是否必须一次补全全部示例。
  - 建议首批只恢复 `Core Examples` + `EnhancedInputExamples`；`GASExamples` 与 `EditorExamples` 在当前计划中显式排除，不作为必做项。
- [ ] **P1.1** 📦 Git 提交：`[Docs/Examples] Docs: freeze Hazelight script example restore matrix`

- [ ] **P1.2** 落盘 Core / EnhancedInput 对位示例
  - 按冻结矩阵把 Hazelight `Examples/`、`EnhancedInputExamples/` 中已选定的示例落成真实 `.as` 文件。
  - 对已经在当前仓库中存在真实资产的 `Script/Example_Actor.as`，应决定是迁移到新目录、保留兼容副本，还是通过脚本根别名兼容旧路径；不能长期同时保留多个不受控源。
  - 对 `EnhancedInput` 示例要显式注明依赖边界，避免把依赖插件示例当成通用 runtime 示例。
- [ ] **P1.2** 📦 Git 提交：`[Script/Examples] Feat: restore core and input script examples`

- [ ] **P1.3** 固定当前不纳入恢复范围的 Hazelight 示例类别
  - `GASExamples` 当前还不支持，不能继续作为本计划的恢复对象；应在恢复矩阵中显式标为“当前排除项”。
  - `EditorExamples` 当前也不作为必做范围，避免把 editor-only 示例重新带回计划主线。
  - 这一步的目标是减少歧义，而不是为这些类别创建新的半成品占位资产。
- [ ] **P1.3** 📦 Git 提交：`[Docs/Examples] Docs: mark excluded Hazelight example categories`

### Phase 2：评估是否需要最小测试支撑适配

> 目标：优先保持现有专门测试稳定，仅在真实示例资产引入后无法复用当前验证入口时，再做最小支撑层适配。

- [ ] **P2.1** 评估现有专门测试是否可直接复用真实示例资产
  - 当前 `AngelscriptScriptExample*Test.cpp` 是专门测试，不应先入为主地视作 20+ 个必改文件；这一步要先确认：在示例资产落盘后，它们是否已经足以继续承担 compile regression。
  - 若现有测试只需继续作为“专门测试”，而不需要直接读取真实 `.as` 文件，则本计划应保持它们不动，并把“示例资产验证”更多收敛为文档化、目录化和少量 smoke 验证问题。
  - 只有在评估后确认“真实示例资产无法被现有测试稳定覆盖”时，才进入后续支撑层改动。
- [ ] **P2.1** 📦 Git 提交：`[Docs/Examples] Docs: assess whether script example tests need adaptation`

- [ ] **P2.2** 如有必要，仅修改 Example 测试支撑层
  - 允许修改面应先收缩到 `AngelscriptScriptExampleTestSupport.h/.cpp`，避免把 20+ 个 Example 测试文件全部卷入迁移。
  - 支撑层若需要读取 `Script/Examples/**`，应统一处理文件发现、文本读取和依赖拼接，不把这些细节散落到单个测试文件。
  - 若评估结果证明现有测试无需适配，则本任务应显式关闭，并在计划 closeout 里记录“测试层保持不动”的原因。
- [ ] **P2.2** 📦 Git 提交：`[AngelscriptTest] Feat: add minimal script example support adaptation if required`

- [ ] **P2.3** 固定示例资产的最小验证矩阵
  - 这一步的重点是定义“示例资产如何被验证”，而不是默认要求每个 Example 测试都改成 file-backed 模式。
  - Core / EnhancedInput / Extended 不同类别可以采用不同验证粒度：已有专门测试复用、单独 smoke、文档化前置条件。
  - 验证矩阵必须显式写出：哪些由现有专门测试覆盖、哪些只做 compile smoke、哪些只先落资产后续补验证。
- [ ] **P2.3** 📦 Git 提交：`[Docs/Examples] Docs: define minimal validation matrix for script assets`

### Phase 3：扩展当前仓库专属示例主题

> 目标：在 Hazelight 对位恢复完成后，补出当前仓库真正值得展示的示例能力面，而不是只做历史镜像。

- [ ] **P3.1** 新增 `Subsystem` / `Interface` / `BlueprintSubclass` 扩展示例
  - 这三类能力是当前仓库相对 Hazelight 更适合作为“插件差异化价值”展示的方向，尤其 `UINTERFACE` 支持已经在测试上形成反向优势。
  - 每个示例都要明确它回答什么问题，例如：`SubsystemLifecycle` 展示脚本子系统可用姿势，`InterfaceDispatch` 展示接口声明/实现/调用链，`BlueprintSubclass` 展示脚本类与 Blueprint 子类化关系。
  - 不要把这一步做成对学习测试源码的简单复制；示例应以“面向插件使用者”的可读性为第一原则。
- [ ] **P3.1** 📦 Git 提交：`[Script/Examples] Feat: add subsystem interface and blueprint extension examples`

- [ ] **P3.2** 新增 `Networking` / `ConsoleWorkflow` 扩展示例
  - 这一步要优先展示当前项目正在收口但对外仍不够可见的能力，如复制基础面、`FConsoleVariable` / `FConsoleCommand` 最小工作流。
  - `Networking` 示例必须显式受当前真实能力边界约束：如果 push-model 仍未完成，就不要把示例写成“已经全量等同 Hazelight”的口径；示例可以先聚焦 `bReplicates`、`RepNotify`、最小 RPC 或文档化限制。
  - `ConsoleWorkflow` 示例则应展示当前插件已稳定具备的 `FConsoleVariable` / 命名空间全局变量 / 最小命令注册路径，避免再次把整个 console 能力误写成全缺失。
- [ ] **P3.2** 📦 Git 提交：`[Script/Examples] Feat: add networking and console workflow examples`

### Phase 4：收口文档入口与交付可见性

> 目标：把示例从“仓库里有文件”提升为“用户能发现、理解并运行”的正式交付面。

- [ ] **P4.1** 建立 `Script/Examples/README.md` 与分类索引
  - README 至少要回答：示例目录如何分组、哪些是 Hazelight 对位示例、哪些是本仓库扩展示例、每个示例展示什么、依赖什么、对应哪个测试。
  - 对 EnhancedInput 示例，要在 README 中显式标明环境前置条件；同时对 `GASExamples` / `EditorExamples` 写清楚当前为何不纳入本计划。
  - 这一步也要定义新增示例时的索引维护规则，避免以后目录继续扩张但没有统一导航。
- [ ] **P4.1** 📦 Git 提交：`[Docs/Examples] Docs: add script examples index and usage guide`

- [ ] **P4.2** 同步 TestCatalog / Hazelight gap / OpportunityIndex
  - `TestCatalog` 需要能直接指向 ScriptExamples 的真实资产和对应测试，不再只体现测试名而没有资产入口。
  - `Plan_HazelightCapabilityGap.md` 中的“示例差距”应明确挂接本计划，避免示例问题继续留在总盘点里悬空。
  - `Plan_OpportunityIndex.md` 需要把本计划作为 active plan 记录下来，说明它与 `Plan_NetworkReplicationTests.md`、`Plan_GlobalVariableAndCVarParity.md`、`Plan_PluginEngineeringHardening.md` 的关系。
- [ ] **P4.2** 📦 Git 提交：`[Docs/Roadmap] Chore: register script examples expansion plan and docs`

- [ ] **P4.3** 执行示例专项回归并固化结果
  - 至少完成一轮示例 compile regression，覆盖恢复后的 Core Examples 与首批 Extended Examples。
  - 对失败项必须明确区分：是示例资产问题、测试支撑层问题、还是仓库既有能力边界未满足；不能把所有失败统一记成“示例还不稳定”。
  - 回归结果需要回写到本计划的 closeout 信息、必要的 guide 文档与相关 sibling plan，而不是只留在一次性日志里。
- [ ] **P4.3** 📦 Git 提交：`[AngelscriptTest] Test: validate script examples restore and extension baseline`

## 阶段依赖关系

```text
Phase 0（规则冻结）
  -> Phase 1（Hazelight 对位示例恢复）
    -> Phase 2（Example 测试切到真实资产）
      -> Phase 3（本仓库专属扩展示例）
        -> Phase 4（文档/索引/回归收口）
```

补充依赖：

- `P1.2` 依赖 `P0.1/P0.2` 已固定 source-of-truth 与目录规则，否则落盘的示例文件会反复改名或重排。
- `P2.2` 依赖 `P1.2` 至少完成首批 Core Examples 落盘，否则 file-backed test 切换没有稳定输入。
- `P3.2` 与 `Plan_NetworkReplicationTests.md`、`Plan_GlobalVariableAndCVarParity.md` 有边界依赖：示例只能展示当前已经稳定或已明确标注限制的能力。
- `P4.2` 依赖至少一轮 `P2/P3` 的验证结果，否则文档与索引只能继续停留在空壳说明。

## 验收标准

1. 当前仓库具备一套真实可见的 `Script/Examples/` 目录，而不再只有 `Script/Example_Actor.as` 单文件示例。
2. Hazelight 原始 Script-Examples 至少形成一份明确的恢复矩阵，并已有首批真实 `.as` 资产落盘。
3. 当前仓库已明确哪些示例由现有专门测试复用、哪些仅做 compile smoke、哪些属于当前排除项，而不是把 20+ 个 Example 测试默认拉进迁移。
4. 本仓库专属扩展示例至少覆盖 `Subsystem`、`Interface/BlueprintSubclass`、`Networking/ConsoleWorkflow` 中的一组或多组，并具备最小验证入口。
5. `Script/Examples/README.md`、`Documents/Guides/TestCatalog.md` 与 `Plan_OpportunityIndex.md` 可以直接把用户导航到示例目录、对应测试和计划状态。

## 风险与注意事项

### 风险

1. **把 Hazelight 示例原文直接镜像成最终交付**
   - 纯复制虽然最省事，但会让示例与当前插件的真实能力边界、命名规范和文档结构脱节。
   - **缓解**：先对位恢复，再明确哪些示例属于“兼容迁移”、哪些属于“当前仓库正式推荐入口”。

2. **把专门测试层误当成示例资产主改动面**
   - 当前 `Plugins/Angelscript/Source/AngelscriptTest/Examples/` 的职责是专项验证，不应因为要补示例资产就默认把 20+ 个测试文件都纳入迁移。
   - **缓解**：在 `P2.1` 先做复用性评估，只有确有必要时才改 `AngelscriptScriptExampleTestSupport.*` 这类最小支撑层。

3. **把扩展示例写成“功能承诺”而不是“能力展示”**
   - 特别是 `Networking`、`ConsoleWorkflow` 这类仍在收口的能力域，示例若写得过满，会反向制造错误预期。
   - **缓解**：示例 README 与测试都要显式标注已支持边界与当前限制。

4. **目录治理和资产恢复互相拖慢**
   - 如果一开始就要求目录、文档、测试、索引全部完美统一，计划会长期停留在规则讨论。
   - **缓解**：坚持 Phase 顺序，先恢复资产面，再逐步收口测试和文档。

### 已知行为变化

1. **示例资产会从“几乎不存在的项目目录文件”变成正式项目目录交付面**：后续 `Script/Examples/**` 会成为用户可见入口，但现有专门测试不一定随之大改。
   - 影响文件：`Script/Examples/**`、`Script/Examples/README.md`、按需影响 `Plugins/Angelscript/Source/AngelscriptTest/Examples/AngelscriptScriptExampleTestSupport.*`
2. **`Script/Example_Actor.as` 的路径语义会变化**：现有单文件示例需迁移、兼容或废弃其旧路径，不能长期让旧路径与新目录并存且都被视为真相来源。
   - 影响文件：`Script/Example_Actor.as`、`Script/Examples/Core/Example_Actor.as`、相关示例测试与文档入口
3. **部分示例会显式带环境前置条件或排除说明**：例如 EnhancedInput 示例不能默认被视作“无条件 compile smoke”，而 `GASExamples` / `EditorExamples` 需明确写成当前计划外项。
   - 影响文件：`Script/Examples/README.md`、相关 Example 测试、`Documents/Guides/TestCatalog.md`
