# 清理 as_callfunc_x64_msvc_asm 死代码

> **归档状态**：已完成，已于 2026-04-04 归档到 `Documents/Plans/Archives/`
> **归档日期**：2026-04-04
> **完成判断**：Phase 1-3 全部完成；验收阶段保留的整仓构建失败项已明确为与本计划无关的仓库既有问题，不阻塞归档。
> **结果摘要**：
> - 已移除 `AngelscriptRuntime.Build.cs` 对 `as_callfunc_x64_msvc_asm.lib` 的无效引用。
> - 已为 `as_callfunc_x64_msvc_asm.asm` 与 `as_callfunc_x64_msvc.cpp` 补充文件级废弃说明，明确 `FunctionCallers.h` 为当前有效调用路径。
> - 已在计划内保留 `ASAutoCaller` 替代链路与未来合入 AS 2.38 时需要复查的兼容注意点。

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

- [x] **P1.1** 删除 `AngelscriptRuntime.Build.cs` 中的 Win64 `.lib` 链接块
  - 当前第 23-26 行整段 `if (Target.Platform == UnrealTargetPlatform.Win64) { PublicAdditionalLibraries.Add(...as_callfunc_x64_msvc_asm.lib...) }` 是无效引用，链接器从未需要解析该符号
  - 直接删除这 4 行，不需要任何替代逻辑
  - 删除后执行完整编译验证，确认 `AngelscriptRuntime.dll` 能正常链接
- [x] **P1.2** 结果记录：`AngelscriptRuntime.Build.cs` 已移除对 `as_callfunc_x64_msvc_asm.lib` 的唯一引用

---

## Phase 2：标注已废弃的汇编和 callfunc 源文件

> 目标：为 `as_callfunc_x64_msvc_asm.asm` 和 `as_callfunc_x64_msvc.cpp` 添加明确的废弃说明，防止未来维护者花时间调查"为何这些文件从不被编译"。

- [x] **P2.1** 在 `as_callfunc_x64_msvc_asm.asm` 文件头添加废弃注释
  - 该文件实现了 `CallX64`、`GetReturnedFloat`、`GetReturnedDouble`，原本用于 x64 MSVC calling convention 桥接
  - Hazelight fork 已用 `ASAutoCaller`（`FunctionCallers.h`）的模板调用器替代，本文件的汇编函数永远不会被引用
  - 在文件最顶部（license 注释之后）添加 `[UE++]` / `[UE--]` 标注，说明此文件已被替代
- [x] **P2.2** 在 `as_callfunc_x64_msvc.cpp` 文件头添加废弃注释
  - 该文件中所有代码均被 `#if 0` 包裹（第 40 行），`extern "C" CallX64` 等声明从不被编译
  - Hazelight 用 `// AS FIX` 注释标记了替代原因，但缺少文件级说明
  - 在文件最顶部（license 注释之后）添加说明：此文件已被 `FunctionCallers.h` 中的模板调用路径替代，`CallX64` 汇编路径已停用
- [x] **P2.3** 结果记录：两个文件均已补充文件级废弃说明，明确它们仅保留作历史参考

---

## Phase 3：在计划文档内记录替代链路

> 目标：将 Hazelight 对 native calling convention 的整体替代改动记录在本计划文档中，便于后续 AS 版本升级时快速了解改动边界。

- [x] **P3.1** 在本计划文档中保留替代链路与处理边界
  - 记录以下事实：Hazelight fork 用 `ASAutoCaller` 模板调用器（`FunctionCallers.h`）替代了原始 `as_callfunc_x64_msvc.cpp` + `as_callfunc_x64_msvc_asm.asm` 的汇编 calling convention 桥接路径
  - 说明受影响的文件：`as_callfunc_x64_msvc.cpp`（`#if 0`）、`as_callfunc.cpp`（`CallSystemFunctionNative` 路径 `#if 0`，改走 `CallFunctionCaller`）、`as_callfunc_x64_msvc_asm.asm`（不再链接）
  - 记录新路径入口：`ASAutoCaller::MakeFunctionCaller` → `RedirectFunctionCaller<R, Args...>` → `FunctionCaller` 函数指针
  - 这一改动在未来合入 AS 2.38 时需要特别注意：upstream 仍使用汇编路径，合并时需保留 Hazelight 的 `#if 0` 覆盖和 `CallFunctionCaller` 路径
- [x] **P3.2** 结果记录：本任务不新建 `AngelscriptChange.md`，长期说明暂以内联方式保留在本计划中

---

## 验收标准

- [x] 已执行 `AngelscriptProjectEditor Win64 Development` 构建验证；构建过程中未出现新的 `callfunc` / `asm` 链接错误
- [x] 当前整仓构建仍未全绿：`Plugins/Angelscript/Source/AngelscriptTest/ClassGenerator/AngelscriptScriptClassCreationTests.cpp` 存在与本任务无关的缺失标识符错误（`GetResetSharedTestEngine`、`ResetSharedInitializedTestEngine`）；该问题已作为仓库既有问题记录，不阻塞本计划归档
- [x] `AngelscriptRuntime.Build.cs` 不再包含对 `as_callfunc_x64_msvc_asm.lib` 的任何引用
- [x] `as_callfunc_x64_msvc_asm.asm` 和 `as_callfunc_x64_msvc.cpp` 文件头有明确的废弃说明
- [x] 本计划文档记录了此次 calling convention 替代的全貌与本次处理结果

## 风险与注意事项

- **构建风险极低**：被删除的 `.lib` 引用从未被实际链接使用（符号从未被引用），删除后链接行为不变
- **跨平台**：此清理仅涉及 Win64 MSVC 路径；其他平台（GCC、MinGW、ARM）的 `as_callfunc_xxx` 文件不受影响，也不需要处理
- **AS 版本升级时的注意点**：若未来合入 AS 2.38 upstream，其 `as_callfunc_x64_msvc.cpp` 中的汇编路径会重新出现；届时需要用 `#if 0` 覆盖并保留 `CallFunctionCaller` 路径，这一事项已在本计划文档的 P3.1 中记录
