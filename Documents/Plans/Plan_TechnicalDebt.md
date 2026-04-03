# Angelscript 插件技术债清理计划

## 背景与目标

### 背景

当前仓库的主目标不是继续扩展宿主工程 `Source/AngelscriptProject/`，而是把 `Plugins/Angelscript/` 收敛成可独立复用、可持续验证的 Unreal Engine Angelscript 插件。此前的技术债计划已经覆盖了不少真实问题，但经过本轮代码与文档复核后，可以确认它把**未解决问题、已落地能力、以及仅适合先审计后实现的事项混在了一起**，继续按旧口径执行会重复规划已经完成的工作。

本计划启动时重新收敛后的事实包括：

> 注：以下背景事实与后文“计划入口事实快照”用于记录本计划启动时的入口审计基线；当前关闭状态以后续 Phase 验证快照、checkbox 完成状态与 `Documents/Guides/TechnicalDebtInventory.md` 为准。

1. **构建兼容性风险仍存在**：`Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h:732-740` 仍无条件声明 `TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 特化，且保留了 `//WILL-EDIT` 标记；`Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs:28-29` 仍无条件 `OptimizeCode = CodeOptimization.Never`。
2. **运行时安全待办注释仍未收口**：`Bind_BlueprintEvent.cpp:144,169,343,353` 仍缺事件/委托签名校验；`AngelscriptDebugServer.cpp:161,185` 仍有线程挂起与异常路径并发保护待办注释；`StaticJIT/StaticJITHeader.cpp:255` 仍明确写着异常路径对象销毁待审计。
3. **弃用 API 与警告压制尚未清理干净**：`ThirdParty/angelscript/source/as_string.h:108` 仍使用 `FCrc::Strihash_DEPRECATED`；`StaticJIT/StaticJITHeader.h:65` 仍在文件级使用 `PRAGMA_DISABLE_DEPRECATION_WARNINGS`。
4. **测试基础设施债务重心已经变化**：`Internals/AngelscriptMemoryTests.cpp` 已经有 5 个内存相关回归；`Internals/AngelscriptRestoreTests.cpp` 已经覆盖 round-trip 与 strip-debug-info 正向路径。当前债务不再是“从零补测试”，而是**补负向边界、把现有基线写进文档并维持不回退**。
5. **测试 helper 命名债务仍然很重**：`Shared/AngelscriptTestUtilities.h:86-147` 仍保留 `GetSharedTestEngine()`、`GetSharedInitializedTestEngine()`、`GetResetSharedTestEngine()`、`GetProductionEngine()` 等语义重叠入口。源码扫描显示这些 helper 名称在 `Plugins/Angelscript/Source/AngelscriptTest/` 下约有 306 处引用，分布于 66 个文件。
6. **全局状态依赖仍在插件核心路径中**：`Core/AngelscriptEngine.h:118-123` 仍公开 `FAngelscriptEngine::Get()` 与 `CurrentWorldContext`；运行时调用点仍出现在 `Bind_SystemTimers.cpp`、`Bind_UUserWidget.cpp`、`AngelscriptGameInstanceSubsystem.cpp`、`ClassGenerator` 与测试辅助路径。
7. **Bind 差距必须先审计再收敛**：当前仓库里能直接确认的只有 `Bind_BlueprintType.cpp:156-157` 仍注释掉 `CPF_TObjectPtr` 相关判断；`Bind_TOptional.cpp`、`Bind_UStruct.cpp`、`Bind_Delegates.cpp` 等文件与 Hazelight/UEAS2 参考源的差距，需要先建立符号级 diff 矩阵，不能继续沿用旧计划里未经复核的“固定缺口列表”。
8. **测试文档口径已经漂移**：`Documents/Guides/TestCatalog.md` 仍记录 `275/275 PASS` 文档化基线，但当前源码扫描已能匹配到 324 处 automation/spec 定义，说明后续技术债治理必须先把“已编目基线”和“实时清单”分开管理。

### 目标

1. 把 `Plan_TechnicalDebt.md` 收敛成一份**只保留真实未完成事项**的执行计划。
2. 优先解决高影响、低到中风险的构建/运行时稳定性问题，避免继续把崩溃点留在插件主线上。
3. 维持并固化“无显式弃用 API 回归”的基线，缩小全局警告压制范围。
4. 将已有内测能力（Memory / Restore）从“隐含事实”提升为“有文档、有验收口径、有负向边界”的正式基线。
5. 分层完成测试 helper 命名迁移，消除当前共享引擎/生产引擎/helper scope 语义混杂的问题。
6. 把 Bind 差距工作从“猜测式 backlog”改成“参考源审计 + 低风险优先落地 + 高风险拆分 sibling plan”的节奏。
7. 对 `FAngelscriptEngine::Get()` / `CurrentWorldContext` 做收口与隔离，而不是在本计划里偷偷膨胀成跨模块大重构。
8. 确保所有实施阶段都严格围绕插件边界推进，并同步更新相关文档与验证入口。

## 范围与边界

