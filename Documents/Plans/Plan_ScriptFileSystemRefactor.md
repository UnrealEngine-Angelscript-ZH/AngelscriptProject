# 脚本文件系统抽象层重构计划

## 背景与目标

### 背景

当前 AngelScript 引擎**没有**独立的虚拟文件系统（VFS）模块。脚本从磁盘到编译的完整链路如下：

```
磁盘 .as 文件
  → DiscoverScriptRoots() 枚举根目录
  → FindAllScriptFilenames() 递归发现 *.as
  → FAngelscriptPreprocessor::AddFile() 读入文件内容
  → Preprocess() 分块/宏/导入解析 → FAngelscriptModuleDesc::FCodeSection
  → asCModule::AddScriptSection() 提交给 AS 编译
```

这条链路的文件操作散落在多个位置，各自直接依赖 UE 底层 API：

| 职责 | 当前实现 | 硬编码依赖 |
| --- | --- | --- |
| 根目录发现 | `FAngelscriptEngineDependencies` | ✅ 已抽象（`GetProjectDir`, `DirectoryExists`, `GetEnabledPluginScriptRoots`） |
| 文件枚举 | `FAngelscriptEngine::FindScriptFiles` | ❌ `IFileManager::Get()` 硬编码传入 |
| 文件读取 | `FAngelscriptPreprocessor::AddFile` | ❌ `FFileHelper::LoadFileToString` 硬编码调用 |
| 异步文件读取 | `FAngelscriptPreprocessor::PerformAsynchronousLoads` | ❌ `FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead` 硬编码 |
| 时间戳变更检测 | `FAngelscriptEngine::CheckForFileChanges` | ❌ `IFileManager::Get().GetTimeStamp` 硬编码 |
| 预处理器配置获取 | `FAngelscriptPreprocessor::Preprocess` | ❌ `FAngelscriptEngine::Get()` 全局单例 |

这些硬编码导致了三个实际问题：

1. **测试无法注入内存文件**：`FAngelscriptEngineDependencies` 只能替换根路径，但预处理器的 `AddFile` 始终走 `FFileHelper::LoadFileToString`，测试必须在磁盘上创建真实文件
2. **异步读取是死代码**：`PerformAsynchronousLoads` 完整实现了 `IPlatformFile::OpenAsyncRead` 路径，但生产代码从未传 `bLoadAsynchronous = true`（UEAS2 参考实现中 `InitialCompile` 默认启用异步读取）
3. **热重载变更检测全量扫描 + 纯时间戳**：`CheckForFileChanges` 每次调用 `FindAllScriptFilenames` 递归遍历所有根目录，仅用时间戳判断变更，未利用已有的 `CodeHash`

### 目标

1. 建立统一的脚本文件系统抽象接口 `IAngelscriptFileSystem`，收拢所有散落的文件操作（发现、读取、监控）到单一可替换入口
2. 提供 `FAngelscriptDiskFileSystem`（生产）和 `FAngelscriptMemoryFileSystem`（测试）两种实现
3. 将 `FAngelscriptPreprocessor` 和 `FAngelscriptEngine` 中的硬编码文件操作迁移到新接口
4. 评估并决定异步加载路径的去留（对齐 UEAS2 或清理死代码）
5. 改善文件读取失败和热重载变更检测的健壮性

## 范围与边界

- **纳入范围**
  - `Plugins/Angelscript/Source/AngelscriptRuntime/` 下的文件系统抽象接口定义与实现
  - `FAngelscriptPreprocessor::AddFile` / `PerformAsynchronousLoads` 迁移到新接口
  - `FAngelscriptEngine::FindScriptFiles` / `FindAllScriptFilenames` / `CheckForFileChanges` 迁移到新接口
  - `FAngelscriptEngineDependencies` 中已有的目录操作合并到新接口，或在 Dependencies 中增加 `FileSystem` 注入点
  - `FAngelscriptPreprocessor::Preprocess()` 解除对 `FAngelscriptEngine::Get()` 全局单例的直接依赖
  - 配套的单元测试（`AngelscriptTest/FileSystem/` 下新增或扩展）
  - 文档同步（`Test.md`、`TestCatalog.md`）
