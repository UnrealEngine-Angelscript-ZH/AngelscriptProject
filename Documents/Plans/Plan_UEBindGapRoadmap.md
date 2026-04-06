# UE 引擎 Bind 缺口多模块扩展路线图

## 背景与目标

### 背景

当前仓库的核心交付物是 `Plugins/Angelscript`，而不是宿主工程本身。现有绑定体系已经覆盖了大量基础类型、容器、`UObject` / `AActor` / `UWorld` / `USceneComponent` 等核心入口，也具备 `Bind_BlueprintCallable.cpp` 这类自动暴露路径；但本轮针对本地 `Bind_*.cpp`、测试用例、`knot` UE5 知识库、以及并行 explore/librarian 检索的交叉盘点表明：

- 当前插件在 **世界级碰撞查询**、基础数学类型、容器、Subsystem 通用 `Get()` 入口上覆盖较强；
- 但在 **实例级组件操作**、**玩家控制与屏幕空间**、**Character/Pawn/Movement**、**AI/Navigation**、**Anim runtime**、**Enhanced Input LocalPlayer 子系统** 等脚本高频区仍存在明显能力薄层；
- 现有很多缺口不是“完全没有类型”，而是“类型能在脚本里出现，但缺少真正高频的方法面”，例如 `APlayerController` 目前仅见极少数绑定，`USkeletalMeshComponent` 仅见 `GetLinkedAnimInstances()`，`UPrimitiveComponent` 只有 Bounds / selectable 等零散方法。

本计划不是重新做一轮泛泛能力调研，而是把这次多轮探索收束成一个 **可执行、可验证、可逐 Phase 推进** 的 bind 扩展路线图。

### 目标

围绕“优先补齐脚本侧急缺、且适合手写 bind 的 UE 能力面”建立一个分阶段推进计划，要求：

- 优先处理 **非 UFUNCTION 原生方法**、**默认不会稳定暴露到脚本层的高频入口**、以及 **虽有 Blueprint 面但当前插件未形成可用脚本接口** 的函数；
- 明确区分 **当前已覆盖**、**当前显式跳过**、**当前值得补绑** 三类项目；
- 每个 Phase 都给出建议落点（`Binds/`、`FunctionLibraries/`、`AngelscriptTest/Bindings/`）与验证方式；
- 后续执行时可以直接按 Phase 建立独立任务，不需要重新做 UE 侧搜集。

## 范围与边界

- 纳入范围：`Plugins/Angelscript/Source/AngelscriptRuntime/` 下的手写 bind、必要 wrapper/mixin、以及对应的 `AngelscriptTest` 绑定验证。
- 重点范围：`AActor`、`UWorld`、`APlayerController`、`USceneComponent`、`UPrimitiveComponent`、`APawn`、`ACharacter`、`UCharacterMovementComponent`、`UEnhancedInputLocalPlayerSubsystem`、`AAIController`、`UNavigationSystemV1`、`UAnimInstance`、`USkeletalMeshComponent`、`UAssetManager` / `FStreamableHandle`。
- 排除范围：
  - `AngelscriptSkipBinds.cpp` 中已显式跳过的 `NiagaraPreviewGrid`、`GameplayCamerasSubsystem`、`AsyncAction_PerformTargeting` 等项目；
  - 已经由 `Bind_WorldCollision.cpp` 形成闭环的世界级 Trace / Sweep / Overlap 基础面；
  - 已经由 `Bind_Subsystems.cpp` 自动生成的通用 Subsystem `Get()` 入口；
  - 只适合通过 UHT/自动蓝图导出处理、且当前没有明确脚本痛点证据的泛滥式函数搬运。

## 当前事实状态快照

### 当前已有覆盖（本轮确认）

- `Bind_AActor.cpp` 已覆盖 `GetActorLocation()`、`GetActorRotation()`、`SetActorScale3D()`、`GetComponentsByClass()`、自定义 SpawnActor 包装等基础面。
- `Bind_UWorld.cpp` 已覆盖世界时间、`GetGameInstance()`、`GetLevelScriptActor()`、`GetPersistentLevel()`、`ServerTravel()` 等少量世界接口。
- `Bind_WorldCollision.cpp` 已覆盖大量 **世界级** `LineTrace` / `Sweep` / `Overlap` API。
- `Bind_USceneComponent.cpp` 已覆盖子组件遍历、`GetComponentTransform()`、`SetRelativeLocation()`、`SetComponentVelocity()`、`FScopedMovementUpdate` 等基础面。
- `Bind_UPrimitiveComponent.cpp` 当前仅覆盖 Bounds/selectable/lightmap 相关方法，实例级碰撞和重叠操作明显偏薄。
- `Bind_APlayerController.cpp` 当前仅覆盖 `SetPlayer()` 与 `GetLocalPlayer()`。
- `Bind_ULocalPlayer.cpp` / `Bind_UGameInstance.cpp` 已提供若干 LocalPlayer/GameInstance 入口，但不足以替代 PlayerController/EnhancedInput/屏幕空间的常用工作流。
- `Bind_SystemTimers.cpp` 只有 `SetTimer`、`Pause/UnPause`、`ClearAndInvalidate`、`IsTimerPaused` 等基础 Handle 包装，缺少剩余时间/已过时间/存在性等状态查询。
- `Bind_UEnhancedInputComponent.cpp` 覆盖了组件级 Enhanced Input 绑定；但 `UEnhancedInputLocalPlayerSubsystem` 当前未见对应 bind。
- `Bind_USkeletalMeshComponent.cpp` 仅暴露 `GetLinkedAnimInstances()`，尚不足以支撑常见动画驱动脚本。

