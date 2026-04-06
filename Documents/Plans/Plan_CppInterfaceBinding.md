# C++ UInterface 绑定缺口修复计划

## 背景与目标

### 背景

当前 Angelscript 插件已实现一套完整的**脚本定义接口**管线（19 个测试覆盖），包括：

- 预处理器识别 `interface` chunk → 方法提取 → 块擦除 → `RegisterObjectType` + `RegisterObjectMethod(CallInterfaceMethod)`
- 类生成器创建接口 `UClass`（`CLASS_Interface | CLASS_Abstract`）+ 极简 UFunction 存根
- 运行时 `Cast<I>` → `ImplementsInterface` → `CallInterfaceMethod` → `FindFunction` → `ProcessEvent`

**但脚本类无法继承 C++ 定义的 `UINTERFACE`**。尽管测试侧有 2 个手动绑定的 Native PoC 用例（`EnsureNativeInterfaceFixturesBound` 硬编码注册特定接口），但插件绑定层没有通用的自动识别与注册机制。

### 现状诊断摘要

经全面审查，发现四个层面的缺口：

| # | 缺口 | 影响 |
|---|------|------|
| 1 | **C++ UInterface 未自动注册为 AS 类型** | 脚本中不能声明 C++ 接口变量、不能 Cast、不能调方法——核心缺口 |
| 2 | **FInterfaceProperty 未参与绑定** | `TScriptInterface<T>` 属性在脚本中不可读写 |
| 3 | **方法签名校验仅按函数名** | `FInterfaceMethodSignature` 只有 `FName`，参数类型/数量不校验 |
| 4 | **架构决策未落地** | `Plan_InterfaceBinding.md` Phase 0 的三个决策点均未记录结论 |

### 与现有 Plan 的关系

`Plan_InterfaceBinding.md` 是接口绑定完善的**完整设计文档**，包含架构决策点分析、Patch 方案对比、横向参考、7 个 Phase 定义。本文档是基于现状诊断的**聚焦执行计划**，目标是以最小变更路径补齐"C++ UInterface 脚本可用"这一核心缺口，并在过程中落实必要的架构决策。

### 目标

1. **C++ UInterface 自动绑定**：`Bind_BlueprintType.cpp` 的 `BindUClass` 流程自动识别 C++ 接口类并注册到 AS，脚本能 Cast、调方法、引用 StaticClass
2. **落实架构决策**：确定 ThirdParty 修改策略和命名约定，记录到 `Plan_InterfaceBinding.md` 的"已确定决策"章节
3. **建立回归基线**：≥11 个自动化测试覆盖 C++ 接口绑定场景

FInterfaceProperty 和方法签名校验增强作为后续阶段，本计划仅定义前置边界。

## 当前事实状态

```text
C++ UInterface 绑定链路（应有 vs 实有）：

Bind_BlueprintType.cpp
  BindUClass() 遍历 TObjectRange<UClass>
    ├── ShouldBindEngineType() — 无 CLASS_Interface 判断         ← 缺口
    ├── BindUClass() — 无接口专用路径                            ← 缺口
    └── BindUClassLookup() — TypeFinder 不识别 FInterfaceProperty ← 缺口

Bind_UObject.cpp
  opCast — 已有 CLASS_Interface + ImplementsInterface 分支       ✓ 可用
  ImplementsInterface() 绑定                                     ✓ 可用

测试侧（Interface/AngelscriptInterfaceNativeTests.cpp）：
  EnsureNativeInterfaceBoundForTests() — 手动 ReferenceClass     ← 测试专用，非通用
  BindNativeInterfaceMethod() — 手动逐方法注册                    ← 测试专用，非通用
  2 个 NativeImplement 测试 — 手动绑定后验证 Cast/调用/Execute_   ✓ 通过
```

## 分阶段执行计划

### Phase 0：架构决策落地

> 目标：快速确定两个关键决策点（ThirdParty 修改策略、命名约定），为后续实现定方向。

