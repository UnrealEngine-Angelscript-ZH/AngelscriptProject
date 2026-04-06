# Angelscript StructUtils 迁移计划

## 背景与目标

### 背景

当前仓库内 `StructUtils` 的直接代码触点集中在 `Plugins/Angelscript/Source/AngelscriptRuntime/`，并未扩散到 Editor / Test 的实现层，但它仍然通过 `AngelscriptRuntime.Build.cs` 的 `PublicDependencyModuleNames` 与 `Angelscript.uplugin` 的插件启用项形成公开 runtime 依赖。这意味着问题不是“4 个文件的局部 include 清理”，而是“插件公开运行时边界是否继续承诺 `FInstancedStruct` / `StructUtils`”的迁移决策。

已确认的直接触点包括：

- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptAnyStructParameter.h`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Core/AngelscriptDelegateWithPayload.h`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_Helpers.h`
- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FInstancedStruct.cpp`
- `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs`
- `Plugins/Angelscript/Angelscript.uplugin`

同时，仓库内未发现 `PropertyBag` 或 `TInstancedStruct` 使用，说明本次工作不需要扩展成更大的 StructUtils 现代化改造；只需围绕现有 `FInstancedStruct` 暴露面收口即可。

### 目标

1. 固定当前 `StructUtils` 在 Angelscript 插件中的真实边界，避免后续把它误判为单纯传递依赖或局部实现细节。
2. 为是否保留、封装或移除 `FInstancedStruct` 公开暴露面准备独立迁移路径，而不是在其他技术债计划中零散蚕食。
3. 把 runtime API、绑定层、构建依赖、插件描述符与验证路径串成一条可执行的迁移链路。

## 当前事实状态

- `Plugins/Angelscript/Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs` 当前通过 `PublicDependencyModuleNames` 暴露 `StructUtils`。
- `Plugins/Angelscript/Angelscript.uplugin` 当前显式启用 `StructUtils` 插件。
- `AngelscriptEditor`、`AngelscriptTest`、`AngelscriptProjectTest` 当前只应视为下游验证目标，而不是已确认需要修改的直接实现触点。
- `FAngelscriptAnyStructParameter` 与 `FAngelscriptDelegateWithPayload` 在公开头中直接使用 `FInstancedStruct`，因此任何迁移都会先触及 runtime 公共 API。
- `Bind_Helpers.h` / `Bind_FInstancedStruct.cpp` 当前为 Angelscript 脚本侧提供 `FInstancedStruct` 的构造、`InitializeAs`、`Get`、`GetMutable`、`Contains`、`GetScriptStruct` 等绑定能力。

## 影响范围

本次迁移涉及以下操作（按需组合）：

- **公开类型边界收口**：决定 `FInstancedStruct` 是否继续出现在 `AngelscriptRuntime` 的 public header 中。
- **绑定入口重定向**：调整 `Bind_FInstancedStruct` / helper，使脚本侧不再直接绑定旧边界，或通过兼容 wrapper 过渡。
- **模块依赖降级**：在 runtime API 不再公开依赖 `StructUtils` 后，将 `PublicDependencyModuleNames` 降为 `PrivateDependencyModuleNames` 或移除。
- **插件描述符清理**：在确认 runtime / tests 不再需要 `StructUtils` 后，更新 `Angelscript.uplugin` 的插件依赖声明。
- **回归与文档同步**：补 focused regression 与文档说明，确保行为变化被明确记录。

按目录分组的受影响文件清单：

`Plugins/Angelscript/Source/AngelscriptRuntime/Core/`（2 个）：
- `AngelscriptAnyStructParameter.h` — 公开类型边界收口
- `AngelscriptDelegateWithPayload.h` — 公开类型边界收口 + 行为兼容性检查

`Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`（2 个）：
- `Bind_Helpers.h` — 绑定入口重定向 + helper 适配
- `Bind_FInstancedStruct.cpp` — 绑定入口重定向 + 行为兼容性检查

`Plugins/Angelscript/Source/AngelscriptRuntime/`（1 个）：
- `AngelscriptRuntime.Build.cs` — 模块依赖降级

