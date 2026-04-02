# Build 指南

### Agent 环境命令

在 AI Agent 环境中执行时，建议使用 PowerShell，确保完整捕获输出。

先根据`AgentConfig.ini`中的`Paths.EngineRoot` 获取 `Engine\Build\BatchFiles\Build.bat` 的完整路径，再将 `<ProjectFile>`、`<EditorTarget>`、`<Platform>`、`<Configuration>` 和 `<Architecture>` 替换为 `AgentConfig.ini` 中的实际值：

```powershell
powershell.exe -Command "& '<FullPathTo>\Engine\Build\BatchFiles\Build.bat' <EditorTarget> <Platform> <Configuration> '-Project=<ProjectFile>' -WaitMutex -FromMsBuild -architecture=<Architecture> 2>&1 | Out-String"
```

示例

```powershell
powershell.exe -Command "& 'C:\UnrealEngine\UE_5.7\Engine\Build\BatchFiles\Build.bat' UnrealEventEditor Win64 Development '-Project=D:\Workspace\UnrealEvent\UnrealEvent.uproject' -WaitMutex -FromMsBuild -architecture=x64 2>&1 | Out-String"
```

- `<Architecture>` 通常为`x64`







### 原始命令

使用 `UnrealEditor-Cmd.exe` 配合 `-ExecCmds` 参数，在 `NullRHI` 模式下运行。

其中 `EngineRoot` 和 `ProjectFile` 应来自 `AgentConfig.ini`。

编辑器命令路径应按以下方式理解：

```text
<EngineRoot> + Engine\Binaries\Win64\UnrealEditor-Cmd.exe
```

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "<ProjectFile>" -ExecCmds="Automation RunTests <TestName>; Quit" -Unattended -NoPause -NoSplash -NullRHI -NOSOUND
```

### AI Agent 环境命令

在 AI Agent 环境中执行时，使用 PowerShell 配合 `Start-Process`：

```powershell
powershell.exe -Command "Start-Process -FilePath '<FullPathTo>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"<ProjectFile>\"','-ExecCmds=\"Automation RunTests <TestName>; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

### 超时建议

- 建议默认超时设置为 `600000ms`。
- 首次启动通常需要加载引擎，并可能触发 shader 编译，因此耗时会明显更长。
- 如果 `AgentConfig.ini` 中配置了 `Test.DefaultTimeoutMs`，应优先使用该值。

## 给 AI Agent 的执行要求

当 AI Agent 需要执行 Unreal 构建或自动化测试时，应遵循以下规则：

- 先读取项目根目录的 `AgentConfig.ini`。
- 如果 `AgentConfig.ini` 不存在，提示用户先运行 `Tools\GenerateAgentConfigTemplate.bat`。
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
请先读取项目根目录的 AgentConfig.ini。文档中的 Build.bat 路径使用相对路径 Engine\Build\BatchFiles\Build.bat 表示，执行前请基于 EngineRoot 解析出完整路径，再结合 ProjectFile、EditorTarget、Platform、Configuration、Architecture 组装命令。请使用 PowerShell 执行，不要使用 cmd.exe /c。必须完整捕获输出，确保 stdout 和 stderr 都被记录，并在命令末尾追加 2>&1 | Out-String。
```

如果需要让 AI Agent 执行自动化测试，可直接使用以下提示语：

```text
请先读取项目根目录的 AgentConfig.ini。文档中的 UnrealEditor-Cmd.exe 路径使用相对路径 Engine\Binaries\Win64\UnrealEditor-Cmd.exe 表示，执行前请基于 EngineRoot 解析出完整路径，再结合 ProjectFile 和 DefaultTimeoutMs 启动 Automation Tests。请使用 PowerShell 的 Start-Process -Wait -NoNewWindow 执行，并保留 -Unattended -NoPause -NoSplash -NullRHI -NOSOUND 参数。
```
