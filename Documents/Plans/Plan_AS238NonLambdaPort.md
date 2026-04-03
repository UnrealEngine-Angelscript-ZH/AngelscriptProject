# Angelscript AS238 非 Lambda 能力迁移计划

## 背景与目标

### 背景

`Documents/Guides/BindGapAuditMatrix.md` 的 `P4` 审计表明，一部分看似属于 Bind parity 的差距，本质上已经跨进了 AngelScript 2.38 非 Lambda 能力迁移与类型系统兼容层面。继续把这些问题留在 `Plan_TechnicalDebt.md` 中，会让原本面向“低风险、局部收口”的计划膨胀成第三方语言层与类型系统的混合改造。

当前最典型的几类跨界差距包括：

- `Bind_TOptional.cpp` 的 intrusive optional state、`CalculateOptionalSize()`、hot-reload 比较接口等，不再只是单个绑定函数缺口，而是会影响泛型类型系统与热重载协议。
- `Bind_UStruct.cpp` 的 `TStructType`、`HasGetter/HasSetter(..., false)`、多 `ScriptName` 别名分号处理，会牵动属性访问器与类型声明语义。
- `Bind_BlueprintType.cpp` 的 `CPF_TObjectPtr` 恢复之外，若继续收口 property getter/setter / accessor 标记，就会直接进入更广的 property system 迁移。

### 目标

1. 把这类“已超出本地低风险 Bind 补齐”的差距从 `Plan_TechnicalDebt.md` 中剥离出来。
2. 以 AS238 非 Lambda 迁移为主轴，明确哪些差距属于语言能力、类型系统、property accessor 语义，而不是普通绑定项。
3. 为后续独立实施准备测试入口、影响文件和验收边界，避免下一轮再从 bind-gap 矩阵反向重判范围。

## 当前事实状态

- `Bind_TOptional.cpp` 当前已通过 `Plan_TechnicalDebt` 完成低风险矩阵审计，但尚未进入实现；矩阵已将其标注为中风险类型系统差距。
- `Bind_UStruct.cpp` 当前局部仍可做小范围 parity，但一旦继续推进 `TStructType` 或 accessor 策略，就会跨入 property / type system 改造。
- `Bind_BlueprintType.cpp` 当前在 `CPF_TObjectPtr` 恢复之外，仍存在 property getter/setter 与 accessor 标记迁移空间；这些项与 AS238 相关语义更强，已经不适合作为 `Plan_TechnicalDebt` 的次级小任务继续推进。

## 分阶段执行计划

### Phase 1：固定范围与边界

> 目标：把哪些 gap 属于 AS238 非 Lambda 迁移范围写成明确边界，避免与 `Plan_TechnicalDebt`、`Plan_InterfaceBinding`、`Plan_HazelightBindModuleMigration` 交叉污染。

- [ ] **P1.1** 固定待迁移文件与差距类型
  - 以 `Bind_TOptional.cpp`、`Bind_UStruct.cpp`、`Bind_BlueprintType.cpp` 为主清单，把每个文件里的 gap 细分为“类型表示 / property accessor / object ptr / hot-reload 协议 / 仅风格差异”。
  - 把 `Plan_TechnicalDebt` 已完成的低风险闭环与这里的高风险延续部分拆开，避免重复实施已关闭项。
  - 同步写清每个 gap 的首批测试落点，优先使用 `Angelscript/`、`Bindings/`、`UpgradeCompatibility` 现有测试文件。
- [ ] **P1.1** 📦 Git 提交：`[Docs/AS238] Feat: freeze non-lambda bind migration scope and gap taxonomy`

### Phase 2：恢复最小可验证的语言 / 类型能力

> 目标：只恢复能被现有测试明确锁住的最小闭环，不把本计划扩张成整套第三方升级重写。

- [ ] **P2.1** 优先处理 `Bind_TOptional` 与 `Bind_UStruct` 的局部能力恢复
  - 先从 `intrusive optional state`、`TStructType`、`HasGetter/HasSetter(..., false)` 这类能直接用内部或 Bindings 测试锁住的差距开始。
  - 每一项都必须先补 failing test，再写最小实现，避免把“可能有用”的其他 2.38 差量顺手带进来。
  - 若某一项需要触及更底层的 `ThirdParty` 编译器语义或模板推导策略，立即暂停并在本计划中单独建后续 task，不在同一提交混写。
- [ ] **P2.1** 📦 Git 提交：`[ThirdParty/AS238] Feat: restore low-scope optional and struct parity needed by non-lambda port`

- [ ] **P2.2** 处理 `Bind_BlueprintType` 的 `CPF_TObjectPtr` 与 property accessor 迁移
  - 先把 `CPF_TObjectPtr` 路径恢复成可验证的最小闭环，再决定是否继续吃 property getter/setter 与 accessor 标记。
  - 不允许把 Blueprint property migration 与 Interface property migration 混在同一批；一旦出现 `FInterfaceProperty` 依赖，转交 `Plan_InterfaceBinding.md`。
  - 完成后必须补 `Bindings/` 或 `UpgradeCompatibility` 级别回归，证明不是只消除了编译警告。
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: recover object-ptr and accessor parity for blueprint property paths`

### Phase 3：验证与文档同步

> 目标：把这个 sibling plan 的实际实现结果与主技术债计划重新对齐。

- [ ] **P3.1** 执行 focused regression 并回写交叉引用
  - 至少覆盖 `Angelscript.TestModule.Angelscript.Upgrade.*`、`Bindings.*`、以及新增的类型系统负向边界测试。
  - 把已关闭项从 `BindGapAuditMatrix.md` 和 `Plan_TechnicalDebt.md` 中移出或标记为“由本计划承接并完成”。
  - 记录仍未完成的 2.38 非 Lambda 能力差距，避免下一轮继续把它们混回主计划。
- [ ] **P3.1** 📦 Git 提交：`[Test/AS238] Test: verify non-lambda bind migration closures and plan handoff`

## 验收标准

- `Plan_TechnicalDebt.md` 不再把 `Bind_TOptional` / `Bind_UStruct` / `Bind_BlueprintType` 的高风险延伸项留作模糊 backlog。
- 新增或修改的类型系统 / accessor 行为有明确测试落点，且至少一组 focused regression 通过。
- `BindGapAuditMatrix.md` 与本计划对高风险 gap 的去向描述一致。

## 风险与注意事项

- 不要把此计划扩展成完整 AngelScript 2.38 升级计划；这里只处理“已经由 bind-gap 审计指向的非 Lambda 能力差距”。
- 一旦实施过程中发现 `FInterfaceProperty`、`TScriptInterface` 或 C++ UInterface 语义要求，应转交 `Plan_InterfaceBinding.md`。
- 一旦需要大面积修改 `ThirdParty` 编译器核心文件，先在本计划中显式新增任务，而不是在现有 task 中偷偷扩大范围。
