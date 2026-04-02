# AngelScript 2.38 Lambda 迁移计划

## 背景与目标

### 背景

当前 `Plugins/Angelscript` 内嵌的 AngelScript 公共头版本仍标记为 `2.33.0 WIP`，见 `Plugins/Angelscript/Source/AngelscriptRuntime/Core/angelscript.h`。

但当前仓库并不是一个“纯 2.33 原样”的分支，而是一个带有大量插件侧与 `[UE++]` 改造、并且已经局部吸收了部分较新 AngelScript 能力接口的混合基线。与 lambda 直接相关的现状如下：

- `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp` 已经存在 `IsLambda()` 与 `ParseLambda()`，但参数解析仍停留在较早期形态：
  - 不具备上游 `2.38` 的 `FindIdentifierAfterScope()` 辅助逻辑；
  - 不能正确覆盖“复杂类型 + 可选参数名 + nameless 参数”这一组官方场景；
  - 仍使用当前分支的旧 token 名（如 `ttOpenParanthesis` / `ttCloseParanthesis`），不能直接无脑贴 upstream 代码。
- `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.h` 暴露了 `asCExprContext::SetLambda()` / `IsLambda()`，但需要对照 `2.38` 的 parser/compiler 语义把真正的编译与签名推导链路补齐。
- `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.h` 已暴露 `RegisterLambda(...)` 声明，说明当前分支已经有过“半移植”痕迹，但仍未形成完整闭环。
- `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp` 当前仍把 `funcdef` / function pointer 语法视为**应当失败**的能力边界，这与 AngelScript 官方 anonymous function 依赖 `funcdef` / function handle 的前提直接冲突。
- 当前 `AngelscriptTest` 已具备比较完善的 `Internals/`、`Compiler/`、`Angelscript/` 三层测试入口，但尚无任何 lambda 专项覆盖。

同时，上游 AngelScript 官方文档与 `Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_functionptr.cpp` 已给出足够明确的目标语义：

- 语法入口是 **anonymous function**：`function(...) { ... }`
- 参数类型可显式声明，也可在目标 `funcdef` 已知时依赖签名推导
- **参数名可省略**（nameless args）
- 动态重复编译时不应因 lambda 内部命名而发生冲突
- **不支持 closure capture**：anonymous function 不能访问同一作用域中的局部变量

### 官方 lambda 语法规则（展示基线）

本计划中的“lambda”统一指 AngelScript 官方语义中的 **anonymous function**。其语法基线以 `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.cpp` 中的 BNF 为准：

```text
LAMBDA ::= 'function' '(' ((TYPE TYPEMOD)? IDENTIFIER? (',' (TYPE TYPEMOD)? IDENTIFIER?)*)? ')' STATBLOCK
```

从该 BNF 可以直接推出以下规则，后续实现与测试都以这些规则为准：

- 必须使用关键字 `function` 作为匿名函数入口，而不是自定义 `lambda` 关键字。
- 参数列表写在 `(` `)` 中，可以为空。
- 每个参数都遵循 `(TYPE TYPEMOD)? IDENTIFIER?`：
  - 类型是**可选**的；
  - 参数名也是**可选**的；
  - 这就是 nameless 参数成立的语法基础。
- 多参数之间用 `,` 分隔。
- 函数体必须是 `STATBLOCK`，也就是 `{ ... }` 语句块，而不是单表达式箭头语法。
- 当目标 `funcdef` / function handle 签名已知时，可以省略参数类型；当存在重载歧义时，可以通过显式参数类型消歧。
- anonymous function 不是“任意位置都能独立存在的表达式”，而是需要放在官方允许的上下文里，例如赋给 `funcdef` handle、作为目标参数传入等。

#### 官方正例

```angelscript
funcdef bool CMP(int first, int second);

bool TestBasic()
{
    CMP@ Fn = function(a, b) { return a < b; };
    return Fn(1, 2);
}
```