### 当前显式不应误判为缺口的项目

- `Subsystem` 通用获取：`Bind_Subsystems.cpp` 已自动为 `EngineSubsystem` / `GameInstanceSubsystem` / `WorldSubsystem` / `LocalPlayerSubsystem` 生成脚本 `Get()`。
- 世界级碰撞：`Bind_WorldCollision.cpp` 已覆盖大量同步 trace/sweep/overlap；短期重点不是重复补世界 API，而是补 **组件/Actor 实例级查询与控制**。
- 显式 skip 项：`StaticMesh/SkeletalMesh` LOD quality setters、`ClothingSimulationInteractorNv`、`NiagaraPreviewGrid`、`GameplayCamerasSubsystem`、`AsyncAction_PerformTargeting` 已在 `AngelscriptSkipBinds.cpp` 中明确跳过，不应纳入急缺名单。

## 高优先缺口总览

| 能力域 | 当前状态 | 值得优先补绑的类/函数 | 推荐优先级 |
| --- | --- | --- | --- |
| Actor / World gameplay helpers | `AActor` / `UWorld` 只有基础面 | `GetActorBounds`、`GetActorEyesViewPoint`、`GetAttachedActors`、`GetFirstPlayerController`、`GetAuthGameMode`、Timer 状态查询 | P1 |
| Player / screen space | `APlayerController` 极薄 | `GetHitResultUnderCursor`、`DeprojectMousePositionToWorld`、`GetMousePosition`、`GetViewportSize`、`SetShowMouseCursor`、`ProjectWorldLocationToScreen`、脚本友好的 `GameOnly` / `UIOnly` / `GameAndUI` 输入模式包装 | P1 |
| Scene / primitive instance operations | `USceneComponent` / `UPrimitiveComponent` 缺少高频实例操作 | `AttachToComponent`、`DetachFromComponent`、`GetSocketLocation`、`GetSocketTransform`、`AddWorldOffset`、`IgnoreActorWhenMoving`、`GetOverlappingActors`、`SetCollisionEnabled`、`SetCollisionProfileName` | P1 |
| Character / pawn / movement | 本地几乎无专用 bind 文件 | `APawn::GetController`、`AddMovementInput`、`GetControlRotation` / `AddControllerYawInput` / `AddControllerPitchInput`、`ACharacter::Jump`、`StopJumping`、`LaunchCharacter`、`Crouch`、`GetCharacterMovement`、`UCharacterMovementComponent::IsFalling` / `SetMovementMode` | P1 |
| Enhanced Input LocalPlayer | 只有 `UEnhancedInputComponent` | `UEnhancedInputLocalPlayerSubsystem::AddMappingContext`、`RemoveMappingContext`、`GetUserSettings`，以及 `FInputActionValue` typed extract | P1 |
| AI / Navigation | 当前无明确 bind | `AAIController::MoveToActor`、`MoveToLocation`、`StopMovement`、`SetFocus` / `ClearFocus`、`UNavigationSystemV1::ProjectPointToNavigation`、`GetRandomReachablePointInRadius`、`GetRandomPointInNavigableRadius` | P2 |
| Animation runtime | `USkeletalMeshComponent` 极薄 | `GetAnimInstance`、`SetAnimInstanceClass`、`PlayAnimation`、`UAnimInstance::Montage_Play`、`Montage_Stop`、`Montage_JumpToSection`、`GetCurveValue` | P2 |
| Physics runtime | 已有世界碰撞与零散物理类型，实例级物理控制仍待核实 | `SetSimulatePhysics`、`AddImpulse`、`AddForce`、`SetPhysicsLinearVelocity`、`WakeAllRigidBodies`、`UPhysicsConstraintComponent` 常用约束接口 | P3（待核实） |
| Reflection / UObject / Struct | `UObject` / `UStruct` 已有基础入口，深层反射价值待核实 | 元数据查询、类/结构 introspection、属性枚举等高层脚本友好入口 | P3（待核实） |
| Render / Material / UI | `UUserWidget` 已有基础树与绘制，但 runtime 材质/UI 便利层仍偏薄 | `UMaterialInstanceDynamic` 参数控制、`UMaterialParameterCollection`、`UWidgetAnimation`、`UPanelWidget`、常用 Widget 类型、`UGameplayStatics` SaveGame 便利入口 | P3（待核实） |
| Networking / Game framework convenience | `ENetMode`、少量 `UWorld` / `GameInstance` 入口已在，但网络工作流仍薄 | `AGameStateBase` 常用查询、SaveGame/PlayerState/GameState convenience、网络相关 helper；RPC/复制本身更适合独立设计 | P3（待核实） |
| 基础 / 数学 / 性能 | 当前基础数学 bind 很强，性能面已有零散观测 API | 更像 convenience 或专用观测入口，原则上不进入急缺主线，除非后续验证存在真实脚本缺口 | Backlog |

