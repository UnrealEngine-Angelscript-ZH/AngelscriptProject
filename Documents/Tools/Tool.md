# Tools

## 工具列表

| 工具名 | 路径 | 用途 | 常用命令 | 输出 | 备注 |
| --- | --- | --- | --- | --- | --- |
| GenerateAgentConfigTemplate | `Tools\GenerateAgentConfigTemplate.bat` | 在项目根目录生成本机专用的 `AgentConfig.ini` 模板，供 AI Agent 和开发者读取引擎路径、项目路径、默认构建参数与测试超时。 | `Tools\GenerateAgentConfigTemplate.bat` | 生成 `AgentConfig.ini` | 如果目标文件已存在，默认不会覆盖。 |
| GenerateAgentConfigTemplate `--force` | `Tools\GenerateAgentConfigTemplate.bat` | 强制覆盖并重新生成 `AgentConfig.ini` 模板。 | `Tools\GenerateAgentConfigTemplate.bat --force` | 重新生成 `AgentConfig.ini` | 仅在确认需要覆盖本地配置时使用。 |
| PullReference `list` | `Tools\PullReference.bat` | 列出当前支持的外部参考仓库 key。 | `Tools\PullReference.bat list` | 输出可用 key 与说明 | 用于查看可拉取和不可拉取的参考源。 |
| PullReference `angelscript` | `Tools\PullReference.bat` | 通过对应 SSH 克隆或同步 AngelScript 上游参考仓库。 | `Tools\PullReference.bat angelscript` | 在 `Reference\angelscript-v2.38.0` 拉取或更新仓库 | 默认同步到当前项目的 `Reference\angelscript-v2.38.0`。 |
| PullReference `unrealcsharp` | `Tools\PullReference.bat` | 通过对应 SSH 克隆或同步 `UnrealCSharp` 参考仓库。 | `Tools\PullReference.bat unrealcsharp` | 在 `Reference\UnrealCSharp` 拉取或更新仓库 | 默认同步到当前项目的 `Reference\UnrealCSharp`。 |
| PullReference `<key> <TargetDir>` | `Tools\PullReference.bat` | 将指定参考仓库同步到自定义目录。 | `Tools\PullReference.bat angelscript "J:\UnrealEngine\AngelscriptProject\Reference\angelscript-v2.38.0"` | 在指定目录拉取或更新仓库 | 目标目录已存在但不是 Git 仓库时会直接失败。 |
| RunAutomationTests (PowerShell) | `Tools\RunAutomationTests.ps1` | 统一运行 Angelscript 自动化测试，按 group 或前缀启动编辑器回归。 | `Tools\RunAutomationTests.ps1 -Group AngelscriptSmoke` | `Saved\Automation\<Bucket>\<Timestamp>\` 下的日志、报告和 JSON 摘要 | 优先读取 `AgentConfig.ini`，缺配置会直接失败。 |
| RunAutomationTests (Batch) | `Tools\RunAutomationTests.bat` | 为 Windows 本地或 CI 提供简短入口，参数原样转发到 PowerShell runner。 | `Tools\RunAutomationTests.bat -Group AngelscriptFast` | 与 PowerShell runner 相同 | 只做参数转发，不重复配置解析逻辑。 |
| GetAutomationReportSummary | `Tools\GetAutomationReportSummary.ps1` | 从日志和 `ReportExportPath` 产物生成可机器消费的结果摘要。 | `Tools\GetAutomationReportSummary.ps1 -ReportPath <dir> -LogPath <log>` | `Summary.json` 或 stdout 摘要对象 | 当编辑器退出码为 0 但日志存在阻断错误时，可辅助识别假绿。 |
| AutomationToolSelfTests | `Tools\Tests\AutomationToolSelfTests.ps1` | 对 automation runner 与 summary 脚本执行轻量自测。 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\AutomationToolSelfTests.ps1` | 控制台 PASS/FAIL | 使用 fixture 校验 summary，使用未知 group 校验 runner 与 `.bat` wrapper 的失败路径。 |

