# Tools

## 工具列表

| 工具名 | 路径 | 用途 | 常用命令 | 输出 | 备注 |
| --- | --- | --- | --- | --- | --- |
| GenerateAgentConfigTemplate | `Tools\GenerateAgentConfigTemplate.bat` | 在项目根目录生成本机专用的 `AgentConfig.ini` 模板，供 AI Agent 和开发者读取引擎路径、项目路径、默认构建参数与测试超时。 | `Tools\GenerateAgentConfigTemplate.bat` | 生成 `AgentConfig.ini` | 如果目标文件已存在，默认不会覆盖。 |
| GenerateAgentConfigTemplate `--force` | `Tools\GenerateAgentConfigTemplate.bat` | 强制覆盖并重新生成 `AgentConfig.ini` 模板。 | `Tools\GenerateAgentConfigTemplate.bat --force` | 重新生成 `AgentConfig.ini` | 仅在确认需要覆盖本地配置时使用。 |
| RunTests | `Tools\RunTests.ps1` | 一键运行 UE 自动化测试，自动读取 AgentConfig.ini，创建带时间戳输出目录，解析结果摘要。 | `.\Tools\RunTests.ps1 -TestPrefix "Angelscript"` | `Saved/Automation/<timestamp>_<Label>/test.log` + `Reports/` | 退出码 0=全通过，1=有失败。 |
| RunTestSuite | `Tools\RunTestSuite.ps1` | 按具名 suite 顺序运行一组标准测试前缀，固化 smoke / native / scenario 等推荐波次。 | `.\Tools\RunTestSuite.ps1 -Suite Smoke` | 多个 `Saved/Automation/<timestamp>_<Label>/` 子目录 | 适合标准化回归流程；底层仍调用 `RunTests.ps1`。 |
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

## RunTests.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunTests.ps1` |
| 主要用途 | 一键运行 UE 自动化测试，自动读取 `AgentConfig.ini`，创建带时间戳的输出目录，运行测试并解析结果摘要。 |
| 依赖 | `AgentConfig.ini` 已配置 `Paths.EngineRoot`。 |

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `-TestPrefix` | string | `Angelscript` | UE 自动化测试过滤前缀。 |
| `-Label` | string | 从 TestPrefix 派生 | 输出目录中的人可读标签。 |
| `-OutputRoot` | string | `<ProjectRoot>/Saved/Automation` | 测试输出根目录。 |
| `-NoReport` | switch | false | 跳过 `-ReportExportPath` JSON 报告导出。 |

### 输出结构

每次运行创建 `<OutputRoot>/<timestamp>_<Label>/` 目录，包含：

| 文件/目录 | 说明 |
| --- | --- |
| `test.log` | 引擎标准输出捕获 |
| `Reports/` | UE 自动化测试 JSON 报告（除非使用 `-NoReport`） |

### 结果解析

脚本自动从日志中提取并打印：
- 进程退出码
- `GIsCriticalError` 值
- 通过/失败测试数量（`TEST COMPLETE` 行）
- 前 50 条失败行

脚本退出码：有测试失败或进程异常退出时返回 `1`，否则返回 `0`。

### 使用示例

```powershell
# 运行全部 Angelscript 测试
.\Tools\RunTests.ps1

# 运行特定前缀
.\Tools\RunTests.ps1 -TestPrefix "Angelscript.CppTests.MultiEngine"

# 自定义标签
.\Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload" -Label "HotReload_Verify"

# AI Agent 环境执行（带超时）
powershell.exe -ExecutionPolicy Bypass -File "Tools\RunTests.ps1" -TestPrefix "Angelscript"
```

## RunTestSuite.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunTestSuite.ps1` |
| 主要用途 | 以具名 suite 方式顺序执行多组标准测试前缀，适合固定 smoke / native / learning / scenario 核查流程。 |
| 依赖 | `Tools\RunTests.ps1`、`AgentConfig.ini`、`Paths.EngineRoot` |

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `-Suite` | string | 空 | 要执行的 suite 名称；不传时配合 `-ListSuites` 使用。 |
| `-LabelPrefix` | string | 从 Suite 派生 | 每一波次输出目录前缀。 |
| `-OutputRoot` | string | 空 | 透传给 `RunTests.ps1` 的输出根目录。 |
| `-NoReport` | switch | false | 透传给 `RunTests.ps1`，跳过 JSON 报告导出。 |
| `-ListSuites` | switch | false | 仅列出内置 suite 与其包含的测试前缀。 |
| `-DryRun` | switch | false | 只打印即将执行的命令，不真正启动 UnrealEditor-Cmd。 |

### 当前内置 suite

- `Smoke`
- `NativeCore`
- `RuntimeCpp`
- `Bindings`
- `Internals`
- `LearningNative`
- `LearningRuntime`
- `HotReload`
- `ScenarioSamples`
- `All`

### 使用示例

```powershell
# 查看当前内置 suite
.\Tools\RunTestSuite.ps1 -ListSuites

# 跑一轮标准 smoke
.\Tools\RunTestSuite.ps1 -Suite Smoke

# 查看场景样本套件会执行哪些命令
.\Tools\RunTestSuite.ps1 -Suite ScenarioSamples -DryRun
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
