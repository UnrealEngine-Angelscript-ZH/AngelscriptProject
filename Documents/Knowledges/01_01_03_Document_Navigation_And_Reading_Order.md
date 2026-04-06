# 现有架构文档导航与阅读顺序

> **所属模块**: 插件总体架构 → 文档导航 / 阅读顺序
> **关键源码**: `AGENTS.md`, `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`, `Documents/Hazelight/ScriptClassImplementation.md`, `Documents/Hazelight/ScriptStructImplementation.md`, `Documents/Plans/Archives/Plan_ASEngineStateDump.md`

到这一节为止，仓库里已经不缺“能看”的文档，缺的是“先看什么、后看什么”。如果一上来就在 `Documents/Plans/`、`Guides/`、`Hazelight/` 和归档计划之间横跳，很容易把“总体定位”“当前实现”“外部参考”“验证规则”混成一锅。这篇文章的目的就是把现有架构资料按阅读目的重新排一遍，让读者能用一条最短路径建立对插件的整体认知。

## 为什么要单独做文档导航

- 根级 `AGENTS.md` 的 `Document Navigation` 只给了一个仓库级入口索引，但它更偏“文档总导航”，不是“架构学习路径”
- 当前真正和插件架构直接相关的材料，分散在 `Documents/Knowledges/`、`Documents/Guides/`、`Documents/Plans/`、`Documents/Plans/Archives/`、`Documents/Hazelight/`
- 所以读架构时，不能按目录名机械线性扫，而要按“先定边界 → 再看主链路 → 再看专题 → 最后看治理和验证”的顺序进入

根级导航区目前列出的文档包括 `Build.md`、`Test.md`、`Plan.md`、`Reference/README.md` 等，它们当然重要，但它们解决的是“怎么构建、怎么验证、怎么找规则”。如果直接把这些文档当成架构主线起点，读者会先陷进流程和规范，而不是先建立插件结构图。

## 推荐阅读主线

- **第一层：先定边界** —— 先读仓库定位、插件边界和目录职责，避免从一开始就把宿主工程、插件模块和测试层混在一起
- **第二层：再抓骨架** —— 读一份能总览当前插件 Runtime / Editor / Test 骨架的文档，建立主链路地图
- **第三层：进入核心实现专题** —— 按类生成、结构体生成、UHT、Dump 这些关键子系统逐个深入
- **第四层：最后补治理和验证视角** —— 再读 containment、测试规范、归档计划，理解为什么仓库要这样组织

```text
入口边界
  -> 当前插件骨架
      -> 核心生成链路
          -> 观测 / 验证 / 技术债
              -> 外部参考与后续演进
```

这条顺序的核心原则只有一个：**先回答“这是什么系统”，再回答“它怎么实现”，最后回答“它怎么被验证和继续演进”。**

## 第一阶段：先读边界文档

- `Documents/Knowledges/01_01_01_Plugin_Goal_And_Host_Boundary.md`：先把“真正交付物是插件，不是宿主项目”这件事钉死
- `Documents/Knowledges/01_01_02_Directory_Responsibilities.md`：再把 `AngelscriptRuntime`、`AngelscriptEditor`、`AngelscriptTest`、`Dump` 的职责层次拆开
- `AGENTS.md`：回头再看仓库级规则，确认这些边界不是临时判断，而是仓库主目标

这三份材料解决的是同一个问题：**别把系统主语认错。**

如果跳过这一层，后面读 `Plan_UnrealCSharpArchitectureAbsorption.md` 或 `ScriptClassImplementation.md` 时，很容易把“插件核心能力”“编辑器接缝”“验证层”误当成同一层实现。

## 第二阶段：用一份文档抓住当前插件骨架

- `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md` 是当前最强的“插件快照文档”之一
- 它虽然名义上是吸收 UnrealCSharp 的计划，但开头几十行已经把当前插件的模块结构、运行时入口、类生成核心、预处理、绑定和热重载链路做了非常高质量的总览
- 因为它同时写了“当前插件快照”和“外部参考快照”，所以特别适合作为**读完整体骨架后的第一份总览性深读文档**

这份文档值得第二阶段就读，有两个原因：

1. 它明确列出了当前插件已经存在的主骨架：`AngelscriptRuntime`、`AngelscriptEditor`、`AngelscriptTest`、`AngelscriptClassGenerator`、`AngelscriptPreprocessor`、`GenerateNativeBinds()`、热重载入口；
2. 它不是静态说明书，而是把“当前结构是什么”和“未来想吸收什么”放在同一视图里，读者能立刻知道哪些是现状，哪些是演进方向。

所以如果只允许选一篇“先抓整体”的长文，优先选它。

## 第三阶段：按实现专题深入