```angelscript
funcdef void F(int, int);

void TestNamelessArg()
{
    F@ Fn = function(int arg1, int ) {};
}
```

```angelscript
namespace UI { enum MouseEvent {} }
namespace GUI
{
    interface CallbackContext {}
    funcdef void MouseCallback(CallbackContext@, const UI::MouseEvent &in);
}

void TestTypedLambda(GUI::MouseCallback@ Fn)
{
}

void Bind()
{
    TestTypedLambda(function(GUI::CallbackContext@ ctx, const UI::MouseEvent &in event) {});
}
```

#### 官方负例边界

以下都应在计划对应的测试里被显式覆盖，而不是靠口头约定：

- stand-alone anonymous function 非法：

```angelscript
function(a, b) { return a < b; };
```

- 直接调用 anonymous function 非法：

```angelscript
function() {}();
```

- 访问同作用域局部变量的 closure capture 非本次能力范围，应保持失败边界：

```angelscript
funcdef int F();

int TestCapture()
{
    int local = 42;
    F@ Fn = function() { return local; };
    return Fn();
}
```

#### 对当前实现的直接启示

- parser 不能只“认出 `function (` 开头”，还必须正确处理：
  - 可选类型
  - 可选参数名
  - nameless 参数
  - 命名空间/作用域限定类型
- compiler 不能把 anonymous function 当成普通 stand-alone block；它必须在 `funcdef` / function handle 目标已知时参与签名推导与隐式转换。
- 测试不能只写正例；官方负例同样是本次迁移的验收边界。

### 目标

本计划的目标不是“整体升级 AngelScript 到 2.38”，而是：

1. 在当前 `Plugins/Angelscript` 基线中，**按 AngelScript 2.38 官方 anonymous function 语义** 补齐 lambda 能力缺口。
2. 允许修改 `ThirdParty/angelscript/source/`，但坚持“最小必需移植”，避免把整个 2.38 升级面一次性卷入。
3. 将当前对 `funcdef` / function pointer 的负例能力边界，收敛为 lambda 所需的最小正向能力基线。
4. 为 parser / compiler / end-to-end 三层补齐单元测试与回归测试。
5. 明确写死本次语义边界：**不实现 closure capture，不借题扩成完整闭包系统**。

## 范围与边界

- **纳入范围**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/` 下与 lambda 直接相关的 parser / builder / compiler / runtime 文件
  - `Plugins/Angelscript/Source/AngelscriptRuntime/Core/angelscript.h` 中与能力暴露直接相关的公共声明审计
  - `Plugins/Angelscript/Source/AngelscriptTest/` 下 parser / compiler / function / pipeline 测试
  - `Documents/Knowledges/` 与 `Documents/Guides/Test.md` 的同步更新
- **不纳入范围**
  - 整体把当前 ThirdParty 从 `2.33.0 WIP` 升到完整 `2.38.0`
  - 顺带落地 `template functions`、`using namespace`、`try/catch` 等非 lambda 主题特性
  - 扩展 anonymous function 为可捕获局部变量的 closure 系统
  - 重构 UE delegate / event 体系本身

## 当前事实状态快照

### 语言与 ThirdParty 事实

- 当前公共版本号：`Plugins/Angelscript/Source/AngelscriptRuntime/Core/angelscript.h`
  - `ANGELSCRIPT_VERSION = 23300`
  - `ANGELSCRIPT_VERSION_STRING = "2.33.0 WIP"`
- 当前本地 parser：`Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`
  - 已有 `IsLambda()` / `ParseLambda()`
  - `ParseLambda()` 只覆盖早期参数解析方式，不具备 `2.38` 那套“复杂作用域类型 + 可选参数名”的完整能力
- 当前上游 parser：`Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.cpp`
  - `IsLambda()`：`1744` 附近
  - `FindIdentifierAfterScope()`：`1780` 附近
  - `ParseLambda()`：`1820` 附近
- 当前本地 compiler header：`Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.h`
  - 暴露 `SetLambda()` / `IsLambda()`，表明编译链路已预留 lambda 扩展点
- 当前上游 compiler header：`Reference/angelscript-v2.38.0/sdk/angelscript/source/as_compiler.h`
  - 同样存在 `SetLambda()` / `IsLambda()`，可作为对齐基线
- 当前本地 builder header：`Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.h`
  - 已声明 `RegisterLambda(...)`

### 测试与能力边界事实

- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptParserTests.cpp`
  - 已有 parser AST / syntax error 入口，适合补 parser 级 lambda 测试