- **纳入范围**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/`、`Plugins/Angelscript/Source/AngelscriptEditor/`、`Plugins/Angelscript/Source/AngelscriptTest/`
  - 与上述改动直接相关的 `Documents/Guides/Build.md`、`Documents/Guides/Test.md`、`Documents/Guides/TestCatalog.md`
  - 与 Bind 审计、测试重构、边界收口直接相关的 sibling plan 同步说明
- **不纳入范围**
  - `Source/AngelscriptProject/` 的业务功能扩展或玩法开发
  - 整体 ThirdParty 一步升级到完整 AngelScript `2.38.0`
  - `lambda / anonymous function` 路线（交由 `Documents/Plans/Plan_AS238LambdaPort.md`）
  - 一次性完成 `FAngelscriptEngine::Get()` 的全量去全局化重写
  - 任何写死本地绝对路径的执行说明；需要本地配置的路径统一通过 `AgentConfig.ini` 获取

## 计划入口事实快照

### 已验证的高优先级债务

- **构建兼容性**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.h:732-740`：`TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 仍无条件特化
  - `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs:28-29`：`OptimizeCode = CodeOptimization.Never` 带开发期调试注释
- **运行时安全**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintEvent.cpp:144,169,343,353`：`ProcessEvent` / Delegate 调用前未做签名一致性检查
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Debugging/AngelscriptDebugServer.cpp:161,185`：线程上下文与异常路径并发保护未完成
  - `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/StaticJITHeader.cpp:255`：异常路径对象生命周期待审计
- **弃用 API / 警告压制**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_string.h:108`：`FCrc::Strihash_DEPRECATED`
  - `Plugins/Angelscript/Source/AngelscriptRuntime/StaticJIT/StaticJITHeader.h:65`：文件级 `PRAGMA_DISABLE_DEPRECATION_WARNINGS`
- **测试基础设施**
  - `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h:86-147`：helper 命名与职责边界不够显式
  - `Plugins/Angelscript/Source/AngelscriptTest/`：helper 旧命名约 306 处引用，横跨 66 个测试文件
- **全局状态**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptEngine.h:118-123`：全局引擎与世界上下文仍为静态入口
  - `Bind_SystemTimers.cpp`、`Bind_UUserWidget.cpp`、`AngelscriptGameInstanceSubsystem.cpp` 等仍直接依赖 `CurrentWorldContext`

### 已验证“不是空白”的区域

- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptMemoryTests.cpp`
  - 已覆盖 Construction、FreeUnused、ScriptNodeReuse、ByteInstructionReuse、PoolLeakTracking 五类回归
- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptRestoreTests.cpp`
  - 已覆盖 round-trip 与 strip-debug-info round-trip 正向路径
- `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_module.cpp:1731-1745`
  - `SaveByteCode()` 在当前带 compiler 的构建路径走正常写入，`asNOT_SUPPORTED` 只在 `AS_NO_COMPILER` 条件编译分支出现

### 需要先审计、不能直接下结论的区域

- `Bind_TOptional.cpp`、`Bind_UStruct.cpp`、`Bind_BlueprintType.cpp`、`Bind_AActor.cpp`、`Bind_USceneComponent.cpp`、`Bind_Delegates.cpp`、`Bind_FHitResult.cpp`、`Bind_FMath.cpp`、`Bind_FVector2f.cpp`
  - 这些文件与 Hazelight/UEAS2 参考源的差距需要先建立审计矩阵，再决定哪些仍属于本计划
- `Source/AngelscriptProject/AngelscriptProject.Build.cs:11`
  - 当前宿主工程模块仍依赖 `EnhancedInput`，需要确认是否仍符合“宿主尽量最小化”的定位
- `Documents/Guides/TestCatalog.md`
  - 文档化基线仍为 `275/275 PASS`，但源码 live inventory 已明显超出这一口径，需要拆分“编目基线”与“实时扫描”

## 债务分级与执行策略

为避免再次把高风险重构和低风险修补揉成一团，本计划固定使用“影响 × 风险”的四象限来安排顺序。

| 分类 | 当前事项 | 执行策略 |
| --- | --- | --- |
| **立即修复**（高影响 / 低到中风险） | `TBaseStructure` 条件保护、`OptimizeCode` 条件化、BlueprintEvent 签名校验、DebugServer 并发保护、StaticJIT 异常路径、`Strihash_DEPRECATED` 替换、缩小警告压制范围 | 直接纳入本计划主线，要求测试与文档同步 |
| **分层推进**（高影响 / 高风险） | helper 命名迁移、全局状态收口 | 只允许分层迁移，不允许一次性大重写 |
| **先审计再实现**（中影响 / 高不确定） | Bind 差距、宿主工程边界最小化 | 先做矩阵，低风险项留本计划，高风险项拆 sibling plan |
| **顺手收口**（低影响 / 低风险） | `Bind_UEnum` stale 性能债务退役、文档基线同步 | 与相邻 Phase 一起做，不单独开大 Phase |

## 验证命令基线

所有实现阶段都统一引用仓库现有指南，不在 Plan 中写死本地机器路径。

### 构建命令模板

- 先读取项目根目录 `AgentConfig.ini` 的 `Paths.EngineRoot`
- `Paths.ProjectFile` 为空时，默认回退到仓库根目录下的 `AngelscriptProject.uproject`
- `Build.EditorTarget` / `Build.Platform` / `Build.Configuration` / `Build.Architecture` 未显式配置时，分别回退到 `AngelscriptProjectEditor` / `Win64` / `Development` / `x64`
- 参考 `Documents/Guides/Build.md` 组装 PowerShell 命令：

```powershell
powershell.exe -Command "& '<EngineRoot>\Engine\Build\BatchFiles\Build.bat' <EditorTarget> <Platform> <Configuration> '-Project=<ProjectFile>' -WaitMutex -FromMsBuild -architecture=<Architecture> 2>&1 | Out-String"
```

### 自动化测试命令模板

- 先读取 `AgentConfig.ini` 中的 `Paths.EngineRoot` 与默认超时（若存在 `Test.DefaultTimeoutMs` 则优先使用）
- `Paths.ProjectFile` 为空时同样回退到仓库根目录下的 `AngelscriptProject.uproject`
- `Test.DefaultTimeoutMs` 未配置时，默认回退到 `600000ms`
- 参考 `Documents/Guides/Test.md` 使用 `UnrealEditor-Cmd.exe` + `Start-Process -Wait -NoNewWindow`

```powershell
powershell.exe -Command "Start-Process -FilePath '<EngineRoot>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"<ProjectFile>\"','-ExecCmds=\"Automation RunTests <TestName>; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

