# Agents_ZH.md

此文档需要同步到 AGENTS.md

## 目的

- 本文件用于指导在 `AngelscriptProject` 中工作的 AI Agent。
- 当前第一目标不是继续扩展一个普通游戏工程，而是把 `Plugins/Angelscript` 整理、验证并沉淀为可独立使用的插件版本 AS 插件。
- 当前仓库是插件开发与验证的承载工程；真正的主产物是 `Angelscript` 插件本身。

## 当前项目定位

- `Plugins/Angelscript/` 是核心工作区，绝大多数实现、修复、清理和测试都应优先落在这里
- `Source/AngelscriptProject/` 仅保留宿主工程必须的最小内容，除非任务明确需要，不要把插件逻辑塞回项目模块

## 关键路径

- `Plugins/Angelscript/Source/AngelscriptRuntime/`：运行时模块，插件核心能力优先落在这里。
- `Plugins/Angelscript/Source/AngelscriptEditor/`：编辑器相关支持。
- `Plugins/Angelscript/Source/AngelscriptTest/`：插件测试与验证。
- `Documents/Guides/`：构建、测试、查询指南。
- `Documents/Rules/`：Git 提交等规则文档。
- `Documents/Plans/`：多阶段任务的计划文档。
- `Documents/Plans/Archives/`：已完成或已关闭 Plan 的归档目录与摘要。
- `Tools/`：本地辅助脚本。

## 外部参考仓库

- 外部参考仓库不属于当前项目提交内容，只用于对照、迁移分析和架构参考。
- 本节只保留索引信息；具体说明、用途边界、优先级判断统一维护在 `Reference/README.md`。

| 名称 | 入口与说明 |
| --- | --- |
| AngelScript v2.38.0 | 使用 `Tools\PullReference.bat angelscript` 默认拉取到 `Reference\angelscript-v2.38.0`；详情见 `Reference/README.md` |
| Hazelight Angelscript | 通过 `AgentConfig.ini` 的 `References.HazelightAngelscriptEngineRoot` 获取；详情见 `Reference/README.md` |
| UnrealCSharp | 使用 `Tools\PullReference.bat unrealcsharp` 默认拉取到 `Reference\UnrealCSharp`；详情见 `Reference/README.md` |

- 后续新增参考仓库时，优先先更新 `Reference/README.md`，再回到本表补索引。

## 本地配置

- 项目根目录的 `AgentConfig.ini` 存放本机引擎路径等配置，已被 `.gitignore` 忽略。
- 首次使用运行 `Tools\GenerateAgentConfigTemplate.bat` 生成模板，再填入本机路径。
- 构建、测试命令中的引擎路径统一从 `AgentConfig.ini` 的 `Paths.EngineRoot` 获取。

## 构建与验证原则

- 构建说明统一参考 `Documents/Guides/Build.md`。
- 测试说明统一参考 `Documents/Guides/Test.md`。
- 若文档与当前插件化目标不一致，应先更新文档，再继续扩展实现。

## 文档维护原则

- 当插件边界、模块职责、构建方式、测试入口发生变化时，相关文档要同步更新。
- 中文说明优先更新到 `Agents_ZH.md` 或对应中文指南，避免只更新英文版。
- 若某个旧工程信息仍然重要，应总结为迁移规则或结构说明，而不是保留为零散背景备注。
- 如果新增了新的外部参考仓库或本机参考路径，应在本文件中补充"用途 + 路径 + 优先级"，避免后续检索成本持续上升。

## Git 与提交

- Git 提交格式与示例统一参考 `Documents/Rules/GitCommitRule.md`。
- 本仓库的 GitHub 标准远端为 `origin -> git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git`。
- 默认发布分支为 `main`；如果本地仓库仍停留在 `master`，首次推送前先创建或切换到 `main`。
- 首次配置 GitHub 远端时，优先使用 `git remote add origin git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git`，然后执行 `git push -u origin main` 建立 upstream 跟踪关系。
- 如果 `origin` 已存在但指向了其他仓库，应使用 `git remote set-url origin git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git` 更新地址，而不是再添加一个重复远端。
- 除非用户明确要求，否则不要对 `main` 执行 force push。

## 计划与 TODO

- 需要多阶段推进的任务，在 `Documents/Plans/` 下创建 Plan 文档，编写规则见 `Documents/Plans/Plan.md`。
- 已完成或已关闭的 Plan 从 `Documents/Plans/` 移入 `Documents/Plans/Archives/`；归档时必须补齐归档状态、归档日期、完成判断和结果摘要，并同步更新索引文档。
- TODO 应按"插件目标"拆解，避免把旧工程遗留问题混成一个大任务。
- 涉及重命名、模块迁移、对外 API 调整时，要同步梳理受影响文件和文档。

## 文档导航

| 文档 | 用途 |
| --- | --- |
| `AGENTS.md` | 英文版总纲 |
| `Reference/README.md` | 外部参考仓库索引与详细说明 |
| `Documents/Guides/Build.md` | 构建与命令执行指南 |
| `Documents/Guides/Test.md` | 测试指南 |
| `Documents/Guides/UE_Search_Guide.md` | UE 知识查询指南 |
| `Documents/Rules/GitCommitRule.md` | 英文提交规范 |
| `Documents/Plans/Plan.md` | Plan 文档编写规则 |
| `Documents/Plans/Archives/README.md` | 已归档 Plan 清单与摘要 |
| `Documents/Tools/Tool.md` | 内部工具说明 |