- `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptCompilerTests.cpp`
  - 已有 bytecode / variable scope / function call / type conversion 入口，适合补 compiler 级 lambda 测试
- `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp`
  - 当前 `Functions.Pointer` 仍显式断言 function pointer 语法应失败，是 lambda 迁移的直接阻塞项
- `Plugins/Angelscript/Source/AngelscriptTest/Compiler/AngelscriptCompilerPipelineTests.cpp`
  - 已有 end-to-end compile pipeline 测试，适合补“UE delegate/event 扩展语法与 funcdef/lambda 共存”的回归用例
- 上游官方测试：`Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_functionptr.cpp`
  - 已覆盖 `function(v) { ... }`
  - 已覆盖 nameless 参数
  - 已覆盖 `CompileFunction()` 重复编译不冲突
  - 已覆盖 stand-alone anonymous function 的非法场景

## 推荐技术路线

本 Plan 采用以下固定路线，而不是把选择留到实现时再漂移：

1. **以当前分支为底座做定点移植**，不做整包 2.38 替换。
2. **先补 `funcdef` / function handle 的最小正向基线，再补 lambda**，因为官方 anonymous function 依赖它。
3. **parser 以 upstream 2.38 为行为基线，但按当前分支 token / helper 命名做人工适配**，不做机械 cherry-pick。
4. **compiler/runtime 只补 lambda 成立所需的最小链路**，不把无关 2.38 特性顺带拉进来。
5. **测试先行**：先把 parser / compiler / end-to-end 期望固定下来，再做实现。
6. **保持官方限制**：本次不设计 closure capture 数据结构；涉及同作用域局部变量访问时，应维持“编译失败”边界。

## 实施纪律

- 所有实现阶段默认遵循 **先写失败测试 → 再补实现 → 再跑目标回归** 的顺序，不能先改 ThirdParty 再补测试。
- 如果 Phase 1 / Phase 2 实施过程中发现必须一并引入 `template functions`、`using namespace`、`try/catch` 或大范围 public API 升级，说明问题已超出“定点 lambda 移植”边界；此时应停止扩 scope，先补写新的 `AS 2.38` 升级计划，再决定是否继续。

## 分阶段执行计划

### Phase 0：冻结语义边界与移植基线

> 目标：把“这次到底要移植什么、明确不移植什么、依赖哪些上游文件”固定下来，避免后续实现阶段持续扩 scope。

- [ ] **P0.1** 固化本次 lambda 语义边界
  - 依据官方文档与 `Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_functionptr.cpp`，将本次能力定义为 **anonymous function**：`function(...) { ... }`
  - 明确写入实施记录：支持显式参数类型、签名推导、nameless 参数、动态重复编译；**不支持 closure capture**
  - 明确“当前 UE 扩展里的 `delegate` / `event` 与 AngelScript 官方 function handle / delegate 不是一回事”，后续实现与文档必须分开表述
- [ ] **P0.1** 📦 Git 提交：`[Lambda] Docs: freeze AS238 anonymous-function scope`