- [ ] **P0.1** 评估 ThirdParty 修改策略并记录决策
  - 当前插件对 AngelScript ThirdParty 零修改，接口用 `RegisterObjectType`（引用类型）模拟。如果修改 `as_objecttype.cpp` 启用 `IsInterface()`，就能用 AS 原生的 `RegisterInterface`，获得编译器级接口语义
  - 评估 Patch 的 3 处 AS 引擎修改对 AS 2.33 稳定性和计划中 AS 2.38 升级的影响。重点确认 `IsInterface()` 启用是否与 `P9-A-Upgrade-Roadmap.md` 冲突
  - 三个选项（A 不修改 / B 改 `as_objecttype.cpp` / C 全量 3 处），记录选定方案和理由
  - 将决策结论写入 `Plan_InterfaceBinding.md` 新增的"已确定决策"章节
- [ ] **P0.1** 📦 Git 提交：`[Interface] Docs: record ThirdParty modification decision`

- [ ] **P0.2** 确定命名约定并记录决策
  - 当前脚本接口只用 `U` 前缀（`UTestDamageableInterface`）。Patch 方案用双重注册 `U+I`（`UTestDamageableInterface` handle + `ITestDamageableInterface` interface）
  - 若 P0.1 选 A（不修改 ThirdParty），则命名约定选仅 U 前缀最自然；若选 B/C，双重注册成为可能
  - 同时确定 FInterfaceProperty 的优先级（先跳过还是一起做），记录到同一决策章节
- [ ] **P0.2** 📦 Git 提交：`[Interface] Docs: finalize naming convention and FInterfaceProperty priority`

### Phase 1：C++ UInterface 自动类型注册

> 目标：让 C++ 定义的 UInterface 在 AS 脚本中可见——解决缺口 1 的类型注册部分和 StaticClass 可用性。

- [ ] **P1.1** 在 `Bind_BlueprintType.cpp` 增加 C++ 接口类的自动识别与注册
  - 当前 `BindUClass` 遍历所有 `UClass` 时完全不区分接口类。需在 `ShouldBindEngineType` 或 `BindUClass` 中增加 `CLASS_Interface` 识别
  - 实现 `IsNativeInterfaceClass(UClass*)` 判定：`Class->HasAnyClassFlags(CLASS_Interface) && Class != UInterface::StaticClass()`
  - 对符合条件的 C++ 接口类调用 `RegisterObjectType`（方案 A）或 `RegisterInterface`（方案 B/C，取决于 P0.1 决策），设置 `asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE`
  - 确保 `UserData` 设置为对应 `UClass*`（与现有 `plainUserData` 模式一致），使 `opCast` 中的 `GetUserData()` 能正确解析
  - 参考现有测试中 `EnsureNativeInterfaceBoundForTests` 的 `ReferenceClass` + `plainUserData` 设置模式，但改为通用自动化
- [ ] **P1.1** 📦 Git 提交：`[Interface] Feat: auto-register C++ UInterface as AS type in BindUClass`

- [ ] **P1.2** 确保 C++ 接口的 `StaticClass()` 全局变量在 AS 中可用
  - 当前 `BindUClass` 之后会走 `BindStaticClass` 流程。需要确认接口类是否已被该流程覆盖
  - 若未覆盖（例如因为接口类被 `ShouldBindEngineType` 早期过滤），需在 P1.1 的注册路径中显式调用 `BindStaticClass`
  - 验证脚本中 `UMyInterface::StaticClass()` 可编译并返回非空
- [ ] **P1.2** 📦 Git 提交：`[Interface] Feat: ensure C++ UInterface StaticClass accessible in script`

### Phase 2：C++ UInterface 自动方法注册

> 目标：为 C++ UInterface 的 `UFUNCTION` 自动生成 AS 调用入口——解决缺口 1 的方法注册部分。

