# Angelscript 插件工程硬化补全计划

## 背景与目标

当前仓库已经具备较强的插件研发底座：`Plugins/Angelscript` 的 `Runtime / Editor / Test` 三模块边界清晰，`Documents/Guides/Build.md`、`Documents/Guides/Test.md` 与 `Tools/RunTests.ps1` 已经把本地构建、自动化测试和分层回归入口整理出来，`Script/Example_Actor.as` 与 `Script/Tests/` 也证明仓库并非完全没有可消费脚本语料。

但从“工程硬化 / hardened / production-grade 插件工程”的口径看，当前仓库仍缺一条对外可交付的主线：顶层 `README.md` 仍是空内容，`Plugins/Angelscript/Angelscript.uplugin` 的 `DocsURL` / `MarketplaceURL` / `SupportURL` 为空，仓库内看不到 `.github/workflows/` 级 CI 守门，也还没有把兼容矩阵、发布流程、安全披露、贡献入口这些对外契约收口成可复用基线。

本计划的目标是把这些“工程硬化缺口”收敛为一条独立可执行路线，让当前仓库从“适合继续研发的插件宿主工程”进一步升级为“具备外部消费入口、自动化守门、版本与支持契约、协作治理基础”的插件交付工程。

## 范围与边界

- **范围内**
  - `README.md`
  - `.github/`
  - `Plugins/Angelscript/Angelscript.uplugin`
  - `Tools/`
  - `Documents/Guides/`
  - `Documents/Plans/`
- **范围外**
  - `Plugins/Angelscript/Source/` 下新的语言能力、运行时功能增强或大规模架构重写
  - `AS 2.38` 迁移、`UInterface` 绑定、`UHT` 导出等已有 sibling plan 的功能主线
  - 以 Marketplace 上架为前提的商店物料制作、商业化文案与截图资产
  - 对 `325` 处全局状态依赖的一次性去全局化总修复（这属于 `Plan_FullDeGlobalization.md` 的范围）
- **执行边界**
  - 先补最小 hardening baseline，不把首轮计划膨胀成“所有工程治理一次做完”。
  - 每个阶段都必须能回答：补了哪个对外交付缺口、证据在哪、如何验证。
  - 对外契约必须以已验证事实为准；没有测试或构建证据支撑的 UE 版本 / 平台 / 发布承诺不得写进最终文档。

## 当前事实状态快照

1. `README.md` 当前仅有 `NULL`，仓库根入口不能承担插件对外说明职责。
2. `Plugins/Angelscript/Angelscript.uplugin` 当前仅声明 `Version=1`、`VersionName="1.0"`，且 `DocsURL`、`MarketplaceURL`、`SupportURL` 全为空。
3. 构建与测试入口已经存在：`Documents/Guides/Build.md`、`Documents/Guides/Test.md`、`Tools/RunTests.ps1`。
4. 当前仓库没有检测到 `.github/workflows/`，说明 PR / push 级别的自动化守门尚未进入仓库。
5. `Script/Example_Actor.as` 与 `Script/Tests/` 已经提供了样例与测试脚本，但仍未形成面向新使用者的仓库级安装 / 升级 / 故障排查入口。
6. `Documents/Guides/TechnicalDebtInventory.md` 已明确记录 `FAngelscriptEngine::Get()` 与 `CurrentWorldContext` 实时扫描共 `325` 处命中；这会影响“宣称 fully hardened”的强度，但不应阻塞本计划首轮把交付基线补起来。

## 影响范围

本计划预计涉及以下操作（按需组合）：

- **仓库入口补全**：把顶层仓库从“内部研发视角”改成“插件消费 / 贡献 / 排障视角”。
- **插件元数据补全**：补齐 `.uplugin` 的描述、支持入口与可分发所需元数据字段。
- **自动化入口新增**：新增 `BuildPlugin` / CI workflow / artifact 输出等自动化守门入口。
- **发布与兼容契约补全**：新增兼容矩阵、发布说明、变更日志与版本策略文档。
- **协作治理文件补全**：新增 `SECURITY.md`、`CONTRIBUTING.md`、Issue / PR 模板等对外协作入口。
- **索引与指南同步**：把新的 hardening 主线回写到索引与指南，避免它继续只是一次性聊天结论。

