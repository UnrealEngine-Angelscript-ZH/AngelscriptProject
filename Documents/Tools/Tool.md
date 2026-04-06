# Tools

## 官方入口

标准入口只保留四类：

- `Tools\BootstrapWorktree.ps1`：初始化或规范化当前 worktree 的 `AgentConfig.ini`
- `Tools\ResolveAgentCommandTemplates.ps1`：生成给 AI Agent/脚本使用的官方命令模板
- `Tools\RunBuild.ps1`：标准构建入口
- `Tools\RunTests.ps1` / `Tools\RunTestSuite.ps1`：标准自动化测试入口

其它脚本只承担诊断、摘要、兼容或自测职责，不应在新文档中取代官方入口。

## 工具总览

| 工具 | 路径 | 用途 | 常用命令 | 主要输出 | 备注 |
| --- | --- | --- | --- | --- | --- |
| BootstrapWorktree | `Tools\BootstrapWorktree.ps1` | 初始化当前 worktree，规范化 `AgentConfig.ini`，并按需预热 `TargetInfo.json` | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1` | `AgentConfig.ini`、`Intermediate\TargetInfo.json` | 新 worktree 优先使用 |
| ResolveAgentCommandTemplates | `Tools\ResolveAgentCommandTemplates.ps1` | 输出官方 build/test/bootstrap 命令模板 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\ResolveAgentCommandTemplates.ps1` | `Status=...` + 一组命令模板 | 配置缺失时先返回 `BootstrapCommand`；配置就绪时返回 `BuildCommand`、`NoXgeBuildCommand`、`SerializedBuildCommand` |
| RunBuild | `Tools\RunBuild.ps1` | 标准 UBT 构建入口，支持多 worktree 并发与引擎级串行锁 | `Tools\RunBuild.ps1 -Label agent-build -TimeoutMs 180000` | `Saved/Build/<Label>/<RunId>/` | 自动写 `Build.log`、`UBT.log` 与 `RunMetadata.json`；内建 `-NoXGE`，并显式禁止 `-UniqueBuildEnvironment` |
| RunTests | `Tools\RunTests.ps1` | 标准自动化测试入口，负责日志、报告、摘要与超时清理 | `Tools\RunTests.ps1 -Group AngelscriptSmoke -Label smoke -TimeoutMs 600000` | `Saved/Tests/<Label>/<RunId>/` | 自动写 `Automation.log`、`Report/`、`Summary.json` |
| RunTestSuite | `Tools\RunTestSuite.ps1` | 按具名 suite 顺序执行一组标准测试前缀 | `Tools\RunTestSuite.ps1 -Suite Smoke -LabelPrefix smoke -TimeoutMs 600000` | 多个 `Saved/Tests/<Label>/<RunId>/` 子目录 | 只做调度，底层仍调用 `RunTests.ps1` |
| Get-UbtProcess | `Tools\Get-UbtProcess.ps1` | 枚举本机 UBT / `Build.bat` / `RunUBT.bat` 相关进程，帮助排查争用 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Get-UbtProcess.ps1 -CurrentWorktreeOnly` | 控制台列表 | 用于定位旧流程或残留进程 |
| GetAutomationReportSummary | `Tools\GetAutomationReportSummary.ps1` | 根据 `Report/` 与 `Automation.log` 生成轻量摘要 | `Tools\GetAutomationReportSummary.ps1 -ReportPath <dir> -LogPath <log>` | `Summary.json` 或 stdout 对象 | 用于识别假绿与失败详情 |
| PullReference | `Tools\PullReference.bat` | 拉取或同步参考仓库 | `Tools\PullReference.bat angelscript` | `Reference\...` | 不参与默认 build/test 流程 |
| GenerateAgentConfigTemplate | `Tools\GenerateAgentConfigTemplate.bat` | 生成本机模板版 `AgentConfig.ini` | `Tools\GenerateAgentConfigTemplate.bat` | `AgentConfig.ini` | 仍可用，但新 worktree 更推荐 `BootstrapWorktree.ps1` |
| RunAutomationTests (legacy) | `Tools\RunAutomationTests.ps1` / `Tools\RunAutomationTests.bat` | 兼容旧脚本的过渡包装层 | 无 | 兼容旧产物布局 | 保留兼容，不作为官方入口 |
| RunToolingSmokeTests | `Tools\Tests\RunToolingSmokeTests.ps1` | 自测 bootstrap、模板解析、输出布局、超时与进程清理 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\RunToolingSmokeTests.ps1` | 控制台 PASS/FAIL | 不依赖 Pester |
| AutomationToolSelfTests | `Tools\Tests\AutomationToolSelfTests.ps1` | 自测自动化报告摘要与 legacy runner 包装层 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\AutomationToolSelfTests.ps1` | 控制台 PASS/FAIL | 主要覆盖兼容层 |
| PolicyAuditSmokeTests | `Tools\Tests\PolicyAuditSmokeTests.ps1` | 审计 live 文档与计划中的旧入口示例，防止回退到 `Build.bat` / 直调编辑器 | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\PolicyAuditSmokeTests.ps1` | 控制台 PASS/FAIL | 覆盖 `Documents/Guides/`、`Documents/Plans/` 等 live 文档 |