- [ ] **P0.2** 建立本地/上游对照清单
  - 对照以下文件，形成实现时的主参考名单：
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.cpp`
    - `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.h`
    - `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.cpp`
    - `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_compiler.h`
    - `Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_functionptr.cpp`
  - 额外记录当前分支的手工适配点：`ttOpenParanthesis` / `ttCloseParanthesis` 命名、`asCExprContext(asCBuilder*)` 构造方式、APV2/`[UE++]` 修改块
- [ ] **P0.2** 📦 Git 提交：`[Lambda] Docs: add local-vs-upstream file map`

- [ ] **P0.3** 固定 function handle 前置缺口
  - 以 `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp` 中当前 `Functions.Pointer` 负例为起点
  - 明确后续执行规则：若 anonymous function 仍需要依赖 `funcdef @` / function handle，则该负例必须被拆分为“新正例 + 保留不再适用的旧负例说明”，不能继续把 function pointer 全量视为不支持
- [ ] **P0.3** 📦 Git 提交：`[Lambda] Docs: record function-handle prerequisite gate`

### Phase 1：移植 ThirdParty parser / builder 的 lambda 注册链路

> 目标：把 anonymous function 从“词法可识别但语义不完整”提升到“parser 可稳定建树、builder 可稳定注册”。

- [ ] **P1.1** 对齐 `as_parser.h` 的 helper 声明与 parser 能力面
  - 修改 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.h`
  - 补齐与上游 `2.38` lambda 解析直接相关的 helper 声明，**必须明确覆盖**：
    - `FindIdentifierAfterScope(...)`
    - `FindTokenAfterType(...)`（或当前分支中语义完全等价、已被 parser 实际复用的 helper）
  - 保持当前分支已有扩展（如 `ConditionAsAssignment`、`AccessDecl`、`ClassDefaultStatement`）不被误删
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty/Lambda] Feat: align parser helper declarations for AS238 lambda`

- [ ] **P1.2** 用 `2.38` 行为基线重写本地 `IsLambda()` / `ParseLambda()`
  - 修改 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`
  - 以 `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.cpp` 的 `IsLambda()` / `ParseLambda()` 为基线
  - 适配当前分支 token 命名（`ttOpenParanthesis` / `ttCloseParanthesis`）与已有 helper 调用风格
  - 补齐以下能力：
    - 多参数 lambda
    - 可选显式类型
    - 可选参数名（包括 nameless 参数）
    - 复杂命名空间类型 / 作用域类型参数
  - 保持 lambda AST 继续复用 `snFunction`，不要在本任务里自行发明新的 `snLambda` 节点类型
- [ ] **P1.2** 📦 Git 提交：`[ThirdParty/Lambda] Feat: port AS238 lambda parser semantics`

- [ ] **P1.3** 审计并补齐 `ParseExprValue()` 对 lambda 的入口接线
  - 确认本地 `ParseExprValue()` 中 `IsLambda()` 检测与 `ParseLambda()` 调用顺序与 `2.38` 一致
  - 确认匿名函数在 `statement block`、实参列表、强制转换等上下文中不会被错误当成 stand-alone expression
  - 若当前分支存在 UE 扩展语法顺序冲突，优先做最小顺序修正，不做 parser 大重排
- [ ] **P1.3** 📦 Git 提交：`[ThirdParty/Lambda] Fix: route lambda through expression parser correctly`

- [ ] **P1.4** 接通 `as_builder.cpp` 中的 lambda 注册路径
  - 修改 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.cpp`
  - 补齐/校正 `RegisterLambda(...)` 的实现，使其能把 parser 产出的 anonymous function 以 module-owned script function 形式注册
  - 对齐上游“重复 `CompileFunction()` 不应因内部 lambda 名称冲突而失败”的行为
  - 采用上游内部命名思路，但需适配当前 APV2/热重载状态管理，不要破坏现有模块生命周期
- [ ] **P1.4** 📦 Git 提交：`[ThirdParty/Lambda] Feat: register anonymous functions during builder pass`

### Phase 2：移植 compiler / runtime 的 lambda 编译与 function-handle 转换链路

> 目标：让 anonymous function 不止能 parse，还能真正被编译、推导签名、生成可调用 function handle。

- [ ] **P2.1** 补齐 `asCExprContext` 的 lambda 语义实现
  - 修改 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.cpp`
  - 对齐 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.h` 中 `SetLambda()` / `IsLambda()` 的实现
  - 确保 lambda 在表达式上下文中能被识别为“等待绑定到 funcdef / function handle 的匿名函数表达式”，而不是普通 stand-alone block
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/Lambda] Feat: implement compiler lambda expression context`

