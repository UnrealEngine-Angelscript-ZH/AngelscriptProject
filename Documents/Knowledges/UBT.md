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

补充限制：

- `-NoEngineChanges` **不能**覆盖 UBT header 阶段最后的 `ExternalExecution.UpdateTimestamps()`。
- 这一步仍会读写 `...\UHT\Timestamp`；对于 shared build environment 的 editor target，它会命中共享 `Engine\Intermediate\Build\...\UHT\Timestamp`。
- 因此，“默认并发模式 + `-NoEngineChanges`”并不等于“绝不会写共享引擎 intermediate”。

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
- 硬上限统一为 `900000ms`（15 分钟），任何配置或命令行参数都不能突破这个上限。
- 默认值：
  - Build：`180000ms`
  - Test：`600000ms`
- 如果超时或脚本异常退出，必须清理对应的进程树，避免残留 `dotnet` / UBT / `UnrealEditor-Cmd.exe` 影响下一轮。

### 2. 并发策略

- **同一 worktree**：只允许一个构建/测试主流程同时运行，脚本层要做 single-flight。
- **不同 worktree + 共享引擎**：
  - 默认允许并发构建，但仅限“不改写引擎输出”的路径。
  - 默认模式应为：直接调用 UBT + `-NoMutex -NoEngineChanges`
- **不同 worktree + XGE executor**：
  - 即使脚本层已经绕开 `Build.bat`、为每次构建指定独立 `UBT.log`，仍可能被 XGE / Incredibuild 的并发槽位限制拦住。
  - `2026-04-05` 的专用 `main` worktree 双并发验证中，第二个 build 明确报出 `Maximum number of concurrent builds reached.`，说明这类失败属于执行器容量问题，而不是 runner 自身互相踩日志或锁。
  - 结论：当目标是验证 worktree 级并发能力，或当日志明确指向 XGE 容量耗尽时，优先使用 runner 的一等参数 `-NoXGE`，先把外部分布式执行器变量排除。
- **需要引擎改动**：
  - 不在默认模式里偷偷等待或放开。
  - 应由脚本提供明确的“串行模式”开关，由引擎级锁串行执行。
- **需要隔离共享 `UHT\Timestamp` / engine intermediates**：
  - 官方上唯一能改路径的是 target 级 `-UniqueBuildEnvironment`
  - 但本仓已明确**禁止使用**：它会把 engine-side generated code / timestamp 改到当前 worktree 私有目录，并触发 worktree 私有的引擎级重编
  - `2026-04-05` 的专用 worktree 实测里，这条路径直接进入约 `3571` actions 的大型构建，因此不符合当前仓库对构建成本的约束
  - 本仓允许的替代方案只保留：
    - `-SerializeByEngine`
    - 独立 `EngineRoot`

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

## 已确认问题：Worktree 下 TargetInfo.json 缺失导致 QueryTargets 超时

### 现象

新建 git worktree 后首次用 `UnrealEditor-Cmd.exe` 跑自动化测试时，编辑器启动后卡在 `Launching UnrealBuildTool... Build.bat -Mode=QueryTargets`，测试根本没开始就超时退出。

### 根因链

1. `Intermediate/` 目录在 `.gitignore` 中被排除，`git worktree add` 不会复制它。
2. 编辑器启动时调用 `FDesktopPlatformBase::GetTargetsForProject`，先检查 `Intermediate/TargetInfo.json` 是否存在且有效。
3. 文件不存在 → 走 `InvokeUnrealBuildToolSync`，经 `Build.bat -Mode=QueryTargets` 调用 UBT。
4. UBT `QueryTargets` 需要用 Roslyn 编译所有 `.Build.cs` / `.Target.cs` 成 `RulesAssembly`，加上 `Build.bat` 自身的锁检测、`DotnetDepends` 检查、`.NET` 环境初始化等开销，整个过程在首次运行时需要 90-120+ 秒。
5. 如果测试超时窗口（之前硬帽 300s，编辑器启动本身已消耗约 28s）不够覆盖 QueryTargets，编辑器被 kill 掉。
6. QueryTargets 被中断 → `TargetInfo.json` 永远不会生成 → 下次启动还会触发 → **恶性循环**。