### 回归执行纪律

- 每个 Phase 先跑**本 Phase 新增或修改的目标测试**，再跑更大范围回归
- 涉及 `Shared/AngelscriptTestUtilities.h`、`Core/AngelscriptEngine.*`、`Binds/*`、`ThirdParty/*` 的改动，最终都要补跑完整 `Angelscript.TestModule` 回归
- 只要测试入口、目录结构或基线口径发生变化，就必须同步更新 `Documents/Guides/TestCatalog.md`

## 分阶段执行计划

### Phase 0：重新基线化与审计冻结

> 目标：先把“哪些债务还开着、哪些已经部分完成、哪些只能先审计再决定”固定下来，避免后续执行继续基于过期事实。

- [x] **P0.1** 核对本地执行前置配置并固定 fallback 规则
  - 先检查 `AgentConfig.ini` 是否已配置 `Paths.EngineRoot`；若未配置，则先运行 `Tools/GenerateAgentConfigTemplate.bat` 生成模板并补齐本地值，再进入任何 build/test 阶段
  - 明确 `Paths.ProjectFile` 为空时可以回退到仓库根 `.uproject`，以及 `Build.EditorTarget`、`Build.Platform`、`Build.Configuration`、`Build.Architecture` 的默认取值来源，避免执行者在第一次构建前猜参数
  - 把“缺失配置时立即阻塞、先补配置而不是继续实现”的规则固定下来，避免后续 Phase 失败后才回头补环境
- [x] **P0.1** 📦 Git 提交：`[Docs] Feat: define execution prerequisites and AgentConfig fallback rules`

- [x] **P0.2** 产出当前技术债 live inventory 与编目基线对照
  - 以当前源码扫描结果为准，区分 `Documents/Guides/TestCatalog.md` 的“已编目基线”与 `Source/AngelscriptTest/` 的“实时扫描规模”，把两者的差值作为后续文档同步的基准
  - 对 helper 旧命名引用、运行时待办注释、弃用 API 命中、全局状态调用点、Bind 候选差距分别给出最小可复查清单，避免执行阶段再从零搜一遍
  - 本项产物可以是本计划附录、配套知识文档或 sibling plan 交叉引用，但必须是仓库内可追溯结果，而不是口头说明
- [x] **P0.2** 📦 Git 提交：`[Docs] Feat: capture live debt inventory against documented baseline`

- [x] **P0.3** 验证 build/test 参数可由配置真实展开
  - 参考 `Tools/GenerateAgentConfigTemplate.bat` 中的默认键位，确认 `EngineRoot`、`ProjectFile`、`EditorTarget`、`Platform`、`Configuration`、`Architecture`、`Test.DefaultTimeoutMs` 都能映射到本计划的命令模板
  - 除 `EngineRoot` 缺失会直接阻塞外，其余参数统一采用模板默认回退：`ProjectFile -> AngelscriptProject.uproject`，`EditorTarget -> AngelscriptProjectEditor`，`Platform -> Win64`，`Configuration -> Development`，`Architecture -> x64`，`Test.DefaultTimeoutMs -> 600000`
  - 在真正跑大构建前，先完成一次命令参数展开级 smoke check，确保后续 Phase 的验证步骤都是可执行命令，而不是抽象模板
- [x] **P0.3** 📦 Git 提交：`[Build/Test] Chore: validate command-template parameter resolution before debt execution`

- [x] **P0.4** 建立 Bind 差距审计矩阵
  - 通过 `AgentConfig.ini` 的 `References.HazelightAngelscriptEngineRoot` 指向 Hazelight/UEAS2 参考源，对 `Bind_TOptional.cpp`、`Bind_UStruct.cpp`、`Bind_BlueprintType.cpp`、`Bind_AActor.cpp`、`Bind_USceneComponent.cpp`、`Bind_Delegates.cpp`、`Bind_FHitResult.cpp`、`Bind_FMath.cpp`、`Bind_FVector2f.cpp` 做文件级 diff
  - 审计结果必须至少包含：本地文件、参考符号/行为、是否依赖可选模块、首批测试落点、风险级别，以及“留在本计划 / 拆 sibling plan”结论
  - 若 `References.HazelightAngelscriptEngineRoot` 未配置，则本项只允许先生成本地矩阵骨架并把跨参考源比对标记为阻塞项；在参考源缺失时不得直接进入 `P4.2`
  - 本项完成前，不再沿用旧计划里未经复核的固定缺口列表直接开工
- [x] **P0.4** 📦 Git 提交：`[Docs] Feat: add audited bind-gap matrix to technical debt plan`

### Phase 1：构建与运行时高优先级稳定性修复

> 目标：先消除最容易导致构建失败、崩溃或行为不可控的高影响问题，把插件主线拉回稳定状态。

