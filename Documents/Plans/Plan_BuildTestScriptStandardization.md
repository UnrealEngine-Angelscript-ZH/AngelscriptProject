# 构建与测试脚本标准化计划

## 背景与目标

### 背景

`Plan_TestEngineIsolation.md` 的后续执行已经暴露出一个独立问题：在 runtime 去全局化真正开始前，当前仓库的构建/测试执行方式本身就不够稳定。

现有问题集中在以下几类：

- `Build.bat` 与 UBT 的互斥逻辑会让共享引擎目录上的多个 worktree 互相卡住。
- 现有命令模板不能做到“进入 UBT 后逐行实时输出”。
- 构建与测试缺少统一的超时、进程树清理和唯一化日志策略。
- 没有一个现成工具能快速回答“当前有无 UBT 在跑，它属于哪个 worktree”。

### 目标

在不碰 runtime 去全局化主线代码的前提下，先补齐一套**脚本级执行基础设施**，为后续 `Plan_TestEngineIsolation.md` 的编译、测试、全量回归提供稳定入口。

目标状态：

- 默认构建入口改为直接调用 `dotnet <EngineRoot>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll`
- 同一 worktree 构建/测试 single-flight；不同 worktree 默认允许共享引擎并发构建
- 默认并发构建带 `-NoMutex -NoEngineChanges`，引擎改动场景必须显式切到串行模式
- 所有构建/测试脚本都强制超时，且超时或异常退出后清理进程树
- 终端实时逐行可见 stdout/stderr，同时保留唯一化日志与测试报告
- 能查询当前 UBT 进程属于哪个 worktree / 分支

### 范围与边界

- 本计划只覆盖 `Tools/` 和相关文档，不处理 Angelscript runtime / test helper 本身的去全局化实现。
- 在脚本真正可用前，不把 `AGENTS_ZH.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md` 改成“必须使用新脚本”，避免文档先于实现。
- 共享引擎并发的安全边界以 “项目/插件输出可并发，引擎输出必须显式串行” 为准。

## 当前事实状态快照（2026-04-04）

- `Tools/RunBuild.ps1`、`Tools/RunTests.ps1`、`Tools/Get-UbtProcess.ps1`、`Tools/Shared/UnrealCommandUtils.ps1` 目前都**还不存在**。
- 已经补了 TDD 起点：
  - `Tools/Tests/RunToolingSmokeTests.ps1`
  - `Tools/Tests/Helpers/WriteLines.ps1`
  - `Tools/Tests/Helpers/SpawnSleepTree.ps1`
- 当前 smoke tests 预期是红的，因为它们依赖的共享执行层还没落地。
- 关于 `Build.bat`、UBT mutex、`-NoEngineChanges`、stdout 输出等源码事实，统一记录在 `Documents/Knowledges/UBT.md`。

## 分阶段执行计划

---

### Phase 1：共享执行层与构建入口

> 目标：先把 timeout / tee / lock / process-tree kill 这些横切能力收口到 `Tools/Shared`，再让构建脚本改用 direct-Ubt 模式。

- [ ] **P1.1** 实现 `Tools/Shared/UnrealCommandUtils.ps1`
  - 这是后续所有脚本的公共依赖，负责统一解析 `AgentConfig.ini`、解析 `EngineRoot` / `ProjectFile` / 默认超时、生成唯一日志目录、构建 worktree / engine 级锁名、流式运行子进程并逐行输出到控制台和日志文件。
  - 需要把 `300000ms` 硬上限写死在共享层里，避免每个脚本各自实现一套 timeout 校验逻辑；任何超过上限的配置或参数都应直接失败，而不是悄悄放宽。
  - 进程运行器需要处理 stdout/stderr 双流、超时、显式 tree kill、退出码回传，以及后续 `RunTests.ps1` 需要的日志 tee 能力。
- [ ] **P1.1** 📦 Git 提交：`[Tools] Feat: add shared Unreal command utilities`

- [ ] **P1.2** 实现 `Tools/RunBuild.ps1`
  - 默认构建入口直接调用 `dotnet` + `UnrealBuildTool.dll`，不再走 `Build.bat` 主路径。
  - 默认参数要包含 `-NoMutex -NoEngineChanges`，让共享引擎目录上的不同 worktree 能并发构建，同时在需要改写引擎输出时快速失败。
  - 同一 worktree 要用 single-flight 锁防止重复触发；当用户显式进入串行模式时，再额外获取引擎级锁，保证“需要引擎改动的构建”可以安全排队。
  - 退出码需要明确区分成功、一般失败、timeout、same-worktree 冲突、配置/启动失败、以及命中 `FailedDueToEngineChange`。
- [ ] **P1.2** 📦 Git 提交：`[Tools] Feat: add direct-Ubt build runner`

- [ ] **P1.3** 接通命令模板与本地配置模板
  - `Tools/ResolveAgentCommandTemplates.ps1` 要改为输出基于 `RunBuild.ps1` / `RunTests.ps1` / `Get-UbtProcess.ps1` 的标准命令模板，不再输出裸 `Build.bat` 或裸 `Start-Process UnrealEditor-Cmd.exe`。
  - `Tools/GenerateAgentConfigTemplate.bat` 要增加 `Build.DefaultTimeoutMs=180000` 与 `Test.DefaultTimeoutMs=300000`，并在注释里明确“脚本硬上限仍为 300000ms”。
  - 这一阶段只改模板脚本，不修改用户指引文档中的强制入口描述；待实际脚本验证完成后再切文档。
- [ ] **P1.3** 📦 Git 提交：`[Tools] Refactor: point command templates to script runners`

---

### Phase 2：测试入口与进程观测

