# Plan 归档目录

本目录存放已完成或已关闭的 Plan 文档。

- 活跃中的 Plan 继续保留在 `Documents/Plans/` 根目录。
- 已完成或已关闭的 Plan 统一移动到 `Documents/Plans/Archives/`。
- 归档前必须在文档顶部补齐 `归档状态`、`归档日期`、`完成判断` 与 `结果摘要`。
- 归档后必须同步更新 `Documents/Plans/Plan_OpportunityIndex.md` 以及相关导航/规则文档中的状态与路径。

## 当前已归档 Plan

| Plan | 归档日期 | 完成摘要 |
| --- | --- | --- |
| `Plan_AngelscriptTestExecutionInfrastructure.md` | 2026-04-04 | 完成了测试 group taxonomy、统一 runner、结构化摘要与执行文档收口，当前主干以 `Tools/RunTests.ps1` / `Tools/RunTestSuite.ps1` 作为正式入口。 |
| `Plan_BuildTestScriptStandardization.md` | 2026-04-04 | 完成了共享执行层、direct UBT 构建 runner、自动化测试 runner、UBT 进程查询与 tooling smoke tests，为后续构建/回归提供了统一脚本基础设施。 |
| `Plan_CallfuncDeadCodeCleanup.md` | 2026-04-04 | 移除了无效 `callfunc` `.lib` 引用，补充了旧汇编路径的废弃说明，并保留了后续 AS 2.38 合入时需要复查的替代链路。 |
| `Plan_TechnicalDebt.md` | 2026-04-04 | 完成了 Phase 0-6 的技术债收口，将高风险问题分流到 sibling plan，并同步了相关构建、测试与清单文档。 |