## 扩展搜索后的主要方向分层

### 第一层：继续留在主路线的急缺方向

这类方向同时满足：**当前本地覆盖明显偏薄**、**脚本日常高频**、**适合用手写 bind / 轻量 wrapper 解决**。

- **Gameplay / Controller / Component / Input / Character**
  - 这是当前主路线的核心：`APlayerController`、`APawn`、`ACharacter`、`USceneComponent`、`UPrimitiveComponent`、`UEnhancedInputLocalPlayerSubsystem` 都有明确且高频的脚本缺口。
- **AI / Navigation**
  - 本地几乎无 dedicated bind 文件，而 UE 侧又存在很清晰的 script-facing 价值（寻路投点、可达点、MoveTo）。
- **Animation runtime**
  - `USkeletalMeshComponent` 当前几乎只有 `GetLinkedAnimInstances()`，离实用 runtime 控制面差距仍很大，因此保留在主路线的第二梯队中是合理的。

### 第二层：适合拆成独立专题计划的扩展方向

这类方向存在明显机会，但不应直接挤进当前“急缺 gameplay bind”主线；更适合等主线稳定后，按专题单独立项。

- **Physics runtime（推荐独立专题）**
  - 本地证据显示 `Bind_WorldCollision.cpp` 已经很强，但 `Bind_UPrimitiveComponent.cpp`、`Bind_FBodyInstance.cpp`、`Bind_UProjectileMovementComponent.cpp` 都非常薄，且没有 `UPhysicsConstraintComponent` / `UPhysicsHandleComponent`。
  - 这说明物理方向不是“没有物理能力”，而是“世界查询完整、实例级物理控制几乎空白”。它值得做，但应该作为独立专题推进，而不是和 `APlayerController`/`Character` 混成一个阶段。
- **Reflection / UObject / Struct runtime introspection（推荐独立专题）**
  - 当前插件在**编译期绑定**层面其实很强：`Bind_UObject.cpp`、`Bind_UStruct.cpp`、`Bind_UEnum.cpp`、`Bind_BlueprintType.cpp` 已经具备较完整的类型系统入口。
  - 真正缺的是**脚本时运行期反射**：属性迭代、metadata 读取、按名读写属性、类成员枚举、动态 introspection。这条线更偏工具/数据驱动能力，适合作为专题设计，而不是塞进纯 gameplay bind phase。
- **Render / Material / UI convenience（推荐独立专题）**
  - 本地已有 `Bind_UUserWidget.cpp`、`FGeometry`、`FSlateBrush`、`FAnchors` 等基础面，但没有 `UMaterialInstanceDynamic`、`UMaterialParameterCollection`、`UWidgetAnimation`、`UPanelWidget`、SaveGame convenience 这类显式 script-facing 入口。
  - 需要特别注意：`Source/AngelscriptRuntime/FunctionCallers/FunctionCallers_*.cpp` 当前是整段注释的生成片段，不应当被当成“已生效 bind 覆盖”证据；判断缺口时仍应以真实 `Bind_*.cpp` / helper / 测试为准。
  - 这条线的共同点是“对项目开发体验很有价值，但更多属于表现层和便利层”，适合后续做成单独专题，而不是挤占当前 gameplay/AI 主线资源。
- **Networking / Game framework convenience（推荐独立专题）**
  - 当前有 `ENetMode`、少量 `UWorld` 网络入口和 `GameInstance` / online ID 零散能力，但没有真正 script-facing 的 `GameState/PlayerState` convenience，也没有清晰的 RPC/helper 设计。
  - 这条线很容易从“补一些 helper”滑向“网络架构设计”，因此必须和一般 bind 增补分开看待。

### 第三层：原则上不进入当前 bind 急缺主线的方向

这类方向要么当前覆盖已经很强，要么虽有缺口但脚本价值相对偏低，优先级不应高于前两层。

