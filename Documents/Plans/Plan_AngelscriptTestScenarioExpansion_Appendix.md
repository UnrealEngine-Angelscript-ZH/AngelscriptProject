# AS 测试角度扩展附件

## 1. 检索结论摘要

### 1.1 仓库内已确认的基础

- `Documents/Guides/TestCatalog.md` 已给出按主题拆分的 Angelscript 测试目录，是现有真实落点基线。
- `Documents/Guides/Test.md` 已覆盖 `NullRHI`、`Automation RunTests`、测试组、`Gauntlet`、Spec 写法等主入口。
- `Documents/Plans/Plan_AngelscriptUnitTestExpansion.md` 已解决“测试分层与大盘扩张策略”。
- `Documents/Plans/Plan_ASInternalClassUnitTests.md` 已解决“internal class 深挖”主题。

### 1.2 本次补充出的关键外部/旁路信息

- `Automation Spec` 适合承接 `BeforeEach` / `AfterEach` / Async / latent completion / 参数化。
- 单机与多人模式切换、world 枚举、多客户端等待完成、超时失败信息，是值得优先模板化的高频边界。
- coverage/report export、hot reload batching、Gauntlet outer smoke 都应该作为“操作面模板”进入 docs，而不是只在经验中流传。

---

## 2. 测试角度矩阵

| 编号 | 测试角度 | 价值 | 推荐测试层 | 推荐目录 | 首批建议 |
| --- | --- | --- | --- | --- | --- |
| A1 | naming / discovery | 防止测试被静默漏收录 | Plugin Fast Tests | `Shared/` 或 `Core/` | 命名前缀、复杂 case pairing |
| A2 | parameterized / complex case | 稳定数据驱动扩测 | Plugin Fast Tests | `Template/` → 复制到主题目录 | case 数据、provider 配对 |
| A3 | standalone mode | 固化单机 UI / 本地控制器边界 | Scenario / Integration | `Template/`、后续 `Actor/` / `Subsystem/` | widget / local controller |
| A4 | client-server mode | 固化 authority 分流 | Scenario / Integration | `Template/`、后续 `Actor/` / `Component/` | server/client 双断言 |
| A5 | world context 枚举 | 解决多客户端上下文混淆 | Scenario / Integration | `Template/`、后续 `Subsystem/` | server world、client world 计数 |
| A6 | latent wait / timeout | 降低 flaky 测试概率 | Scenario / Integration | `Template/`、后续主题目录 | observable condition + timeout |
| A7 | report export / unattended | 服务 CI / agent 自动化 | Operational / Docs | `Documents/Guides/Test.md` | `-ReportExportPath`、恢复运行 |
| A8 | hot reload batching | 固化操作性和执行成本边界 | Plugin Fast Tests | `HotReload/` | batch size、module cap |
| A9 | network emulation | 验证 hostile network 情景 | Scenario / Integration | `HotReload/` 或 `Subsystem/` | latency / packet loss |
| A10 | coverage toggle | 让 coverage 变成可复用流程 | Runtime Unit / Docs | `Runtime/Tests/` + docs | 开关、输出路径 |
| A11 | screenshot optional | 保留视觉回归扩张口 | Graphics / Optional | docs only initially | 不进第一波 |
| A12 | performance optional | 防止测试执行成本失控 | Operational / Optional | docs only initially | 不进第一波 |
| A13 | Gauntlet outer smoke | 提供最外层会话壳 | Gauntlet | docs first | `UE.EditorAutomation`、`UE.Networking` |
| A14 | test group taxonomy | 稳定筛选入口 | Docs + Config | `Config/DefaultEngine.ini`、`Test.md` | smoke / fast / scenario |

---

## 3. 首波 12 个高性价比情景

1. simple template smoke：模板本身可编译、可注册、可运行。
2. complex template smoke：参数化模板能消费至少 3 个 case。
3. naming/discovery：无效前缀不会被误注册。
4. complex provider pairing：数据 provider 与 case 一一对应。
5. standalone UI：本地 `PlayerController` + widget 路径可执行。
6. server/client split：同一测试能明确区分 server 与 client 断言。
7. world enumeration：能拿到唯一 server world 和正确 client worlds。
8. latent timeout：条件达成前继续等待，超时后给出可读错误信息。
9. hot reload batching：批次上限与模块上限不会被默默越过。
10. unattended run：`NullRHI` 路径可稳定退出并导出结果。
11. report export：`-ReportExportPath` 路径可被正常消费。
12. Gauntlet outer smoke：一条最小命令能启动 outer shell 并跑一个 Angelscript smoke bucket。

---

## 4. 模板骨架

### 4.1 简单单元模板

**用途**：helper、discovery、配置解析、最小脚本执行。

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceholderTest,
    "Angelscript.TestModule.Template.Placeholder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceholderTest::RunTest(const FString& Parameters)
{
    // Arrange
    // 构建最小输入或最小脚本片段

    // Act
    // 执行被测 helper 或最小编译/执行路径

    // Assert
    TestTrue(TEXT("placeholder should succeed"), true);
    return true;
}
```

**固定要求**：
- 注册路径必须带清晰主题前缀。
- 断言消息描述行为，不复述函数名。
- 不能混入 world、PIE、多客户端依赖。

### 4.2 复杂参数化模板

**用途**：一组结构相同但输入不同的 case。

```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTemplateComplexTest,
    "Angelscript.TestModule.Template.ComplexPlaceholder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTemplateComplexTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("HappyPath"));
    OutBeautifiedNames.Add(TEXT("NullDependency"));
    OutBeautifiedNames.Add(TEXT("MalformedInput"));

    OutTestCommands.Add(TEXT("HappyPath"));
    OutTestCommands.Add(TEXT("NullDependency"));
    OutTestCommands.Add(TEXT("MalformedInput"));
}