- **不纳入范围**
  - `AngelscriptEditor` 中的 `IDirectoryWatcher` 编辑器监听逻辑（保持现状，仅确保新接口不破坏其调用链）
  - `PrecompiledData` / `StaticJIT` 的序列化路径（与脚本源码加载是并行路径）
  - 绑定分片模块（`ASRuntimeBind_*`、`ASEditorBind_*`）
  - `ThirdParty/angelscript/source/` 内部的任何改动

## 当前事实状态快照

### UEAS2（Hazelight 参考）对比

| 项 | UEAS2 参考 | 当前仓库 |
| --- | --- | --- |
| 管理器类名 | `FAngelscriptManager` | `FAngelscriptEngine` |
| `InitialCompile` 异步读取 | **默认开启**（可 `-as-disable-asyncload` 关闭） | **未启用**（`AddFile` 仅传两参数，`bLoadAsynchronous` 默认 false） |
| 异步等待机制 | while 循环 + `volatile bool` | 同结构，但加了 `Sleep(0.001f)` 轮询 |
| 文件枚举 | `IFileManager` 直接调用 | 同 |
| 变更检测 | 时间戳 | 同 |
| VFS 抽象层 | 无 | 无 |

### 关键源文件

| 文件 | 涉及的文件系统职责 |
| --- | --- |
| `AngelscriptRuntime/Core/AngelscriptEngine.h` | `FAngelscriptEngineDependencies`、`AllRootPaths`、`FFilenamePair`、`FHotReloadState`、`FileChangesDetectedForReload` |
| `AngelscriptRuntime/Core/AngelscriptEngine.cpp` | `DiscoverScriptRoots`、`FindScriptFiles`、`FindAllScriptFilenames`、`CheckForFileChanges`、`InitialCompile` |
| `AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.h` | `FFile`（`RawCode`、`AbsoluteFilename`、异步句柄）、`AddFile`、`PerformAsynchronousLoads` |
| `AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp` | `AddFile` 中 `FFileHelper::LoadFileToString`、`PerformAsynchronousLoads` 中 `OpenAsyncRead` |
| `AngelscriptTest/FileSystem/AngelscriptFileSystemTests.cpp` | 现有文件系统测试 |

### 六个具体问题的代码级分析

以下是对当前脚本文件系统逐一审查后识别出的具体问题，每个问题附带精确代码位置和 UEAS2 对比。

#### 问题 1：预处理器文件读取缺乏抽象——无法注入/替换

`AngelscriptPreprocessor.cpp` 第 91-139 行，`AddFile` 方法直接硬编码 `FFileHelper::LoadFileToString`：

```cpp
void FAngelscriptPreprocessor::AddFile(
    const FString& RelativeFilename, const FString& AbsoluteFilename,
    bool bLoadAsynchronous, bool bTreatAsDeleted)
{
    // ...
    if (FFileHelper::LoadFileToString(File.RawCode, *AbsoluteFilename))  // 硬编码
    // ...
}
```

`FAngelscriptEngineDependencies` 虽然能替换根路径（`GetProjectDir`、`DirectoryExists`），但**无法替换「读文件内容」这一步**。测试只能通过伪造真实磁盘文件来驱动预处理器，无法注入纯内存脚本内容。这是当前最大的可测试性瓶颈。

#### 问题 2：文件枚举直接依赖 `IFileManager::Get()`——不可注入

`AngelscriptEngine.cpp` 第 1427-1480 行，`FindScriptFiles` 方法签名设计合理（接受 `IFileManager&` 参数），但所有调用方总是硬编码传入全局单例：