- **基础 / 数学**
  - 本地已有 `Bind_FMath.cpp`、`Bind_FVector*.cpp`、`Bind_FQuat*.cpp`、`Bind_FTransform*.cpp`、`Bind_FDateTime.cpp`、`Bind_FTimespan.cpp` 等高强度覆盖。
  - 继续在这一层追加更多 helper，多数属于 convenience，不再是主路线 blocker。
- **性能 / profiling**
  - 当前已有 `Bind_Stats.cpp`、`Bind_FCpuProfilerTraceScoped.cpp`、少量 `FApp`/平台工具入口；继续扩 profiling 更偏 niche/开发辅助，而不是 gameplay 实用缺口。
- **深层渲染/网络/Chaos 内部接口**
  - 这类 API 的复杂度和维护成本都很高，而且常常超出“给脚本补几个高频入口”的范畴，不应纳入当前路线图主线。

## 影响范围

本路线图后续执行涉及以下操作（按需组合）：

- **已有 bind 扩充**：在现有 `Bind_*.cpp` 中补充方法、全局函数和轻量 wrapper。
- **新增类 bind 文件**：为当前几乎空白的高频类新增 `Bind_<Class>.cpp`。
- **mixin / helper 包装**：对模板、句柄、Subsystem 或 world-context 依赖较重的 API 在 `FunctionLibraries/` 中提供脚本友好的包装入口。
- **绑定测试补齐**：在 `Plugins/Angelscript/Source/AngelscriptTest/Bindings/` 下按主题补充 compile/runtime 测试。
- **文档同步**：在能力边界发生改变时同步 `Documents/Guides/` 或相关 roadmap 文档。

建议影响目录分组如下：

- `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`（约 10~16 个文件）
  - `Bind_AActor.cpp`、`Bind_UWorld.cpp`、`Bind_USceneComponent.cpp`、`Bind_UPrimitiveComponent.cpp`、`Bind_APlayerController.cpp`、`Bind_SystemTimers.cpp` — 已有 bind 扩充
  - `Bind_APawn.cpp`、`Bind_ACharacter.cpp`、`Bind_UCharacterMovementComponent.cpp`、`Bind_AAIController.cpp`、`Bind_UNavigationSystemV1.cpp`、`Bind_UAnimInstance.cpp`、`Bind_UEnhancedInputLocalPlayerSubsystem.cpp` — 新增类 bind 文件
- `Plugins/Angelscript/Source/AngelscriptRuntime/FunctionLibraries/`（约 2~5 个文件）
  - 需要时补充 Gameplay / Screen / AssetStreaming 包装函数
- `Plugins/Angelscript/Source/AngelscriptTest/Bindings/`（约 6~10 个文件）
  - 按 `Actor`、`Component`、`Input`、`AI`、`Animation` 等主题拆分验证文件，而不是继续堆进笼统场景桶

## 分阶段执行计划

### Phase 1：补齐最急缺的 Actor / Controller / Component 表面能力

> 目标：先解决脚本日常最容易碰到的“类型存在、但高频方法缺失”问题，让基础 gameplay、交互和空间逻辑不再需要频繁下沉 C++。

- [ ] **P1.1** 固定 Phase 1 的 bind 目标清单与“已存在能力”边界
  - 先把本轮探索确认过的急缺函数固化为执行清单，避免后续实现时重复把 `Bind_WorldCollision.cpp` 已有世界级 trace、`Bind_Subsystems.cpp` 已有通用 `Get()` 入口、`AngelscriptSkipBinds.cpp` 显式 skip 项再次列为待做。
  - 建议在执行前为 `AActor`、`UWorld`、`APlayerController`、`USceneComponent`、`UPrimitiveComponent` 建一个小型覆盖矩阵，标记“已有 / 待补 / 不做”，后续所有 PR 都以该矩阵为准。
- [ ] **P1.1** 📦 Git 提交：`[AngelscriptRuntime] Docs: freeze phase1 UE bind gap matrix`

- [ ] **P1.2** 扩充 `AActor` / `UWorld` / Timer 状态查询的脚本基础面
  - `Bind_AActor.cpp` 优先补 `GetActorBounds()`、`GetActorEyesViewPoint()`、`GetActorScale3D()`、`GetAttachedActors()` 这组真正会阻塞脚本空间查询与附着逻辑的接口。
  - `Bind_UWorld.cpp` 或配套 world helper 中补 `GetFirstPlayerController()`、`GetAuthGameMode()`、`GetCurrentLevel()` 这一层世界级快速入口。
  - `Bind_SystemTimers.cpp` 扩充 `TimerExists`、`IsTimerActive`、`GetTimerElapsed`、`GetTimerRemaining` 等 handle 状态 API，优先沿用当前 world-context 包装风格，不要引入过重的 `FTimerManager` 生命周期类型设计。
  - `GetActorForwardVector()` / `GetActorRightVector()` / `GetActorUpVector()` 这类 convenience helper 保留为同文件第二批跟进项，不再作为 Phase 1 的核心 blocker。
  - 若某些方法签名不适合直接暴露，优先用轻量 wrapper，而不是为了“纯直绑”把脚本调用面做得过于底层。