- [ ] **P2.2** 实现 lambda → funcdef/function handle 的签名推导与隐式转换
  - 修改 `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.cpp`
  - 以当前已声明的 `ImplicitConvLambdaToFunc(...)` 为**主对齐入口**，优先沿现有 seam 实现，而不是额外发明一条平行转换路径
  - 目标行为对齐上游：
    - 在目标 `funcdef` 已知时，可省略匿名函数参数类型
    - 在重载不明确时，支持显式参数类型帮助消歧
    - stand-alone anonymous function 仍应报错，而不是被默默接受
  - 若当前分支局部实现与上游命名/拆分方式不一致，以现有 seam 为主，但必须保证行为等价于上游官方 anonymous-function 语义
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/Lambda] Feat: infer lambda signature from target funcdef`

- [ ] **P2.3** 审计 function-handle / delegate 运行时与序列化热点
  - 重点检查并按需修改：
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptfunction.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_scriptfunction.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_typeinfo.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_restore.cpp`
  - 目标不是大规模重写，而是验证 lambda 生成后的 function handle 在当前分支的热重载 / 恢复 / module 生命周期下不会留下明显破口
- [ ] **P2.3** 📦 Git 提交：`[ThirdParty/Lambda] Fix: stabilize anonymous function runtime metadata paths`

- [ ] **P2.4** 显式维持“无 closure capture”边界
  - 不为 anonymous function 设计额外捕获存储结构
  - 当脚本试图访问同作用域局部变量时，应保持编译失败边界，并让诊断信息可测试
  - 任何“为了让测试过而偷偷捕获外层局部”的实现都不在本计划范围内
- [ ] **P2.4** 📦 Git 提交：`[ThirdParty/Lambda] Docs: freeze no-closure-capture behavior`

### Phase 3：接通插件编译管线与现有语法共存回归

> 目标：确认 lambda 不只是 ThirdParty 自测可用，而是能穿过当前插件的 `FAngelscriptEngine` 编译路径稳定工作。

- [ ] **P3.1** 修正当前 `funcdef` / function pointer 的测试与能力边界
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp`
  - 把当前“函数指针语法应失败”的旧负例，重构为：
    - `funcdef` / function handle 正例
    - anonymous function 赋值与调用正例
    - stand-alone anonymous function 负例
  - 目标是让测试边界与新能力一致，而不是保留过时断言
- [ ] **P3.1** 📦 Git 提交：`[Test/Lambda] Refactor: replace obsolete function-pointer negative gate`

- [ ] **P3.2** 检查 `FAngelscriptEngine` 编译路径是否会干扰 `function(...) {}` 语法
  - 审计但仅按需修改以下路径：
    - `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestUtilities.h`
    - `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestEngineHelper.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/Preprocessor/` 下与脚本源码重写直接相关的文件
  - 确保 anonymous function 源码能原样到达 ThirdParty parser，而不是被预处理器或注解编译链路篡改
- [ ] **P3.2** 📦 Git 提交：`[Lambda] Fix: preserve anonymous-function syntax through compile pipeline`

- [ ] **P3.3** 建立“UE delegate/event 扩展语法与 funcdef/lambda 共存”回归
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Compiler/AngelscriptCompilerPipelineTests.cpp`
  - 新增混合脚本：同一模块内同时声明
    - `delegate void F...(...)`
    - `event void F...(...)`
    - `funcdef bool CMP(...)`
    - `CMP@ Fn = function(...) { ... };`
  - 目标是防止 parser 顺序调整后把现有 UE 扩展语法打坏
- [ ] **P3.3** 📦 Git 提交：`[Test/Lambda] Feat: add mixed delegate-funcdef-lambda pipeline regression`

