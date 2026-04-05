# Test 指南

## 强制规则

- 本仓库的标准自动化测试入口是 `Tools\RunTests.ps1`。
- 测试放置、命名、前缀与具名 suite 约定以 `Documents/Guides/TestConventions.md` 为准；`Test.md` 只说明执行入口与运行约束。
- 不再允许把 `UnrealEditor-Cmd.exe` 直调命令、`Start-Process UnrealEditor-Cmd.exe` 拼参命令、或旧的 `Tools\RunAutomationTests.ps1` 作为标准执行方式写入指南。
- 所有测试命令都必须显式带超时，且超时不得超过 `900000ms`（15 分钟）。
- 默认测试超时来自 `AgentConfig.ini` 中的 `Test.DefaultTimeoutMs`，仓库模板默认值为 `600000ms`（10 分钟）。
- 测试过程必须实时输出。进入编辑器后，终端要逐行看到日志，不允许静默等待。
- 测试超时或异常退出后，脚本必须终止整个进程树，避免残留编辑器、子进程或 UBT。

## AgentConfig.ini

执行测试前，先读取项目根目录的 `AgentConfig.ini`。

关键配置项：

```ini
[Paths]
EngineRoot=<UE 根目录>
ProjectFile=<当前 worktree 的 .uproject>

[Test]
DefaultTimeoutMs=600000
```

如果本地还没有该文件，先执行：

```powershell
Tools\GenerateAgentConfigTemplate.bat
```

## 标准入口

### 按测试前缀运行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Bindings."
```

### 按测试组运行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTests.ps1 -Group AngelscriptSmoke
```

### 按具名 suite 运行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTestSuite.ps1 -Suite Smoke
```

### 需要图形输出时

默认测试会启用 `-NullRHI`。只有明确需要真实渲染时才使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTests.ps1 -Group AngelscriptSmoke -Render
```

## 脚本默认行为

`Tools\RunTests.ps1` 会自动：

- 读取 `AgentConfig.ini`
- 调用 `UnrealEditor-Cmd.exe`
- 追加 `-stdout -FullStdOutLogOutput -UTF8Output`
- 默认追加 `-Unattended -NoPause -NoSplash -NOSOUND`
- 在非渲染模式下追加 `-NullRHI`
- 为每次运行生成独立的 `-ABSLOG` 和 `-ReportExportPath`
- 预热并校验当前 worktree 的 `Intermediate/TargetInfo.json`；如果发现旧缓存包含 parent assembly targets，会按当前项目作用域自动重建，避免启动阶段再次触发 `QueryTargets`
- 对同一 worktree 启用单飞锁，防止同一 worktree 内并发跑多个 build/test
- 在超时或异常退出时结束整个进程树

`Tools\RunTestSuite.ps1` 是基于 `Tools\RunTests.ps1` 的薄封装，用于把常用 smoke / native / runtime / scenario 波次固化成具名入口；需要单条前缀或 group 控制时，仍直接使用 `Tools\RunTests.ps1`。

## 常用参数

```powershell
Tools\RunTests.ps1 -Group AngelscriptSmoke -TimeoutMs 120000
Tools\RunTests.ps1 -TestPrefix "Angelscript.CppTests." -Label runtime-unit
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.Dump" -TimeoutMs 600000
Tools\RunTests.ps1 -Group AngelscriptScenario -Render
Tools\RunTests.ps1 -Group AngelscriptFast -- -log
```

参数说明：

- `-TestPrefix`：按测试名前缀运行
- `-Group`：按 `Config/DefaultEngine.ini` 中定义的 automation group 运行
- `-TimeoutMs`：本次测试超时，必须大于 0 且不超过 `900000`
- `-Label`：输出目录标签
- `-OutputRoot`：自定义输出根目录
- `-Render`：关闭 `-NullRHI`，允许真实渲染
- `-NoReport`：跳过结构化摘要生成
- `-- <ExtraArgs>`：透传额外编辑器命令行参数

