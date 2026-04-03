# Angelscript 启动 Bind 与目录监视验证矩阵

## 目标

本文档冻结 `Plan_AngelscriptEngineBindAndFileWatchValidation.md` 的首轮验证口径，先统一“要测什么、当前已有什么、缺什么”，再继续补测试 seam、行为回归和性能采样。本文档服务对象是 `Plugins/Angelscript/` 插件本身，而不是宿主工程逻辑。

## 建模前提

- 引擎创建轴固定为 `Full`、`Clone`、`CreateForTesting()` 三种模式。
- 启动入口轴固定为 `StartupModule()`、Subsystem 主引擎附着、Runtime fallback tick 三条路径。
- 目录监视轴固定为 `.as` 文件 `add`、`modify`、`remove`、folder `add`、folder `remove`、rename-window 六类输入。
- rename 现状统一按 `removed + added + 0.2s deletion delay` 建模，不把显式 rename 事件当作当前能力基线。
- reload 结果轴固定为 `soft reload`、`full reload`、`generated class/struct/delegate`、`diagnostics`、`artifact output` 五类输出。
- 本轮先建立可回归的正确性矩阵与性能记录口径，不在第一阶段扩展 packaged/cooked、跨平台 watcher 后端差异或重量级 Functional Test 依赖。

## 核心验证矩阵

| 验证轴 | 当前基线 | 现有锚点 | 当前空白 |
| --- | --- | --- | --- |
| 创建模式与启动 bind | 已有部分覆盖 | `AngelscriptRuntime/Tests/AngelscriptMultiEngineTests.cpp`、`AngelscriptTest/Core/AngelscriptBindConfigTests.cpp` | 缺实际 startup bind 执行次数、顺序、Clone 去重观测 |
| 启动入口与主引擎附着 | 已有部分覆盖 | `AngelscriptRuntime/Tests/AngelscriptSubsystemTests.cpp`、`AngelscriptRuntime/Tests/AngelscriptDependencyInjectionTests.cpp` | 缺 `StartupModule` / Subsystem / fallback tick 的统一 smoke matrix |
| 启动 bind 生产引擎烟雾层 | 已有基础覆盖 | `AngelscriptTest/Core/AngelscriptEngineParityTests.cpp` | 缺 bind family 级别的代表性可见性断言 |
| 共享状态与测试隔离 | 已有基础覆盖 | `AngelscriptTest/Shared/AngelscriptTestEngineHelperTests.cpp`、`AngelscriptTest/Core/AngelscriptEngineCoreTests.cpp` | 缺“bind/type 状态 reset + recreate 幂等”正式纳管 |
| watcher 输入到 reload 队列 | 已有零散覆盖 | `AngelscriptTest/HotReload/AngelscriptHotReloadFunctionTests.cpp` 的 `HotReload.ModuleWatcherQueuesFileChanges` | 缺 callback 层 deterministic seam、folder add/remove、rename-window、storm 去重 |
| 文件系统发现与模块映射 | 已有基础覆盖 | `AngelscriptTest/FileSystem/AngelscriptFileSystemTests.cpp` | 缺 rename 后 module lookup、skip rule + watcher root 组合回归 |
| reload 分析与 generated symbol 行为 | 已有基础覆盖 | `AngelscriptTest/HotReload/AngelscriptHotReloadAnalysisTests.cpp`、`Compiler/AngelscriptCompilerPipelineTests.cpp` | 缺 generated class / struct / delegate rename 与切换正确性 |
| 失败反馈与旧代码保留 | 已有局部覆盖 | HotReload 既有测试与引擎失败路径 | 缺 diagnostics、`PreviouslyFailedReloadFiles`、stale-code fallback 明确回归 |
| 真实脚本语料热重载 | 已有初始覆盖 | `AngelscriptTest/Angelscript/AngelscriptNativeScriptHotReloadTests.cpp` | 缺执行后可见性、lookup 与重复 reload 稳定性断言 |
| 结果产物写出 | 已有邻近模式 | `AngelscriptRuntime/Tests/AngelscriptCodeCoverageTests.cpp`、`AngelscriptRuntime/Tests/AngelscriptPrecompiledDataTests.cpp` | 缺性能摘要/报告目录结构与落盘回归 |

## 分层执行视图

### 启动 Bind 正确性层

- `MultiEngine`：负责 `Full / Clone / CreateForTesting` 模式、共享状态和生命周期。
- `BindConfig`：负责禁用项、排序、命名/未命名 bind 的配置可见性。
- `Subsystem / DependencyInjection`：负责主引擎归属、入口与 attach 事实。
- `Parity`：负责生产引擎表面是否暴露出关键 bind family。

### Watcher 与 Reload 正确性层

- Editor callback 层负责 `FFileChangeData -> queue` 的 deterministic 语义，不承担 generated class 断言。
- `HotReload` 层负责队列消费、compile/reload 结果、diagnostics 与旧代码保留语义。
- `FileSystem` 层负责真实磁盘状态、module lookup、skip rule 和路径归一化。
- `ClassGenerator` 层负责 generated class / struct / delegate、旧类隐藏与替换后的可见性。

### 邻近支撑层

- `Shared.EngineHelper.*` 负责共享引擎隔离、global scope/world context 恢复与脏状态防泄漏。
- `Parity.*` 负责生产引擎 compile smoke，防止测试专用引擎与真实运行态出现偏差。
- `NativeScriptHotReload.*` 负责真实脚本语料回归，避免 watcher/hot reload 只在 synthetic fixture 上成立。
- 结果产物落盘测试负责验证报告与性能摘要不是“一次性人工日志”。