- [ ] **P1.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: expand actor world and timer bind surface`

- [ ] **P1.3** 扩充 `APlayerController` 与屏幕空间/鼠标交互能力
  - 在 `Bind_APlayerController.cpp` 中补 `GetHitResultUnderCursor()`、`DeprojectMousePositionToWorld()`、`GetMousePosition()`、`GetViewportSize()`、`SetShowMouseCursor()` / 查询项；如果 `ProjectWorldLocationToScreen()` 更适合包装，也可放到 helper/mixin。
  - 这批实现必须明确补出脚本友好的输入模式包装（例如 `SetInputModeGameOnly`、`SetInputModeUIOnly`、`SetInputModeGameAndUI` 风格入口），不要只在文字上写“支持 UI 输入模式切换”却不给具体目标。
  - 若 `UGameplayStatics` 型静态入口更符合脚本使用习惯，可在 `FunctionLibraries/` 中补充一组 player/screen mixin，但要先确认不会与自动 Blueprint 暴露重复注册。
- [ ] **P1.3** 📦 Git 提交：`[AngelscriptRuntime] Feat: add player controller screen-space helpers`

- [ ] **P1.4** 扩充 `USceneComponent` / `UPrimitiveComponent` 的实例级操作
  - `Bind_USceneComponent.cpp` 优先补 `AttachToComponent()`、`DetachFromComponent()`、`GetSocketLocation()`、`GetSocketTransform()`、`AddWorldOffset()`、`AddLocalOffset()`；`GetSocketRotation()` 与方向向量 helper 统一降为第二批 convenience 项。
  - `Bind_UPrimitiveComponent.cpp` 优先补 `IgnoreActorWhenMoving()`、`IgnoreComponentWhenMoving()`、`GetOverlappingActors()`、`GetOverlappingComponents()`、`SetCollisionEnabled()`、`SetCollisionProfileName()`、`SetCollisionResponseToChannel()`、`GetCollisionProfileName()`。
  - `FCollisionQueryParams` 如确有必要，再补 `AddIgnoredActor()` / `AddIgnoredComponent()` 这种查询构造辅助；不要一开始就把所有物理细节 API 全铺开。
- [ ] **P1.4** 📦 Git 提交：`[AngelscriptRuntime] Feat: add component attachment and collision helpers`

- [ ] **P1.5** 为 Phase 1 新增主题化绑定测试
  - 在 `Plugins/Angelscript/Source/AngelscriptTest/Bindings/` 下按主题补测试，如 `Actor/AngelscriptActorSpatialBindingsTests.cpp`、`Component/AngelscriptComponentCollisionBindingsTests.cpp`、`Input/AngelscriptPlayerControllerBindingsTests.cpp`。
  - 测试目标不是只验证“脚本能编译”，而是至少覆盖一次真实调用结果，例如 forward vector 数值、cursor hit 查询返回、socket transform 读取、timer remaining/elapsed 状态变化。
  - 对 `GetAttachedActors()`、overlap 查询之类需要场景对象的 API，优先用 transient actor/component 组装最小运行时验证，而不是依赖地图资产。
- [ ] **P1.5** 📦 Git 提交：`[AngelscriptTest] Test: cover actor controller and component bind phase1`

### Phase 2：补齐 Character / Pawn / EnhancedInput 的实际操控面

> 目标：让脚本真正能驱动玩家与 Pawn/Character 的日常移动、姿态与输入映射，而不只是声明类型或响应 Blueprint override。

- [ ] **P2.1** 新增 `APawn` / `ACharacter` 专用 bind 文件并补核心操控函数
  - 新建 `Bind_APawn.cpp` 和 `Bind_ACharacter.cpp`，优先暴露 `APawn::GetController()`、`GetPlayerState()`、`GetPawnViewLocation()`、`GetViewRotation()`、`IsControlled()`，以及真正的日常控制面：`AddMovementInput()`、`GetControlRotation()`、`AddControllerYawInput()`、`AddControllerPitchInput()`、`ACharacter::GetCharacterMovement()`、`Jump()`、`StopJumping()`、`CanJump()`、`LaunchCharacter()`、`Crouch()`、`UnCrouch()`、`IsCrouched()`。
  - 这一批函数优先服务于玩家角色、可交互 Pawn、简单移动/跳跃与视角驱动玩法，不要把全部 Character 网络/根运动细节一次打包。
  - 需要显式验证这些 API 是否已被自动 Blueprint 路径覆盖；若只是“理论上可自动暴露，但当前脚本侧不可稳定使用”，仍应保留手写 bind。
- [ ] **P2.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: add pawn and character control binds`