### 按目录分组的文件清单

仓库根与 GitHub 元数据（7 个）：
- `README.md` — 仓库入口补全
- `SECURITY.md` — 协作治理文件补全
- `CONTRIBUTING.md` — 协作治理文件补全
- `.github/workflows/plugin-ci.yml` — 自动化入口新增
- `.github/ISSUE_TEMPLATE/bug-report.yml` — 协作治理文件补全
- `.github/ISSUE_TEMPLATE/feature-request.yml` — 协作治理文件补全
- `.github/PULL_REQUEST_TEMPLATE.md` — 协作治理文件补全

插件描述与工具入口（3 个）：
- `Plugins/Angelscript/Angelscript.uplugin` — 插件元数据补全
- `Tools/BuildPlugin.ps1` — 自动化入口新增
- `Tools/RunTests.ps1` — 自动化入口增强（按需）

文档与索引（7 个）：
- `Documents/Guides/Build.md` — 构建 / 打包流程同步
- `Documents/Guides/Test.md` — CI / smoke / artifact 路径同步
- `Documents/Guides/Compatibility.md` — 发布与兼容契约补全（新建）
- `Documents/Guides/Release.md` — 发布流程补全（新建）
- `Documents/Guides/Licensing.md` — 许可与第三方来源说明（新建）
- `Documents/Plans/Plan_OpportunityIndex.md` — 索引同步
- `Documents/Plans/Plan_PluginEngineeringHardening.md` — 当前计划本体

## 分阶段执行计划

### Phase 0：冻结工程硬化口径与执行闸门

> 目标：先把“这次 hardening 到底补什么、不补什么、凭什么算完成”固定下来，避免后续任务不断漂移到功能研发主线。

- [ ] **P0.1** 固定“工程硬化 baseline”与 sibling plan 的边界
  - 这一步要明确首轮 hardening 只覆盖六条主线：仓库入口、插件元数据、BuildPlugin / CI 自动化、兼容矩阵、发布变更纪律、协作治理 / 安全入口。
  - 现有 `AS 2.38`、`UInterface`、`UHT`、去全局化、性能框架等 sibling plan 都只能作为依赖背景或风险来源，不能被重新揉进本计划。
  - 同时明确“能够在首轮完成的最小状态”：即使还没做到全平台 / 多 UE 版本全自动，也必须先形成一套可证明、可运行、可复用的 Windows 主线硬化基线。
- [ ] **P0.1** 📦 Git 提交：`[Docs/Hardening] Docs: freeze plugin engineering hardening scope`

- [ ] **P0.2** 冻结 hardening 成功口径与回归入口
  - 把“完成”定义成可验证事实：顶层 README 可用、`.uplugin` 对外字段补齐、`BuildPlugin` 能被文档化与脚本化、CI workflow 已进入仓库、兼容 / 发布 / 安全文档存在且互相链接。
  - 把回归入口固定为：`Tools/RunTests.ps1` 的 smoke 前缀、`Tools/BuildPlugin.ps1` 的包生成、仓库文档链接检查、`.uplugin` 元数据 sanity check。
  - 若某个阶段无法给出明确回归入口，只能说明该阶段定义仍然过宽，必须继续收窄。
- [ ] **P0.2** 📦 Git 提交：`[Docs/Hardening] Chore: define hardening gates and verification entrypoints`

### Phase 1：补齐对外消费入口与插件元数据

> 目标：先让新使用者进入仓库时能快速理解“这是什么、怎么装、怎么测、去哪里看支持边界”，而不是靠翻 `Documents/` 和计划文档猜。