### 已确认的 worktree TargetInfo.json 状态（2026-04-05）

| 状态 | Worktree 数量 | 说明 |
|------|-------------|------|
| TargetInfo.json 存在 | 19/23 | 全部能正常跑测试 |
| TargetInfo.json 缺失 | 4/23 | 见下表 |

缺失的 worktree：

| Worktree | 原因分析 |
|----------|---------|
| `as238-bool-conversion-port` | 首次启动时超时中断，QueryTargets 未完成 |
| `ue-bind-gap-roadmap` | 从未启动过编辑器 |
| `enable-python-plugin` | `Intermediate/` 目录本身不存在 |
| `enable-python-plugin-mainline` | 从未成功完成 QueryTargets |

### 为什么其他 worktree 能正常跑

**核心结论：TargetInfo.json 是一次性生成的缓存文件，只要历史上有过一次成功的编辑器启动（QueryTargets 完成），后续所有启动都会跳过这一步。**

- 大部分 worktree 在创建后的首次编辑器启动时，给了足够长的超时（或手动启动过编辑器），让 QueryTargets 有时间完成。
- `as238-jitv2-port` 也遇到了同样的问题，但通过手动预热解决：TargetInfo.json 创建于 2026-04-05 11:21:15，随后 `AfterPrewarm` 测试于 11:22:45 成功执行。
- TargetInfo.json 只有 264-276 字节，内容固定（记录项目的 Target 名称和路径），不随代码变更而失效，一旦生成几乎永久有效。

### 已实现的自动修复（2026-04-05）

`Tools/Shared/UnrealCommandUtils.ps1` 提供 `Ensure-TargetInfoJson` 函数，`Tools/RunTests.ps1` 在启动编辑器之前自动调用。

工作流程：

1. 检测 `<ProjectRoot>/Intermediate/TargetInfo.json` 是否存在
2. 若已存在 → 跳过（`Status = Skipped`）
3. 若不存在 → 直接调用 `dotnet UnrealBuildTool.dll -Mode=QueryTargets -Project=... -Output=...`（绕过 `Build.bat`），超时 120 秒
4. 结果记录到 `RunMetadata.json` 的 `Prewarm` 字段和终端输出

绕过 `Build.bat` 的好处：跳过脚本级 `.lock`、`DotnetDepends` 检查、`.NET` 环境自举等开销，只执行实际的 QueryTargets 逻辑。

### 手动解决（备用）

从有 TargetInfo.json 的 worktree（如 main）复制文件：

```powershell
$src = "J:\UnrealEngine\AngelscriptProject\Intermediate\TargetInfo.json"
$dst = "<worktree-path>\Intermediate\TargetInfo.json"
if (!(Test-Path (Split-Path $dst))) { New-Item -ItemType Directory -Path (Split-Path $dst) -Force }
Copy-Item $src $dst
```

## 当前仓库状态（2026-04-04）

- `Tools/Shared/UnrealCommandUtils.ps1`、`Tools/RunBuild.ps1`、`Tools/RunTests.ps1` 与 `Tools/Get-UbtProcess.ps1` 已落地。
- `Tools/Tests/RunToolingSmokeTests.ps1` 与 `Tools/Tests/Helpers/*` 已用于验证 timeout、single-flight、流式输出、进程树清理与命令行解析。
- 当前构建/测试脚本标准化已归档为已完成工作，后续如需调整执行约束，应直接更新脚本与 `Documents/Guides/Build.md` / `Documents/Guides/Test.md`，并视情况补新的 sibling plan。
- `AGENTS_ZH.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md` 这类“强制执行入口”文档暂未切换到新脚本，避免在脚本真正落地前把不存在的命令写成硬规则。
- `RunBuild.ps1` 已将 `-NoXGE` 提升为一等脚本参数，并显式禁止 `-UniqueBuildEnvironment`，避免再次误走 worktree 私有引擎重编路径。