- [x] **P1.1** 为 `TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 特化加保护
  - 先对照 UE 引擎侧定义，确认哪些版本已经内建同名特化，避免插件侧继续无条件重定义
  - 修改 `Core/AngelscriptType.h` 与对应实现文件时，需要同步核对 `Bind_UStruct.cpp` 和 `StaticJIT` 使用侧，确保不会把脚本侧结构体绑定打断
  - 如果插件与引擎侧 `UScriptStruct*` 语义不完全等价，本项必须先补适配说明或测试，不允许只“关掉报错”
- [x] **P1.1** 📦 Git 提交：`[Build] Fix: guard FBox and FBoxSphereBounds base-structure specializations`

- [x] **P1.2** 将 `AngelscriptRuntime.Build.cs` 的优化开关改成条件化策略
  - 当前 `OptimizeCode = CodeOptimization.Never` 只适合开发期调试，不应继续作为所有构建配置的默认值
  - 需要把“什么时候关闭优化方便调试、什么时候恢复正常优化”写成可维护规则，而不是把临时调试注释留在 Build.cs 中
  - 若调试工作流因此变化，必须同步更新 `Documents/Guides/Build.md` 或相邻文档说明
- [x] **P1.2** 📦 Git 提交：`[Build] Fix: make AngelscriptRuntime optimization policy configuration-aware`

- [x] **P1.3** 为 `Bind_BlueprintEvent.cpp` 补齐运行时签名校验
  - 在进入 `ProcessEvent`、`ProcessDelegate`、`ProcessMulticastDelegate` 前，核对当前脚本参数缓存与 `UFunction` / delegate 签名是否一致，不一致时记录错误并安全返回
  - 实现时既要覆盖 `ExecutePreamble()` / `ExecuteEvent()`，也要覆盖 delegate 与 multicast delegate 的通用入口，避免只修一种调用路径
  - 测试应优先落在现有 `Bindings` 或 `Internals` 体系，不新增无主题归属的 catch-all 测试文件
- [x] **P1.3** 📦 Git 提交：`[Binds] Fix: validate BlueprintEvent signatures before dispatch`

- [x] **P1.4** 为 `AngelscriptDebugServer.cpp` 增加并发保护并固定线程模型说明
  - 先划清哪些共享状态允许在异常处理路径读取、哪些必须提前快照，避免在异常处理器里引入新的分配或锁顺序风险
  - 优先采用最小范围锁或无分配快照策略；如果某些路径不能安全加锁，要明确写出原因与防护边界，不能保留泛化待办注释
  - 涉及调试线程 / 游戏线程协作的改动，需要在计划里明确对应的验证场景
- [x] **P1.4** 📦 Git 提交：`[Debug] Fix: add thread-safety guards for debug server breakpoint paths`

- [x] **P1.5** 审计并修复 `StaticJITHeader.cpp` 异常路径对象生命周期
  - 以 `StaticJIT/StaticJITHeader.cpp:255` 附近逻辑为起点，梳理“返回值走栈 / 返回对象句柄 / 异常退出”三类路径的对象所有权
  - 能用 RAII、`TUniquePtr` 或明确析构路径替代裸生命周期的地方，优先做最小替换；如果不能替换，也要补充确定性的清理逻辑
  - 本项只解决对象生命周期与异常路径，不顺手展开成整个 `StaticJIT` 子系统重写
- [x] **P1.5** 📦 Git 提交：`[StaticJIT] Fix: close exception-path object lifetime leak risks`

- [x] **P1.6** 执行构建与稳定性回归
  - 先跑与本 Phase 直接相关的目标测试，再跑完整 `Angelscript.TestModule` 回归；若某个修复没有现成测试入口，需先补最小回归再进入全量回归
  - 构建与测试命令必须遵循本计划“验证命令基线”与 `Documents/Guides/Build.md` / `Documents/Guides/Test.md`
  - 若本 Phase 改动暴露出新的引擎版本差异或目录边界问题，应先回写文档，再继续后续 Phase
- [x] **P1.6** 📦 Git 提交：`[Test] Test: verify build and runtime stability debt fixes`

### Phase 2：弃用 API 收口与内部回归加固

> 目标：把已经暴露的弃用调用和过宽警告压制收口，同时把现有 internal test 基线补成“有正向也有负向边界”的稳定防线。

- [x] **P2.1** 替换 `as_string.h` 中的 `FCrc::Strihash_DEPRECATED`
  - 修改 `ThirdParty/angelscript/source/as_string.h` 时延续现有 `[UE++]` 标记习惯，避免 ThirdParty 变更再次成为黑盒补丁
  - 需要确认替换后的哈希语义仍满足 `asCString` 在当前插件中的比较与映射使用方式，而不是只为了消除 warning 换一个看似可编译的 API
  - 若替换会影响任何缓存键或查找行为，本项必须补回归测试或兼容说明
- [x] **P2.1** 📦 Git 提交：`[ThirdParty] Fix: replace deprecated hash usage in as_string with UE++ marker`

- [x] **P2.2** 缩小 `StaticJITHeader.h` 的 `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 作用域
  - 在 P2.1 以及 Phase 1 的根因清理完成后，再判断这条文件级压制是否仍有必要
  - 若确实只需覆盖极少量兼容代码，应把压制范围缩到最小必要代码块；若已不需要，则直接删除
  - 本项不能以“保留全局压制但补注释”代替真正收口
- [x] **P2.2** 📦 Git 提交：`[StaticJIT] Refactor: narrow deprecation warning suppression scope`

- [x] **P2.3** 为 Restore 路径补负向边界测试
  - 现有 `Internals/AngelscriptRestoreTests.cpp` 已有 round-trip 与 strip-debug-info 正向测试，因此本项不再重复“补 round-trip”，而是补空/损坏流、读取失败、边界条件恢复等负向场景
  - 如果需要覆盖 `AS_NO_COMPILER` 分支，则应显式标注这是条件编译/专用构建路径，不得把当前正常构建里的 `asSUCCESS` 事实写回成过时前提
  - 目标是让 Restore 测试同时锁住“当前支持能力”和“失败时不崩溃”的边界
- [x] **P2.3** 📦 Git 提交：`[Test/Internals] Feat: add negative and corruption coverage for restore paths`