```cpp
// 方法签名接受 IFileManager& —— 设计上是好的
void FAngelscriptEngine::FindScriptFiles(
    IFileManager& FileManager, /* ... */);

// 但调用方总是传入全局单例
FindScriptFiles(IFileManager::Get(), /* ... */);  // 第 1489 行
```

同样，`CheckForFileChanges`（第 2354 行）也直接调用 `IFileManager::Get()`。`Dependencies` 已覆盖 `DirectoryExists` / `MakeDirectory` 等目录操作，但**文件枚举和时间戳查询绕过了依赖注入**，导致测试中无法模拟文件系统变更。

#### 问题 3：异步加载路径存在但未接线——实质是死代码

`AngelscriptPreprocessor.cpp` 第 141-210 行，`PerformAsynchronousLoads` 完整实现了 `IPlatformFile::OpenAsyncRead` 异步读取路径。然而生产代码调用 `AddFile` 时**从未传递 `bLoadAsynchronous = true`**：

```cpp
// InitialCompile 中的调用（第 1561 行）——只传两个参数，bLoadAsynchronous 默认 false
Preprocessor.AddFile(Filename.RelativePath, Filename.AbsolutePath);
```

**UEAS2 对比**：Hazelight 参考实现中 `InitialCompile` **默认启用异步读取**（`bLoadScriptsAsynchronous = true`），可通过命令行 `-as-disable-asyncload` 关闭。这在大型项目首次编译时有明显的性能意义。

此外异步路径本身也有潜在问题：
- 等待逻辑使用 `volatile bool` + `Sleep(0.001f)` 轮询（busy-wait），效率低
- lambda 捕获 `FFile&` 引用，若 `TArray<FFile> Files` 在回调前 resize 则引用悬空

#### 问题 4：文件读取失败策略过于静默

`AngelscriptPreprocessor.cpp` 第 116-137 行的重试与容错逻辑：

```cpp
int32 Tries = 0;
for (; Tries < 6; ++Tries)  // 硬编码魔数
{
    if (FFileHelper::LoadFileToString(File.RawCode, *AbsoluteFilename))
    {
        bLoaded = true;
        break;
    }
    if (Tries >= 4)      FPlatformProcess::Sleep(0.2f);   // 硬编码
    else if (Tries >= 3) FPlatformProcess::Sleep(0.1f);   // 硬编码
    else if (Tries >= 2) FPlatformProcess::Sleep(0.01f);  // 硬编码
}

if (!bLoaded)
    UE_LOG(Angelscript, Warning, TEXT("Unable to open script file %s ..."));  // 仅 Warning
```

存在三个问题：
1. 重试次数（6）和睡眠间隔是**硬编码魔数**，没有配置入口
2. 失败后仅打一条 `Warning` 并将文件**静默当作删除**处理。首次编译时文件因锁被暂时占用导致缺失，但不报 Error
3. 没有区分「文件确实不存在」和「文件被锁/IO 错误」两种情况

#### 问题 5：热重载变更检测仅依赖时间戳

`AngelscriptEngine.cpp` 第 2342-2388 行，`CheckForFileChanges` 方法：

```cpp
FDateTime FileTime = FileManager.GetTimeStamp(*Filename.AbsolutePath);
FHotReloadState* FileState = FileHotReloadState.Find(Filename.RelativePath);
if (FileState == nullptr)
{
    FileChangesDetectedForReload.Add(Filename);  // 新文件
}
else if (FileTime != FileState->LastChange)
{
    FileChangesDetectedForReload.Add(Filename);  // 时间戳变化就认为改变了
    FileState->LastChange = FileTime;
}
```

存在三个问题：
1. **纯时间戳比较会误触发**：`touch` 一个文件但内容未变、版本控制切换分支恢复同内容文件等场景
2. **全量扫描开销**：每次调用都做 `FindAllScriptFilenames` 递归遍历所有根目录，大项目开销不可忽视
3. **未利用已有的 `CodeHash`**：`FAngelscriptModuleDesc::FCodeSection` 中已有 `CodeHash` 字段（编译时通过 xxhash 计算），但变更检测阶段完全没有用它来做内容级比对

