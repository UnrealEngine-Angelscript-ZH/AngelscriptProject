# UBT / Build.bat 并发与脚本化约束

## 背景

在 `Plan_TestEngineIsolation.md` 的回归过程中，构建与自动化测试本身已经成为阻塞项：

- 原始 `Build.bat` / `Start-Process UnrealEditor-Cmd.exe` 流程缺少统一超时和进程树清理，卡死后会污染下一轮验证。
- 现有文档命令偏向“等命令结束再看结果”，不满足“进入 UBT 后每一行实时可见”的要求。
- 同一台机器上多个 worktree 共用一个引擎目录时，`Build.bat` 和 UBT 的互斥机制会干扰并发开发。

这份文档只记录**已确认的源码事实**和本仓后续要采用的脚本化约束，供后续实现 `Tools/RunBuild.ps1`、`Tools/RunTests.ps1` 与 `Tools/Get-UbtProcess.ps1` 时直接引用。

## 已确认的源码事实

### 1. `Build.bat` 自身就有脚本级互斥锁

`Engine/Build/BatchFiles/Build.bat` 一开始就会创建基于脚本路径的 `.lock` 文件，并在冲突时循环等待。

这意味着即使底层 UBT 允许多实例，**只要入口还是 `Build.bat`，共享引擎目录上的多个 worktree 构建仍会先被 batch 脚本串行化**。

结论：

- 本仓默认构建入口不能再以 `Build.bat` 作为主路径。
- `Build.bat` 只保留为诊断/引导 UBT 自举时的参考，不作为标准日常命令。

### 2. UBT 自身还有一层基于程序集路径的 single-instance mutex

`Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs` 中，UBT 在 single-instance 模式下会基于 `Assembly.GetExecutingAssembly().Location` 创建全局 mutex。

这意味着：

- 多个 worktree 只要共用同一份 `UnrealBuildTool.dll`，默认就会互相等待或直接冲突。
- 如果要允许共享引擎目录上的多 worktree 并发构建，必须显式使用 `-NoMutex`，并由本仓自己的脚本层负责更细粒度的约束。

### 3. `-NoEngineChanges` 正好可用来拦住“共享引擎 + 并发构建”里的危险路径

`Engine/Source/Programs/UnrealBuildTool/Modes/BuildMode.cs` 中，`-NoEngineChanges` 会在执行 action graph 前检查是否会改写现有引擎输出；若会改写，UBT 会直接失败。

对应退出码来自 `Engine/Source/Programs/Shared/EpicGames.Core/UnrealEngineTypes.cs`：

- `FailedDueToEngineChange = 5`

结论：

- 本仓默认并发构建模式应使用 `-NoMutex -NoEngineChanges`。
- 如果命中退出码 `5`，说明这次构建需要改写共享引擎产物，必须切换到“显式串行模式”后重试，而不是继续放开并发。

### 4. `uebp_UATMutexNoWait=1` 不是本问题的默认解法

`Engine/Source/Programs/Shared/EpicGames.Build/Automation/ProcessSingleton.cs` 中的 `uebp_UATMutexNoWait=1` 只影响 AutomationTool / UAT 的单实例逻辑。

它**不解决**以下两层约束：

- `Build.bat` 脚本自己的 `.lock`
- UBT 主程序自己的 single-instance mutex

结论：

- 本仓不把 `uebp_UATMutexNoWait=1` 作为默认方案。
- 如果后续某些 UAT 场景需要它，应作为明确的附加模式处理，而不是日常构建的前提。

### 5. `UnrealEditor-Cmd.exe` 想要实时日志，必须显式打开 stdout 输出

`Engine/Source/Runtime/Core/Private/Misc/OutputDeviceStdOut.cpp` 中，`-FullStdOutLogOutput` 会把 stdout 的允许日志级别提升到 `All`。

结论：

- 自动化测试脚本必须至少带：
  - `-stdout`
  - `-FullStdOutLogOutput`
  - `-UTF8Output`
- 脚本层还要把 stdout/stderr 做逐行 tee 到控制台和日志文件，不能再走 `Out-String` 或 `Start-Process -RedirectStandardOutput` 这种“最后一次性看结果”的模式。

## 本仓已确认的脚本约束

### 1. 超时策略

- 所有构建和测试脚本必须有**显式超时**。
- 硬上限统一为 `300000ms`，任何配置或命令行参数都不能突破这个上限。
- 默认值：
  - Build：`180000ms`
  - Test：`300000ms`
- 如果超时或脚本异常退出，必须清理对应的进程树，避免残留 `dotnet` / UBT / `UnrealEditor-Cmd.exe` 影响下一轮。

### 2. 并发策略

- **同一 worktree**：只允许一个构建/测试主流程同时运行，脚本层要做 single-flight。
- **不同 worktree + 共享引擎**：
  - 默认允许并发构建，但仅限“不改写引擎输出”的路径。
  - 默认模式应为：直接调用 UBT + `-NoMutex -NoEngineChanges`
- **需要引擎改动**：
  - 不在默认模式里偷偷等待或放开。
  - 应由脚本提供明确的“串行模式”开关，由引擎级锁串行执行。

### 3. 日志与产物策略

- Build：脚本日志目录需要唯一化，保证多 worktree / 多次执行互不覆盖。
- Test：每次执行必须唯一化：
  - `-ABSLOG`
  - `-ReportExportPath`
- 目标是同时满足：
  - 终端逐行可见
  - 本地有完整日志可追溯
  - 多实例互不覆盖

### 4. 进程可观测性

需要一个独立脚本查询“当前是否有 UBT 在跑，以及它属于哪个 worktree”。

最少要输出：

- `ProcessId`
- `ParentProcessId`
- `Name`
- `StartTime`
- `EngineRoot`
- `ProjectFile`
- `WorktreeRoot`
- `Branch`
- `Kind`
- `CommandLine`

## 后续实现边界

计划中的实现文件：

- `Tools/Shared/UnrealCommandUtils.ps1`
- `Tools/RunBuild.ps1`
- `Tools/RunTests.ps1`
- `Tools/Get-UbtProcess.ps1`
- `Tools/ResolveAgentCommandTemplates.ps1`
- `Tools/GenerateAgentConfigTemplate.bat`

对应计划文档：

- `Documents/Plans/Archives/Plan_BuildTestScriptStandardization.md`

## 当前仓库状态（2026-04-04）

- `Tools/Shared/UnrealCommandUtils.ps1`、`Tools/RunBuild.ps1`、`Tools/RunTests.ps1` 与 `Tools/Get-UbtProcess.ps1` 已落地。
- `Tools/Tests/RunToolingSmokeTests.ps1` 与 `Tools/Tests/Helpers/*` 已用于验证 timeout、single-flight、流式输出、进程树清理与命令行解析。
- 当前构建/测试脚本标准化已归档为已完成工作，后续如需调整执行约束，应直接更新脚本与 `Documents/Guides/Build.md` / `Documents/Guides/Test.md`，并视情况补新的 sibling plan。
- `AGENTS_ZH.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md` 这类“强制执行入口”文档暂未切换到新脚本，避免在脚本真正落地前把不存在的命令写成硬规则。