- [x] **P2.4** 澄清 `TestCatalog.md` 的基线口径，并按需扩展 Memory/Restore 条目
  - `Documents/Guides/TestCatalog.md` 已经登记了 `Restore` 与 `Memory` 章节，因此本项不是“首次补登记”，而是把“已编目基线 vs 实时扫描规模”的差异解释清楚
  - 如果本 Phase 新增了 Restore 负向测试或调整了测试名，再把这些新增边界补进现有章节；若测试名未变化，则只更新口径说明，不重复改写已存在的条目
  - 目标是让执行者知道哪些 internal test 是当前正式基线、哪些是后续 live inventory 扩展，而不是继续把两者混为一个数字
- [x] **P2.4** 📦 Git 提交：`[Docs] Refactor: clarify documented baseline versus live inventory for internal tests`

- [x] **P2.5** 执行“零显式弃用调用”检查并完成回归
  - 对 `Plugins/Angelscript/Source/AngelscriptRuntime/` 做 `_DEPRECATED` 与 `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 定向扫描，确认收口后的实际状态
  - 若仍需保留少量例外，必须把例外路径、原因与后续消化方式写回文档，不能把搜索结果留在口头说明里
  - 完成本 Phase 的目标测试与全量回归，并确认 `TestCatalog.md` 与源码状态一致
- [x] **P2.5** 📦 Git 提交：`[Test] Test: verify deprecated API cleanup and internal regression baseline`

### Phase 3：测试 Helper 命名重构

> 目标：把当前“共享引擎 / 初始化引擎 / 重置引擎 / 生产引擎 / global scope”语义混杂的 helper，迁移为一组名字即含义的测试入口。

- [x] **P3.1** 在 `Shared` 层引入语义显式的新 helper 名称
  - 以 `Shared/AngelscriptTestUtilities.h` 为主入口，必要时联动 `Shared/AngelscriptTestEngineHelper.h/.cpp`，让 helper 名称显式区分“共享 clone 引擎”“附着生产引擎”“是否自动重置”“是否带 global scope”
  - 这一阶段允许保留旧名称作为兼容别名，但旧名称必须变成清晰的转发层，而不是继续承载真实语义
  - 所有新名字要能直接从名字判断副作用，避免继续出现 `Initialized`/`Shared` 实际等价的情况
- [x] **P3.1** 📦 Git 提交：`[Test] Refactor: introduce explicit helper names and compatibility aliases`

- [x] **P3.2** 先迁移基础设施与低耦合目录
  - 首批目录固定为：`Shared/`、`Core/`、`Compiler/`、`Preprocessor/`、`Internals/`
  - 这些目录对 helper 的语义要求最直接，最适合作为“命名规则模板”验证场；任何在这里暴露出的命名问题，都要先收敛，再往大规模目录扩散
  - 每完成一批目录，都要做 focused regression，而不是攒到最后一起赌全量回归
- [x] **P3.2** 📦 Git 提交：`[Test] Refactor: migrate infrastructure and low-coupling tests to explicit helper names`

- [x] **P3.3** 分批迁移主题化集成测试目录
  - 第二批目录固定为：`Actor/`、`Blueprint/`、`Component/`、`Delegate/`、`GC/`、`HotReload/`、`Inheritance/`、`Interface/`、`Subsystem/`、`ClassGenerator/`、`Template/`
  - 迁移时要特别留意 `FScopedTestEngineGlobalScope`、`FScopedTestWorldContextScope`、生产引擎附着等副作用型 helper，防止只改名字不改语义误用
  - 该批次文件量大，必须按主题分批回归，不能一次性扫全目录后再集中修错误
- [x] **P3.3** 📦 Git 提交：`[Test] Refactor: migrate themed integration tests to explicit helper names`

- [x] **P3.4** 迁移高频行为测试与剩余主题目录
  - 第三批目录固定为：`Angelscript/`、`Bindings/`、`Examples/`、`FileSystem/`、`Editor/`、`Native/`
  - `Native/` 目录必须遵守 `Plugins/Angelscript/AGENTS.md` 的边界要求，只使用公共 `AngelscriptInclude.h` / `angelscript.h` 暴露的 API，不把私有运行时类型借 helper 迁移之机带进去
  - 对 `Bindings/` 和 `Angelscript/` 目录，迁移完成后要额外检查是否还有“名字变了但仍依赖旧全局行为”的假收口
- [x] **P3.4** 📦 Git 提交：`[Test] Refactor: migrate behavior, bindings, editor, native, and example tests to explicit helper names`

- [x] **P3.5** 移除旧兼容别名并同步测试目录文档
  - 只有在 grep 确认 `Source/AngelscriptTest/` 下已无旧 helper 名称残留后，才能删除兼容别名
  - 删除别名同时更新 `Documents/Guides/TestCatalog.md` 与相关计划文档，避免文档仍指导使用旧名字
  - 任何遗留的 helper 语义争议必须在本项前收敛，不允许靠“保留别名以防万一”拖延结束条件
- [x] **P3.5** 📦 Git 提交：`[Test] Chore: remove deprecated helper aliases after migration completion`

- [x] **P3.6** 执行 helper 命名迁移总回归
  - 除完整自动化回归外，还要对旧 helper 名称做定向 grep，确认源码与文档都已经完成切换
  - 若因为批量改名引入 include、命名空间或宏路径破坏，先就地收敛基础错误，不得带病进入下一 Phase
  - 本项完成后，`Plan_TechnicalDebt.md` 中不再把 helper 命名问题视为“待设计”，而应视为已关闭债务
- [x] **P3.6** 📦 Git 提交：`[Test] Test: verify helper naming migration regression`

### Phase 4：Bind 差距审计与低风险收敛

> 目标：把 Bind 差距从“旧 backlog 猜测”改成“有参考源、有测试落点、有拆分策略”的可维护清单，只在本计划里吃下低风险高价值部分。

- [x] **P4.1** 固化本地 vs Hazelight/UEAS2 的 Bind 差距矩阵
  - 以 Phase 0 的审计矩阵为起点，把每个候选文件的差距具体到“缺哪个符号 / 缺哪段行为 / 受哪些模块依赖约束 / 建议落在哪个测试文件”
  - 不能只写“与参考源有差距”，而要写清楚差距类型是 API 缺失、行为偏差、热重载/调试器支持缺口，还是仅为风格差异
  - 对 `Bind_BlueprintType.cpp`，至少把 `CPF_TObjectPtr` 注释路径是否仍需恢复写成明确结论
- [x] **P4.1** 📦 Git 提交：`[Docs] Feat: freeze audited bind-gap matrix with test landing points`

- [x] **P4.2** 实现低风险且不引入额外模块依赖的 Bind 补齐项
  - 只接收两类事项：一类是通过审计确认的局部逻辑缺口，另一类是可以直接由现有 `Bindings` / `UpgradeCompatibility` 测试锁住的缺口
  - 如果某项需要额外插件模块、较大 API 设计变更、或会把问题扩展到 `InterfaceBinding` / `AS238` 等其他主题，必须在本项中止并转 sibling plan
  - 本项优先考虑 `Bind_BlueprintType.cpp`、`Bind_TOptional.cpp`、`Bind_UStruct.cpp` 这类可能存在局部收敛空间的文件
- [x] **P4.2** 📦 Git 提交：`[Binds] Feat: close low-risk audited bind parity gaps`

- [x] **P4.3** 将高风险或跨主题差距拆分到 sibling plan
  - `Interface` 相关绑定差距优先并入 `Documents/Plans/Plan_InterfaceBinding.md`
  - 与 `AS238` 语言/ThirdParty 能力迁移直接相关的差距优先并入 `Documents/Plans/Plan_AS238NonLambdaPort.md`
  - 若发现某些差距本质上属于 Hazelight 模块迁移问题，则并入 `Documents/Plans/Plan_HazelightBindModuleMigration.md` 或新 sibling plan，而不是继续堆在本计划内
- [x] **P4.3** 📦 Git 提交：`[Docs] Refactor: split high-risk bind gaps into focused sibling plans`

- [x] **P4.4** 为本计划保留的 Bind 补齐项补测试并做完整回归
  - 所有保留在本计划内的 Bind 改动，都必须在 `Plugins/Angelscript/Source/AngelscriptTest/Bindings/`、`Angelscript/` 或 `UpgradeCompatibility` 体系内找到明确测试落点
  - 测试新增后同步更新 `TestCatalog.md`，确保 Bind 层能力变化不再只存在于代码 diff 中
  - 本项完成后，旧的“6 个 Minor Drift 文件清单”只保留为审计参考，不再作为未经验证的待办来源
- [x] **P4.4** 📦 Git 提交：`[Test/Binds] Test: verify audited bind parity closures`

### Phase 5：全局状态收口与宿主工程边界压缩

> 目标：优先收口最容易隔离的全局状态依赖，并确认宿主工程仍保持最小职责；完整去全局化另起节奏，不在这里偷跑。

- [x] **P5.1** 盘点 `FAngelscriptEngine::Get()` / `CurrentWorldContext` 的使用模式
  - 至少按三类整理：编译/类生成路径、世界上下文绑定路径、测试/调试辅助路径
  - 盘点结果要写清楚哪些调用点已经有 `FScopedTestEngineGlobalScope` / `FScopedTestWorldContextScope` / `FAngelscriptGameThreadScopeWorldContext` 之类的现成替代物，哪些仍缺显式上下文入口
  - 本项的产出不是“列路径而已”，而是要为后续 containment 步骤固定优先级
- [x] **P5.1** 📦 Git 提交：`[Docs] Feat: classify global engine and world-context dependency callsites`

- [x] **P5.2** 先收口最容易显式化的 world-context / engine 入口
  - 只处理已经具备 scoped wrapper 或参数透传路径的调用点，优先解决测试与绑定层里最容易误用的场景
  - 不能把这一项膨胀成 `Core/AngelscriptEngine.cpp` 全面架构翻修；目标是减少隐式全局入口，而不是一次性消灭全部静态状态
  - 改动后必须补回归，证明 isolate test engine 不会被生产引擎或共享世界上下文静默劫持
- [x] **P5.2** 📦 Git 提交：`[Runtime] Refactor: contain low-risk global engine and world-context access paths`

- [x] **P5.3** 复核宿主工程模块依赖是否仍然最小化
  - `Source/AngelscriptProject/AngelscriptProject.Build.cs:11` 当前仍公开依赖 `EnhancedInput`，本项需要确认这是宿主工程真实需要，还是可以下沉到插件或示例层
  - 若不能移除，也必须在计划里补清楚“为什么宿主仍需要这个依赖”，以符合宿主仅作为插件验证载体的定位
  - 若还能进一步最小化宿主模块，相关改动要同步检查是否影响现有启动、测试或模板内容
- [x] **P5.3** 📦 Git 提交：`[Project] Refactor: review and minimize host project module dependencies`

- [x] **P5.4** 为完整去全局化写出后续边界说明
  - 本项不是直接展开跨模块重构，而是把“哪些路径已经 containment 完成、哪些仍需独立计划”明确写下来，防止下一轮工作再从零判边界
  - 若盘点表明后续工作量已超出本计划可控范围，应补 sibling plan 或本计划附录说明，而不是继续往本计划里堆 Phase
  - 写清楚启动后续计划的前置条件，例如 helper 命名迁移完成、Bind 审计矩阵冻结、测试基线稳定等
- [x] **P5.4** 📦 Git 提交：`[Docs] Feat: define follow-up boundary for full de-globalization work`

- [x] **P5.5** 执行全局状态 containment 回归
  - 覆盖共享测试引擎、生产引擎附着、world context scope 恢复、类生成与运行时路径不串线等关键场景
  - 若 containment 改动需要补充新的 helper 或测试工具，也必须先把这些工具纳入 `Shared` 层并记录到文档目录
  - 本项结束条件是“隐式全局依赖收缩且行为有测试证明”，不是“静态入口彻底消失”
- [x] **P5.5** 📦 Git 提交：`[Test] Test: verify global-state containment regression`

### Phase 6：性能验证与最终文档同步

> 目标：只对仍然存在且已落地的优化点做性能复核，并把整个技术债计划的最终状态同步回文档体系。

- [x] **P6.1** 验证 `Bind_UEnum.cpp` 的性能优化债务是否仍然成立
  - 当前复核优先看代码事实：`Bind_UEnum.cpp` 是否仍存在显式哈希查找优化路径、性能 TODO、或文档中仍引用的未验证性能结论。
  - 若 `Bind_UEnum.cpp` 里的哈希查找优化仍是当前主线中的显式改动，就用 focused benchmark 或自动化计时验证其收益。
  - 若代码或文档已经不再把这项视为待验证优化，就从技术债列表中移除旧说法，而不是为了“保持 Phase 数量”强行做无意义测量。
  - 任何性能结论都需要附带测量口径与场景，不写“体感更快”这种不可回归描述
- [x] **P6.1** 📦 Git 提交：`[Binds] Test: verify or retire stale enum lookup performance debt`

- [x] **P6.2** 完成文档体系同步
  - 至少复核并按需更新：`Documents/Plans/Plan_TechnicalDebt.md`、`Documents/Guides/Build.md`、`Documents/Guides/Test.md`、`Documents/Guides/TestCatalog.md`、`Documents/Guides/TechnicalDebtInventory.md`，以及 `Documents/Plans/Plan_InterfaceBinding.md`、`Documents/Plans/Plan_HazelightBindModuleMigration.md`、`Documents/Plans/Plan_AS238NonLambdaPort.md`、`Documents/Plans/Plan_FullDeGlobalization.md` 的边界交叉引用
  - 确认文档中不再出现本地绝对路径，且所有引用的计划/指南都与当前插件优先边界一致
  - 如果某项债务已通过 sibling plan 承接，必须在本计划中写明去向，不允许“从这里删掉但没有着落”
- [x] **P6.2** 📦 Git 提交：`[Docs] Chore: sync technical debt closeout across guides and plans`

- [x] **P6.3** 执行最终构建与全量回归，并沉淀结果摘要
  - 已按 `AgentConfig.ini` / `Documents/Guides/Test.md` 的配置规则解析命令，并在独立 worktree 上重新执行 `Automation RunTests Angelscript.TestModule`。
  - 最终 full-suite 仍保留且仅保留 4 个已知失败项：`Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2A`、`Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2B`、`Angelscript.TestModule.Editor.SourceNavigation.Functions`、`Angelscript.TestModule.ScriptExamples.Actor`；`Saved/Logs/AngelscriptProject.log` 中未见新增与本计划技术债收口直接相关的失败。
  - 本阶段结果已回写到 `Documents/Guides/TechnicalDebtInventory.md` 与 `Documents/Guides/TestCatalog.md`，用于统一“已编目基线 / 实时扫描规模 / full-suite 已知失败项”的最终口径。
- [x] **P6.3** 📦 Git 提交：`[Test] Test: finalize technical debt cleanup verification and summary`

## 验收标准

1. **计划与仓库现状一致**：本计划不再把已落地的 Memory/Restore 测试能力当作“待补空白”，也不再保留本地绝对路径与过时事实。
2. **构建稳定性提升**：`TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 不再无条件重定义；`AngelscriptRuntime.Build.cs` 的优化策略不再把开发期调试配置硬编码给所有构建。
3. **运行时安全债务关闭**：`Bind_BlueprintEvent`、`AngelscriptDebugServer`、`StaticJITHeader` 的显式待办注释被代码和测试收口，不再作为开放性待办注释留在主线。
4. **弃用 API 基线可验证**：`as_string.h` 的显式弃用调用完成替换；`StaticJITHeader.h` 的警告压制缩到最小；针对 `_DEPRECATED` 与警告压制定向扫描后，剩余例外均有文档记录。
5. **内部测试基线成文且可回归**：`Memory` / `Restore` 现有正向测试能力被写入 `TestCatalog.md`，并补齐必要的负向边界测试。
6. **测试 helper 命名完成迁移**：`Source/AngelscriptTest/` 下旧 helper 名称残留为零，兼容别名已移除，文档同步完成。
7. **Bind 差距不再是黑盒 backlog**：本计划保留的 Bind 差距项都有参考源、测试落点和风险结论；不适合本计划的项已拆到 sibling plan。
8. **全局状态与宿主边界收口可证明**：低风险的全局依赖调用点已经 containment，宿主工程依赖最小化状态有明确结论，完整去全局化的后续边界已写明。
9. **验证证据完整**：最终构建、目标测试、完整回归、以及文档同步都按既定口径执行并留下结果摘要。

