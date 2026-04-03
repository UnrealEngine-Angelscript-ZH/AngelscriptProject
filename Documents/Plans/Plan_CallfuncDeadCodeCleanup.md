# 清理 as_callfunc_x64_msvc_asm 死代码

## 背景与目标

当前 `AngelscriptRuntime.Build.cs` 显式链接了一个不存在的静态库：

```csharp
if (Target.Platform == UnrealTargetPlatform.Win64)
{
    PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Intermediate", "Build", "as_callfunc_x64_msvc_asm.lib"));
}
```

这个 `.lib` 来自 `as_callfunc_x64_msvc_asm.asm`，其中实现了 `CallX64`、`GetReturnedFloat`、`GetReturnedDouble` 三个底层汇编函数，原本用于在 Windows x64 MSVC 平台上按 calling convention 调用 native C++ 函数。

然而，Hazelight 在这个 fork 中已用模板元编程（`ASAutoCaller` / `FunctionCallers.h`）**完全替代**了上述汇编路径：

- `as_callfunc_x64_msvc.cpp`：整个文件被 `#if 0` 包裹，从不参与编译
- `as_callfunc.cpp`：`CallSystemFunctionNative` 调用链也被 `#if 0` 关闭；实际路径改为 `CallFunctionCaller()`，走 `sysFunc->caller.FunctionCaller` 函数指针
- 新路径通过 `MakeFunctionCaller<R, Args...>` 在绑定阶段生成具体的类型安全调用器，运行时直接用函数指针一次间接调用，无汇编依赖

因此 `CallX64` 的符号**永远不会被引用**，`.lib` 引用实际上是无效的死代码。目前构建之所以能通过，是因为链接器从未需要解析该符号（`#if 0` 已经切断了所有引用路径）。

**目标**：彻底清除这段死代码，消除对不存在的 `.lib` 文件的引用，同时将已废弃的相关源文件标注清楚，防止未来的维护者再次困惑。

## 范围与边界

- 仅改动 `AngelscriptRuntime` 模块内的相关文件
- 不修改 `FunctionCallers.h`、`as_callfunc.cpp`（核心调用路径）的实际逻辑
- 不涉及其他平台（GCC / MinGW / ARM）的 callfunc 文件
- 测试：构建通过即验收（无运行时行为变化，删除的是纯死代码）

---

## Phase 1：移除 Build.cs 中的死 `.lib` 引用

> 目标：消除对不存在的 `.lib` 文件的引用，使 Build.cs 不再依赖 `Intermediate/Build/` 中的人工产物。

- [ ] **P1.1** 删除 `AngelscriptRuntime.Build.cs` 中的 Win64 `.lib` 链接块
  - 当前第 23-26 行整段 `if (Target.Platform == UnrealTargetPlatform.Win64) { PublicAdditionalLibraries.Add(...as_callfunc_x64_msvc_asm.lib...) }` 是无效引用，链接器从未需要解析该符号
  - 直接删除这 4 行，不需要任何替代逻辑
  - 删除后执行完整编译验证，确认 `AngelscriptRuntime.dll` 能正常链接
- [ ] **P1.1** 📦 Git 提交：`[AngelscriptRuntime] Chore: remove dead as_callfunc_x64_msvc_asm.lib reference from Build.cs`

---

## Phase 2：标注已废弃的汇编和 callfunc 源文件

> 目标：为 `as_callfunc_x64_msvc_asm.asm` 和 `as_callfunc_x64_msvc.cpp` 添加明确的废弃说明，防止未来维护者花时间调查"为何这些文件从不被编译"。

- [ ] **P2.1** 在 `as_callfunc_x64_msvc_asm.asm` 文件头添加废弃注释
  - 该文件实现了 `CallX64`、`GetReturnedFloat`、`GetReturnedDouble`，原本用于 x64 MSVC calling convention 桥接
  - Hazelight fork 已用 `ASAutoCaller`（`FunctionCallers.h`）的模板调用器替代，本文件的汇编函数永远不会被引用
  - 在文件最顶部（license 注释之后）添加 `[UE--]` 标注说明此文件已被替代，格式参照 `AngelscriptChange.md` 中的惯例
- [ ] **P2.2** 在 `as_callfunc_x64_msvc.cpp` 文件头添加废弃注释
  - 该文件中所有代码均被 `#if 0` 包裹（第 40 行），`extern "C" CallX64` 等声明从不被编译
  - Hazelight 用 `// AS FIX` 注释标记了替代原因，但缺少文件级说明
  - 在文件最顶部（license 注释之后）添加说明：此文件已被 `FunctionCallers.h` 中的模板调用路径替代，`CallX64` 汇编路径已停用
- [ ] **P2.1~P2.2** 📦 Git 提交：`[ThirdParty/AS] Chore: annotate deprecated x64_msvc callfunc asm files as replaced by template callers`

---

## Phase 3：更新 AngelscriptChange.md

> 目标：将 Hazelight 对 native calling convention 的整体替代改动记录到变更日志中，便于后续 AS 版本升级时快速了解改动边界。

- [ ] **P3.1** 在 `AngelscriptChange.md` 中新增一条变更记录
  - 记录以下事实：Hazelight fork 用 `ASAutoCaller` 模板调用器（`FunctionCallers.h`）替代了原始 `as_callfunc_x64_msvc.cpp` + `as_callfunc_x64_msvc_asm.asm` 的汇编 calling convention 桥接路径
  - 说明受影响的文件：`as_callfunc_x64_msvc.cpp`（`#if 0`）、`as_callfunc.cpp`（`CallSystemFunctionNative` 路径 `#if 0`，改走 `CallFunctionCaller`）、`as_callfunc_x64_msvc_asm.asm`（不再链接）
  - 记录新路径入口：`ASAutoCaller::MakeFunctionCaller` → `RedirectFunctionCaller<R, Args...>` → `FunctionCaller` 函数指针
  - 这一改动在未来合入 AS 2.38 时需要特别注意：upstream 仍使用汇编路径，合并时需保留 Hazelight 的 `#if 0` 覆盖和 `CallFunctionCaller` 路径
- [ ] **P3.1** 📦 Git 提交：`[Docs] Chore: document Hazelight callfunc template replacement in AngelscriptChange.md`

---

## 验收标准

- [ ] 编译成功：`AngelscriptProjectEditor Win64 Development` 无任何关于 callfunc/asm 的链接错误或警告
- [ ] 测试通过：全量运行 `Angelscript.TestModule` 仍为 341/341
- [ ] `AngelscriptRuntime.Build.cs` 不再包含对 `as_callfunc_x64_msvc_asm.lib` 的任何引用
- [ ] `as_callfunc_x64_msvc_asm.asm` 和 `as_callfunc_x64_msvc.cpp` 文件头有明确的废弃说明
- [ ] `AngelscriptChange.md` 记录了此次 calling convention 替代的全貌

## 风险与注意事项

- **构建风险极低**：被删除的 `.lib` 引用从未被实际链接使用（符号从未被引用），删除后链接行为不变
- **跨平台**：此清理仅涉及 Win64 MSVC 路径；其他平台（GCC、MinGW、ARM）的 `as_callfunc_xxx` 文件不受影响，也不需要处理
- **AS 版本升级时的注意点**：若未来合入 AS 2.38 upstream，其 `as_callfunc_x64_msvc.cpp` 中的汇编路径会重新出现；届时需要用 `#if 0` 覆盖并保留 `CallFunctionCaller` 路径，这一事项已在 P3.1 中记录到 `AngelscriptChange.md`