- [ ] **P2.2** 新增 `UCharacterMovementComponent` 基础移动状态查询与模式控制
  - 新建 `Bind_UCharacterMovementComponent.cpp`，优先补 `IsMovingOnGround()`、`IsFalling()`、`SetMovementMode()`、`DisableMovement()` 等脚本高频状态/模式操作。
  - 这一层的目标是让脚本能做落地判定、强制切换 movement mode、临时禁用移动等常见玩法逻辑，不要先陷入复杂网络预测/根运动 API。
  - 若某些字段更适合通过属性访问完成，应在 bind 里保持最少但高价值的方法面，避免把组件做成“完整镜像”。
- [ ] **P2.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: expose character movement state helpers`

- [ ] **P2.3** 补齐 `UEnhancedInputLocalPlayerSubsystem` 与 `FInputActionValue` 的脚本可用性
  - 新建 `Bind_UEnhancedInputLocalPlayerSubsystem.cpp`，优先补 `AddMappingContext()`、`RemoveMappingContext()`、`GetUserSettings()` 等 LocalPlayer 级输入映射入口。
  - 同步检查 `FInputActionValue` 当前是否已经有稳定的 typed extract；如果没有，就补 `Get<bool>()` / `Get<float>()` / `Get<FVector2D>()` 等脚本友好的显式包装，避免脚本拿到值却无法可靠解包。
  - 注意这一层和 `UEnhancedInputComponent` 的职责边界：组件侧负责绑定，LocalPlayerSubsystem 负责 mapping context 生命周期，计划执行时不要混成一个文件。
- [ ] **P2.3** 📦 Git 提交：`[AngelscriptRuntime] Feat: add enhanced input local player bindings`

- [ ] **P2.4** 为 Character / Input Phase 建立最小运行时测试矩阵
  - 按 `Actor` / `Input` / `Component` 主题新增测试，至少验证：Character 可跳跃/蹲伏/发射 `LaunchCharacter()`；Pawn 能拿到 controller/view rotation；Enhanced Input LocalPlayerSubsystem 能正确增删 mapping context。
  - 对输入值提取，优先写静态/纯 C++ 驱动的值包装测试，避免把验证完全绑到真实硬件输入。
- [ ] **P2.4** 📦 Git 提交：`[AngelscriptTest] Test: cover character and enhanced input bind phase2`

### Phase 3：补齐 AI / Navigation 的脚本驱动闭环

> 目标：让脚本能直接发起常用寻路与 AI 控制，而不只是写 BehaviorTree BlueprintBase override。

- [ ] **P3.1** 新增 `AAIController` 常用控制 bind
  - 新建 `Bind_AAIController.cpp`，优先补 `MoveToActor()`、`MoveToLocation()`、`StopMovement()`、`SetFocus()`、`ClearFocus()`、必要时加 `GetFocusActor()`。
  - 先覆盖最常见的行为树外部驱动和即时 AI 命令入口，暂不追求把黑板、感知、EQS 一次性铺开。
  - 如果某个调用涉及复杂 `FAIMoveRequest` 包装，优先先暴露最常用重载，而不是把所有参数层级一起导出。
- [ ] **P3.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: add AI controller movement binds`

- [ ] **P3.2** 新增 `UNavigationSystemV1` 脚本友好查询与寻路入口
  - 新建 `Bind_UNavigationSystemV1.cpp` 或对应 navigation helper，首批优先补 `ProjectPointToNavigation()`、`GetRandomReachablePointInRadius()`、`GetRandomPointInNavigableRadius()` 这类不需要复杂路径对象面的查询。
  - `FindPathToLocationSynchronously()`、`FindPathToActorSynchronously()` 归为第二批扩展：只有在前述查询稳定落地后，再设计脚本侧可接受的 path 结果数据面或轻量 wrapper。
  - 不要把 navmesh rebuild、agent props、filter 全部并入首批；先解决最常见的“找可达点”“把点吸到导航上”“拿到可用随机点”。
  - [ ] **P3.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: add navigation system bind surface`

- [ ] **P3.3** 补 AI / Navigation 运行时测试
  - 增加 `AI/` 主题测试，验证脚本能发起 `MoveToLocation()` / nav projection / reachable point 查询，且在无导航数据或上下文错误时返回可诊断的失败结果，而不是静默崩溃。
  - 结合现有 BehaviorTree 示例测试，区分“类型可编译”与“API 可驱动运行时行为”两类验证。
- [ ] **P3.3** 📦 Git 提交：`[AngelscriptTest] Test: cover AI and navigation bind phase3`

### Phase 4：补齐 Animation 的第二梯队能力

> 目标：把仍然明显偏薄、且直接影响 gameplay 表现层的动画 runtime 能力补齐；Asset / Audio / 治理类工作转入延期附录，不与核心急缺主线并列。

- [ ] **P4.1** 新增 `UAnimInstance` / 扩充 `USkeletalMeshComponent` 的 runtime 动画入口
  - 在 `Bind_USkeletalMeshComponent.cpp` 基础上补 `GetAnimInstance()`、`SetAnimInstanceClass()`、`PlayAnimation()`；必要时新建 `Bind_UAnimInstance.cpp`，优先补 `Montage_Play()`、`Montage_Stop()`、`Montage_JumpToSection()`、`IsAnyMontagePlaying()`、`GetCurveValue()`。
  - 这批 API 对脚本驱动动作、武器状态机和程序化动画反馈都很关键，但涉及对象生命周期与 montage 运行态，因此需要比前几个阶段更重视测试。
  - 先解决 runtime 常用入口，不急于处理编辑器预览、动画蓝图内部工具函数。
- [ ] **P4.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: add animation runtime binds`