#### 问题 6：`Preprocess()` 静态耦合 `FAngelscriptEngine::Get()`

`AngelscriptPreprocessor.cpp` 第 212-214 行：

```cpp
bool FAngelscriptPreprocessor::Preprocess()
{
    ConfigSettings = FAngelscriptEngine::Get().ConfigSettings;  // 全局单例
```

预处理器在 `Preprocess()` 入口直接调用全局单例 `FAngelscriptEngine::Get()`，意味着预处理器**无法在没有全局引擎的上下文中独立工作**（比如测试中想单独验证预处理器的解析行为）。所有生产路径的调用方（`InitialCompile`、`PerformHotReload`）其实已经持有引擎实例引用，完全可以由调用方传入 `ConfigSettings`。

### 改进优先级排序

从实际痛点和改动收益比来看，六个问题的改进优先级从高到低：

1. **问题 1 + 6**（Dependencies 增加 `LoadFileToString` 注入 + 解耦全局单例）→ 解决测试痛点，改动最小
2. **问题 3**（清理或启用异步加载）→ 消除死代码 / 对齐 UEAS2 性能
3. **问题 2**（文件枚举通过接口注入）→ 完善依赖注入一致性
4. **问题 4**（读取失败策略区分场景）→ 提升诊断能力
5. **问题 5**（变更检测增加内容 hash）→ 减少误触发热重载
6. **以上所有问题的系统性解决方案**→ 统一 VFS 接口（本计划的 Phase 1-4）

---

## Phase 1：定义文件系统抽象接口

> 目标：建立 `IAngelscriptFileSystem` 接口，定义脚本文件操作的统一契约，同时提供磁盘实现 `FAngelscriptDiskFileSystem` 作为默认行为的 1:1 替代。

- [ ] **P1.1** 创建 `AngelscriptRuntime/Core/AngelscriptFileSystem.h`，定义 `IAngelscriptFileSystem` 接口
  - 当前文件操作散落在 `FAngelscriptPreprocessor`（读取）和 `FAngelscriptEngine`（枚举、时间戳）中，各自硬编码 UE API。需要一个统一接口把这些操作收拢
  - 接口方法对应当前的具体调用：`FindFiles` → `IFileManager::FindFiles`、`LoadFileToString` → `FFileHelper::LoadFileToString`、`GetTimeStamp` → `IFileManager::GetTimeStamp`、`FileExists` → `IFileManager::FileExists`、`DirectoryExists` → 已在 Dependencies 中
  - 接口签名初步设计：
    - `FindFiles(TArray<FString>&, const FString& Directory, const TCHAR* Pattern, bool bFiles, bool bDirectories)`
    - `LoadFileToString(FString& Out, const TCHAR* Filename) -> bool`
    - `GetTimeStamp(const TCHAR* Filename) -> FDateTime`
    - `FileExists(const TCHAR* Filename) -> bool`
    - `DirectoryExists(const TCHAR* Path) -> bool`
    - `MakeDirectory(const TCHAR* Path, bool bTree) -> bool`
    - `ConvertRelativePathToFull(const FString& Path) -> FString`
  - 使用纯虚基类，不引入 UObject 依赖
- [ ] **P1.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: define IAngelscriptFileSystem interface`

- [ ] **P1.2** 创建 `AngelscriptRuntime/Core/AngelscriptDiskFileSystem.h/.cpp`，实现 `FAngelscriptDiskFileSystem`
  - 这是生产环境的默认实现，内部包装 `IFileManager::Get()` + `FFileHelper` + `FPaths`
  - 每个方法的实现应该是对当前硬编码调用的 1:1 包装，确保行为完全不变
  - `FindFiles` 转发到 `IFileManager::Get().FindFiles`
  - `LoadFileToString` 转发到 `FFileHelper::LoadFileToString`
  - `GetTimeStamp` 转发到 `IFileManager::Get().GetTimeStamp`
  - 其余方法类似转发到对应的 UE API
- [ ] **P1.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: implement FAngelscriptDiskFileSystem wrapping UE file APIs`