### Phase 4：补齐 parser / compiler / runtime 三层单元测试

> 目标：把官方 `test_functionptr.cpp` 里的核心 anonymous-function 场景映射到当前仓库的现有测试分层中。

- [ ] **P4.1** 在 `Internals/AngelscriptParserTests.cpp` 添加 parser 级 lambda 用例
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptParserTests.cpp`
  - 至少覆盖：
    - `Parser.Lambda.Basic` — `function(a, b) { return a < b; }` 能建树
    - `Parser.Lambda.TypedParameters` — 显式类型参数能建树
    - `Parser.Lambda.NamelessParameter` — `function(int arg1, int ) {}` 能建树
    - `Parser.Lambda.NamespacedTypeParameter` — 命名空间类型参数解析稳定
- [ ] **P4.1** 📦 Git 提交：`[Test/Lambda] Feat: add parser-level anonymous-function tests`

- [ ] **P4.2** 在 `Internals/AngelscriptCompilerTests.cpp` 添加 compiler 级 lambda 用例
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptCompilerTests.cpp`
  - 至少覆盖：
    - `Compiler.Lambda.FuncdefAssign` — lambda 可绑定到 funcdef/function handle
    - `Compiler.Lambda.Bytecode` — 编译后函数体可执行
    - `Compiler.Lambda.RecompileNoNameConflict` — 重复 `CompileFunction()` 不因匿名函数内部命名冲突失败
    - `Compiler.Lambda.CompileGlobalVar` — 含匿名函数的 `CompileGlobalVar()`/等价路径具备稳定行为
    - `Compiler.Lambda.StandaloneInvalid` — stand-alone anonymous function 维持非法边界
- [ ] **P4.2** 📦 Git 提交：`[Test/Lambda] Feat: add compiler-level anonymous-function tests`