bool FTemplateComplexTest::RunTest(const FString& Parameters)
{
    // Arrange from Parameters
    // Act
    // Assert
    return true;
}
```

**固定要求**：
- `BeautifiedName` 与 `TestCommand` 必须一一对应。
- 必须至少包含 happy path + 异常 path + 边界 path。

### 4.3 单机 UI / 本地控制器模板

**用途**：任何依赖 standalone、本地 player controller、widget 的场景。

```text
步骤：
1. 以 standalone 模式启动测试。
2. 获取本地 PlayerController。
3. 创建一个最小 Widget 或本地对象。
4. 验证 viewport / 本地权限 / 本地输入模式。
```

**固定要求**：
- 附带说明为什么该 case 不能默认跑在 client-server 模式。
- 不把 server/client 双分支混进这类模板。

### 4.4 多客户端 / world 枚举模板

**用途**：server/client 分流、多客户端世界统计、复制前置等待。

```text
步骤：
1. 启动带多个客户端的 PIE / Automation 环境。
2. 枚举 `WorldContext`。
3. 明确区分 server world 与 client worlds。
4. 在 server 与 client 分别执行断言。
```

**固定要求**：
- 至少有一条 server-only 断言。
- 至少有一条 client-only 断言。
- world 计数必须是显式断言，不是隐式假设。

### 4.5 latent wait / timeout 模板

**用途**：等待连接、等待复制、等待地图切换完成。

```text
规则：
1. 只等待 observable condition。
2. 必须有 timeout。
3. timeout 失败消息必须能说明在等什么。
4. 禁止直接 sleep 固定秒数替代真实条件。
```

### 4.6 operational / unattended 模板

**用途**：CI、agent、批量自动化。

```text
命令要素：
- `Tools\RunTests.ps1`
-- `-Group AngelscriptScenario` / `-TestPrefix Angelscript.TestModule.Scenario.*`
-- `-ReportExportPath=<Project>/Saved/Automation/Scenario/<RunId>/Reports`
-- `-ABSLOG=<Project>/Saved/Automation/Scenario/<RunId>/Logs/Editor.log`
-- `-TimeoutMs <ms>`、`-Label <name>`、`-- -NullRHI -Unattended -NoPause -NoSplash -NOSOUND`（或 `-Render`）
```

**固定要求**：
- 测试名要能独立筛选。
- 导出路径策略要稳定，不依赖人工点 UI。

### 4.7 Gauntlet outer smoke 模板

**用途**：outer shell，不是内层业务断言替代品。

```text
最小方案：
1. 选择 `UE.EditorAutomation` 或 `UE.Networking`。
2. 启动 1 个 editor/server + N 个 client（按场景而定）。
3. 运行一个 Angelscript smoke bucket。
4. 检查 pass/fail/crash/timeout。
```

**固定要求**：
- 只断言 outer shell 级别的完成状态。
- 不把所有业务断言搬进 Gauntlet。

---

## 5. 推荐目录映射

| 模板 / 情景 | 首选目录 | 复制后的真实落点 |
| --- | --- | --- |
| simple unit template | `Plugins/Angelscript/Source/AngelscriptTest/Template/` | `Shared/`、`Core/`、`Bindings/` |
| complex parameterized template | `Plugins/Angelscript/Source/AngelscriptTest/Template/` | `Core/`、`Compiler/`、`Internals/` |
| standalone UI template | `Plugins/Angelscript/Source/AngelscriptTest/Template/` | `Actor/`、`Subsystem/`、`Editor/` |
| multiplayer/world template | `Plugins/Angelscript/Source/AngelscriptTest/Template/` | `Actor/`、`Component/`、`Subsystem/` |
| latent wait template | `Plugins/Angelscript/Source/AngelscriptTest/Template/` | 所有需要等待的主题目录 |
| hot reload operational case | 直接落 `Plugins/Angelscript/Source/AngelscriptTest/HotReload/` | 保持在 `HotReload/` |
| coverage/report export docs | `Documents/Guides/Test.md` | docs 入口 |
| Gauntlet outer smoke docs | `Documents/Guides/Test.md` / 计划文档 | 后续脚本目录 |

---

## 6. 执行优先级建议

- **优先级最高**：`A1`、`A2`、`A3`、`A5`、`A6`、`A8`、`A13`
- **第二波**：`A4`、`A7`、`A9`、`A10`、`A14`
- **第三波 / 可选**：`A11`、`A12`

## 7. 给执行者的最后约束

- 先复制模板，再落真实主题 case，不要跳过模板层直接各写各的。
- 一个 case 只表达一种主要边界，不要把 standalone、multiplayer、latency、coverage 全塞进同一条用例里。
- 任何一类模板一旦发现不适配真实目录，先回写附件再继续扩张。
- 如果一条测试需要 world、tick、PIE、多客户端，就默认它不是 `Runtime Unit`。