## GenerateAgentConfigTemplate.bat

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\GenerateAgentConfigTemplate.bat` |
| 配置文件位置 | 项目根目录 `AgentConfig.ini` |
| 主要用途 | 生成 AI Agent 使用的本地配置模板，避免在文档和脚本中写死引擎路径。 |
| 默认行为 | 如果 `AgentConfig.ini` 已存在，则停止执行并提示使用 `--force`。 |
| 覆盖行为 | 传入 `--force` 后允许覆盖现有 `AgentConfig.ini`。 |
| Git 策略 | `AgentConfig.ini` 为本地机器配置，已加入 `.gitignore`，不应提交到仓库。 |

## 生成内容

脚本生成的 `AgentConfig.ini` 模板包含以下配置段：

| 段 | 键 | 说明 |
| --- | --- | --- |
| `Paths` | `EngineRoot` | 本机 Unreal Engine 根目录。 |
| `Paths` | `ProjectFile` | 项目的 `.uproject` 绝对路径。 |
| `Build` | `EditorTarget` | 默认构建目标，例如 `AngelscriptProjectEditor`。 |
| `Build` | `Platform` | 默认构建平台，例如 `Win64`。 |
| `Build` | `Configuration` | 默认构建配置，例如 `Development`。 |
| `Build` | `Architecture` | 默认架构，例如 `x64`。 |
| `Test` | `DefaultTimeoutMs` | 自动化测试或长时间命令的默认超时时间，单位毫秒。 |
| `References` | `HazelightAngelscriptEngineRoot` | HazelightAngelscriptEngine 本地参考路径，需用户手动填写，仅用于对照和参考，不参与默认构建流程。 |

## 使用说明

| 步骤 | 操作 |
| --- | --- |
| 1 | 在项目根目录运行 `Tools\GenerateAgentConfigTemplate.bat`。 |
| 2 | 打开生成的 `AgentConfig.ini`。 |
| 3 | 按本机实际情况修改 `EngineRoot` 等字段。 |
| 4 | 构建、测试或 AI Agent 执行命令前，先读取该配置文件。 |

## 示例

```bat
Tools\GenerateAgentConfigTemplate.bat
Tools\GenerateAgentConfigTemplate.bat --force
```

## PullReference.bat

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\PullReference.bat` |
| 主要用途 | 用统一入口拉取或同步当前项目使用的外部参考仓库。 |
| 拉取方式 | 对 GitHub 来源的参考仓库，统一使用各自对应的 SSH 地址拉取到当前项目的 `Reference\` 目录。 |
| 当前可拉取 key | `angelscript`、`unrealcsharp` |
| 当前不可拉取 key | `hazelight` |
| 安全策略 | 如果目标目录存在未提交改动，脚本会停止，避免覆盖本地参考修改。 |

## PullReference 使用说明

| 步骤 | 操作 |
| --- | --- |
| 1 | 确认本机已安装 Git，并已配置 GitHub SSH Key。 |
| 2 | 先运行 `Tools\PullReference.bat list` 查看支持的参考仓库 key。 |
| 3 | 运行 `Tools\PullReference.bat <key>` 拉取默认目录，或传入自定义目标目录。 |
| 4 | 如果目标目录已是 Git 仓库，脚本会按该参考源的分支或标签进行同步。 |

## PullReference 支持项

| key | 默认目录 | 远端 | 同步方式 |
| --- | --- | --- | --- |
| `angelscript` | `Reference\angelscript-v2.38.0` | `git@github.com:anjo76/angelscript.git` | 固定同步 `v2.38.0` 标签 |
| `unrealcsharp` | `Reference\UnrealCSharp` | `git@github.com:crazytuzi/UnrealCSharp.git` | 固定同步 `main` 分支 |
| `hazelight` | 读取 `AgentConfig.ini` 中 `HazelightAngelscriptEngineRoot` | - | 本地配置来源，不支持自动拉取 |

## PullReference 示例

```bat
Tools\PullReference.bat list
Tools\PullReference.bat angelscript
Tools\PullReference.bat unrealcsharp
Tools\PullReference.bat angelscript "J:\UnrealEngine\AngelscriptProject\Reference\angelscript-v2.38.0"
```

## RunAutomationTests.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunAutomationTests.ps1` |
| 主要用途 | 以统一入口运行 Angelscript 自动化测试，按 group 或测试前缀组织 bucket，并固定日志/报告输出目录。 |
| 配置来源 | 读取项目根目录 `AgentConfig.ini` 中的 `Paths.EngineRoot`、`Paths.ProjectFile` 与 `Test.DefaultTimeoutMs`。 |
| 默认输出 | `Saved\Automation\<Bucket>\<Timestamp>\Automation.log`、`Report\`、`RunMetadata.json`、`Summary.json` |
| 常用参数 | `-Group`、`-Prefix`、`-AbsLog`、`-ReportExportPath`、`-TimeoutMs`、`-ExtraArgs` |
| 错误策略 | 缺少 `AgentConfig.ini` / `EngineRoot` / `ProjectFile` 会直接失败；若日志存在阻断错误，runner 会把最终退出码提升为失败。 |

### 示例

```powershell
Tools\RunAutomationTests.ps1 -Group AngelscriptSmoke
Tools\RunAutomationTests.ps1 -Prefix Angelscript.TestModule.Bindings.
Tools\RunAutomationTests.ps1 -Group AngelscriptFast -ExtraArgs '-log'
```

## RunAutomationTests.bat

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunAutomationTests.bat` |
| 主要用途 | 为 Windows 本地使用和简单 CI 提供短命令入口。 |
| 行为边界 | 只负责把参数转发给 `RunAutomationTests.ps1`，不复制配置解析、日志策略或摘要逻辑。 |

## GetAutomationReportSummary.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\GetAutomationReportSummary.ps1` |
| 主要用途 | 聚合 `-ReportExportPath` 产物与日志，生成轻量 JSON 摘要。 |
| 输入 | `-ReportPath`、`-LogPath`、可选 `-SummaryPath` |
| 输出 | `BucketName`、退出码、报告路径、失败测试列表、阻断日志提示 |
| 设计约束 | 优先消费结构化报告；当报告缺失时，用日志补足“是否是假绿”的判断。 |

## AutomationToolSelfTests.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\Tests\AutomationToolSelfTests.ps1` |
| 主要用途 | 对 `RunAutomationTests.ps1`、`RunAutomationTests.bat` 和 `GetAutomationReportSummary.ps1` 做轻量自测。 |
| 覆盖点 | fixture 报告解析、未知 group 预检失败、`.bat` wrapper 参数转发失败路径 |
| 运行方式 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\AutomationToolSelfTests.ps1` |
| 依赖 | 不要求 Pester；直接使用 PowerShell 进程捕获和断言。 |