- [ ] **P2.1** 实现 C++ 接口方法的自动扫描与注册
  - 当前测试中的 `BindNativeInterfaceMethod` 是逐方法手写声明字符串的。需要实现通用的 `BindNativeInterfaceMethods(UClass* InterfaceClass)` 函数
  - 遍历 C++ 接口 `UClass` 上的 `TFieldIterator<UFunction>`，过滤出 `FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_BlueprintPure` 的方法
  - 对每个方法：从 `UFunction` 的 `FProperty` 链自动生成 AS 方法声明字符串 → 调用 `RegisterObjectMethod` + `asFUNCTION(CallInterfaceMethod)` + `asCALL_GENERIC` → `RegisterInterfaceMethodSignature(FuncName)` 写入 `UserData`
  - 跳过 `UInterface::StaticClass()` 本身的基础方法（与现有 `FinalizeClass` 中跳过 `GetOuter() == UInterface::StaticClass()` 的逻辑一致）
  - 复用 ClassGenerator 中已有的 `CallInterfaceMethod` generic 回调（行 55-97），它已能正确处理参数拷入/拷出和返回值
- [ ] **P2.1** 📦 Git 提交：`[Interface] Feat: auto-register C++ UInterface methods for script access`

- [ ] **P2.2** 实现接口继承链的方法链接
  - C++ 接口可以有继承关系（如 `UAngelscriptNativeChildInterface : UAngelscriptNativeParentInterface`）。子接口需要能访问父接口的方法
  - 实现 `LinkNativeInterfaceInheritance`：沿 `UClass::GetSuperClass()` 递归，将父接口的方法注册到子接口 AS 类型上（去重）
  - 需要两轮遍历：先注册所有接口的自身方法（P2.1），再链接继承（本步骤），避免注册时序导致父接口尚未可见
  - 测试 fixture `UAngelscriptNativeChildInterface` 继承 `UAngelscriptNativeParentInterface` 正好用于验证此场景
- [ ] **P2.2** 📦 Git 提交：`[Interface] Feat: link C++ interface inheritance chain for method visibility`

- [ ] **P2.3** 移除 NativeTests 中的手动绑定代码
  - P2.1 和 P2.2 完成后，`AngelscriptInterfaceNativeTests.cpp` 中的 `EnsureNativeInterfaceFixturesBound()` 及其辅助函数 `EnsureNativeInterfaceBoundForTests`、`BindNativeInterfaceMethod`、`TestCallInterfaceMethod` 应该不再需要
  - 移除手动绑定代码，确认 2 个 Native 测试仍然通过（现在走的是通用自动绑定路径）
  - 如果通用路径有差异导致测试失败，需要调整通用注册逻辑而不是保留手动绑定
- [ ] **P2.3** 📦 Git 提交：`[Interface] Refactor: remove manual native interface binding from tests`

### Phase 3：C++ UInterface 测试验证

> 目标：建立完整的 C++ 接口绑定测试覆盖，确保自动注册路径健壮可靠。

- [ ] **P3.1** 扩展 `Shared/AngelscriptNativeInterfaceTestTypes.h` 测试 fixture
  - 当前已有 `UAngelscriptNativeParentInterface`（2 个方法）和 `UAngelscriptNativeChildInterface`（1 个额外方法）
  - 新增第二个独立接口 `UAngelscriptNativeSecondaryInterface`（1-2 个方法，不继承前者），用于测试"同时实现多个 C++ 接口"场景
  - 确保所有方法使用 `BlueprintCallable, BlueprintNativeEvent` 修饰，与典型 C++ 接口用法一致
- [ ] **P3.1** 📦 Git 提交：`[Test/Interface] Feat: add secondary C++ UInterface test fixture`