- [ ] **P4.3** 在 `AngelscriptFunctionTests.cpp` 添加运行时 / 行为级用例
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp`
  - 至少覆盖：
    - `Functions.Lambda.BasicInvoke` — `funcdef` + anonymous function 赋值并执行
    - `Functions.Lambda.TypedDisambiguation` — 通过显式参数类型解决重载歧义
    - `Functions.Lambda.NoClosureCapture` — 访问同作用域局部变量时编译失败
    - `Functions.Lambda.InvalidStandalone` — 非目标上下文中的 anonymous function 报错
    - `Functions.Lambda.FunctionIdentifierNotReserved` — `function` 作为普通标识符/调用目标时不被 `IsLambda()` 误判
    - `Functions.Lambda.InvalidDirectInvoke` — `function(){}()` 仍按官方边界报错
    - `Functions.Lambda.InvalidByValueFuncdef` — 不把超出官方匿名函数语义的 by-value funcdef 形式误放开
- [ ] **P4.3** 📦 Git 提交：`[Test/Lambda] Feat: add runtime anonymous-function behavior tests`

- [ ] **P4.4** 补齐模块生命周期与共享路径回归
  - 修改 `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptCompilerTests.cpp` 与/或 `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp`
  - 至少覆盖：
    - shared function 内部匿名函数可编译/执行
    - save/load 或当前分支等价 restore 路径不会破坏匿名函数句柄
    - 模块丢弃 / 重新编译后，lambda 元数据不会遗留无效命名冲突
  - 若当前仓库没有直接可复用的 save/load 测试 helper，则至少以现有 restore/hot-reload 相关入口建立等价回归，而不是仅停留在代码审计
- [ ] **P4.4** 📦 Git 提交：`[Test/Lambda] Feat: add lifecycle regression for anonymous functions`

- [ ] **P4.5** 运行分层测试与全量回归
  - 使用 `Documents/Guides/Test.md` 中约定的 `UnrealEditor-Cmd.exe` 自动化测试入口
  - 至少执行：
    - `Automation RunTests Angelscript.TestModule.Internals.Parser`
    - `Automation RunTests Angelscript.TestModule.Internals.Compiler`
    - `Automation RunTests Angelscript.TestModule.Angelscript.Functions`
    - `Automation RunTests Angelscript.TestModule.Compiler.EndToEnd`
    - `Automation RunTests Angelscript.TestModule`
  - 若 `AgentConfig.ini` 配置了 `Paths.EngineRoot` 与 `Test.DefaultTimeoutMs`，执行时按 `Documents/Guides/Build.md` / `Documents/Guides/Test.md` 要求解析完整命令
- [ ] **P4.5** 📦 Git 提交：`[Test/Lambda] Test: verify parser-compiler-runtime lambda regression`

### Phase 5：文档、知识沉淀与 ThirdParty 变更可追踪性

> 目标：防止这次定点移植在后续 2.38 路线或第三方更新时再次变成“黑盒补丁”。

- [ ] **P5.1** 新增 lambda 能力知识文档
  - 创建 `Documents/Knowledges/AngelscriptLambda.md`
  - 内容至少包括：
    - 当前仓库支持的 anonymous function 语法
    - 与官方 `2.38` 对齐的语义清单
    - 明确“不支持 closure capture”
    - 关键测试名称与执行入口
    - 本次修改过的 ThirdParty 文件清单
- [ ] **P5.1** 📦 Git 提交：`[Docs/Lambda] Feat: add anonymous-function knowledge document`

- [ ] **P5.2** 更新 `Documents/Guides/Test.md`
  - 增补 lambda 测试分组与推荐运行命令
  - 明确 `Parser / Compiler / Functions / EndToEnd` 四类与 lambda 相关的测试入口
- [ ] **P5.2** 📦 Git 提交：`[Docs/Lambda] Feat: document lambda automation test entrypoints`

- [ ] **P5.3** 在所有新增或重写的 ThirdParty 关键改动处补充 `[UE++]` / `[UE--]` 变更标记与上游来源说明
  - 至少覆盖：
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.h`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.cpp`
    - `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.cpp`
  - 目标是让后续做更大范围 `2.38` 对齐时，可以快速识别本次 lambda 定点移植的边界
- [ ] **P5.3** 📦 Git 提交：`[Docs/Lambda] Chore: annotate third-party lambda port boundaries`

## 验收标准

1. **基础语法可用**：以下脚本能通过当前插件编译并执行：
   ```angelscript
   funcdef bool CMP(int a, int b);
   bool Run()
   {
       CMP@ Fn = function(a, b) { return a < b; };
       return Fn(1, 2);
   }
   ```
2. **显式类型与 nameless 参数可用**：`function(int arg1, int ) {}` 这类匿名函数可以通过 parser / compiler 测试。
3. **复杂参数类型可用**：带命名空间或作用域限定的参数类型不会在 lambda 解析时被误拆。
4. **官方负例边界不被误放开**：以下场景仍按官方边界稳定报错：
   - stand-alone anonymous function
   - `function(){}()` 直接调用
   - by-value `funcdef` 形式（若超出官方匿名函数适用范围）
5. **重复编译与模块生命周期稳定**：`CompileFunction()`、`CompileGlobalVar()`、shared function、save/load 或当前分支等价 restore/hot-reload 路径不会因 lambda 引入命名或句柄失效问题。
6. **无 closure capture 漂移**：试图访问同作用域局部变量时，保持编译失败并有稳定诊断。
7. **`function` 不被错误保留字化**：当 `function` 出现在非 lambda 语境中时，不会被 parser 误识别。
8. **旧功能不回退**：现有 `delegate` / `event` 扩展测试与 `Angelscript.TestModule` 全量回归通过。
9. **文档可追踪**：`Documents/Knowledges/AngelscriptLambda.md` 与 `Documents/Guides/Test.md` 明确记录了语义、限制与测试入口。

## 风险与注意事项

### 风险 1：当前分支并非纯净 2.33，也不是纯净 2.38

当前仓库已经混入大量 `[UE++]` 与部分 `2.38` API 表面兼容点。若直接按“整包替换”思路推进，很容易把 UE/热重载/APV2 特性一并打坏。

**缓解**：坚持“parser / builder / compiler / runtime 的最小定点移植”，每个文件都用上游对应文件做行为基线，但不做整目录替换。

### 风险 2：function handle 是 lambda 的前置能力，不是附带小修

当前 `Functions.Pointer` 负例说明本分支对 `funcdef` / function handle 仍有显式限制。如果这个前置缺口不先收敛，anonymous function 无法按照官方方式落地。

**缓解**：Phase 0 / Phase 3 明确把旧负例转为迁移门槛，不允许“lambda 做完了，但 function handle 仍被视为不支持”的半完成状态。

### 风险 3：`delegate` 术语冲突会误导实现范围

当前插件已有 UE 扩展语法 `delegate void F...(...)` / `event void F...(...)`，而 AngelScript 官方测试里“delegate”常指 script function delegate / function handle 语义。两者混写极易导致 parser 或文档误判。

**缓解**：所有实现与文档必须把“UE delegate 扩展”和“官方 funcdef/function handle/lambda”分开命名，并用混合回归测试守住共存性。

### 风险 4：用户对“lambda”直觉可能期待 closure capture

官方 `2.38` anonymous function 明确**不支持 closure capture**。如果实现阶段忘了这个边界，容易在需求理解上不断加码，最终演化成新的语言设计项目。

**缓解**：把“无 closure capture”写入本 Plan、知识文档与负例测试，作为显式能力边界。

### 风险 5：动态编译 / 热重载命名冲突

上游已有“重复 `CompileFunction()` 时 lambda 内部命名冲突”的历史问题；当前插件又叠加了模块缓存、热重载、restore 路径，风险更高。

**缓解**：把重复动态编译列为 compiler 级必测项，并把 `as_builder.cpp` / `as_restore.cpp` 纳入审计范围。

## 参考索引

| 参考项 | 路径/链接 | 用途 |
| --- | --- | --- |
| 当前公共版本头 | `Plugins/Angelscript/Source/AngelscriptRuntime/Core/angelscript.h` | 确认当前公共版本仍为 `2.33.0 WIP` |
| 当前本地 parser | `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_parser.cpp` | 当前 lambda parser 行为与上游差异 |
| 当前本地 compiler header | `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_compiler.h` | 当前 lambda 编译接口预留情况 |
| 当前本地 builder header | `Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source/as_builder.h` | `RegisterLambda(...)` 入口 |
| 上游 parser | `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_parser.cpp` | `IsLambda()` / `ParseLambda()` 行为基线 |
| 上游 compiler header | `Reference/angelscript-v2.38.0/sdk/angelscript/source/as_compiler.h` | `asCExprContext` lambda 语义基线 |
| 上游 feature 测试 | `Reference/angelscript-v2.38.0/sdk/tests/test_feature/source/test_functionptr.cpp` | 官方 anonymous function 回归用例来源 |
| 当前 parser 测试 | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptParserTests.cpp` | 新增 parser 级 lambda 测试位置 |
| 当前 compiler 测试 | `Plugins/Angelscript/Source/AngelscriptTest/Internals/AngelscriptCompilerTests.cpp` | 新增 compiler 级 lambda 测试位置 |
| 当前 functions 测试 | `Plugins/Angelscript/Source/AngelscriptTest/Angelscript/AngelscriptFunctionTests.cpp` | function handle / lambda 行为测试位置 |
| 当前 pipeline 测试 | `Plugins/Angelscript/Source/AngelscriptTest/Compiler/AngelscriptCompilerPipelineTests.cpp` | 混合语法共存回归位置 |
| 测试指南 | `Documents/Guides/Test.md` | 自动化测试执行命令模板 |
| 构建指南 | `Documents/Guides/Build.md` | `AgentConfig.ini` 与 `EngineRoot` 解析规则 |
