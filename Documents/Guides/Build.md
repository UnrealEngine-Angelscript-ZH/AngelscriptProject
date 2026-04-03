# Build 指南

## 执行前置配置

- 执行任何构建或自动化测试前，先读取项目根目录 `AgentConfig.ini`。
- 若 `AgentConfig.ini` 不存在，先运行 `Tools\GenerateAgentConfigTemplate.bat` 生成模板，再补齐本机配置；未补齐前不要继续执行构建或测试。
- `Paths.EngineRoot` 是唯一的硬阻塞项；缺失或为空时必须先补配置，不能继续猜路径。
- `Paths.ProjectFile` 允许留空；留空时统一回退到仓库根目录下的 `AngelscriptProject.uproject`。
- `Build.EditorTarget`、`Build.Platform`、`Build.Configuration`、`Build.Architecture` 缺失时，分别回退到 `AngelscriptProjectEditor`、`Win64`、`Development`、`x64`。
- 在真正运行构建命令前，可先执行 `Tools\ResolveAgentCommandTemplates.ps1` 做参数展开级 smoke check。

## Agent 环境命令

在 AI Agent 环境中执行时，建议使用 PowerShell，确保完整捕获输出。

先根据 `AgentConfig.ini` 中的 `Paths.EngineRoot` 获取 `Engine\Build\BatchFiles\Build.bat` 的完整路径；若 `Paths.ProjectFile` 为空，则先回退到仓库根目录 `.uproject`，其余构建参数按默认值补齐：

```powershell
powershell.exe -Command "& '<EngineRoot>\Engine\Build\BatchFiles\Build.bat' <EditorTarget> <Platform> <Configuration> '-Project=<ResolvedProjectFile>' -WaitMutex -FromMsBuild -architecture=<Architecture> 2>&1 | Out-String"
```

默认参数速查

- `<ResolvedProjectFile>`：`Paths.ProjectFile`，为空时回退到 `<RepoRoot>\AngelscriptProject.uproject`
- `<EditorTarget>`：`Build.EditorTarget`，默认为 `AngelscriptProjectEditor`
- `<Platform>`：`Build.Platform`，默认为 `Win64`
- `<Configuration>`：`Build.Configuration`，默认为 `Development`
- `<Architecture>`：`Build.Architecture`，默认为 `x64`

示例（占位符形式）

```powershell
powershell.exe -Command "& '<EngineRoot>\Engine\Build\BatchFiles\Build.bat' AngelscriptProjectEditor Win64 Development '-Project=<RepoRoot>\AngelscriptProject.uproject' -WaitMutex -FromMsBuild -architecture=x64 2>&1 | Out-String"
```

- `<Architecture>` 通常为 `x64`

## 原始命令

使用 `UnrealEditor-Cmd.exe` 配合 `-ExecCmds` 参数，在 `NullRHI` 模式下运行。

其中 `EngineRoot` 和 `ProjectFile` 应来自 `AgentConfig.ini`；`ProjectFile` 为空时应先回退到仓库根 `.uproject`。

编辑器命令路径应按以下方式理解：

```text
<EngineRoot> + Engine\Binaries\Win64\UnrealEditor-Cmd.exe
```

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "<ResolvedProjectFile>" -ExecCmds="Automation RunTests <TestName>; Quit" -Unattended -NoPause -NoSplash -NullRHI -NOSOUND
```

## AI Agent 环境命令

在 AI Agent 环境中执行时，使用 PowerShell 配合 `Start-Process`：

```powershell
powershell.exe -Command "Start-Process -FilePath '<EngineRoot>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"<ResolvedProjectFile>\"','-ExecCmds=\"Automation RunTests <TestName>; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

## 超时建议

- 建议默认超时设置为 `600000ms`。
- 首次启动通常需要加载引擎，并可能触发 shader 编译，因此耗时会明显更长。
- 如果 `AgentConfig.ini` 中配置了 `Test.DefaultTimeoutMs`，应优先使用该值。

## 给 AI Agent 的执行要求

当 AI Agent 需要执行 Unreal 构建或自动化测试时，应遵循以下规则：

- 先读取项目根目录的 `AgentConfig.ini`。
- 如果 `AgentConfig.ini` 不存在，先运行 `Tools\GenerateAgentConfigTemplate.bat` 生成模板，并补齐本机值后再继续。
- 如果 `Paths.EngineRoot` 缺失或为空，应立即阻塞执行，先补配置再继续。
- 如果 `Paths.ProjectFile` 为空，回退到仓库根目录下的 `AngelscriptProject.uproject`。
- 如果 `Build.EditorTarget`、`Build.Platform`、`Build.Configuration`、`Build.Architecture` 缺失，分别回退到 `AngelscriptProjectEditor`、`Win64`、`Development`、`x64`。
- 文档中的工具路径均为相对路径，执行前必须基于 `Paths.EngineRoot` 解析为完整路径。
- 构建 `Build.bat` 时，必须使用 `powershell.exe -Command` 调用。
- 不要使用 `cmd.exe /c` 包装 `Build.bat`。
- 构建命令必须追加 `2>&1 | Out-String`，确保完整捕获输出。
- 运行 `UnrealEditor-Cmd.exe` 时，优先使用 `Start-Process -Wait -NoNewWindow`。
- 自动化测试建议启用 `-Unattended -NoPause -NoSplash -NullRHI -NOSOUND`。
- 长时间任务应预留至少 `600000ms` 超时时间，或使用 `Test.DefaultTimeoutMs`。

## 可直接引用的提示语

如果需要让 AI Agent 执行构建，可直接使用以下提示语：

```text
请先读取项目根目录的 AgentConfig.ini。若文件不存在，先运行 Tools\GenerateAgentConfigTemplate.bat 生成模板并补齐本机值。若 Paths.EngineRoot 缺失则立即阻塞；若 Paths.ProjectFile 为空则回退到仓库根目录 AngelscriptProject.uproject；若 Build.EditorTarget、Build.Platform、Build.Configuration、Build.Architecture 缺失，则分别回退到 AngelscriptProjectEditor、Win64、Development、x64。文档中的 Build.bat 路径使用相对路径 Engine\Build\BatchFiles\Build.bat 表示，执行前请基于 EngineRoot 解析出完整路径。请使用 PowerShell 执行，不要使用 cmd.exe /c。必须完整捕获输出，确保 stdout 和 stderr 都被记录，并在命令末尾追加 2>&1 | Out-String。
```

如果需要让 AI Agent 执行自动化测试，可直接使用以下提示语：

```text
请先读取项目根目录的 AgentConfig.ini。若文件不存在，先运行 Tools\GenerateAgentConfigTemplate.bat 生成模板并补齐本机值。若 Paths.EngineRoot 缺失则立即阻塞；若 Paths.ProjectFile 为空则回退到仓库根目录 AngelscriptProject.uproject；若 Test.DefaultTimeoutMs 缺失则回退到 600000。文档中的 UnrealEditor-Cmd.exe 路径使用相对路径 Engine\Binaries\Win64\UnrealEditor-Cmd.exe 表示，执行前请基于 EngineRoot 解析出完整路径，再结合 ProjectFile 和 DefaultTimeoutMs 启动 Automation Tests。请使用 PowerShell 的 Start-Process -Wait -NoNewWindow 执行，并保留 -Unattended -NoPause -NoSplash -NullRHI -NOSOUND 参数。
```