## 风险与注意事项

### 风险 1：引擎版本差异导致 `TBaseStructure` 保护条件不止一种

不同 UE 版本是否内建 `TBaseStructure<FBox>` / `TBaseStructure<FBoxSphereBounds>` 并不一定完全一致；若只用单一宏条件，可能仍会在某些版本出现重定义或语义偏差。

**缓解**：在真正下手改动前先对照引擎侧定义与返回值语义；必要时用“版本判断 + 符号存在判断”双保险，而不是拍脑袋写一个 `#if`。

### 风险 2：DebugServer 异常路径不能随意引入重锁或堆分配

异常处理路径本身对锁顺序、堆分配和线程上下文极度敏感；错误的“线程安全修复”反而可能把调试器变得更脆弱。

**缓解**：先区分可加锁路径和必须零分配/零阻塞的路径；必要时采用共享状态快照而非在异常处理器中直接做复杂逻辑。

### 风险 3：helper 命名迁移横跨几十个文件，容易出现连锁语法破坏

当前 helper 旧命名在大量测试文件中存在，单次大规模替换很容易引入 include、命名空间或 scope 恢复错误。

**缓解**：严格按 Phase 3 的分层顺序推进，每一批迁移后先收敛 compile error 和 focused regression，再进入下一批，不允许攒大爆炸。