- [ ] **P1.1** 重写仓库根 `README.md`，建立插件优先的入口文档
  - 当前根 README 为空，这会直接让“插件可复用交付”在第一步就失去入口；必须改成围绕 `Plugins/Angelscript` 的说明，而不是宿主工程说明。
  - README 至少要回答：插件定位、模块结构、依赖插件、如何生成 `AgentConfig.ini`、如何构建、如何跑 smoke 测试、哪里看兼容 / 发布 / 安全 / 贡献文档。
  - 这一步不要求把所有细节都塞进 README，而是要让 README 成为通往 `Build.md`、`Test.md`、`Compatibility.md`、`Release.md`、`SECURITY.md` 的主导航。
- [ ] **P1.1** 📦 Git 提交：`[Docs/Hardening] Docs: add plugin-centric repository entry README`

- [ ] **P1.2** 补齐 `Plugins/Angelscript/Angelscript.uplugin` 的对外契约字段
  - 当前 `DocsURL` / `MarketplaceURL` / `SupportURL` 为空，`VersionName` 也没有和未来发布纪律建立同步规则；这一步要先补齐“仓库可对外解释自己”的最低元数据。
  - 对于 `DocsURL` / `SupportURL`，只能指向仓库内已真实存在、且后续可稳定维护的公开入口；如果 Marketplace 尚未启动，就不要伪造商店 URL，而是明确留空的原因或延后策略。
  - 同时审计是否需要纳入 `SupportedTargetPlatforms`、更明确的 `Description` 与发布相关字段，但所有新声明都必须由本地打包 / 测试事实支撑。
- [ ] **P1.2** 📦 Git 提交：`[Build/Hardening] Chore: complete plugin descriptor support metadata`

- [ ] **P1.3** 新建兼容与发布基础文档骨架
  - 新增 `Documents/Guides/Compatibility.md`，明确当前已验证的 UE 版本、平台、依赖插件、已知限制与“不宣称支持”的组合。
  - 新增 `Documents/Guides/Release.md`，明确 source 分发 / package 分发的最小流程、版本同步点和发布前回归清单。
  - 这一步先建立骨架与事实边界，不要求首轮就覆盖多平台大矩阵，但必须消灭“版本支持只存在于口头印象中”的状态。
- [ ] **P1.3** 📦 Git 提交：`[Docs/Hardening] Docs: add compatibility and release guide skeletons`

### Phase 2：建立 BuildPlugin 与 CI 自动化守门

> 目标：把“能在本机手动运行”升级为“有标准化打包入口、且提交级别有自动化守门”，这是从研发工程走向 hardened 工程的核心闸门。

- [ ] **P2.1** 新增 `Tools/BuildPlugin.ps1`，固化插件打包入口
  - 当前仓库已有构建与测试指南，但缺一个以插件分发为中心的 `BuildPlugin` 包装入口；这会让“可打包交付”长期停留在手工命令层面。
  - 脚本应复用 `AgentConfig.ini`、统一参数回退与 `Build.md` 的 PowerShell 风格，显式调用 `RunUAT BuildPlugin`，输出稳定的 package 目录与日志位置。
  - 首轮目标是让单机打包路径可复用、可记录、可供 CI 复用，而不是一开始就覆盖所有高级发布逻辑。
- [ ] **P2.1** 📦 Git 提交：`[Build/Hardening] Feat: add reusable BuildPlugin packaging entrypoint`

- [ ] **P2.2** 把 smoke 构建 / 测试 / 打包收进 `.github/workflows/plugin-ci.yml`
  - 当前仓库没有 `.github/workflows/`，因此首轮至少要把 workflow 文件放入仓库，并明确其 runner 约束、输入配置与 artifact 输出。
  - Unreal 工程通常无法直接依赖 GitHub hosted runner 的裸环境；这一步必须先写清楚是基于 self-hosted Windows runner 还是其他受控环境，不得写出无法执行的虚假 workflow。
  - CI 首轮建议只守住 `BuildProject` / `RunTests smoke` / `BuildPlugin` 三条主线，并把日志与报告目录产物化；不要在第一版 workflow 里塞进过多长时全量回归。