- [ ] **P3.2** 创建 `Interface/AngelscriptCppInterfaceTests.cpp`，覆盖核心用例
  - 这些测试不再依赖手动绑定，验证的是 Phase 1-2 实现的通用自动绑定路径
  - 用例列表：
    - `CppInterface.RegisterType` — C++ 接口在 AS 中作为类型可见（编译含该类型变量的脚本）
    - `CppInterface.StaticClass` — 脚本中 `UAngelscriptNativeParentInterface::StaticClass()` 非空
    - `CppInterface.Implement` — 脚本类声明实现 C++ 接口（`class Foo : AActor, UAngelscriptNativeParentInterface`），编译成功
    - `CppInterface.ImplementsInterface` — C++ 侧 `ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass())` 返回 true
    - `CppInterface.CastSuccess` — `Cast<UAngelscriptNativeParentInterface>(ScriptActor)` 非空
    - `CppInterface.CastFail` — 未实现该接口的对象 Cast 返回 nullptr
    - `CppInterface.CallMethod` — 通过接口引用调用方法，验证脚本实现被执行并返回正确值
- [ ] **P3.2** 📦 Git 提交：`[Test/Interface] Feat: add C++ UInterface binding scenario tests (7 cases)`

- [ ] **P3.3** 创建 `Interface/AngelscriptCppInterfaceAdvancedTests.cpp`，覆盖高级场景
  - `CppInterface.ChildInterface` — 脚本类实现 C++ 子接口，自动满足父接口 `ImplementsInterface`
  - `CppInterface.MissingMethod` — 脚本类声明实现 C++ 接口但缺少方法，编译报错（`missing required method`）
  - `CppInterface.MultipleNativeInterfaces` — 脚本类同时实现 `UAngelscriptNativeParentInterface` + `UAngelscriptNativeSecondaryInterface`
  - `CppInterface.MixedInterfaces` — 脚本类同时实现 C++ 接口和脚本定义接口
  - `CppInterface.ExecuteBridge` — C++ 侧通过 `Execute_GetNativeValue(Actor)` 调用脚本实现（验证双向调用）
- [ ] **P3.3** 📦 Git 提交：`[Test/Interface] Feat: add C++ UInterface advanced scenario tests (5 cases)`

- [ ] **P3.4** 回归全部接口测试，确认现有 19 个测试 + 新增测试全部通过
  - 运行 `Angelscript.TestModule.Interface.*`（现有 19 个 + 新增 12 个）
  - 特别关注：原有 NativeImplement / NativeInheritedImplement 从手动绑定切换到自动绑定后是否仍通过
  - 确认无回归：脚本定义接口的 Declare / Implement / Cast / Advanced 全部不受影响
- [ ] **P3.4** 📦 Git 提交：`[Test/Interface] Test: verify all interface tests pass including C++ auto-binding`

### Phase 4：后续缺口边界定义

> 目标：明确 FInterfaceProperty 和签名校验增强的启动条件，不在本计划中实现但确保有清晰的后续入口。

- [ ] **P4.1** 更新 `Plan_InterfaceBinding.md` 状态
  - 将 Phase 0 决策结论、Phase 1-3 完成状态标记到原 Plan 文档
  - 明确 Phase 4（FInterfaceProperty）和 Phase 5（签名校验）的启动前提条件和优先级
  - 如果 P0.2 决策为"FInterfaceProperty 先跳过"，在原 Plan 中标注延后理由
- [ ] **P4.1** 📦 Git 提交：`[Interface] Docs: update Plan_InterfaceBinding status and remaining phase priorities`

- [ ] **P4.2** 创建 `Documents/Knowledges/InterfaceBinding.md` 知识文档
  - 记录当前接口支持的完整范围：脚本接口（完整）+ C++ 接口绑定（Phase 1-3 完成后）
  - 记录已知限制：FInterfaceProperty 未支持、签名校验仅按名字、接口块不进入 AS 编译器
  - 记录架构决策结论和理由
  - 提供脚本语法示例（声明、实现、Cast、C++ 接口绑定）
- [ ] **P4.2** 📦 Git 提交：`[Docs] Feat: add InterfaceBinding knowledge document`

## 验收标准