## BootstrapWorktree.ps1

## BlueprintImpactScanCommandlet

| 项目 | 说明 |
| --- | --- |
| 入口位置 | `Plugins\Angelscript\Source\AngelscriptEditor\Private\BlueprintImpact\AngelscriptBlueprintImpactScanCommandlet.cpp` |
| 主要用途 | 扫描项目中的 Blueprint 资产，并报告哪些资产受给定 Angelscript 脚本变更影响。 |
| 依赖 | 当前 worktree 根目录 `AgentConfig.ini`、已成功初始化的 `FAngelscriptEngine`、`AssetRegistry` |
| 典型参数 | `-ChangedScript="Foo.as;Bar.as"`、`-ChangedScriptFile="<path>"` |
| 输出 | 日志摘要（扫描模式、变更脚本数、命中模块数、候选资产数、命中资产数、失败加载数）以及逐资产命中原因 |

### 使用示例

```powershell
J:\UnrealEngine\UERelease\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <ProjectFile> -run=AngelscriptBlueprintImpactScan -stdout -FullStdOutLogOutput -Unattended -NoPause -NoSplash -NullRHI
J:\UnrealEngine\UERelease\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <ProjectFile> -run=AngelscriptBlueprintImpactScan -ChangedScript="Foo.as;Bar.as" -stdout -FullStdOutLogOutput -Unattended -NoPause -NoSplash -NullRHI
J:\UnrealEngine\UERelease\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <ProjectFile> -run=AngelscriptBlueprintImpactScan -ChangedScriptFile="J:\Temp\changed-scripts.txt" -stdout -FullStdOutLogOutput -Unattended -NoPause -NoSplash -NullRHI
```

`<ProjectFile>` 应来自当前 worktree 的 `AgentConfig.ini`，不要复用其他 worktree 的 `.uproject` 路径。

## GenerateAgentConfigTemplate.bat

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\BootstrapWorktree.ps1` |
| 主要用途 | 为当前 worktree 创建或规范化 `AgentConfig.ini`，并预热 `TargetInfo.json` |
| 常用参数 | `-AllRegisteredWorktrees`、`-EngineRoot`、`-NoPrewarm`、`-Force` |
| 推荐场景 | 新建 worktree、发现 `ProjectFile` 指向了其他 worktree、缺少默认超时配置 |

示例：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1 -AllRegisteredWorktrees
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1 -EngineRoot "J:\UnrealEngine\UERelease" -NoPrewarm
```

## ResolveAgentCommandTemplates.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\ResolveAgentCommandTemplates.ps1` |
| 主要用途 | 输出当前 worktree 的 bootstrap/build/test 官方命令模板 |
| 输出模式 | `Status=BootstrapRequired` 或 `Status=Ready` |
| 关键字段 | `BootstrapCommand`、`BuildCommand`、`NoXgeBuildCommand`、`SerializedBuildCommand`、`TestCommand`、`TestSuiteSmokeCommand` |

说明：