- [ ] **P2.2** 📦 Git 提交：`[CI/Hardening] Feat: add plugin build smoke and packaging workflow`

- [ ] **P2.3** 补齐 CI runner 合约与 artifact 留痕约定
  - workflow 进仓后，还要同步文档说明 runner 需要哪些前置条件、如何提供 `AgentConfig.ini` / 引擎路径、哪些 artifact 会被上传、失败时如何排查。
  - `Documents/Guides/Build.md`、`Documents/Guides/Test.md` 需要同步加入“本地执行 vs CI 执行”的分叉说明，避免把 agent 文档和 GitHub runner 文档混成一套模糊流程。
  - 这一步的目标不是让 CI“看起来存在”，而是确保后来者真的知道怎样维护这条 pipeline。
- [ ] **P2.3** 📦 Git 提交：`[CI/Hardening] Docs: document runner contract and artifact retention`

### Phase 3：补齐版本、发布与兼容性纪律

> 目标：让插件的版本、变更、支持边界与发布步骤从“隐含知识”变成仓库内可追踪契约。

- [ ] **P3.1** 建立 `CHANGELOG.md` 与版本同步规则
  - 当前 `.uplugin` 只有 `1.0`，没有对应的 changelog 与发布节奏说明；这会让后续任何 breaking change、兼容变更或测试基线变化都缺少对外叙述位置。
  - 这一步要固定：哪些改动需要变更 `Version` / `VersionName`，changelog 记录哪些类型的变化，tag / release note 与 `.uplugin` 的关系是什么。
  - 首轮不要求自动 release drafter，但必须把“怎么记录一次正式变化”写成仓库纪律。
- [ ] **P3.1** 📦 Git 提交：`[Release/Hardening] Docs: establish changelog and versioning policy`

- [ ] **P3.2** 新增发布准备入口，打通版本同步与 package 产物
  - 在 `BuildPlugin` 入口与 `Release.md` 存在后，需要再补一层“发布准备”脚本或流程，把版本号、changelog、package 目录、回归记录串起来。
  - 这一步可以先做成脚本化 checklist，而不是直接接 GitHub Releases 自动发布；重点是防止每次发布都重新人工拼流程。
  - 若当前阶段还不适合接远端 release API，也必须先把本地 / CI 产物与 tag 前检查清单固化下来。
- [ ] **P3.2** 📦 Git 提交：`[Release/Hardening] Feat: add release preparation flow`

- [ ] **P3.3** 把兼容矩阵从骨架升级为“已验证事实表”
  - `Compatibility.md` 首轮可以只有骨架，但在这一阶段必须补齐至少一轮真实验证结果：当前验证的 UE 版本、Windows 打包状态、依赖插件组合、已知不支持项。
  - 不能把“未来想支持”写成“当前支持”，也不能把只在宿主工程内编译过的结论直接上升为 package 级支持声明。
  - 若多版本矩阵暂时做不到自动化，也要先明确“当前只验证了哪一版 / 哪一平台”，把未知状态显式标成未知而不是留白。
- [ ] **P3.3** 📦 Git 提交：`[Build/Hardening] Docs: publish verified compatibility matrix`

### Phase 4：补齐安全与协作治理入口

> 目标：把仓库从“只有作者知道怎么协作”推进到“外部贡献者和使用者有标准入口可循”。

- [ ] **P4.1** 新增 `SECURITY.md`，明确漏洞披露与支持版本口径
  - 如果外部用户找不到漏洞报告路径，那么从 security hardening 视角看，这个仓库始终是不完整的。
  - `SECURITY.md` 至少要说明：如何私下报告安全问题、哪些版本 / 分支接收安全修复、哪些路径不适合公开 issue 披露。
  - 同时把该文档与 `README.md`、`Release.md`、`Licensing.md` 互相链接，避免安全信息继续埋在零散文档里。
