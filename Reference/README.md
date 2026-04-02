# Reference

## 目的

- 本目录用于集中维护当前项目依赖的外部参考仓库说明。
- 这些仓库不属于当前项目提交内容，只用于对照、迁移分析、架构参考和实现取舍判断。
- `Agents_ZH.md` 只保留索引级信息；具体说明、用途边界、优先级判断统一维护在本文件。

## 外部参考仓库总表

| 名称 | 入口与说明 |
| --- | --- |
| AngelScript v2.38.0 | 使用 `Tools\PullReference.bat angelscript` 默认拉取到当前项目的 `Reference\angelscript-v2.38.0`；GitHub `https://github.com/anjo76/angelscript.git`；SSH `git@github.com:anjo76/angelscript.git`；用于对照 AngelScript 语言本体与官方测试 |
| Hazelight Angelscript | 读取 `AgentConfig.ini` 中 `References.HazelightAngelscriptEngineRoot`；本地配置来源；当前未记录到可直接拉取的 GitHub 地址；用于参考 Hazelight 的 Angelscript 集成、模块拆分、绑定、测试组织以及引擎侧改造 |
| UnrealCSharp | 使用 `Tools\PullReference.bat unrealcsharp` 默认拉取到当前项目的 `Reference\UnrealCSharp`；GitHub `https://github.com/crazytuzi/UnrealCSharp.git`；SSH `git@github.com:crazytuzi/UnrealCSharp.git`；用于横向参考 Unreal 脚本插件工程架构 |

## 参考源说明

### 1. AngelScript v2.38.0

- 默认路径：当前项目的 `Reference\angelscript-v2.38.0`
- GitHub：`https://github.com/anjo76/angelscript.git`
- SSH：`git@github.com:anjo76/angelscript.git`
- 拉取命令：`Tools\PullReference.bat angelscript`
- 重点目录：
- `Reference\angelscript-v2.38.0\sdk\angelscript\source\`
- `Reference\angelscript-v2.38.0\sdk\add_on\`
- `Reference\angelscript-v2.38.0\sdk\tests\`
- 主要用于确认 AngelScript 原生运行时、编译器、语法行为、调用约定、标准附加组件和官方测试基线。
- 涉及引擎核心源码文件时，应优先以这个上游版本为准，避免把 Unreal 集成差异误判成 AngelScript 原生行为。

### 2. Hazelight Angelscript

- 路径来源：读取 `AgentConfig.ini` 中的 `References.HazelightAngelscriptEngineRoot`
- GitHub：当前未记录到可直接使用的远程地址
- SSH：当前未记录到可直接使用的远程地址
- 拉取命令：当前不支持通过 `Tools\PullReference.bat` 自动拉取
- 该参考源统一承载原先拆开的两类信息：Hazelight Unreal 插件集成方式，以及 Hazelight 引擎侧的 Angelscript 改造与底层支撑。
- 主要用于确认 Unreal 集成方式，包括插件结构、模块拆分、UE 类型绑定、编辑器扩展、测试组织方式以及脚本资产工作流。
- 同时也用于比对引擎级补丁、引擎内扩展点、底层绑定支撑和插件与引擎协同方式。
- 当前仓库里的 `Plugins/Angelscript` 本质上是朝“插件化、可维护”的方向整理这个参考源，因此后续迁移、对齐、补能力时都优先参考这个本地配置路径。
- 该参考源由本机配置显式指定，不走当前项目内置的 GitHub 同步脚本流程。

### 3. UnrealCSharp

- 默认路径：当前项目的 `Reference\UnrealCSharp`
- GitHub：`https://github.com/crazytuzi/UnrealCSharp.git`
- SSH：`git@github.com:crazytuzi/UnrealCSharp.git`
- 拉取命令：`Tools\PullReference.bat unrealcsharp`
- 该参考源主要用于参考另一套成熟的 Unreal 脚本插件工程如何组织模块、桥接运行时、管理代码生成、处理编辑器集成以及维护插件工程边界。
- 对于“插件架构怎么拆”“宿主工程怎么最小化”“代码生成和绑定流程怎么组织”这类问题，可以把 `UnrealCSharp` 作为横向参考。

## 如何选择参考源

- AngelScript 语言或运行时本体问题，优先参考 `angelscript-v2.38.0`。
- Unreal 集成、绑定策略、编辑器交互、测试工程组织问题，优先参考 `HazelightAngelscriptEngineRoot` 指向的 Hazelight 参考仓库。
- 涉及引擎级补丁、引擎内扩展点或插件无法独立解释的底层行为时，同样优先参考 `HazelightAngelscriptEngineRoot` 指向的仓库。
- 跨语言但同属 Unreal 脚本插件架构、模块边界、工程组织问题，可额外参考 `UnrealCSharp`。

## 使用约束

- 外部参考仓库不应直接作为当前项目的一部分提交。
- GitHub 来源的参考仓库，应优先使用各自对应的 SSH 地址，通过统一入口 `Tools\PullReference.bat` 拉取或同步到当前项目的 `Reference/` 目录。
- 对于 AngelScript v2.38.0，默认按“每个项目各自拉取到自己的 `Reference/` 目录”处理，不再依赖项目外部的固定公共路径。
- 对于 `UnrealCSharp`，同样按“每个项目各自拉取到自己的 `Reference/` 目录”处理。
- 本地配置来源的参考仓库，使用前先读取 `AgentConfig.ini`，不要在通用文档或脚本中写死机器路径。
- 当前本地配置来源包括：`HazelightAngelscriptEngineRoot`。
- 如果不同参考源之间存在差异，应显式区分“语言本体行为”“UE 插件集成差异”“引擎侧改造差异”，不要混写。

## 维护规则

- 后续新增参考仓库时，优先按总表补充：名称、入口与说明。
- 如果新增仓库可通过 GitHub 拉取，应优先接入 `Tools\PullReference.bat`，而不是再新增单独脚本。
- 如果新增仓库需要更详细的优先级说明，应在本文件追加独立小节，而不是继续堆积到 `Agents_ZH.md`。