`Plugins/Angelscript/`（1 个）：
- `Angelscript.uplugin` — 插件描述符清理

`Plugins/Angelscript/Source/AngelscriptTest/`（按需新增 focused regression）：
- `Delegate/AngelscriptStructUtilsDelegateTests.cpp` — 覆盖 payload / delegate 语义
- `Bindings/AngelscriptStructUtilsBindingTests.cpp` — 覆盖脚本侧 `FInstancedStruct` 绑定或其替代入口

`Documents/Guides/` / `Documents/Plans/`（按需）：
- `Documents/Guides/Test.md` 或相关迁移说明文档 — 若验证入口或依赖边界发生变化则同步更新
- `Documents/Plans/Plan_OpportunityIndex.md` — 记录该计划入口与优先级

## 分阶段执行计划

### Phase 1：冻结迁移边界与目标状态

> 目标：先回答“迁移什么、为什么迁移、哪些地方绝不能顺手扩展”，避免一上来动代码时把问题放大成全仓库结构重写。

- [ ] **P1.1** 固定 `StructUtils` 当前暴露面的事实基线
  - 以 `AngelscriptRuntime.Build.cs`、`Angelscript.uplugin`、四个 runtime 直接触点为基线，补一份事实清单，明确哪些是公开 API、哪些是绑定内部实现、哪些只是下游传递依赖。
  - 明确当前不在范围内的项：`PropertyBag`、`TInstancedStruct`、Editor/Test 自身实现层改造、以及与本次迁移无关的其他 Bind parity 工作。
  - 若后续发现新的直接触点，必须先回写本计划的影响范围，再决定是否继续实施，禁止边做边扩。
- [ ] **P1.1** 📦 Git 提交：`[Docs/StructUtils] Feat: freeze current StructUtils migration boundary for Angelscript runtime`

- [ ] **P1.2** 选定目标状态：保留 wrapper、局部封装，还是完全去除公开 `FInstancedStruct`
  - 至少比较三种目标形态：继续公开 `FInstancedStruct`、改为插件自有 wrapper 但内部仍用 `StructUtils`、完全移除 `StructUtils` 公开存在。
  - 对每种目标形态写明收益、代价、兼容性风险与回退难度，尤其说明脚本 API、蓝图可见结构体、delegate payload 行为会如何变化。
  - 只有在目标状态被明确选定后，Phase 2 才能进入实现；不允许在实现过程中临时改变目标。
- [ ] **P1.2** 📦 Git 提交：`[Docs/StructUtils] Design: choose target runtime boundary for StructUtils migration`

### Phase 2：收口 runtime 公共类型边界

> 目标：先处理 public header，决定 `AngelscriptRuntime` 对外是否继续以 `FInstancedStruct` 作为稳定契约。

- [ ] **P2.1** 重构 `FAngelscriptAnyStructParameter` 的公开承载形式
  - 先补 failing test，覆盖脚本传参与蓝图暴露场景，再决定是保留 `FInstancedStruct` 字段、引入插件自有 wrapper，还是改成其他公开承载方式。
  - 任何替换都必须说明序列化、反射、BlueprintType、脚本隐式构造链路是否保持兼容；如果不能兼容，需把行为变化写入“已知行为变化”。
  - 不要同时改 delegate payload；该文件只负责 `AnyStructParameter` 的边界收口。
- [ ] **P2.1** 📦 Git 提交：`[Runtime/StructUtils] Refactor: migrate AnyStructParameter public boundary off direct StructUtils exposure`

- [ ] **P2.2** 重构 `FAngelscriptDelegateWithPayload` 的 payload 承载边界
  - 先用 focused regression 固定 `BindUFunctionWithPayload()`、`ExecuteIfBound()` 与 boxed primitive payload 的当前行为，再决定新边界如何承接这些语义。
  - 如果 `Payload` 不再直接是 `FInstancedStruct`，必须给出 boxed primitive 与任意 `UScriptStruct` payload 的兼容策略，避免 delegate 绑定能力退化。
  - 若实现后仍需 `StructUtils` 作为内部存储，应确保该依赖不再从 public header 外溢。
