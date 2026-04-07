# Reference 对比分析规则

## 目的

本规则用于约束对 `Reference/` 目录下外部 UE 脚本插件的自动化对比分析。目标是通过深入阅读参考插件源码，产出结构化的对比文档，帮助当前 Angelscript 插件识别差距、吸收经验、明确改进方向。

分析产出必须回答三个核心问题：

- 参考插件在每个核心功能维度上**怎么做的**（架构决策、关键实现）
- 与当前 Angelscript 插件相比**差异在哪**（有无、优劣、取舍）
- 哪些经验**值得吸收**，以及吸收的优先级和可行性

## 适用范围

- 适用于对 `Reference/` 下的 UE 脚本插件仓库进行系统性分析
- 当前覆盖 4 个参考插件：UnrealCSharp、UnLua、puerts、sluaunreal
- 对比基准始终是当前仓库的 `Plugins/Angelscript/`

## 对比维度

### 维度清单

以下维度为默认分析范围，可按需裁剪或扩展：

| 编号 | 维度 | 分析重点 |
| --- | --- | --- |
| D1 | 插件架构与模块划分 | 模块数量与职责、Build.cs 依赖关系、第三方库集成方式、插件与宿主工程的边界 |
| D2 | 反射绑定机制 | UClass / UStruct / UEnum / UInterface / Delegate 如何暴露给脚本、绑定代码生成 vs 手写、绑定注册时机与生命周期 |
| D3 | Blueprint 交互 | 脚本覆写 Blueprint 事件、脚本调用 Blueprint 函数、Blueprint 调用脚本函数、混合继承链 |
| D4 | 热重载 | 脚本变更检测机制、重载粒度（全量 vs 增量）、状态保持策略、重载失败恢复 |
| D5 | 调试与开发体验 | 调试协议（DAP / 自定义）、断点与单步、变量查看、IDE 集成、日志与诊断 |
| D6 | 代码生成与 IDE 支持 | 类型声明文件生成、智能提示、代码补全、跳转定义 |
| D7 | 编辑器集成 | 编辑器菜单 / 面板扩展、资产浏览器集成、Commandlet 支持 |
| D8 | 性能与优化 | JIT / AOT 支持、调用开销基准、内存管理策略、批量绑定优化 |
| D9 | 测试基础设施 | 测试框架选择、测试分层与组织、CI 集成、覆盖率支持 |
| D10 | 文档与示例组织 | 用户文档结构、API 参考生成、教程与示例项目、上手引导流程 |
| D11 | 部署与打包 | 脚本打包方式、加密 / 签名、平台适配、版本兼容性 |

### 维度分析要求

每个维度的分析必须包含：

1. **实现概述**：用 1-3 段文字 + 至少一张 ASCII 架构图说清核心实现
2. **关键源码引用**：贴出关键函数 / 类的源码片段（带中文注释），标注文件路径
3. **设计取舍**：为什么选这个方案、放弃了什么、换来了什么
4. **与 Angelscript 对比**：明确列出当前 Angelscript 插件在该维度的现状，差异点用表格或对比列表呈现

---

## 文档结构

### 纵向分析文档（Per-Repo）

每个参考仓库一篇完整分析文档，结构如下：

```markdown
# [插件名] 源码分析

> **分析对象**: [仓库名称与版本/分支]
> **源码路径**: `Reference/[仓库目录]/`
> **分析日期**: YYYY-MM-DD

[1-3 句概述：这个插件是什么、核心特点、与 Angelscript 的关键差异]

## 插件架构总览

[ASCII 架构图 + 模块关系说明]

## [维度 D1] 模块划分与构建

[按维度逐一展开]

## [维度 D2] 反射绑定机制

...

## 小结

[几条关键结论，说清最值得记住的点]

## 与 Angelscript 差异速查

[表格形式的差异对照]
```

### 横向对比文档（Cross-Comparison）

每个对比维度一篇文档，结构如下：

```markdown
# [维度名称] 横向对比

> **对比范围**: UnrealCSharp / UnLua / puerts / sluaunreal / Angelscript
> **分析日期**: YYYY-MM-DD

[1-3 句概述本维度各插件的总体差异格局]

## 各插件实现概览

[ASCII 对比图或表格]

## 详细对比

### [子维度 1]

[逐项对比，每个插件的做法、优劣]

### [子维度 2]

...

## 对比矩阵

[功能点 × 插件 的表格，标注支持程度]

## 小结与建议

[哪些做法值得 Angelscript 吸收，优先级建议]
```

### 差距分析文档（Gap Analysis）

最终汇总文档，结构如下：

```markdown
# Angelscript 插件差距分析与经验吸收建议

> **分析日期**: YYYY-MM-DD
> **对比基准**: UnrealCSharp / UnLua / puerts / sluaunreal

## 执行摘要

## 差距矩阵

[维度 × 差距等级 的总览表]

## 按维度详细分析

### [维度] 差距与建议

[当前状态 → 差距描述 → 参考方案 → 吸收建议 → 优先级]

## 值得吸收的设计模式

## 改进路线建议

## 结论
```

---

## 代码块规范

沿用 `Knowledges_Rule.md` 的代码块规范：

### 基本格式

所有源码使用对应语言的代码块包裹（`cpp`、`csharp`、`lua`、`typescript`、`python` 等）。

### 文件定位注释

每个代码块开头标注源码路径：

```cpp
// ============================================================================
// 文件: Reference/UnrealCSharp/Source/[Module]/Private/[FileName].cpp
// 函数: [ClassName]::[FunctionName]
// 位置: [一句话定位]
// ============================================================================
```