- [ ] **P4.2** 补充 Animation 主题测试
  - 新增 `Animation/` 主题测试，验证 montage 控制、anim instance 获取等典型路径。
  - 对生命周期敏感 API，要至少覆盖一次对象销毁/空上下文的负例，避免新 bind 只在 happy path 可用。
- [ ] **P4.2** 📦 Git 提交：`[AngelscriptTest] Test: cover animation bind phase4`

## 延期附录：等待进一步证据的方向

### A. Asset Streaming / Async Handle

- 当前 `Bind_UAssetManager.cpp` 已有 `LoadPrimaryAsset(s)` / `UnloadPrimaryAsset(s)`，但 `FStreamableHandle` 的状态观察与取消控制仍是潜在缺口。
- 该方向先保留为独立后续项：只有当后续验证证明脚本确实需要直接观察 `IsLoaded()` / `IsLoading()` / `CancelHandle()`，才再拆独立计划或附加 Phase。

### B. Audio Runtime

- 本轮本地 bind 扫描未见明确 `UAudioComponent` 手写覆盖，但也尚未证明自动 Blueprint 路径不够用。
- 因此 Audio 暂不列入急缺主线，只保留为“先验证、后决定是否手写 bind”的 backlog 方向。

### C. Bind 治理规则与覆盖矩阵

- “手写 bind 优先级判定规则”和“bind 覆盖矩阵”仍然重要，但它们更适合作为本路线图执行后的文档治理收尾，而不是与核心 gameplay 缺口同优先级并列。
- 如果后续开始批量实现多个 Phase，可再单独追加 `Docs` / `Test` 收口任务，避免现在的主计划被治理项稀释。

### D. Physics Runtime 专题

- 本地探索确认：`Bind_WorldCollision.cpp` 已形成较完整的世界级查询能力，但 `Bind_UPrimitiveComponent.cpp` 仅有极薄实例级覆盖，`Bind_FBodyInstance.cpp` 只有 `Weld/UnWeld/SetUseCCD/GetBodySetup`，`Bind_UProjectileMovementComponent.cpp` 也只有 `HomingTargetComponent`。
- UE 侧候选明确集中在 `AddForce`、`AddImpulse`、`AddTorque`、速度读写、睡眠/唤醒、质量/质心控制、Constraint/Handle 组件等高频 gameplay 物理接口。
- 结论：Physics 值得进入后续专题，但不应与当前 Phase 1~4 混写；更合适的做法是后续单独起一份 `Physics` 方向 plan。

### E. Reflection / UObject / Struct Runtime Introspection 专题

- 本地探索确认：当前插件在 **UObject/UClass/UStruct/UEnum 的基础绑定与编译期 property 绑定** 上已较强，但缺少脚本时 runtime introspection 工具。
- 高价值候选主要集中在：`TFieldIterator<FProperty>` 包装、`GetMetaData/HasMetaData`、枚举/属性/函数遍历、按名读写属性、类/结构成员枚举。
- 结论：这是非常有价值的“能力扩展线”，但它更偏工具链 / 数据驱动 / 调试，而不是立即阻塞 gameplay 的 bind 主线，因此应单独专题化。

### F. Render / Material / UI / SaveGame 便利层专题