- [ ] **P1.3** 在 `FAngelscriptEngineDependencies` 中增加 `TSharedPtr<IAngelscriptFileSystem> FileSystem` 字段
  - `FAngelscriptEngineDependencies` 已经是引擎的依赖注入入口（`GetProjectDir`、`DirectoryExists` 等），在此基础上增加 `FileSystem` 是最自然的扩展点
  - `CreateDefault()` 中用 `MakeShared<FAngelscriptDiskFileSystem>()` 初始化
  - 同时保留已有的 `GetProjectDir` / `ConvertRelativePathToFull` / `DirectoryExists` / `MakeDirectory` / `GetEnabledPluginScriptRoots` 字段不变，后续 Phase 逐步迁移调用方
  - 如果 `FileSystem` 为空（兼容旧代码路径），内部 fallback 到直接调用 UE API
- [ ] **P1.3** 📦 Git 提交：`[AngelscriptRuntime] Feat: add FileSystem injection point to FAngelscriptEngineDependencies`

---

## Phase 2：迁移预处理器文件读取到新接口

> 目标：将 `FAngelscriptPreprocessor` 中硬编码的文件读取替换为通过 `IAngelscriptFileSystem` 调用，使测试可以注入内存文件。

- [ ] **P2.1** 给 `FAngelscriptPreprocessor` 增加 `IAngelscriptFileSystem*` 成员，通过构造函数或 `SetFileSystem` 注入
  - 当前 `FAngelscriptPreprocessor` 的构造函数是无参的，`AddFile` 内部直接调用 `FFileHelper::LoadFileToString`。需要让预处理器持有一个文件系统指针以支持替换
  - 考虑到 `FAngelscriptPreprocessor` 是短生命周期对象（每次编译或热重载新建一个），用裸指针 + 外部保证生命周期即可，不需要智能指针
  - 如果指针为空则保持当前行为（直接调用 `FFileHelper`），确保零破坏性
- [ ] **P2.1** 📦 Git 提交：`[AngelscriptRuntime/Preprocessor] Refactor: accept IAngelscriptFileSystem in preprocessor`

- [ ] **P2.2** 将 `AddFile` 中的 `FFileHelper::LoadFileToString` 调用替换为 `FileSystem->LoadFileToString`
  - 当前 `AddFile`（约第 121 行）直接调用 `FFileHelper::LoadFileToString(File.RawCode, *AbsoluteFilename)`，重试 6 次后静默当作删除
  - 替换为 `FileSystem->LoadFileToString(File.RawCode, *AbsoluteFilename)`；重试逻辑保持不变（重试策略是预处理器自身的职责，不属于文件系统层）
  - 确保 `InitialCompile` 和 `PerformHotReload` 两个路径创建 `FAngelscriptPreprocessor` 时传入文件系统指针
- [ ] **P2.2** 📦 Git 提交：`[AngelscriptRuntime/Preprocessor] Refactor: route file reads through IAngelscriptFileSystem`

- [ ] **P2.3** 解除 `Preprocess()` 对 `FAngelscriptEngine::Get()` 全局单例的直接依赖
  - 当前 `Preprocess()` 开头有 `ConfigSettings = FAngelscriptEngine::Get().ConfigSettings;`，这使得预处理器无法在没有全局引擎的环境中独立工作
  - 改为在创建 `FAngelscriptPreprocessor` 时或调用 `Preprocess()` 前由调用方设置 `ConfigSettings`，所有生产路径（`InitialCompile`、`PerformHotReload`）中调用方已有引擎实例的引用
  - 验证所有 `FAngelscriptPreprocessor` 的创建点（`InitialCompile`、`PerformHotReload`、测试代码）都在创建后正确设置了 `ConfigSettings`
