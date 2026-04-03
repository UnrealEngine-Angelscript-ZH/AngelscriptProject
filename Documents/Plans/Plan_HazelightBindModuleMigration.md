# Angelscript Hazelight Bind 模块迁移计划

## 背景与目标

### 背景

`Documents/Guides/BindGapAuditMatrix.md` 已经确认，一部分 Bind parity 差距虽然起点是单文件 diff，但继续推进后会演变成一整组 Hazelight 风格模块迁移与配置策略问题。这类差距不再适合由 `Plan_TechnicalDebt.md` 作为“局部 low-risk 收口”来承接。

当前最典型的迁移入口包括：

- `Bind_AActor.cpp`：`DeprecateOldActorGenericMethods` 配置控制、`GetComponentsByClassWithTag()`、`VerifySpawnActor` delegate hook。
- `Bind_USceneComponent.cpp`：在 `Plan_TechnicalDebt` 已闭合 `SetComponentVelocity` / `FScopedMovementUpdate` 的最小入口后，剩余差距仍涉及 `DeprecateOldActorGenericMethods` 配置化与更广的 SceneComponent 模块迁移策略。

### 目标

1. 把这类“跨多个引擎绑定模块、带配置策略和 Hazelight 风格迁移色彩”的 gap 从主技术债计划中拆出来。
2. 确定哪些差距只是局部 API 补齐，哪些已经属于模块级迁移与配置一致性问题。
3. 为后续集中迁移提供文件清单、测试落点与停机条件，避免在主计划里一点点蚕食成不可控范围。

## 当前事实状态

- `Plan_TechnicalDebt` 已在 `P4.2` 中闭合 `Bind_USceneComponent` 的最小低风险能力补齐（`SetComponentVelocity`、`FScopedMovementUpdate` 绑定）。
- `Bind_AActor.cpp` 与 `Bind_USceneComponent.cpp` 仍保留更大范围的 Hazelight 风格配置差距，不适合作为主技术债计划里的单个后续 commit 继续推进。
- 当前仓库尚未有专门承接这些模块迁移的 sibling plan，因此本文件需要先作为分流入口建立下来。

## 分阶段执行计划

### Phase 1：冻结模块级迁移范围

> 目标：把真正属于 Hazelight 风格模块迁移的差距从主计划中剥离出来。

- [ ] **P1.1** 固定 `AActor` / `SceneComponent` 迁移差距清单
  - 以 `Bind_AActor.cpp`、`Bind_USceneComponent.cpp` 为起点，把 gap 细分为“局部 API 补齐”“配置策略差距”“spawn / lifecycle hook 差距”“风格差异”。
  - 明确哪些局部项已经在 `Plan_TechnicalDebt` 关闭，哪些剩余项必须在本计划内继续推进。
  - 为每个剩余项指定测试落点，优先使用 `Actor/`、`Bindings/`、`Component/` 现有目录。
- [ ] **P1.1** 📦 Git 提交：`[Docs/Hazelight] Feat: freeze bind module migration scope for actor and scene component paths`

### Phase 2：迁移局部配置与 hook 差距

> 目标：先吃下不需要额外插件模块的配置与 hook 闭环，不扩展到更大系统。

- [ ] **P2.1** 处理 `DeprecateOldActorGenericMethods` 与局部 actor helper 差距
  - 把 `DeprecateOldActorGenericMethods` 的配置控制接到当前插件配置体系，并确认旧 generic actor method 的保留 / 弃用边界。
  - 对 `GetComponentsByClassWithTag()`、`VerifySpawnActor` 这类局部差距，先补 failing test，再做最小实现。
  - 若实现时需要引入新的 gameplay / editor module 依赖，应在本计划中显式升级范围，而不是在主计划里隐式带入。
- [ ] **P2.1** 📦 Git 提交：`[Binds/Hazelight] Feat: migrate low-risk actor helper and spawn-validation parity from Hazelight`

- [ ] **P2.2** 处理 `USceneComponent` 的剩余配置策略差距
  - 仅承接 `Plan_TechnicalDebt` 已完成最小补齐之后的剩余差距，例如配置化保留 / 弃用旧 generic methods。
  - 不重复回写已在主计划关闭的 `SetComponentVelocity` / `FScopedMovementUpdate` 入口。
  - 完成后需要用 `Bindings.NativeComponentMethods` 或主题化组件场景测试验证行为。
- [ ] **P2.2** 📦 Git 提交：`[Binds/Hazelight] Refactor: migrate remaining scene component parity beyond low-risk technical-debt scope`

### Phase 3：与主计划同步

> 目标：确保主技术债计划、bind-gap 矩阵与这里的迁移结果一致。

- [ ] **P3.1** 回写主计划与矩阵的承接关系
  - 把已由本计划承接的 gap 从 `Plan_TechnicalDebt.md` 和 `BindGapAuditMatrix.md` 中标注为“由 Hazelight migration plan 负责”。
  - 说明仍未承接的项为什么继续留在主计划，防止职责再次混淆。
  - 记录 focused regression 结果与仍保留的风险。
- [ ] **P3.1** 📦 Git 提交：`[Docs/Hazelight] Refactor: sync actor and scene component migration handoff back to technical debt plan`

## 验收标准

- `Bind_AActor.cpp` / `Bind_USceneComponent.cpp` 的模块级迁移差距不再以模糊 backlog 形式挂在 `Plan_TechnicalDebt.md` 中。
- 新增迁移项均有明确测试落点，且至少一轮 focused regression 通过。
- 主计划与本计划对承接边界的描述一致。

## 风险与注意事项

- 不要把本计划扩展成“全仓库 Hazelight 风格统一化”；这里只处理已由 bind-gap 审计指向的 Actor / SceneComponent 模块迁移问题。
- 任何需要新增插件模块依赖或 editor-only 行为的项，都必须在本计划中显式写出来，不得继续隐式挂回主计划。
- 已在 `Plan_TechnicalDebt` 关闭的低风险绑定项不可在本计划中重复实现。 