- 本地探索确认：`UUserWidget` 基础树和绘制、`FSlateBrush`/`FGeometry` 已有，但 `UMaterialInstanceDynamic`、`UMaterialParameterCollection`、`UWidgetAnimation`、`UPanelWidget`、常用 UMG 控件和 SaveGame 入口均未形成 active bind；全局 grep 也未命中 `SetScalarParameterValue`、`CreateDynamicMaterialInstance`、`PlayAnimation`、`SaveGameToSlot` 等真实绑定实现。
- 需要特别排除误判：`Source/AngelscriptRuntime/FunctionCallers/FunctionCallers_*.cpp` 虽然包含大量 `ERASE_METHOD_PTR(...)` 片段，但文件整体处于注释块中，不能视为当前插件已生效的自动覆盖面。
- **Keep（值得纳入后续专题）**：`UMaterialInstanceDynamic::Set/GetScalar/Vector/TextureParameterValue`、`UMaterialParameterCollection` 运行时参数访问、`UWidgetAnimation` 播放与 started/finished 回调、`UPanelWidget` 子节点管理、`UGameplayStatics::CreateSaveGameObject/SaveGameToSlot/LoadGameFromSlot/DeleteGameInSlot`。
- **Defer（可以后续考虑的高层便利层）**：`UKismetRenderingLibrary` 风格的 render target 创建/绘制 helper、`UPostProcessComponent` / `APostProcessVolume` 的高层 blendable/开关包装、`URuntimeVirtualTextureComponent` 这类高层运行时组件控制。
- **Skip / Overreach（不进入当前路线）**：`UMaterialExpression` 图编辑、`UMaterialFunction`/material layer authoring、`FSceneView`、`FShader`、shader compilation、render pipeline internals、nDisplay、深层 virtual texture internals、渲染诊断/帧调试器等。这些要么是编辑器/渲染器内部能力，要么维护成本远超脚本运行时价值。
- 结论：材质渲染方向里真正适合 Angelscript hand-bind 的，是 runtime convenience 层，而不是材质编辑器和渲染管线内部实现；因此这一组应以“表现层与游戏框架 convenience 专题”推进，不与物理/反射/网络混成单一 phase。

### G. Networking / RPC / Replication 专题

- 本地探索确认：`ENetMode`、`ServerTravel()`、少量 `GameInstance`/online ID 入口存在，但没有完整的 `GameState/PlayerState` convenience，也没有面向脚本的 RPC/helper 设计。
- 进一步扩展网络面时，很容易从 bind 增补升级成 **脚本网络架构设计问题**。
- 结论：网络方向应被视为独立专题，除非只是补 SaveGame/GameState 这类轻量 convenience；否则不建议直接塞进当前 bind 急缺主路线。

## 验收标准

1. 计划明确列出当前 **最急缺** 的模块与函数组，而不是泛泛罗列整个 UE API。
2. 每个 Phase 都给出建议修改目录、代表性目标类/函数和测试落点。
3. 文档显式区分 **当前已有覆盖**、**当前显式 skip**、**当前值得补绑**，避免误报。
4. 计划执行后，至少 `Actor` / `Controller` / `Component` / `Input` / `AI` / `Animation` 六类主题能各自建立独立绑定测试，不再把新增验证继续堆到笼统文件中。
5. 后续执行者可以直接按 `P1.x` ~ `P4.x` 切分任务，不需要重新做一轮跨模块搜索；Asset / Audio / 治理项则明确留在延期附录中等待进一步证据。

## 风险与注意事项

### 风险

1. **自动 Blueprint 暴露与手写 bind 重复注册**
   - 当前插件同时存在 `Bind_BlueprintCallable.cpp` 自动路径与手写 bind。执行前必须先确认目标函数是否真的没有稳定脚本入口，否则容易引入重复注册、命名冲突或双语义行为。

2. **复杂生命周期类型不适合直接裸暴露**
   - `FTimerManager`、`FStreamableHandle`、导航路径对象、动画实例运行态都带有 world/context/lifetime 约束。对于这类能力，应优先做脚本友好的 wrapper，而不是机械搬运底层签名。

3. **“类型可用”不等于“方法够用”**
   - 本轮示例测试已证明 `ACharacter`、`AAIController`、`APawn` 等类型可在脚本签名中出现，但这并不意味着运行时常用 API 已经足够。因此后续验证必须区分“可编译”与“可驱动逻辑”两层。

4. **测试粒度继续膨胀回单文件大杂烩**
   - 如果后续仍然把所有 bind 验证塞进单个 `Bindings` 文件，会重新回到难以定位缺口、难以维护的状态。必须按主题建测试文件，并保持命名与目录边界清晰。

### 已知行为变化

1. **新增手写 bind 后，脚本侧调用路径可能从自动 Blueprint 暴露切换为手写 wrapper**
   - 如果函数名与参数签名做了脚本友好调整，现有脚本示例/测试可能需要同步改写为新入口。

2. **组件/输入/导航类 bind 扩充会提高运行时注册表规模**
   - 每一批新增 bind 都会增加 `CallBinds()` 注册量；执行时应同步关注绑定测试耗时与引擎启动阶段观测，避免在没有测试支撑的情况下持续膨胀。