## 输出与产物

若不指定 `-OutputRoot`，测试产物默认写入：

```text
Saved/Tests/<Label>/<Timestamp>/
  Automation.log
  Report/
  RunMetadata.json
  Summary.json
```

其中：

- `Automation.log`：编辑器实时日志
- `Report/`：`-ReportExportPath` 导出的结构化结果
- `RunMetadata.json`：本次执行参数、超时、退出码、耗时
- `Summary.json`：供 AI Agent / CI 消费的轻量摘要

## 当前常用测试组

当前仓库已定义的常用 group 以 `Config/DefaultEngine.ini` 为准，典型入口包括：

- `AngelscriptSmoke`
- `AngelscriptNative`
- `AngelscriptRuntimeUnit`
- `AngelscriptFast`
- `AngelscriptScenario`
- `AngelscriptEditor`
- `AngelscriptExamples`

推荐顺序：

1. 快速冒烟：`AngelscriptSmoke`
2. 无 world 的运行时回归：`AngelscriptRuntimeUnit`、`AngelscriptFast`
3. 需要 world / actor / subsystem 的集成回归：`AngelscriptScenario`
4. 编辑器相关：`AngelscriptEditor`

常用具名 suite 以 `Tools\RunTestSuite.ps1 -ListSuites` 的输出为准，当前重点包括：

- `Smoke`
- `NativeCore`
- `RuntimeCpp`
- `Bindings`
- `HotReload`
- `ScenarioSamples`
- `All`

## 与 Gauntlet 的边界

- `Tools\RunTests.ps1` 负责仓库内的标准自动化测试入口、日志、摘要和超时收口。
- `Gauntlet` 只在需要 outer shell、networking、多进程会话编排或更复杂生命周期管理时使用。
- 不要在常规本地回归、AI Agent 执行或普通 CI 里直接跳过 `Tools\RunTests.ps1` 改用手写 `RunUAT` 命令。

## 故障排除

### 构建阶段卡死（XGE 槽位争抢）

测试前通常需要先构建。如果构建阶段长时间无编译输出，可能是多个 worktree 同时使用 XGE 分布式执行器导致槽位争抢。解决方案是在构建时透传 `-NoXGE`：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunBuild.ps1 -- -NoXGE
```

详见 `Documents/Guides/Build.md` 的"多 Worktree 并发构建故障排除"章节。

### 测试进程无输出超时

如果测试启动后长时间没有任何输出：

1. **确认参数名正确**：`-TestPrefix`（不是 `-Filter`）。错误的参数名会导致 PowerShell 在参数绑定阶段挂起，脚本根本不会执行到超时保护代码
2. **确认没有残留的 UBT / Editor 进程**：用 `Tools\Get-UbtProcess.ps1` 检查
3. **确认 worktree 锁未被占用**：同一 worktree 不能同时运行两个 build/test 任务

## 对 AI Agent 的要求

当 AI Agent 需要执行自动化测试时，必须遵守以下要求：

1. 先读取根目录 `AgentConfig.ini`
2. 只通过 `Tools\RunTests.ps1` 执行
3. 必须显式传入或继承一个不超过 `900000ms` 的超时
4. 不得直接拼装 `UnrealEditor-Cmd.exe` 命令作为标准入口
5. 执行时必须实时打印日志
6. 超时或异常退出后必须结束整个进程树
7. 同一 worktree 已有 build/test 运行时，不得重复启动第二个任务

## 推荐提示词

```text
请先读取项目根目录的 AgentConfig.ini，然后仅通过 Tools\RunTests.ps1 执行自动化测试。测试必须显式带超时，且不得超过 900000ms（15 分钟）。执行时必须实时打印编辑器日志，并在超时或异常退出后结束整个进程树。除非明确需要真实渲染，否则保持默认 headless 模式，不要直接手写 UnrealEditor-Cmd.exe 命令。
```
