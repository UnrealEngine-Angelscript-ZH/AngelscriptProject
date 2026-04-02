# 运行自动化测试

## Angelscript 插件专项文档

- 启动 bind / watcher 验证矩阵：`Documents/Guides/AngelscriptValidationMatrix.md`
- 性能采样与产物规范：`Documents/Guides/TestPerformance.md`
- 测试目录总览：`Documents/Guides/TestCatalog.md`

### Angelscript 性能采样快捷前缀

- 启动基线：`Automation RunTests Angelscript.TestModule.Core.Performance.Startup`
- 热重载延迟：`Automation RunTests Angelscript.TestModule.HotReload.Performance`
- 产物回归：`Automation RunTests Angelscript.TestModule.Core.Performance.ArtifactGeneration`

# 图形测试

(目前项目中无图形测试,不考虑这种启动方式)

图形测试需要 GPU 支持（**不能加 `-NullRHI`**），用于验证渲染输出的正确性。仍然可以使用 `UnrealEditor-Cmd.exe` 以命令行方式运行。

### 主要测试类型

- **截屏功能测试 (AScreenshotFunctionalTest)**：在关卡中放置测试 Actor，自动捕获截图并与基准图片对比
- **UI 截屏测试 (AFunctionalUIScreenshotTest)**：专门用于 UI Widget 的截图对比测试
- **截屏比较测试**：快速比较截屏以识别不同版本之间的潜在渲染问题

### 命令行运行图形测试

使用 `UnrealEditor-Cmd.exe`，**不加 `-NullRHI`**（需要 GPU 进行渲染）：

```
C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Workspace\UnrealEvent\UnrealEvent.uproject" -ExecCmds="Automation RunTests <TestName>; Quit" -Unattended -NoPause -NoSplash -NOSOUND
```

在 AI Agent 环境中执行（通过 PowerShell + Start-Process）：

```
powershell.exe -Command "Start-Process -FilePath 'C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"D:\Workspace\UnrealEvent\UnrealEvent.uproject\"','-ExecCmds=\"Automation RunTests <TestName>; Quit\"','-Unattended','-NoPause','-NoSplash','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

超时建议：600000ms（10 分钟），首次启动需要加载引擎和编译 shader

# 非图形测试

目前主要的测试方式

使用 `-NullRHI` 参数可以在不需要 GPU 的情况下运行测试，适用于 CI/CD 环境和 AI Agent 自动化场景。

## 命令行运行方式

### 基本命令行参数

```
-ExecCmds="Automation RunTests <TestName>; Quit"    # 运行指定测试后退出
-ExecCmds="Automation RunTest Test1+Test2; Quit"     # 运行多个测试
-ExecCmds="Automation RunTest MySet.MySubSet; Quit"  # 运行某分段下所有测试
-ExecCmds="Automation RunTest Group:MyGroup; Quit"   # 运行测试组
```

### 常用命令行开关

| 参数 | 说明 |
|------|------|
| `-NullRHI` | 不初始化图形设备，无需 GPU |
| `-Unattended` | 无人值守模式，不弹出对话框 |
| `-NoPause` | 不暂停等待用户输入 |
| `-NoSplash` | 不显示启动画面 |
| `-NOSOUND` | 禁用音频 |
| `-ReportExportPath="<path>"` | 导出测试结果为 JSON/HTML |
| `-ResumeRunTest` | 配合 ReportExportPath 使用，从上次中断处恢复测试 |

### 命令行运行示例（无需打开编辑器 GUI）

使用 `UnrealEditor-Cmd.exe` 配合 `-ExecCmds` 参数，在 NullRHI 模式下运行（不需要 GPU）：

原始命令：

```
C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Workspace\UnrealEvent\UnrealEvent.uproject" -ExecCmds="Automation RunTests <TestName>; Quit" -Unattended -NoPause -NoSplash -NullRHI -NOSOUND
```

在 AI Agent 环境中执行（通过 PowerShell + Start-Process）：

```
powershell.exe -Command "Start-Process -FilePath 'C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"D:\Workspace\UnrealEvent\UnrealEvent.uproject\"','-ExecCmds=\"Automation RunTests <TestName>; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