> 目标：把自动化测试入口和 UBT 进程查询补齐，解决“实时输出、唯一化报告、进程归属可观测”三个问题。

- [ ] **P2.1** 实现 `Tools/RunTests.ps1`
  - 保留现有常用参数表面：`-TestPrefix`、`-Label`、`-OutputRoot`、`-NoReport`，并新增 `-TimeoutMs` 与 `-Render`。
  - 默认走 headless 路径，即 `-NullRHI`；只有显式 `-Render` 才切换到图形模式。
  - 每次运行都要唯一化 `-ABSLOG` 和 `-ReportExportPath`，保证多轮执行互不覆盖；同时必须带 `-stdout -FullStdOutLogOutput -UTF8Output`，让终端与日志都能逐行看到实时结果。
  - 测试结果优先从 `index.json` 汇总，必要时再回退到日志解析，避免单看进程退出码导致“测试失败但脚本仍返回成功”。
- [ ] **P2.1** 📦 Git 提交：`[Tools] Feat: add streamed automation test runner`

- [ ] **P2.2** 实现 `Tools/Get-UbtProcess.ps1`
  - 该脚本要把运行中的 UBT 相关进程映射回 worktree / branch / project 文件路径，便于回答“当前哪个 worktree 在占用共享引擎”。
  - 最小输出字段应包含：`ProcessId`、`ParentProcessId`、`Name`、`StartTime`、`EngineRoot`、`ProjectFile`、`WorktreeRoot`、`Branch`、`Kind`、`CommandLine`。
  - 进程识别上至少要覆盖 direct `dotnet ... UnrealBuildTool.dll` 和 `Build.bat` 包装场景；允许把这部分逻辑做成 `Tools/Shared` 内的纯解析函数，便于 smoke tests 直接覆盖。
- [ ] **P2.2** 📦 Git 提交：`[Tools] Feat: add UBT process query by worktree`

- [ ] **P2.3** 让现有 `Tools/Tests/RunToolingSmokeTests.ps1` 变绿
  - 当前已经存在的 smoke test 脚手架，需要覆盖 timeout 上限、same-worktree lock、逐行流式输出、timeout 后进程树清理，以及命令行到 worktree 的解析。
  - 先以脚本级快速验证为主，不引入长时间的真实 UE build/test；所有验证命令都必须显式带 timeout，且不超过 `300000ms`。
  - 这一步完成后，才说明脚本层基础设施真正可用。
- [ ] **P2.3** 📦 Git 提交：`[Tools] Test: validate shared runner smoke cases`

---

### Phase 3：文档切换与主计划对接

> 目标：在脚本真正稳定可用之后，再把用户指引文档切换到“必须通过脚本执行”的新规范。

- [ ] **P3.1** 同步中文优先文档
  - 先更新中文文档：`AGENTS_ZH.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md`、`Documents/Tools/Tool.md`。
  - 这一步需要把“构建/测试必须通过脚本执行”“build 默认 180000ms，test 默认 300000ms，硬上限 300000ms”“超时后清理进程树”“默认 direct-Ubt 并发模式与显式串行模式”写成明确规则。
  - 还要把 `Documents/Knowledges/UBT.md` 作为知识说明入口链接进去，避免执行规则和源码原因分散在不同文档里。
- [ ] **P3.1** 📦 Git 提交：`[Docs] Docs: standardize Chinese build and test script entrypoints`

- [ ] **P3.2** 再同步英文总纲与主计划状态
  - 中文文档完成后，再同步 `AGENTS.md`。
  - 同时在 `Documents/Plans/Plan_TestEngineIsolation.md` 中补一条状态更新，说明脚本标准化已落地，可作为后续 runtime 去全局化验证入口。
  - 只有在这一阶段完成后，仓库才真正进入“所有构建与测试都必须通过脚本执行”的状态。
- [ ] **P3.2** 📦 Git 提交：`[Docs] Docs: sync English agent rules with script-based execution`

## 验收标准

- `Tools/RunBuild.ps1` 默认使用 direct UBT，且并发 worktree 在共享引擎目录上不会被 `Build.bat` 锁串行化。
- 构建脚本默认带 `-NoMutex -NoEngineChanges`，命中引擎改动路径时能以明确失败码和清晰提示退出。
- `Tools/RunTests.ps1` 能逐行实时输出 UE 日志，并为每次运行生成唯一的 `-ABSLOG` / `-ReportExportPath`。
- 所有 build/test 入口都强制 timeout，且 timeout 或异常退出后不会留下可复现的残留 UBT / `UnrealEditor-Cmd.exe` 进程树。
- `Tools/Get-UbtProcess.ps1` 能回答“当前哪个 worktree / branch 正在跑 UBT”。
- `Tools/Tests/RunToolingSmokeTests.ps1` 在短超时下通过。
- 只有在脚本验证完成后，中文与英文执行指南才切换为强制脚本入口。

## 风险与注意事项

- `Build.bat` 与 UBT 是两层不同的互斥机制，后续实现时不要只处理其中一层。
- `-NoEngineChanges` 的失败不是普通编译错误，而是默认并发模式故意设下的安全阀；脚本和文档里必须把这层语义讲清楚。
- `UnrealEditor-Cmd.exe` 的实时输出依赖 `-stdout -FullStdOutLogOutput -UTF8Output`，缺任何一层都可能退化成“卡住等结果”。
- same-worktree single-flight 与 cross-worktree concurrency 是两个不同层次的约束，后续实现时不要混成同一个全局锁。
- 目前 `Tools/Tests/` 只是 TDD 起点，不代表脚本层已实现，也不能据此修改正式执行指南。