1. **C++ UInterface 自动绑定**：C++ 定义的 `UINTERFACE` 在 AS 脚本中自动可见，无需手动 `ReferenceClass`；能 Cast、调方法、引用 `StaticClass()`
2. **测试覆盖**：≥12 个新增 C++ 接口场景测试通过（7 核心 + 5 高级），现有 19 个接口测试全部回归通过
3. **手动绑定移除**：`AngelscriptInterfaceNativeTests.cpp` 不再包含 `EnsureNativeInterfaceFixturesBound` 等手动绑定代码
4. **架构决策文档化**：ThirdParty 修改策略和命名约定有明确记录
5. **后续路径清晰**：FInterfaceProperty 和签名校验的优先级和启动条件已明确

## 风险与注意事项

### 风险 1：接口注册时序

`BindUClass` 的调用顺序可能导致接口类型在方法注册时尚未可见。特别是接口继承链中，子接口可能先于父接口被遍历到。

**缓解**：Phase 2 采用两轮遍历模式——P2.1 先注册所有接口的自身方法，P2.2 再链接继承。不在单次遍历中同时做注册和继承链接。

### 风险 2：BindDB 双路径

`Bind_BlueprintType.cpp` 中有 `#if AS_USE_BIND_DB` / `#else` 双路径。BindDB 路径（约 712-724 行）遍历 `FAngelscriptBindDatabase::Get().Classes`，非 DB 路径（约 1029-1047 行）遍历 `TObjectRange<UClass>()`。两条路径的过滤逻辑不同。

**缓解**：Phase 1 实现时需确认当前构建使用哪条路径（检查 `AS_USE_BIND_DB` 宏定义状态），确保接口注册逻辑在正确的路径中生效。两条路径都需处理或至少在不使用的路径中留注释。

### 风险 3：C++ 接口名称映射

`FAngelscriptType::GetBoundClassName` 返回的类名可能包含 `U` 前缀。脚本中写 `Cast<UMyInterface>()` 还是 `Cast<IMyInterface>()` 取决于 P0.2 的命名约定决策。如果选双重注册（U+I），需要额外的 `RegisterAlias` 调用。

**缓解**：P0.2 先决策，P1.1 按决策实现。对非标准命名的接口添加 `check()` 断言验证。

### 风险 4：ShouldBindEngineType 过滤

`ShouldBindEngineType`（行 962-1027）当前的过滤条件可能意外排除接口类。例如接口类不一定有 `BlueprintType` meta 或 `BlueprintCallable` 函数，可能被过滤掉。

**缓解**：P1.1 中需要确认接口类的 meta 特征，可能需要在 `ShouldBindEngineType` 中为 `CLASS_Interface` 添加早期放行路径，或在 `ShouldBindEngineType` 之外单独处理接口类注册。

## 依赖关系

```text
Phase 0（架构决策）
  ↓ 决定 RegisterObjectType vs RegisterInterface、U vs U+I 命名
Phase 1（类型注册）
  ↓ 接口在 AS 中可见
Phase 2（方法注册 + 继承链接 + 手动绑定移除）
  ↓ 接口方法可调用
Phase 3（测试验证）
  ↓ 回归确认
Phase 4（文档 + 后续路径）
```

## 参考文档索引

| 文档 | 用途 |
|------|------|
| `Documents/Plans/Plan_InterfaceBinding.md` | 完整接口绑定设计文档（含 Patch 方案对比、FInterfaceProperty、签名校验） |
| `Plugins/Angelscript/Source/AngelscriptTest/Interface/AngelscriptInterfaceNativeTests.cpp` | 现有手动绑定 PoC，Phase 2 的改造对象 |
| `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptNativeInterfaceTestTypes.h` | C++ 测试 UInterface fixture |
| `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_BlueprintType.cpp` | 核心改造文件（BindUClass / ShouldBindEngineType） |
| `Plugins/Angelscript/Source/AngelscriptRuntime/ClassGenerator/AngelscriptClassGenerator.cpp` | CallInterfaceMethod 实现（行 55-97）、接口 UClass 创建、FinalizeClass 接口挂接 |
| `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/Bind_UObject.cpp` | opCast 接口分支（已可用）、ImplementsInterface 绑定 |