- [ ] **P2.2** 📦 Git 提交：`[Runtime/StructUtils] Refactor: migrate delegate payload boundary away from direct FInstancedStruct exposure`

### Phase 3：重定向脚本绑定与内部 helper

> 目标：让脚本侧的 `FInstancedStruct` 绑定与新的 runtime 边界一致，避免 public API 收口后，内部绑定仍继续把旧边界暴露回去。

- [ ] **P3.1** 改造 `Bind_Helpers.h` 的 instanced-struct helper 入口
  - 将 helper 层抽象成“面向目标边界”的适配器，而不是继续默认所有上层都直接操作 `FInstancedStruct`。
  - 如果最终保留内部 `StructUtils` 存储，helper 只允许在私有实现层持有该类型；若完全去除，则 helper 必须同步删除旧类型分支。
  - 避免把 unrelated helper 清理、命名统一或大文件拆分夹带进本次提交。
- [ ] **P3.1** 📦 Git 提交：`[Runtime/StructUtils] Refactor: route instanced-struct helpers through new runtime boundary`

- [ ] **P3.2** 改造 `Bind_FInstancedStruct.cpp` 的脚本绑定面
  - 先补失败用例，固定 `Make`、`InitializeAs`、`Get`、`GetMutable`、`Contains`、`GetScriptStruct` 的当前契约，再根据新目标边界决定哪些 API 保留、兼容、弃用或替换。
  - 若脚本侧仍需兼容 `FInstancedStruct` 名称，必须明确这是过渡兼容层还是长期 API；若只是过渡层，应同步给出弃用与测试策略。
  - 如果脚本绑定发生行为变化，必须同步更新测试说明与文档，不允许只改实现不记账。
- [ ] **P3.2** 📦 Git 提交：`[Binds/StructUtils] Refactor: align script-facing InstancedStruct bindings with new runtime contract`

### Phase 4：降级构建依赖并清理插件描述符

> 目标：只有当 public header 与脚本绑定都完成收口后，才开始真正削弱 `StructUtils` 的模块/插件级依赖。

- [ ] **P4.1** 重新判定 `AngelscriptRuntime.Build.cs` 中 `StructUtils` 的依赖级别
  - 如果 `StructUtils` 仍只存在于 runtime 私有实现中，则将其从 `PublicDependencyModuleNames` 降到 `PrivateDependencyModuleNames`。
  - 只有在 runtime 私有实现、测试与下游消费者都不再需要 `StructUtils` 时，才允许完全移除模块依赖；否则保持 private 级别作为过渡态。
  - 需要通过一次 clean build 验证没有遗漏 include 或链接依赖，并至少验证一个下游 consumer 模块仍能完成编译，证明 public 依赖降级没有把传递依赖问题留给消费侧。
- [ ] **P4.1** 📦 Git 提交：`[Build/StructUtils] Refactor: downgrade runtime StructUtils dependency after public boundary cleanup`

- [ ] **P4.2** 清理 `Angelscript.uplugin` 中的 `StructUtils` 插件启用项
  - 仅在确认插件运行、测试执行、打包路径都不再依赖 `StructUtils` 时执行；否则保留该项，并在计划中记为未完成的显式边界。
  - 若移除后出现 editor/test/packaging 问题，应先恢复该启用项，再补齐遗漏依赖，而不是强行把问题压给下游模块。
  - 本步完成后需回写最终依赖图，说明 `StructUtils` 已完全移除还是仅保留内部过渡，并补一次插件加载/打包级验证，证明 `.uplugin` 清理不会只在本地 editor build 中“看起来没事”。
- [ ] **P4.2** 📦 Git 提交：`[Plugin/StructUtils] Refactor: remove explicit StructUtils plugin enablement after dependency elimination`

### Phase 5：验证、文档与后续承接

> 目标：用 focused regression 和文档回写证明迁移结果是可交付状态，而不是“代码能编译但边界描述仍混乱”。