### 风险 4：Bind 差距中混有可选模块依赖与其他计划边界

有些 Bind 差距可能依赖 `GameplayAbilities`、`EnhancedInput`、接口绑定设计或 `AS238` 路线；如果直接吞进本计划，会快速扩大成多主题耦合改造。

**缓解**：先审计、后分流。只有低风险、可独立回归的项留在本计划；其余一律并入对应 sibling plan。

### 风险 5：`TestCatalog.md` 的文档化基线与实时扫描规模不等价

如果后续仍把 `275/275 PASS` 当成“当前全部测试总数”，会继续误导范围判断和验收口径。

**缓解**：明确把它标记为“已编目基线”，同时补 live inventory 说明，并在相关计划里统一这个口径。

### 风险 6：宿主工程边界优化可能影响现有验证入口

即使宿主工程依赖看起来可移除，也可能被当前示例、模板或启动流程隐式使用；贸然删掉会让“宿主最小化”变成新 breakage 来源。

**缓解**：先审计依赖用途，再决定移除或保留；若保留，也要把理由写清楚，避免它再次成为“未来某天再看”的模糊债务。

## 优先级与依赖关系

```text
Phase 0（基线与审计冻结）── 所有后续阶段的前置依赖
  ↓
Phase 1（构建 / 运行时高优先级稳定性）── 最先执行
  ↓
Phase 2（弃用 API / internal regression）── 依赖 Phase 1 的稳定主线
  ↓
Phase 3（helper 命名迁移）── 依赖 Phase 0-2 固定基础设施口径
  ↓
Phase 4（Bind 差距审计与低风险收敛）── 可在 Phase 0 后启动审计，但真正落地最好在 Phase 1-2 后进行
  ↓
Phase 5（全局状态 containment / 宿主边界）── 依赖 helper 语义与 Bind 审计都已收敛
  ↓
Phase 6（性能验证与最终同步）── 作为最终收尾执行
```