超时建议：600000ms（10 分钟），首次启动需要加载引擎和编译 shader。

### 另一个项目示例

使用 `UnrealEditor-Cmd.exe` 配合 `-ExecCmds` 参数，在 NullRHI 模式下运行（不需要 GPU）：

原始命令：

```
C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Workspace\TAPathon\PythonUIProject\PythonUIProject.uproject" -ExecCmds="Automation RunTests PythonUIWidget; Quit" -Unattended -NoPause -NoSplash -NullRHI -NOSOUND
```

在 AI Agent 环境中执行（通过 PowerShell + Start-Process）：

```
powershell.exe -Command "Start-Process -FilePath 'C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"D:\Workspace\TAPathon\PythonUIProject\PythonUIProject.uproject\"','-ExecCmds=\"Automation RunTests PythonUIWidget; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND' -Wait -NoNewWindow; Write-Host 'DONE'"
```

超时建议：600000ms（10 分钟），首次启动需要加载引擎和编译 shader。



# 完整启动测试

使用 Gauntlet 框架可以进行完整的启动测试：

```
RunUAT.bat RunUnreal -test=UE.BootTest -project=<path to uproject>
RunUAT.bat RunUnreal -test=UE.EditorBootTest -project=<path to uproject>
```

---

# 通过 Gauntlet 框架运行测试



Gauntlet 提供针对虚幻引擎的自动化测试封装，主要命令是 `RunUnreal`。

### 内置测试类型

| 测试 | 说明 |
|------|------|
| `UE.BootTest` | 启动项目客户端，初始化完成后退出 |
| `UE.EditorBootTest` | 启动编辑器，初始化完成后退出 |
| `UE.EditorAutomation` | 在编辑器中运行自动化测试 |
| `UE.TargetAutomation` | 在打包客户端上运行自动化测试 |
| `UE.Networking` | 运行自动化网络测试 |

### Gauntlet 命令行示例

编辑器测试：
```
RunUAT.bat RunUnreal -test=UE.EditorAutomation -runtest=Mytest.one -project=<path to uproject> -build=editor
```

客户端测试：
```
RunUAT.bat RunUnreal -test=UE.TargetAutomation -runtest=Mytest.one -project=<path to uproject> -build=<path to packaged game>
```

目标平台测试：
```
RunUAT.bat RunUnreal -test=UE.TargetAutomation -runtest=Mytest.one -project=<path to uproject> -build=<path to packaged game> -platform=<platform> -device=<ip>:<platform>
```

添加 `-ResumeOnCriticalFailure` 可在崩溃后自动恢复测试（最多恢复 3 次）。

---

# 配置自动化测试

## 测试组

在 `DefaultEngine.ini` 中定义测试组，将不同分段的测试集合在一起：

```ini
[/Script/AutomationTest.AutomationTestSettings]
+Groups=(Name="Group1", Filters=((Contains=".Some String.")))
+Groups=(Name="Group2", Filters=((Contains="Group2.", MatchFromStart=true),(Contains=".Group2.")))
```

在命令行中引用测试组：`-ExecCmds="Automation RunTest Group:MyGroup; Quit"`

## 排除测试

在 `DefaultEngine.ini` 中排除测试：

```ini
+ExcludeTest=(Test="<test name>", Reason="<原因>", Warn=False)
```

按 RHI 排除：
```ini
+ExcludeTest=(Test="<test name>", Reason="<原因>", RHIs=("Vulkan", "DirectX 11"), Warn=False)
```

被排除的测试将显示为"跳过（Skipped）"状态。

---

# 编写自动化测试

## C++ 简单测试

使用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 宏声明：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceholderTest, "TestGroup.TestSubgroup.Placeholder Test",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceholderTest::RunTest(const FString& Parameters)
{
    // 返回 true 表示通过，false 表示失败
    return true;
}
```

## C++ 复杂测试

使用 `IMPLEMENT_COMPLEX_AUTOMATION_TEST` 宏，需覆盖 `RunTest` 和 `GetTests`：

```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FLoadAllMapsTest, "Maps.LoadAllInGame", ATF_Game)

void FLoadAllMapsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    // 收集所有地图文件作为测试用例
}

bool FLoadAllMapsTest::RunTest(const FString& Parameters)
{
    FEngineAutomationTestUtilities::LoadMap(Parameters);
    return true;
}
```

## 自动化规范 (Spec)

使用 BDD 风格编写测试：

```cpp
DEFINE_SPEC(MyCustomSpec, "MyGame.MyCustomSpec",
    EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)

void MyCustomSpec::Define()
{
    BeforeEach([this]() {
        // 每个测试前的设置
    });

    Describe("Feature", [this]() {
        It("should do something", [this]() {
            // 测试逻辑
        });
    });

    AfterEach([this]() {
        // 每个测试后的清理
    });
}
```

规范文件建议使用 `.spec.cpp` 扩展名。

## 功能测试（关卡测试）

1. 在关卡中放置 `AFunctionalTest` Actor
2. 在关卡蓝图中绑定 `OnTestStart` 事件
3. 执行测试逻辑后调用 `FinishTest`

关键函数：

| 函数 | 说明 |
|------|------|
| `PrepareTest` | 测试前的初始设置 |
| `IsReady` | 检查准备是否就绪（每 tick 调用） |
| `OnTestStart` | 测试开始时触发 |
| `OnTestFinished` | 测试完成后清理 |
| `FinishTest` | 标记测试完成 |

## CQTest 框架

CQTest (Code Quality Test) 是 `FAutomationTestBase` 的扩展，提供测试夹具和通用命令，支持 `before` 和 `after` 操作自动重置测试状态。

主要宏：

| 宏 | 说明 |
|---|------|
| `TEST` | 基本测试对象 |
| `TEST_CLASS` | 带 setup/teardown 的测试对象 |
| `TEST_CLASS_WITH_FLAGS` | 可自定义测试标志 |
| `TEST_CLASS_WITH_BASE` | 可继承自其他测试对象 |

## Angelscript 测试分层

当前 `Plugins/Angelscript/Source/AngelscriptTest/` 采用三层测试边界：

| 层级 | 目标 | 典型目录 | 允许依赖 |
|------|------|----------|----------|
| **Native Core** | 验证原生 AngelScript 公共 API 的最小可信基线（编译、执行、参数、返回值、注册、错误回调） | `Native/` | `AngelscriptInclude.h` / `angelscript.h` / `asIScriptEngine` / `asIScriptModule` / `asIScriptContext` |
| **Runtime Integration** | 验证 `FAngelscriptEngine`、模块管理、预处理、热重载分析、辅助工具等封装层行为 | `Angelscript/`、`Core/`、`Shared/`、`HotReload/` 中的非场景测试、`ClassGenerator/ClassGeneratorTests.cpp` | `FAngelscriptEngine`、`FAngelscriptModuleDesc`、`Shared` helper、预处理器、运行时封装 |
| **UE Scenario** | 验证 Actor / Blueprint / Interface / Component / GC / 世界生命周期等 UE 集成场景 | `Actor/`、`Blueprint/`、`Interface/`、`Component/`、`GC/`、`Delegate/`、`Inheritance/`、`Subsystem/`、`HotReload/AngelscriptHotReloadScenarioTests.cpp`、`ClassGenerator/AngelscriptScriptClassCreationTests.cpp` | UE 世界、生成类、Blueprint、场景辅助工具 |

### Native Core 规则

- `Native/` 测试命名采用 `Angelscript.TestModule.Native.<Category>.<TestName>`。
- `Native/` helper 和测试默认只允许使用 `AngelscriptInclude.h` / `angelscript.h` 暴露的公共 API。
- `Native/` 中**不要**包含 `AngelscriptEngine.h`、`AngelscriptPreprocessor.h`、`AngelscriptGameInstanceSubsystem.h` 这类项目封装层头。
- `Native/` 中**不要**直接包含 `source/as_*.h`；若需要验证 parser / bytecode / GC 等内部实现，应放到 `Internals/` 或单独的内部层。

### Internals 规则

- `Internals/` 允许通过 `StartAngelscriptHeaders.h` / `EndAngelscriptHeaders.h` + `source/as_*.h` 访问已导出的内部类型。
- `AngelscriptRuntime` 已公开 third-party `angelscript` 根目录和 `source/` include path；另外，public C API 通过 `ANGELSCRIPT_EXPORT` / `ANGELSCRIPT_DLL_LIBRARY_IMPORT` 构建定义向外部模块提供导入导出。
- 需要新增 internal class 导出时，应按需补齐，不要为了测试方便一次性公开所有内部类型。

### 选层决策

- 只想验证原生脚本引擎是否还能编译/执行一个最小片段：放 `Native/`。
- 需要 `FAngelscriptEngine`、`BuildModule()`、`CompileModuleFromMemory()`、模块记录或热重载封装行为：放 Runtime Integration。
- 需要 `UWorld`、`Actor`、`Blueprint`、`Component`、`Interface`、GC 生命周期或生成类交互：放 UE Scenario。

## 源文件位置约定

- 自动化测试放置在模块的 `Private\Tests` 目录中
- 文件命名为 `[ClassFilename]Test.cpp`，如 `FText` 的测试放在 `TextTest.cpp`
- 本仓库的 `Plugins/Angelscript/Source/AngelscriptTest/` 采用按层级与主题结合的目录布局：`Native/` 负责 Native Core，`Angelscript/` / `Core/` / `Shared/` 负责 Runtime Integration，`Actor/` / `Blueprint/` / `Interface/` 等目录负责 UE Scenario。
- 新增测试应先决定层级，再落到该层级下的具体主题目录中；不要把不相关的集成测试重新堆回笼统的 `Scenarios/` 目录。
- 自动化测试路径已去掉历史上的 `Scenario` 中间层，例如 `Angelscript.TestModule.Actor.*`、`Angelscript.TestModule.BlueprintChild.*`、`Angelscript.TestModule.Interface.*`。
- 自动化测试名与目录不必一一同名，但必须能从前缀看出层级，例如 `Angelscript.TestModule.Native.*`、`Angelscript.TestModule.Angelscript.*`、`Angelscript.TestModule.Actor.*`。

---

# 审查测试结果

## 日志文件

默认格式：
```
Test Started. Name={<short name>} Path={<full name>}
Test Completed. Result={<status>}. Name={<short name>} Path={<full name>}
**** TEST COMPLETE. EXIT CODE: <exit code number> ****
```

退出代码为 0 表示无测试失败。

## JSON 报告

使用 `-ReportExportPath="<output path>"` 导出 JSON 报告，包含：
- 所有事件
- 各测试的时长
- 设备详情

# 自定义日志输出（支持并行化测试）

并行跑多个测试实例时，默认日志都写入同一个 `Saved/Logs/ProjectName.log`，会互相覆盖。通过自定义每个实例的日志路径，可以安全地并行执行。

## 引擎日志文件路径控制

引擎解析日志文件名的优先级（源码 `GetAbsoluteLogFilename`）：

```
1. -LOG=<filename.log>        → 相对于 ProjectLogDir (Saved/Logs/) 的文件名
2. -LogFileName=<filename.log> → 同上，别名
3. -ABSLOG=<绝对路径.log>      → 使用完整绝对路径（忽略 ProjectLogDir）
```

文件扩展名必须是 `.log` 或 `.txt`，否则会被忽略。

### 命令行参数一览

| 参数 | 说明 | 示例 |
|------|------|------|
| `-LOG=<name.log>` | 相对 Saved/Logs/ 的日志文件名 | `-LOG=Test_Run1.log` |
| `-LogFileName=<name.log>` | 同 -LOG，别名 | `-LogFileName=Test_Run1.log` |
| `-ABSLOG=<path.log>` | 绝对路径日志文件 | `-ABSLOG=D:\TestLogs\Run1.log` |
| `-ReportExportPath=<dir>` | 测试报告 JSON 导出目录 | `-ReportExportPath=D:\Reports\Run1` |
| `-LogCmds="<cat> <level>"` | 设置日志类别详细级别 | `-LogCmds="LogAutomationTest Verbose"` |

## 并行化测试的日志隔离方案

### 方案一：使用 `-ABSLOG` 为每个实例指定独立日志文件

```powershell
# 实例1
powershell.exe -Command "Start-Process -FilePath 'UnrealEditor-Cmd.exe' -ArgumentList '\"Project.uproject\"','-ExecCmds=\"Automation RunTests TestA; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND','-ABSLOG=D:\TestLogs\TestA.log','-ReportExportPath=D:\Reports\TestA' -Wait -NoNewWindow"