- [ ] **P2.3** 📦 Git 提交：`[AngelscriptRuntime/Preprocessor] Refactor: decouple Preprocess() from global engine singleton`

---

## Phase 3：迁移引擎侧文件枚举与变更检测到新接口

> 目标：将 `FAngelscriptEngine` 中硬编码的 `IFileManager::Get()` 调用替换为通过 `IAngelscriptFileSystem` 调用。

- [ ] **P3.1** 将 `FindScriptFiles` 内部的 `IFileManager&` 参数替换为使用 `Dependencies.FileSystem`
  - `FindScriptFiles` 方法签名中已经接受 `IFileManager& FileManager` 参数（设计上是好的），但所有调用方都传入 `IFileManager::Get()`。改为从 `Dependencies.FileSystem` 获取
  - 具体改动：`FindScriptFiles` 不再接受 `IFileManager&` 参数，内部通过 `Dependencies.FileSystem->FindFiles(...)` 完成文件和目录的查找
  - `FindAllScriptFilenames` 中对 `FindScriptFiles` 的调用相应简化
- [ ] **P3.1** 📦 Git 提交：`[AngelscriptRuntime] Refactor: route FindScriptFiles through IAngelscriptFileSystem`

- [ ] **P3.2** 将 `CheckForFileChanges` 中的 `IFileManager::Get()` 调用替换为 `Dependencies.FileSystem`
  - 当前 `CheckForFileChanges` 直接用 `IFileManager::Get().GetTimeStamp()`。替换为 `Dependencies.FileSystem->GetTimeStamp()`
  - 同步处理 `FindAllScriptFilenames` 的调用（此时应已完成 P3.1）
- [ ] **P3.2** 📦 Git 提交：`[AngelscriptRuntime] Refactor: route CheckForFileChanges through IAngelscriptFileSystem`

- [ ] **P3.3** 清理 `FAngelscriptEngineDependencies` 中被 `IAngelscriptFileSystem` 取代的冗余字段
  - `Dependencies` 中的 `DirectoryExists`、`MakeDirectory`、`ConvertRelativePathToFull` 的职责已被 `IAngelscriptFileSystem` 覆盖
  - 检查所有调用这些 lambda 的位置，逐一迁移到 `FileSystem->DirectoryExists()` 等
  - 迁移完成后将这些 lambda 字段标记为 deprecated 或直接移除（取决于外部调用点的数量）
  - `GetProjectDir` 和 `GetEnabledPluginScriptRoots` 保留，它们是语义层面的配置获取而非文件操作
- [ ] **P3.3** 📦 Git 提交：`[AngelscriptRuntime] Refactor: consolidate Dependencies file ops into IAngelscriptFileSystem`

---

## Phase 4：实现内存文件系统与测试增强

> 目标：提供 `FAngelscriptMemoryFileSystem` 实现，使测试可以完全不依赖磁盘即可驱动预处理器和文件发现。

- [ ] **P4.1** 创建 `AngelscriptRuntime/Core/AngelscriptMemoryFileSystem.h/.cpp`，实现 `FAngelscriptMemoryFileSystem`
  - 内部用 `TMap<FString, FString>` 存储虚拟路径到文件内容的映射，`TMap<FString, FDateTime>` 存储时间戳
  - 提供 `AddVirtualFile(Path, Content)` / `RemoveVirtualFile(Path)` / `SetFileTimestamp(Path, Time)` 等操作方法
  - `FindFiles` 在内存映射上做前缀+通配符匹配
  - `LoadFileToString` 从映射中查找返回，不存在时返回 false
  - 此实现仅用于测试，不需要 ANGELSCRIPTRUNTIME_API 导出（可以放在 Test 模块中，但接口定义留在 Runtime）
- [ ] **P4.1** 📦 Git 提交：`[AngelscriptRuntime] Feat: implement FAngelscriptMemoryFileSystem for testing`