**推荐执行顺序**：`P0 → P1 → P2 → P3 → P4 → P5 → P6`。

其中：

- `P4.1` 的审计动作可以在 `P1` 并行准备，但 `P4.2-P4.4` 最好在 `P1-P2` 稳定后进入
- `P5` 不得早于 `P3`，否则 helper 语义与全局状态 containment 会互相放大不确定性

## 参考文档索引

| 文档 | 位置 | 用途 |
| --- | --- | --- |
| Plan 编写规则 | `Documents/Plans/Plan.md` | 约束本计划的结构、checkbox 与提交条目格式 |
| 构建指南 | `Documents/Guides/Build.md` | 构建命令、PowerShell 包装与 AgentConfig 取值方式 |
| 测试指南 | `Documents/Guides/Test.md` | 自动化测试入口、NullRHI 参数与 AI Agent 执行方式 |
| 技术债实时盘点 | `Documents/Guides/TechnicalDebtInventory.md` | P0.2 的 live inventory、旧 helper 热点与全局状态调用快照 |
| Bind 差距审计矩阵 | `Documents/Guides/BindGapAuditMatrix.md` | P0.4 的参考源对照矩阵、测试落点与留/拆结论 |
| 测试目录基线 | `Documents/Guides/TestCatalog.md` | 已编目测试基线、live inventory 指引与目录说明 |
| 全局状态分类矩阵 | `Documents/Guides/GlobalStateContainmentMatrix.md` | P5.1 的全局引擎 / world-context containment 分类与优先级 |
| 单元测试扩张计划 | `Documents/Plans/Plan_AngelscriptUnitTestExpansion.md` | live inventory 与文档化基线漂移的配套计划 |
| Interface 绑定计划 | `Documents/Plans/Plan_InterfaceBinding.md` | 高风险接口绑定差距的承接计划 |
| Hazelight Bind 模块迁移计划 | `Documents/Plans/Plan_HazelightBindModuleMigration.md` | 需要跨模块迁移的 Bind 主题承接计划 |
| AS238 非 Lambda 迁移计划 | `Documents/Plans/Plan_AS238NonLambdaPort.md` | 与语言/ThirdParty 升级直接相关的主题边界 |
| 全量去全局化计划 | `Documents/Plans/Plan_FullDeGlobalization.md` | P5 后续的完整去全局化边界与实施节奏 |
| Native 核心测试重构计划 | `Documents/Plans/Plan_NativeAngelScriptCoreTestRefactor.md` | Native 测试层与公共 API 边界配套说明 |
| Git 提交规范 | `Documents/Rules/GitCommitRule.md` | 所有阶段提交格式参考 |
| Hazelight 参考源配置 | `AgentConfig.ini` → `References.HazelightAngelscriptEngineRoot` | Bind 差距审计的本地引用入口 |