- 当 `AgentConfig.ini` 缺失或不属于当前 worktree 时，脚本不会再把调用方导向旧入口，而是先返回 `BootstrapCommand`
- 当配置就绪时，所有模板都显式包含超时，并且只引用官方 runner

## RunBuild.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunBuild.ps1` |
| 主要用途 | 通过 `dotnet + UnrealBuildTool.dll` 执行标准构建 |
| 关键参数 | `-TimeoutMs`、`-Label`、`-LogRoot`、`-SerializeByEngine`、`-NoXGE` |
| 默认输出 | `Saved/Build/<Label>/<RunId>/Build.log`、`UBT.log`、`RunMetadata.json` |
| 关键保护 | worktree 单飞锁、引擎级串行锁、实时日志、超时后清理进程树 |

示例：

```powershell
Tools\RunBuild.ps1 -Label agent-build -TimeoutMs 180000
Tools\RunBuild.ps1 -Label engine-write -TimeoutMs 180000 -SerializeByEngine
Tools\RunBuild.ps1 -Label noxge -TimeoutMs 180000 -NoXGE
```

## RunTests.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunTests.ps1` |
| 主要用途 | 运行单条前缀或单个 automation group，并生成日志、报告、摘要 |
| 关键参数 | `-TestPrefix`、`-Group`、`-Label`、`-OutputRoot`、`-TimeoutMs`、`-Render`、`-NoReport` |
| 默认输出 | `Saved/Tests/<Label>/<RunId>/Automation.log`、`Report/`、`RunMetadata.json`、`Summary.json` |
| 关键保护 | `TargetInfo.json` 预热、旧 `Build.bat` 锁防御等待、worktree 单飞锁、超时后清理进程树 |

示例：

```powershell
Tools\RunTests.ps1 -Group AngelscriptSmoke -Label smoke -TimeoutMs 600000
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Bindings." -Label bindings -TimeoutMs 600000
Tools\RunTests.ps1 -Group AngelscriptScenario -Label scenario -TimeoutMs 900000 -Render
```

## RunTestSuite.ps1

| 项目 | 说明 |
| --- | --- |
| 工具路径 | `Tools\RunTestSuite.ps1` |
| 主要用途 | 顺序执行内置 suite 中的一组标准前缀 |
| 关键参数 | `-Suite`、`-LabelPrefix`、`-TimeoutMs`、`-OutputRoot`、`-NoReport`、`-ListSuites`、`-DryRun` |
| 输出行为 | 每个子 run 仍按 `RunTests.ps1` 生成独立输出目录 |

示例：

```powershell
Tools\RunTestSuite.ps1 -ListSuites
Tools\RunTestSuite.ps1 -Suite Smoke -LabelPrefix smoke -TimeoutMs 600000
Tools\RunTestSuite.ps1 -Suite Debugger -LabelPrefix debugger -TimeoutMs 600000 -DryRun
```

## legacy 兼容层

| 工具 | 说明 |
| --- | --- |
| `Tools\RunAutomationTests.ps1` | 旧 PowerShell 兼容层，内部仍会转到新测试 runner |
| `Tools\RunAutomationTests.bat` | 旧 batch 兼容层，仅做参数转发 |

约束：

- 这两者只用于兼容已有脚本或历史 CI
- 新文档、新计划、新提示词不再提供它们的可执行命令示例
- 需要标准入口时，一律回到 `RunTests.ps1` / `RunTestSuite.ps1`

## 自测脚本

| 脚本 | 重点覆盖 |
| --- | --- |
| `Tools\Tests\RunToolingSmokeTests.ps1` | bootstrap、超时预算、输出目录隔离、命令模板回退、suite 参数透传 |
| `Tools\Tests\AutomationToolSelfTests.ps1` | `GetAutomationReportSummary.ps1` 与 legacy runner 兼容性 |
| `Tools\Tests\PolicyAuditSmokeTests.ps1` | 文档/计划里的旧入口、错误输出路径、共享日志示例 |

推荐在修改构建/测试工具后依次执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\RunToolingSmokeTests.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\AutomationToolSelfTests.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Tests\PolicyAuditSmokeTests.ps1
```