## rename-window 基线说明

- 当前实现默认把 rename 解释为“旧文件 removed，新文件 added，并在 0.2 秒删除延迟窗口内完成归并判断”。
- callback 层测试只锁定这套已存在语义，不提前把平台显式 rename 事件写成断言前提。
- 如果未来某个平台 watcher 后端开始稳定提供 rename 事件，应新增 sibling plan，而不是直接修改本矩阵基线。

## 当前优先补齐项

1. 启动 bind 生命周期观测 helper，使测试能看见实际执行的 bind 集合、顺序和 Clone 复用行为。
2. Editor callback 的最小 deterministic seam，使 synthetic `FFileChangeData` 能进入真实 `OnScriptFileChanges()` 处理路径。
3. 统一的性能产物与指标口径，使 startup bind / reload baseline 能稳定记录并比较。
4. generated class / struct / delegate 的 rename 和类切换闭环断言。
5. watcher 失败路径的 feedback 与 stale-code fallback 回归。

## 快速结论

- 现有仓库已经具备 `MultiEngine`、`BindConfig`、`HotReload`、`FileSystem`、`Shared`、`Parity`、`NativeScriptHotReload` 七块高价值基础。
- 当前真正缺的是把这些测试层组织成统一矩阵，并补上 startup bind 观测 seam、callback seam、性能基线和 generated symbol rename 闭环。
- 后续新增测试必须继续遵循目录分层：`Runtime/Tests` 管启动与状态机，`AngelscriptTest/Core` 管插件级核心回归，`HotReload` 管消费与场景，`FileSystem` 管磁盘映射，`Editor/Private/Tests` 管 callback 输入输出。

## 分层执行模板（Phase 6）

| 层级 | 推荐命令前缀 | 典型用途 |
| --- | --- | --- |
| 快速烟雾层 | `Automation RunTests Angelscript.CppTests.MultiEngine+Angelscript.TestModule.Engine.BindConfig+Angelscript.TestModule.Shared.EngineHelper+Angelscript.TestModule.Core.Parity` | PR 前快速确认启动链路与 bind 表面可见性 |
| 功能正确性层 | `Automation RunTests Angelscript.Editor.DirectoryWatcher+Angelscript.TestModule.HotReload+Angelscript.TestModule.ScriptClass+Angelscript.TestModule.FileSystem` | callback 队列、reload 行为、generated class 与文件映射回归 |
| 真实语料层 | `Automation RunTests Angelscript.TestModule.Angelscript.NativeScriptHotReload` | `Script/Tests/*.as` 真实语料热重载探针 |
| 长时压力层 | `Automation RunTests Angelscript.TestModule.Core.Performance.Startup+Angelscript.TestModule.HotReload.Performance` | 阶段收口/夜间性能与 burst churn 观测 |
| 产物验证层 | `Automation RunTests Angelscript.TestModule.Core.Performance.ArtifactGeneration+Angelscript.CppTests.CodeCoverage.HtmlReport.Generation` | 验证性能/报告产物落盘结构稳定 |

### 执行注意事项

- 每层建议独立 `-ABSLOG` 与 `-ReportExportPath`，保留可追溯产物。
- rename 行为仍按 `removed + added + 0.2s delay` 基线解释；若平台差异存在，优先依赖 callback deterministic 层断言。
- 长时压力层不默认放入每次 PR 快速回归，应在文档执行模板中单独标注触发时机。

### 首轮执行快照（2026-04-03）

- `Angelscript.TestModule.Angelscript.NativeScriptHotReload`：`P5_NativeScriptHotReload_Rerun2/Reports/index.json`，`Phase2A/2B/2C` 全部成功。
- `Angelscript.TestModule.ScriptClass.RenameReplacesOldClass`：`P5_ClassRename_Rerun2/Reports/index.json`，成功。
- `Angelscript.TestModule.HotReload.AddModifyLookupFlow`：`P5_AddModifyLookupFlow_Rerun2/Reports/index.json`，成功。
- `Angelscript.TestModule.HotReload.FailureKeepsOldCodeAndDiagnostics`：`P5_FailureKeepsOldCode_Rerun2/Reports/index.json`，成功。

### Phase 6 分层执行快照（2026-04-03）

- 快速烟雾层：`P6_MultiEngine`、`P6_DependencyInjection`、`P6_Subsystem`、`P6_BindConfig`、`P6_Parity` 全部完成，`failed=0`。
- 功能正确性层：`P6_EditorDirectoryWatcher`、`P6_FileSystem`、`P6_ScriptClass` 成功；`P6_HotReload` 因 `Angelscript.TestModule.HotReload.Performance.BurstChurnLatency` 失败（需要 full reload 但当前执行窗口不可用）。
- 真实语料层：`P6_NativeScriptHotReload` 成功（`failed=0`）。
- 长时压力层：`P6_PerfStartup` 成功；`P6_PerfHotReload` 同样在 `BurstChurnLatency` 失败。
- 产物验证层：`P6_PerfArtifactGeneration` 成功（`failed=0`）。

### Phase 6 文档状态

- [x] `Test.md` 已登记分层命令与执行顺序，并补全 P6 全波次执行证据。
- [x] `TestCatalog.md` 已登记分层回归矩阵与关键测试入口。
- [x] `TestPerformance.md` 已纳入 P5/P6 相关报告产物示例。
- [x] 已记录首轮已知不稳定项：`BurstChurnLatency` 在当前执行环境要求 full reload 时会失败。
