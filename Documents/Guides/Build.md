# Build 指南

## 强制规则

- 本仓库的标准构建入口只有 `Tools\RunBuild.ps1`。
- 不再允许把 `Build.bat`、`RunUBT.bat` 或 `dotnet UnrealBuildTool.dll` 直接写进日常操作指引、Agent 提示词或自动化外壳。
- 所有构建命令都必须显式带超时，且超时不得超过 `900000ms`。
- 默认构建超时来自 `AgentConfig.ini` 的 `Build.DefaultTimeoutMs`；仓库标准默认值为 `180000ms`。
- 构建过程必须实时输出；超时或异常退出后，脚本必须清理整棵 UBT 进程树。
- 每次构建都必须写入自己的独立日志目录；禁止把多个 worktree 的构建日志写到同一个共享文件。

## AgentConfig.ini 与 bootstrap

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

[Test]
DefaultTimeoutMs=600000
```

如果当前 worktree 还没有 `AgentConfig.ini`，优先执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1
```

常用 bootstrap 方式：

```powershell
# 初始化当前 worktree
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1

# 初始化所有已注册 worktree
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1 -AllRegisteredWorktrees

# 显式指定引擎目录并跳过预热
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\BootstrapWorktree.ps1 -EngineRoot "J:\UnrealEngine\UERelease" -NoPrewarm
```

`Tools\BootstrapWorktree.ps1` 会：

- 生成或规范化当前 worktree 的 `AgentConfig.ini`
- 回填 `Build.DefaultTimeoutMs=180000` 与 `Test.DefaultTimeoutMs=600000`
- 把 `Paths.ProjectFile` 固定到当前 worktree 的 `.uproject`
- 预热 `Intermediate/TargetInfo.json`，避免首次 build/test 把时间浪费在旧的 `Build.bat` 查询阶段

只想给 Agent 生成官方命令模板时，使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\ResolveAgentCommandTemplates.ps1
```

该脚本在配置缺失时会先返回 `BootstrapCommand`，配置正常时才返回构建与测试模板。

## 标准入口

### 并发开发默认模式

多个 worktree 共享同一个引擎目录时，默认使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -Label agent-build -TimeoutMs 180000
```

默认行为：

- 直接调用 `dotnet <EngineRoot>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll`
- 自动读取 `AgentConfig.ini`
- 默认追加 `-NoMutex -NoEngineChanges`
- 对同一 worktree 加单飞锁，禁止同一 worktree 内重复 build/test
- 通过 `-Log=` 把 UBT 日志重定向到当前 run 的私有目录，避免写入共享 `Log.txt`
- 不依赖 `Build.bat` 的全局脚本锁，因此允许不同 worktree 并发构建

### 需要改动引擎输出时

如果本次构建会改写共享引擎产物，必须显式切换到串行模式：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -Label engine-write -TimeoutMs 180000 -SerializeByEngine
```

该模式会基于 `EngineRoot` 获取命名互斥锁，避免多个 worktree 同时写引擎输出。

## 常用参数

```powershell
Tools\RunBuild.ps1 -Label compile-bindings -TimeoutMs 120000
Tools\RunBuild.ps1 -Label compile-bindings -TimeoutMs 180000 -- -Verbose
Tools\RunBuild.ps1 -Label engine-write -TimeoutMs 180000 -SerializeByEngine
Tools\RunBuild.ps1 -Label local-log-root -TimeoutMs 180000 -LogRoot "D:\Tmp\AngelscriptLogs"
```

参数说明：

- `-TimeoutMs`：本次构建超时，必须大于 `0` 且不超过 `900000`
- `-Label`：输出目录标签
- `-LogRoot`：自定义输出根目录；脚本会把它当成父目录，再创建独立的 `Build/<Label>/<RunId>/`
- `-SerializeByEngine`：启用引擎级串行锁
- `-- <ExtraArgs>`：透传额外 UBT 参数，例如 `-NoXGE`

## 输出与产物

默认输出目录：

```text
Saved/Build/<Label>/<RunId>/
  Build.log
  UBT.log
  RunMetadata.json
```

如果传入 `-LogRoot D:\Tmp\Logs`，实际目录会变成：

```text
D:\Tmp\Logs\Build\<Label>\<RunId>\
```

注意：

- `-LogRoot` / 自定义目录只是父目录，不是最终运行目录
- 每次调用都会新建独立 `RunId`，防止多个 worktree 或多次重跑把日志写进同一文件
- `Build.log` 是脚本流式日志，`UBT.log` 是 UBT 自己的日志，`RunMetadata.json` 记录参数、阶段、超时与退出码

## 查询当前 UBT 进程

排查卡死、残留 UBT 或多 worktree 并发情况时使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Get-UbtProcess.ps1
```

只看当前 worktree：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\Get-UbtProcess.ps1 -CurrentWorktreeOnly
```

## 多 worktree 故障排除

### XGE 槽位争抢

症状：

- 构建启动后长时间没有 `[N/M] Compile`
- 日志出现 `Using XGE executor` 后停住
- 当前 worktree 没有残留 UBT，但其他 worktree 正在活跃构建

处理方式：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -Label noxge -TimeoutMs 180000 -- -NoXGE
```

### 旧 `Build.bat` 锁争用

旧流程走 `Build.bat` 时，会在共享引擎目录上占用全局脚本锁。标准构建已经绕过这条路径；如果仍遇到锁争用，说明还有旧文档、旧脚本或其他 worktree 没切到 `Tools\RunBuild.ps1`。

处理顺序：

1. 用 `Tools\Get-UbtProcess.ps1` 找出还在跑旧流程的 worktree
2. 用 `Tools\BootstrapWorktree.ps1 -AllRegisteredWorktrees` 统一补齐配置
3. 只通过 `Tools\ResolveAgentCommandTemplates.ps1` / 本文档下发构建命令

### UBT 共享日志冲突

UBT 默认会写共享 `Log.txt`。`RunBuild.ps1` 已通过 `-Log=` 把它重定向到当前 run 的 `UBT.log`；如果仍看到共享日志冲突，说明调用方绕过了 `Tools\RunBuild.ps1`。

## 对 AI Agent 的要求

1. 先读取根目录 `AgentConfig.ini`
2. 配置缺失时先跑 `Tools\BootstrapWorktree.ps1`
3. 仅通过 `Tools\RunBuild.ps1` 执行构建
4. 显式传入或继承一个不超过 `900000ms` 的超时
5. 默认使用并发模式；只有确认会写引擎共享输出时才加 `-SerializeByEngine`
6. 不要手写 `Build.bat` / `RunUBT.bat` / `dotnet UnrealBuildTool.dll`

## 推荐提示词

```text
请先读取项目根目录的 AgentConfig.ini；如果缺失或 ProjectFile 不属于当前 worktree，先执行 Tools\BootstrapWorktree.ps1。构建只能通过 Tools\RunBuild.ps1 进行，并显式带一个不超过 900000ms 的超时。默认保持并发模式；只有确认要写共享引擎输出时才追加 -SerializeByEngine。日志必须实时输出，并写入当前 run 的独立目录；不要手写 Build.bat、RunUBT.bat 或 dotnet UnrealBuildTool.dll 命令。
```