- [ ] **P4.2** 在 `AngelscriptTest/FileSystem/` 下新增测试验证内存文件系统与预处理器的集成
  - 构造 `FAngelscriptMemoryFileSystem`，注入虚拟 `.as` 文件内容
  - 创建 `FAngelscriptPreprocessor`，设置内存文件系统
  - 验证 `AddFile` + `Preprocess` 能正确处理内存中的脚本内容
  - 验证文件不存在时的错误处理行为
  - 验证 `FindScriptFiles` 在内存文件系统上能正确枚举
- [ ] **P4.2** 📦 Git 提交：`[AngelscriptTest] Test: add memory file system integration tests`

- [ ] **P4.3** 将现有依赖磁盘的文件系统测试迁移到可选使用内存文件系统
  - 检查 `AngelscriptTest/FileSystem/AngelscriptFileSystemTests.cpp` 中的现有测试
  - 对于不需要真实磁盘行为的测试，改用 `FAngelscriptMemoryFileSystem` 驱动
  - 保留需要验证真实磁盘行为的测试不变（如路径规范化、跳目录规则等）
- [ ] **P4.3** 📦 Git 提交：`[AngelscriptTest] Refactor: migrate applicable file system tests to memory VFS`

---

## Phase 5：异步加载路径评估与处理

> 目标：对齐 UEAS2 的异步加载行为或清理死代码，消除当前代码中"存在但从未使用"的异步路径。

- [ ] **P5.1** 调研异步加载对当前仓库的适用性并做出决策
  - UEAS2 参考实现中 `InitialCompile` 默认 `bLoadScriptsAsynchronous = true`，通过 `IPlatformFile::OpenAsyncRead` 并行读取所有脚本文件
  - 当前仓库的 `PerformAsynchronousLoads` 实现完整但有两个问题：(a) 等待逻辑用 `volatile bool` + `Sleep(0.001f)` 轮询（busy-wait），(b) 回调中对 `FFile&` 的引用在 `TArray<FFile>` resize 时可能悬空
  - 需要评估：启用异步加载对首次编译的实际加速效果、`IAngelscriptFileSystem` 接口是否需要支持异步、是否存在线程安全问题
  - 决策结果记录在本 Phase 的完成注释中
- [ ] **P5.1** 📦 Git 提交：`[AngelscriptRuntime] Doc: async script loading feasibility assessment`

- [ ] **P5.2** 根据 P5.1 决策执行：启用异步加载或清理死代码
  - **如果启用**：在 `IAngelscriptFileSystem` 中增加 `OpenAsyncRead` 方法，`FAngelscriptDiskFileSystem` 转发到 `IPlatformFile`，`FAngelscriptMemoryFileSystem` 同步模拟。修复等待逻辑改用 `FEvent` 替代 busy-wait。在 `InitialCompile` 中默认启用，增加 `-as-disable-asyncload` 命令行开关
  - **如果清理**：移除 `FAngelscriptPreprocessor::PerformAsynchronousLoads` 及相关代码（`bLoadAsynchronous` 参数、`AsyncReadHandle`/`AsyncSizeRequest`/`AsyncReadRequest` 字段、`bLoadingAnyFilesAsynchronous` 标志）。在 `AddFile` 签名中移除 `bLoadAsynchronous` 参数
  - 无论哪种决策，都需要验证编译通过且热重载不受影响
- [ ] **P5.2** 📦 Git 提交：根据决策选择提交消息

---

## Phase 6：文件读取失败策略与变更检测改进

> 目标：改善文件读取失败的诊断能力，利用已有的 `CodeHash` 减少热重载误触发。

- [ ] **P6.1** 改善文件读取失败策略：区分首次编译与热重载场景
  - 当前 `AddFile` 失败后一律打 `Warning` 并将文件当作删除处理。在首次编译场景中，文件确实存在于磁盘但因短暂 IO 错误无法读取时，应该给出更强的诊断信号
  - 给 `AddFile` 增加一个 `EFileLoadContext` 枚举参数（`InitialCompile` / `HotReload`），首次编译时读取失败升级为 `Error` 级别日志，热重载时保持 `Warning`（容错更重要）
  - 提取重试次数和睡眠时间为具名常量，放在文件顶部或 `IAngelscriptFileSystem` 配套的配置中