- [ ] **P4.1** 📦 Git 提交：`[Security/Hardening] Docs: add security disclosure and support policy`

- [ ] **P4.2** 新增 `CONTRIBUTING.md` 与 Issue / PR 模板
  - 当前仓库虽有 commit 规则和计划规则，但缺少面向外部贡献者的入门说明：怎么准备环境、怎么选 Plan、怎么跑最小回归、PR 需要附什么证据。
  - 这一步应新增 `CONTRIBUTING.md`，并在 `.github/ISSUE_TEMPLATE/`、`.github/PULL_REQUEST_TEMPLATE.md` 中固定 bug / feature / PR 所需上下文，减少后续 triage 成本。
  - 内容应与 `Documents/Rules/GitCommitRule*.md`、`Build.md`、`Test.md` 保持一致，避免出现“模板要求”和仓库实际流程相互打架。
- [ ] **P4.2** 📦 Git 提交：`[Docs/Hardening] Docs: add contributor and triage templates`

- [ ] **P4.3** 补齐许可与第三方来源说明
  - 当前插件级 `LICENSE.md` 已存在，但仓库根没有一个面向 GitHub 访问者的“许可怎么理解、ThirdParty 从哪来、哪些部分受 UE EULA 约束”的入口。
  - 首轮不应为了追求 GitHub 识别而草率伪造单一顶层许可证，而应新增 `Documents/Guides/Licensing.md`，把插件代码、ThirdParty AngelScript、宿主工程 / UE EULA 的边界写清楚。
  - 这样可以减少后续外部使用者对仓库许可口径的误读，也为未来若要补顶层 `LICENSE` 留出清晰迁移路径。
- [ ] **P4.3** 📦 Git 提交：`[Docs/Hardening] Docs: clarify licensing and third-party provenance`

### Phase 5：回归收口、证据留痕与索引同步

> 目标：确保 hardening 不是写了一堆文档和 workflow 就算完成，而是留下真实可复用的验证记录，并把状态同步到仓库导航。

- [ ] **P5.1** 执行 hardening baseline 回归并固化结果
  - 至少完成一轮“README / 文档入口检查 + `.uplugin` 元数据检查 + `Tools/RunTests.ps1` smoke + `Tools/BuildPlugin.ps1` 打包 + workflow 语法 / 约束复核”的组合回归。
  - 对失败项必须区分：是本计划引入的新问题，还是仓库既有阻塞项；不能让“已有问题”重新把 hardening 结果描述成一团模糊状态。
  - 回归结果要回写到 `Release.md`、必要的 guide 文档和本计划中，而不是只存在于一次性日志输出。
- [ ] **P5.1** 📦 Git 提交：`[Test/Hardening] Test: validate plugin hardening baseline end to end`

- [ ] **P5.2** 同步 `Plan_OpportunityIndex` 与后续执行入口
  - 本计划一旦创建并开始执行，就必须把它挂接回索引，避免“工程硬化”继续只是聊天里发现的 gap，而不是仓库内可追踪主线。
  - 执行收口时，需要明确：哪些条目已经完成、哪些仍待继续、哪些高阶目标（如多 UE 版本矩阵、自动 release 发布）被刻意延后。
  - 最终索引需要能回答：为什么先补 hardening baseline、它与 `Plan_FullDeGlobalization.md`、`Plan_TestCoverageExpansion.md`、`Plan_AngelscriptResearchRoadmap.md` 是什么关系。
- [ ] **P5.2** 📦 Git 提交：`[Docs/Roadmap] Chore: sync plugin hardening plan status and next actions`

## 阶段依赖关系（执行顺序）

```text
Phase 0（硬化边界 / 验证闸门）
  -> Phase 1（仓库入口 / 元数据）
    -> Phase 2（BuildPlugin / CI）
      -> Phase 3（版本 / 发布 / 兼容矩阵）
        -> Phase 4（安全 / 贡献 / 许可）
          -> Phase 5（回归收口 / 索引同步）
```