- `Documents/Hazelight/ScriptClassImplementation.md`：先读脚本类生成，因为它同时串起 `UASClass`、对象构造、函数注册、GC、复制、热重载，是最核心的运行时实现专题之一
- `Documents/Hazelight/ScriptStructImplementation.md`：再读脚本结构体支持，它比类生成更聚焦，但能帮助读者理解 `UASStruct` 和 `ICppStructOps` 这条独特路径
- `Documents/Plans/Plan_UhtPlugin.md`：接着看 UHT 工具链，因为它解释的是“运行时绑定为什么需要生成器侧补位”，能把类生成与函数表生成衔接起来
- `Documents/Plans/Archives/Plan_ASEngineStateDump.md`：然后看 Dump，因为它站在“全插件状态导出”的视角，把 Runtime / Editor / Test 三侧状态重新做了一遍结构化扫描

这几份材料合起来，基本覆盖了当前插件最重要的四条实现主线：

- 动态类生成
- 动态结构体支持
- UHT / 生成器补链
- 运行时状态观测

它们的推荐顺序不是按文件夹决定，而是按依赖关系决定：**类生成先于结构体细节，生成链路先于导出观测。**

## 第四阶段：补治理、验证和风险视角

- `Documents/Guides/GlobalStateContainmentMatrix.md`：这是读“问题地图”的入口，适合在已经理解主链路后再看，它会告诉你当前系统哪里耦合最深、哪些路径已经 containment、哪些路径仍然危险
- `Documents/Guides/TestConventions.md`：这份文档不解释运行时怎么工作，但它解释“验证层是怎么围绕运行时子系统组织起来的”，适合放在实现专题之后阅读
- `Documents/Plans/Archives/README.md`：用来确认哪些计划已经落地、哪些归档结论还值得继续参考

这组文档的阅读位置很关键：它们不适合作为架构第一入口，但非常适合在已经建立系统图之后，用来回答两个问题：

1. 当前系统最脆弱的地方在哪；
2. 当前仓库是如何把这些风险折叠进测试、计划和归档治理里的。

## 一条更实用的阅读顺序

如果目标不是“把所有文档都看完”，而是**最快建立能继续写后续文章的知识骨架**，建议按下面顺序读：

1. `Documents/Knowledges/01_01_01_Plugin_Goal_And_Host_Boundary.md`
2. `Documents/Knowledges/01_01_02_Directory_Responsibilities.md`
3. `AGENTS.md`
4. `Documents/Plans/Plan_UnrealCSharpArchitectureAbsorption.md`
5. `Documents/Hazelight/ScriptClassImplementation.md`
6. `Documents/Hazelight/ScriptStructImplementation.md`
7. `Documents/Plans/Plan_UhtPlugin.md`
8. `Documents/Plans/Archives/Plan_ASEngineStateDump.md`
9. `Documents/Guides/GlobalStateContainmentMatrix.md`
10. `Documents/Guides/TestConventions.md`

这条顺序背后的意图是：

- **前 3 篇**：只负责定边界和主语
- **中间 5 篇**：负责建立实现主链路
- **最后 2 篇**：负责理解验证与治理

这样读完之后，读者基本已经具备继续写 `1.2 Runtime 总控与生命周期`、`2.1 脚本类生成机制`、`3.4 State Dump 可观测性架构` 这些后续文章所需的前置背景。

## 补充索引与外部参考入口

- `Documents/Knowledges/AngelscriptPluginArchitecture_Analysis_TodoList.md`：这不是用来替代正文的架构文档，而是当前整套文章的路线图。读者如果想知道“接下来还会写什么、当前已覆盖到哪”，应该回到这份 TodoList 看全局地图
- `Reference/README.md`：当本仓库文档已经把当前实现讲清，但读者还想继续追上游或横向参考时，再从这里进入外部仓库。它的职责是“决定去哪里比对”，不是取代本地架构主线

这两个入口的位置也有讲究：TodoList 是**本地知识地图**，应该在阅读过程中反复回看；`Reference/README.md` 是**外部延伸入口**，应该在已经理解当前插件骨架之后再用，避免一开始就掉进外部实现细节。

## 这条阅读顺序的使用方式

- 如果你是第一次接触仓库，按“完整顺序”读
- 如果你已经知道插件是主语，只需要跳到第 4 步开始补总体骨架
- 如果你正在写某个专题文章，就把对应专题前面的“边界文档 + 骨架文档”先复习一遍，再进入专题文档

换句话说，这不是一条“只读一次就结束”的顺序，而是一条可重复回到不同层次的导航路径。

## 小结

- 当前仓库并不缺架构文档，真正缺的是面向插件主线的阅读顺序
- `AGENTS.md` 提供的是仓库级文档入口，但要理解插件架构，还需要把 Knowledges、Plans、Hazelight、Guides 重新按阅读目的编排
- 最实用的顺序是：先定边界，再抓整体骨架，再读生成 / Dump 等核心专题，最后补 containment 与测试治理