- [ ] **P6.1** 📦 Git 提交：`[AngelscriptRuntime/Preprocessor] Fix: distinguish file load failure severity by compile context`

- [ ] **P6.2** 在 `CheckForFileChanges` 中增加内容 hash 比对，减少时间戳误触发
  - `FAngelscriptModuleDesc::FCodeSection` 中已有 `CodeHash` 字段，在编译时计算并保存。当 `CheckForFileChanges` 检测到时间戳变更后，可以快速读取文件内容并计算 hash，与上次编译时的 `CodeHash` 比较
  - 如果 hash 相同则跳过该文件的热重载队列，避免 `touch` 或版本控制操作引起的无意义重编译
  - 注意：这会在变更检测路径增加一次文件读取，需要确保开销可接受。可以只对时间戳变化的文件做 hash 比对（增量开销可控）
  - 通过 `IAngelscriptFileSystem->LoadFileToString` 读取，hash 算法复用现有的 xxhash
- [ ] **P6.2** 📦 Git 提交：`[AngelscriptRuntime] Feat: add content hash comparison to reduce false hot-reload triggers`

---

## 验收标准

- [ ] `IAngelscriptFileSystem` 接口定义完整，覆盖发现、读取、时间戳、目录操作
- [ ] `FAngelscriptDiskFileSystem` 作为默认实现，行为与当前硬编码调用 1:1 一致
- [ ] `FAngelscriptMemoryFileSystem` 可用于测试，支持注入虚拟文件
- [ ] `FAngelscriptPreprocessor` 和 `FAngelscriptEngine` 中所有文件操作均通过 `IAngelscriptFileSystem` 调用
- [ ] `FAngelscriptPreprocessor::Preprocess()` 不再直接调用 `FAngelscriptEngine::Get()`
- [ ] 异步加载路径已明确处理（启用或清理）
- [ ] 所有现有测试继续通过，新增内存文件系统集成测试
- [ ] 编辑器热重载功能不受影响
- [ ] `AngelscriptProjectEditor Win64 Development` 构建通过

## 风险与注意事项

- **行为回归风险**：Phase 2-3 的迁移本质是 Extract Interface 重构，每一步都应该是纯行为保持的。建议每个 P 步骤完成后都做一次完整编译验证
- **异步加载的线程安全**：`PerformAsynchronousLoads` 中的 `FFile&` 引用在 lambda 中捕获，如果 `TArray<FFile> Files` 在异步回调触发前发生了 resize，引用会悬空。如果决定启用异步路径，必须先修复此问题（预分配或改用索引）
- **编辑器目录监听**：`AngelscriptEditor` 中的 `IDirectoryWatcher` 集成不在本计划范围内，但 Phase 3 迁移时需确保编辑器路径（`QueueScriptFileChanges` → `FindScriptFiles`）仍然正常工作
- **性能影响**：Phase 6.2 的内容 hash 比对在变更检测路径增加了一次文件读取。对于大型项目应评估：仅在时间戳变更的文件上做 hash（增量），还是维护一个持久化的 hash 缓存
- **Dependencies 兼容性**：Phase 1.3 增加 `FileSystem` 字段后，所有已有的 `FAngelscriptEngineDependencies` 构造点都需要兼容（`CreateDefault()` 自动填充默认实现，手动构造的测试代码可以显式注入内存版本）
- **与 PrecompiledData 路径的关系**：预编译缓存路径（`PrecompiledScript*.Cache`）不经过预处理器的文件读取逻辑，本计划的 VFS 抽象不影响它。但未来如果需要从虚拟文件系统加载预编译缓存，需要额外扩展接口