补充依赖：

- `P2.2` 依赖 `P1.1` 与 `P1.2` 至少完成最小入口与元数据补全，否则 workflow 无法指向稳定文档与产物语义。
- `P3.2` 依赖 `P2.1` 的 `BuildPlugin` 包装入口已稳定存在，否则 release 准备流程没有统一打包源。
- `P3.3` 依赖 `P2.1/P2.2` 至少完成一轮可留痕验证，否则兼容矩阵只能继续停留在口头印象。
- `P4.1/P4.2/P4.3` 可与 `P3.1` 并行推进，但最终文档链接关系必须在 `P5.1` 前收口。

## 验收标准

1. 仓库根 README 不再为空，且能把新使用者正确导航到构建、测试、兼容、发布、安全与贡献入口。
2. `Plugins/Angelscript/Angelscript.uplugin` 的对外支持字段不再保持无解释空白；任何仍为空的字段都有明确原因与后续策略。
3. 仓库内存在可复用的 `BuildPlugin` 打包入口，并已被文档和至少一轮验证结果引用。
4. `.github/workflows/` 中已有可执行的 plugin CI workflow，且其 runner 合约、artifact 约定、失败排查路径已文档化。
5. `Compatibility.md`、`Release.md`、`SECURITY.md`、`CONTRIBUTING.md`、Issue / PR 模板已进仓，且内容与现有 `Build.md` / `Test.md` / commit 规则一致。
6. 仓库内存在一轮 hardening baseline 回归记录，能说明当前“已验证支持什么、尚未验证什么、哪些问题不阻塞本计划”。
7. `Plan_OpportunityIndex.md` 已同步纳入本计划，使后续执行者能从索引直接发现这条主线。

## 风险与注意事项

### 风险

1. **CI 环境假设过重**：Unreal 构建往往依赖本地引擎路径和 runner 前置，如果 workflow 直接按 GitHub hosted runner 假设编写，极易变成“进仓即坏”的假自动化。
   - **缓解**：首轮先锁定 self-hosted Windows runner 或等价受控环境，把 runner 合约写进文档，再扩展更复杂矩阵。

2. **支持矩阵膨胀风险**：一旦开始写兼容文档，很容易把“希望支持”误写成“已经支持”。
   - **缓解**：所有矩阵项都必须绑定到已有测试 / 构建 / 打包证据；没有证据的项统一标记为未知或待验证。

3. **文档与实现脱节风险**：如果只新增 README / SECURITY / CONTRIBUTING，却不把 `BuildPlugin`、CI、发布脚本真正落到仓库里，hardening 仍然会停留在表层治理。
   - **缓解**：坚持先有自动化入口，再写对应文档；每个文档阶段都绑定一个脚本或 workflow 落点。

4. **与功能主线互相抢优先级**：AS2.38、接口绑定、UHT 等功能计划体量大、吸引力高，容易重新挤掉 hardening 主线。
   - **缓解**：把本计划定位为“外部可交付基线”的独立闸门；凡涉及对外发布 / 兼容承诺的条目，优先按本计划收口。

### 已知行为变化

1. **根 README 视角会切换为插件优先**：后续仓库入口将不再以宿主工程视角展开，而是优先解释 `Plugins/Angelscript` 的可复用交付路径。
   - 影响文件：`README.md`

2. **`.uplugin` 元数据将不再长期保持空白字段**：`DocsURL` / `SupportURL` / 兼容相关字段会从“无解释留空”变成“已填值或有明确延后策略”的状态。
   - 影响文件：`Plugins/Angelscript/Angelscript.uplugin`

3. **构建与测试指南将新增 CI / package 视角**：现有 `Build.md`、`Test.md` 将不再只面向本地 agent 执行，还会包含 runner / artifact / package 的维护说明。
   - 影响文件：`Documents/Guides/Build.md`、`Documents/Guides/Test.md`
