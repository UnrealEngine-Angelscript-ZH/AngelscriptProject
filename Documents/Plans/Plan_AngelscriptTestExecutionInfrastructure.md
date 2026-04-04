# Angelscript 测试执行基础设施补强计划

## 背景与目标

当前仓库的测试文档已经比较完整，`Documents/Guides/Test.md` 和 `Documents/Guides/Build.md` 也把 `NullRHI`、`Gauntlet`、`-ReportExportPath`、`-ABSLOG`、`AgentConfig.ini` 的使用方式写得很清楚；但真正支撑稳定回归的“执行基础设施”仍然不够成型。

本次审计确认了几个操作面缺口：

1. `Config/DefaultEngine.ini` 当前没有 `[/Script/AutomationTest.AutomationTestSettings]` 组定义，意味着 `Group:AngelscriptSmoke`、`Group:AngelscriptFast` 这类稳定筛选入口并不存在。
2. `Tools/` 目录里没有面向测试执行的标准脚本；跑一轮自动化测试仍需要手动拼接 `UnrealEditor-Cmd.exe` 命令。
3. 虽然文档写了 `-ReportExportPath` 与 `-ABSLOG`，但仓内没有统一的包装层来保证日志/报告目录隔离、返回码提取和失败摘要输出。
4. 目前也没有最小 CI 外壳或“机器可消费的测试出口”，导致回归依赖手工观察日志。

这意味着测试体系虽然“能跑”，但还没有形成稳定、低心智负担、可批量复用的执行面。没有这层基础设施，即使后续补了更多测试，也很难把它们变成真正的工程化回归资产。

### 目标

1. 为 Angelscript 测试建立稳定的分组、命令包装和结果产出约定。
2. 降低 AI Agent / 本地开发者执行回归的成本，让常用测试入口不再依赖手拼命令。
3. 为后续 CI / Gauntlet outer shell / coverage 流程留出统一接入点。

## 范围与边界

- **范围内**
  - `Config/DefaultEngine.ini`
  - `Tools/` 下新增或整理的测试执行脚本
  - `Documents/Guides/Test.md`
  - `Documents/Guides/Build.md`（如需补运行入口说明）
  - 必要时新增简单的结果汇总脚本或批处理包装
- **范围外**
  - 新增具体业务测试逻辑
  - 完整企业级 CI 平台接入细节
  - 需要 GPU 的图形截图回归
- **边界约束**
  - 所有脚本必须从 `AgentConfig.ini` 读取 `EngineRoot` / `ProjectFile` / `DefaultTimeoutMs`，不写死本地路径。
  - 不用脚本去隐藏失败；退出码、失败日志和报告路径必须被显式暴露。
  - 组定义和脚本命名要围绕插件目标，而不是宿主项目历史习惯。

## 当前事实状态快照

1. `Config/DefaultEngine.ini` 已新增 `AutomationTestSettings` 基础分组，包含 `AngelscriptSmoke`、`AngelscriptNative`、`AngelscriptRuntimeUnit`、`AngelscriptFast`、`AngelscriptScenario`、`AngelscriptEditor`、`AngelscriptExamples`。
2. `Tools/RunAutomationTests.ps1` 与 `Tools/RunAutomationTests.bat` 已提供统一入口，可从 `AgentConfig.ini` 读取 `Paths.EngineRoot`、`Paths.ProjectFile` 与 `Test.DefaultTimeoutMs`。
3. `Tools/GetAutomationReportSummary.ps1` 已补齐默认日志/报告汇总，输出 `RunMetadata.json` 与 `Summary.json`，并在需要时提升最终退出码。
4. `Documents/Guides/Test.md` 已改写为仓库通用模板，补齐了 group taxonomy、runner 用法、最小 CI 接入建议与 runner/Gauntlet 边界。
5. `Documents/Tools/Tool.md` 已同步收录新的 runner 与 summary 工具入口。

## 分阶段执行计划

### Phase 1：固化测试分组 taxonomy

> 目标：先把常用回归入口命名固定下来，再谈脚本包装，否则脚本只会继续拼散乱前缀。

- [x] **P1.1** 在 `Config/DefaultEngine.ini` 新增 `AutomationTestSettings` 基础分组
  - 增加 `[/Script/AutomationTest.AutomationTestSettings]`。
  - 第一批分组至少覆盖：`AngelscriptSmoke`、`AngelscriptNative`、`AngelscriptRuntimeUnit`、`AngelscriptFast`、`AngelscriptScenario`、`AngelscriptEditor`、`AngelscriptExamples`。
  - 过滤条件要基于已经稳定存在的前缀，不要一开始就绑定那些还在调整中的命名。
  - 对 `Template/` 是否单列一组要明确表态；若目前仍属脚手架，应避免让它混进默认 smoke。
- [x] **P1.1** 📦 Git 提交：`[Test/Infra] Feat: add initial automation test groups`

- [x] **P1.2** 把“组定义与目录分层”的映射写回 `Documents/Guides/Test.md`
  - 明确每个 group 面向什么场景、主要包含哪些目录/前缀、何时适合本地快速跑、何时适合 CI。
  - 给出一张从 `Native / Runtime / Scenario / Editor / Examples` 到 group 的对照表，避免使用者看到 group 名却不知道桶里装了什么。
  - 同步把 `Test.md` 中那些硬编码到其他工程的示例路径收敛为 `AgentConfig.ini` 驱动的仓库通用模板，避免文档继续暗示 groups 或命令已经是现状事实。