# 实例2（可同时启动）
powershell.exe -Command "Start-Process -FilePath 'UnrealEditor-Cmd.exe' -ArgumentList '\"Project.uproject\"','-ExecCmds=\"Automation RunTests TestB; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND','-ABSLOG=D:\TestLogs\TestB.log','-ReportExportPath=D:\Reports\TestB' -Wait -NoNewWindow"
```

### 方案二：使用 `-LOG` 指定相对文件名

```
-LOG=TestA_Run.log            → 输出到 Saved/Logs/TestA_Run.log
-LOG=TestB_Run.log            → 输出到 Saved/Logs/TestB_Run.log
```

### 方案三：脚本动态生成带时间戳的路径

```powershell
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$testName = "MyTest"
$logPath = "D:\TestLogs\${testName}_${timestamp}.log"
$reportPath = "D:\Reports\${testName}_${timestamp}"

powershell.exe -Command "Start-Process -FilePath 'C:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList '\"D:\Workspace\Project\Project.uproject\"','-ExecCmds=\"Automation RunTests $testName; Quit\"','-Unattended','-NoPause','-NoSplash','-NullRHI','-NOSOUND','-ABSLOG=$logPath','-ReportExportPath=$reportPath' -Wait -NoNewWindow; Write-Host 'DONE'"
```

## 完整并行化命令行参数总结

每个并行实例需要隔离以下三个输出：

| 输出类型 | 参数 | 并行隔离方式 |
|---------|------|------------|
| **引擎日志** | `-ABSLOG=<绝对路径.log>` | 每个实例一个独立 .log 文件 |
| **测试报告** | `-ReportExportPath=<目录>` | 每个实例一个独立目录 |
| **截图对比** | 输出到 ReportExportPath 下 | 随报告目录自动隔离 |

## 日志详细级别控制

通过 `-LogCmds` 可调整特定日志类别的输出级别：

```
-LogCmds="LogAutomationTest Verbose"           # 自动化测试详细日志
-LogCmds="LogAutomationController VeryVerbose" # 控制器极详细日志
-LogCmds="Global Verbose"                      # 全局详细模式
```

日志级别（从高到低）：

| 级别 | 说明 | 打印到控制台 | 打印到日志文件 |
|------|------|:-----------:|:------------:|
| Fatal | 致命错误，导致崩溃 | ✅ | ✅ |
| Error | 错误（红色） | ✅ | ✅ |
| Warning | 警告（黄色） | ✅ | ✅ |
| Display | 显示信息 | ✅ | ✅ |
| Log | 普通日志 | ❌ | ✅ |
| Verbose | 详细 | ❌ | ❌（需启用） |
| VeryVerbose | 极详细 | ❌ | ❌（需启用） |

也可以在 `DefaultEngine.ini` 中配置：

```ini
[Core.Log]
LogAutomationTest=Verbose
LogAutomationController=Log
```

---

# 设计准则

参考 Epic 的通用测试准则：

1. **不假设状态**：测试可以在各机器中无序或并行运行
2. **不修改磁盘文件**：测试生成的文件在完成时删除
3. **假设状态不佳**：先清理再开始测试
4. **冒烟测试应快速**：1 秒内完成
5. **单元测试保持独立**：不依赖外部环境