- [ ] **P5.1** 补齐 focused regression 与人工验证记录
  - 测试至少覆盖：`AnyStructParameter` 构造/传参、delegate payload 调用链、脚本侧 instanced-struct 读写、以及 clean build / editor build。
  - 若新增测试文件，命名遵守 `Angelscript` 前缀与主题目录规则；不要把这类回归堆到通用 `Scenario` 桶中。
  - 除自动化测试外，还需要一次真实入口验证，证明新的承载边界在目标场景中可运行。
- [ ] **P5.1** 📦 Git 提交：`[Test/StructUtils] Test: verify runtime boundary migration and script compatibility`

- [ ] **P5.2** 同步文档与机会索引
  - 在相关指南中记录新的依赖边界、行为变化、测试入口与残留限制；如果 `StructUtils` 仅被降级为 private 依赖，也要明确写清楚，不得假装已经完全移除。
  - 将本计划与 `Plan_OpportunityIndex.md`、其他承接它的技术债/架构计划保持一致，防止后续又把同一工作重新提案一次。
  - 若仍有未收口项，应明确写成 sibling 计划或后续 Phase，不要留下没有 owner 的模糊 TODO。
- [ ] **P5.2** 📦 Git 提交：`[Docs/StructUtils] Docs: document final StructUtils runtime boundary and remaining follow-ups`

## 验收标准

- `StructUtils` 在 Angelscript 插件中的角色被明确收口为以下三者之一：公开依赖、私有依赖、或完全移除，且文档、构建配置与代码状态一致。
- `FAngelscriptAnyStructParameter`、`FAngelscriptDelegateWithPayload` 与脚本绑定层对目标边界的描述一致，不再出现 public header 已迁移但绑定层仍偷偷暴露旧契约的情况。
- 至少一轮 focused regression + clean build 能证明迁移后的 runtime 边界可工作。
- 任何保留的兼容层、已知行为变化或残余依赖都在计划或文档中被显式记录。

## 风险与注意事项

### 风险

1. **公开 API 兼容性风险**：`FAngelscriptAnyStructParameter` 与 `FAngelscriptDelegateWithPayload` 都是 public header，改动承载类型可能影响脚本、蓝图和下游模块。
   - **缓解**：先固定现有行为，再决定目标边界；必要时通过 wrapper 或过渡兼容层分阶段推进。

2. **绑定语义漂移风险**：`Bind_FInstancedStruct.cpp` 当前暴露的方法较多，如果目标边界变化过快，容易出现“编译通过但脚本语义悄悄缩水”。
   - **缓解**：对每个保留/弃用 API 建立 focused regression，并把弃用策略显式写入计划与文档。

3. **模块依赖判断过早风险**：如果在 public header 收口前就先移除 `Build.cs` / `.uplugin` 依赖，会把本来容易定位的问题变成一串下游编译/加载错误。
   - **缓解**：强制按本计划顺序执行，Phase 4 只能在 Phase 2-3 之后开展。

### 已知行为变化

1. **构建依赖对下游的可见性会变化**：一旦 `StructUtils` 从 `PublicDependencyModuleNames` 降为 `PrivateDependencyModuleNames`，所有消费 `AngelscriptRuntime` 的下游模块都需要重新验证传递依赖是否仍然成立。
   - 重点验证目标：`Plugins/Angelscript/Source/AngelscriptEditor/AngelscriptEditor.Build.cs`、`Plugins/Angelscript/Source/AngelscriptTest/AngelscriptTest.Build.cs`、`Source/AngelscriptProjectTest/AngelscriptProjectTest.Build.cs`

2. **插件加载边界可能变化**：如果 `Angelscript.uplugin` 最终移除 `StructUtils` 启用项，打包、加载顺序与运行时模块可用性都需要重新验证。
   - 影响文件：`Plugins/Angelscript/Angelscript.uplugin`

3. **脚本 API 名称可能进入过渡态**：如果选择保留 `FInstancedStruct` 名称作为兼容层，短期内会同时存在“内部新边界 + 外部旧名称”的过渡结构。
   - 影响文件：`Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_FInstancedStruct.cpp`、相关测试与文档