- [x] **P1.2** 📦 Git 提交：`[Test/Infra] Docs: publish automation group taxonomy`

### Phase 2：提供标准测试执行脚本入口

> 目标：让最常见的跑法变成仓内可调用命令，而不是每次重新拼 `Start-Process`。

- [x] **P2.1** 在 `Tools/` 新增 PowerShell 主入口脚本
  - 新建例如 `Tools/RunAutomationTests.ps1`。
  - 脚本至少支持：按 test prefix 跑、按 group 跑、自定义 `-ABSLOG` / `-ReportExportPath`、透传额外命令行参数、使用 `DefaultTimeoutMs`。
  - 脚本要先读取 `AgentConfig.ini`，并在缺失配置时给出明确错误，而不是静默失败。
- [x] **P2.1** 📦 Git 提交：`[Test/Infra] Feat: add powershell automation test runner`

- [x] **P2.2** 为 PowerShell 入口增加轻量 bat 包装
  - 在 `Tools/` 下补一个 `.bat` 包装，方便 Windows 本地双击或简短调用。
  - `.bat` 只负责参数转发，不重复实现配置解析逻辑，避免两套行为漂移。
- [x] **P2.2** 📦 Git 提交：`[Test/Infra] Feat: add batch wrapper for automation runner`

- [x] **P2.3** 固定日志与报告目录策略
  - 在 runner 中明确默认输出目录，例如 `Saved/Automation/<Bucket>/<Timestamp>/`，并把 `ABSLOG` 与 `ReportExportPath` 一起落到该目录下。
  - 处理并行执行时的命名隔离，避免日志相互覆盖。
  - 输出总结时至少打印：调用的 group/prefix、日志路径、报告路径、编辑器退出码。
- [x] **P2.3** 📦 Git 提交：并入 `Tools/RunAutomationTests.ps1` / `Tools/GetAutomationReportSummary.ps1` 主入口提交

### Phase 3：补结果汇总与最小 CI 壳

> 目标：让脚本输出能被人和机器同时消费，为后续 GitHub Actions / 其他流水线留接口。

- [x] **P3.1** 增加最小结果汇总工具
  - 新增简单脚本或工具函数，从 `-ReportExportPath` 和日志里提取 pass/fail 数量、失败测试名、退出码。
  - 输出格式要足够稳定，便于 AI Agent 和后续 CI 使用，而不是只打一段长日志。
  - 如果 JSON 报告已足够稳定，就以 JSON 为主、日志为补充；不要反过来只靠解析自然语言日志。
- [x] **P3.1** 📦 Git 提交：并入 `Tools/GetAutomationReportSummary.ps1` 主入口提交

- [x] **P3.2** 给出最小 CI/outer shell 对接方案
  - 在 docs 中补一节“最小 CI 接入建议”，说明至少应如何调用 runner、读取退出码、存档日志/报告。
  - 这里不强制一次性提交完整工作流，但至少要让后续接 CI 的人不必从零设计目录和参数。
  - 如果仓库决定顺手补一个最小 GitHub Actions / Jenkins 示例，应保持为薄壳，只负责调用已有 runner。
- [x] **P3.2** 📦 Git 提交：并入 `Docs: define runner handoff and headless smoke guidance`

### Phase 4：补齐 Gauntlet outer shell 与执行手册

> 目标：让 command line runner 和 Gauntlet outer shell 形成分工，而不是彼此替代。

- [x] **P4.1** 在 `Documents/Guides/Test.md` 明确 runner 与 Gauntlet 的边界
  - 说明本地/CI 常规自动化优先走 runner；需要 editor boot、networking、outer shell 会话控制时再走 `Gauntlet`。
  - 给出最小命令模板，说明如何通过 runner 产出的 bucket 去喂给 `UE.EditorAutomation` 或 `UE.Networking`。
- [x] **P4.1** 📦 Git 提交：并入 `Docs: define runner handoff and headless smoke guidance`

## 验收标准

1. `Config/DefaultEngine.ini` 中存在一组可实际执行的 Angelscript 自动化测试分组。
2. `Tools/` 下存在统一的测试执行脚本入口，可从 `AgentConfig.ini` 读取配置并运行指定 bucket。
3. 默认运行会产出独立的日志和报告目录，并能明确给出退出码与失败摘要。
4. `Documents/Guides/Test.md` 能指导后来者使用 groups、runner、报告目录和最小 CI/Gauntlet 对接方式。

## 风险与注意事项

- 最容易出问题的是一开始把不稳定前缀写进 group，导致后续目录调整时筛选失效；分组必须优先选择稳定边界。
- Runner 不能变成“万能脚本”；只包装已有规范，不要在脚本里偷偷埋业务判断。
- 如果结果汇总只依赖日志文本，未来本地化语言或输出格式变化会把它打碎；优先利用结构化报告。
- `Template`、`Examples`、`Editor` 等目录的组归属要先和覆盖计划保持一致，避免 infra 先把错误结构固化下来。
