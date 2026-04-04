# Build 指南

## 强制规则

- 本仓库的标准构建入口是 `Tools\RunBuild.ps1`。
- 不再允许把 `Build.bat`、`RunUBT.bat` 或 `dotnet UnrealBuildTool.dll` 直接写进日常操作指引、Agent 提示词或自动化外壳中。
- 所有构建命令都必须显式带超时，且超时不得超过 `300000ms`。
- 默认构建超时来自 `AgentConfig.ini` 中的 `Build.DefaultTimeoutMs`，仓库模板默认值为 `180000ms`。
- 构建超时或异常退出后，脚本必须终止整个 UBT 进程树，避免污染下次构建。
- 构建过程要求实时输出。进入 UBT 后，终端应按行持续看到日志，不允许“静默等待到结束”。

## AgentConfig.ini

执行任何构建命令前，先读取项目根目录的 `AgentConfig.ini`。

关键配置项：

```ini
[Paths]
EngineRoot=<UE 根目录>
ProjectFile=<当前 worktree 的 .uproject>

[Build]
EditorTarget=AngelscriptProjectEditor
Platform=Win64
Configuration=Development
Architecture=x64
DefaultTimeoutMs=180000
```

如果 `AgentConfig.ini` 不存在，先执行：

```powershell
Tools\GenerateAgentConfigTemplate.bat
```

## 标准入口

### 并发开发默认模式

默认模式用于多个 worktree 共享同一个引擎目录并发开发。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1
```

该模式的默认行为：

- 直接调用 `dotnet <EngineRoot>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll`
- 自动读取 `AgentConfig.ini`
- 默认追加 `-NoMutex -NoEngineChanges`
- 对同一 worktree 启用单飞锁，防止同一个 worktree 内重复构建
- 不依赖 `Build.bat` 的全局脚本锁，因此支持不同 worktree 并发构建

### 需要改动引擎输出时

如果本次构建预期会生成或改写引擎侧产物，必须显式切换到串行模式：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -SerializeByEngine
```

该模式会额外对 `EngineRoot` 加全局锁，避免多个 worktree 同时写引擎输出。

## 常用参数

```powershell
Tools\RunBuild.ps1 -TimeoutMs 120000
Tools\RunBuild.ps1 -Label fix-bindings
Tools\RunBuild.ps1 -SerializeByEngine
Tools\RunBuild.ps1 -TimeoutMs 180000 -- -Verbose
```

参数说明：

- `-TimeoutMs`：本次构建超时，必须大于 0 且不超过 `300000`
- `-Label`：输出目录标签，便于区分多次运行
- `-LogRoot`：自定义输出根目录
- `-SerializeByEngine`：启用引擎级串行锁
- `-- <ExtraArgs>`：透传额外 UBT 参数

## 输出与产物

若不指定 `-LogRoot`，构建产物默认写入：

```text
Saved/Build/<Label>/<Timestamp>/
  Build.log
  RunMetadata.json
```

其中：

- `Build.log`：实时流式构建日志
- `RunMetadata.json`：参数、超时、退出码、耗时等结构化元数据

## 查询当前 UBT 进程

排查构建卡死、残留 UBT 或多 worktree 并发情况时，使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Get-UbtProcess.ps1
```

只看当前 worktree：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Get-UbtProcess.ps1 -CurrentWorktreeOnly
```

脚本会尽量标出：

- 进程 ID
- 进程类型（`dotnet + UBT.dll` / `Build.bat` / `RunUBT.bat`）
- 所属 worktree
- 所属分支
- 目标项目与目标平台
- 是否使用 `-NoMutex`
- 是否使用 `-NoEngineChanges`

## 对 AI Agent 的要求

当 AI Agent 需要执行构建时，必须遵守以下要求：

1. 先读取根目录 `AgentConfig.ini`
2. 只通过 `Tools\RunBuild.ps1` 执行构建
3. 必须显式传入或继承一个不超过 `300000ms` 的超时
4. 不得直接调用 `Build.bat`
5. 不得直接调用 `RunUBT.bat`
6. 不得把 UBT 输出重定向成“结束后一次性读取”的静默模式
7. 发现需要引擎输出写入时，必须改用 `-SerializeByEngine`

## 推荐提示词

```text
请先读取项目根目录的 AgentConfig.ini，然后仅通过 Tools\RunBuild.ps1 执行构建。构建必须显式带超时，且不得超过 300000ms。默认使用并发开发模式，不要直接调用 Build.bat；如果本次构建需要写入引擎输出，再显式加上 -SerializeByEngine。执行时必须实时打印 UBT 日志，超时或异常退出后必须结束整个 UBT 进程树。
```