### 中文内嵌注释

- 关键分支条件、输入输出和副作用必须解释
- 长代码块按步骤拆分注释
- 用 ★ 标记核心调用
- 禁止低信息量注释（纯复述代码的注释）

### 代码块编号

多个代码块时用 `[1]`, `[2]`, `[3]` 编号。

---

## ASCII 图示规范

### 基本要求

- 必须放在裸代码块中（无语言标记）
- 图放在对应概念首次出现的位置附近

### 中英文规则

| 图表类型 | 语言要求 | 中文说明方式 |
| --- | --- | --- |
| 树形结构图 | 节点名用英文，行尾 `//` 注释可用中文 | 中文写在 `//` 注释中 |
| 框图 / 包围框图 | 全部使用英文 | 图后紧跟「术语说明」表格 |
| 分隔符标题图 | 标题用英文 | 图后紧跟中文段落说明 |
| 流程步骤图 | 树形 `├─` + 步骤编号 + 英文关键词 | 行尾 `//` 注释可用中文 |
| 对比矩阵图 | 表头用英文缩写 | 图后紧跟「缩写说明」表格 |

### 推荐图表类型

**插件架构总览图**：

```
[PluginName] Plugin Architecture
├─ [Module 1] Runtime                              // 运行时核心
│   ├─ Binding/                                     // 反射绑定
│   ├─ TypeBridge/                                  // 类型桥接
│   └─ ScriptEngine/                                // 脚本引擎封装
├─ [Module 2] Editor                                // 编辑器扩展
│   ├─ CodeGen/                                     // 代码生成
│   └─ UI/                                          // 编辑器面板
└─ [Module 3] ThirdParty                            // 第三方依赖
    └─ [Runtime Name] v[Version]                    // 脚本运行时版本
```

**对比矩阵图**：

```
Feature Matrix: Reflection Binding
                    UC      UL      PU      SL      AS
UClass binding      [Auto]  [Auto]  [Auto]  [Semi]  [Manual]
UStruct binding     [Auto]  [Auto]  [Auto]  [Auto]  [Manual]
Delegate support    [Full]  [Full]  [Full]  [Part]  [Full]
UInterface support  [Full]  [Part]  [Full]  [None]  [WIP]
Blueprint interop   [Full]  [Full]  [Full]  [Full]  [Full]
```

缩写说明：UC = UnrealCSharp, UL = UnLua, PU = puerts, SL = sluaunreal, AS = Angelscript

**调用链对比图**：

```
[UnrealCSharp] Reflection Binding Flow
├─ [1] UHT generates .generated.cs                  // UHT 生成 C# 声明
├─ [2] Mono runtime loads assemblies                 // Mono 加载程序集
├─ [3] ★ FCSharpBind::Bind()                        // 核心绑定入口
│   ├─ Enumerate UClass properties                   // 枚举 UClass 属性
│   └─ Register managed delegates                    // 注册托管委托
└─ [4] Script calls via P/Invoke bridge              // 通过 P/Invoke 桥接调用

[Angelscript] Reflection Binding Flow
├─ [1] Bind_*.cpp static registration               // 静态注册绑定文件
├─ [2] FAngelscriptBinds::RegisterAll()             // 批量注册
├─ [3] ★ asIScriptEngine::RegisterObjectType()      // AngelScript 引擎注册
└─ [4] Script calls via generated bridge functions   // 通过生成的桥接函数调用
```

---

## 写作风格

### 语言

- 正文、代码注释：中文
- 代码、术语：英文原文
- 首次出现的英文术语附中文翻译

### 语气

- 以码说话，文字叙述极简
- 开头直奔主题
- 对比时客观陈述，不贬低任何参考项目

### 内容组织

- 先给全景再钻细节
- 关键函数要配调用图
- 代码与解释交织，不堆砌
- 对比结论必须落到具体源码位置，不能只说"A 比 B 好"

### 深度基线

1. 每个维度至少包含一张 ASCII 图和一段带注释的关键源码
2. 对比结论必须有源码证据支撑
3. 差距判断必须区分"没有实现" vs "实现方式不同" vs "实现质量差异"

---

## 迭代探索机制

本工具采用多轮迭代探索模式，而非一次性生成。每篇文档经过多轮 AI 扫描，逐轮加深：

### 工作方式

```
Round 1: Read source code → Generate initial document
Round 2: Read Round 1 output + explore deeper → Supplement and expand
Round 3: Read Round 2 output + fill remaining gaps → Final deepening
```

### 每轮的职责

- **第 1 轮**：建立文档骨架，覆盖所有维度的基本分析
- **第 2 轮**：读取上一轮产出，识别哪些维度浅、哪些缺源码引用、哪些缺 ASCII 图，针对性补充
- **第 3+ 轮**：继续深入探索源码，补充更深层的实现细节和设计取舍分析

### 续写原则

- 读取已有文档，在原文基础上**就地补充**，不从头重写
- 已经足够深入的章节保持原样，把精力放在薄弱处
- 每轮探索源码时可能发现新的关联点，应追加到对应章节

---

## 禁止事项

- 不生成 TodoList 章节
- 不把分析写成泛泛的项目介绍
- 不只罗列目录结构不做分析
- 不用空泛表扬或贬低替代客观对比
- 结论不能只说"建议参考""后续可改进"，必须给出具体的参考点和优先级
- 不从官方文档照搬描述，必须对应到具体源码
- 不在 ASCII 图表框线内放置中文字符
