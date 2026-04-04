# Plan 编写规则

`Documents/Plans/` 下计划文档的编写约束。

## 基本约定

- 文件名：`Plan_<Topic>.md`，Topic 用英文 CamelCase。
- 默认中文撰写；代码符号、类名、路径保持英文。
- commit message 遵守 `Documents/Rules/GitCommitRule_ZH.md`。
- 不要在 Plan 中写死本地路径；路径配置引用 `AgentConfig.ini`。

## 归档规则

- `Documents/Plans/` 根目录只保留活跃 Plan 与索引/规则文档；已完成或已关闭的 Plan 移入 `Documents/Plans/Archives/`。
- 归档后的文件继续沿用原始 `Plan_<Topic>.md` 文件名，避免历史引用失真。
- 归档前必须在文档顶部补齐 `归档状态`、`归档日期`、`完成判断` 与 `结果摘要`，说明为什么可以关闭以及产出了什么。
- 如果存在与 Plan 无关的仓库既有问题，需要在归档说明或验收项中明确写成“不阻塞归档”的结论，不能保留含糊的未完成 checkbox。
- 归档后必须同步更新 `Plan_OpportunityIndex.md`、相关导航文档以及 `Documents/Plans/Archives/README.md` 的状态与摘要。

## 结构

一份 Plan 至少包含：

1. **标题** — `# <计划标题>`
2. **背景与目标** — 为什么做、要达到什么状态
3. **分阶段执行计划** — 按 Phase 组织
4. **验收标准**
5. **风险与注意事项**

根据需要可增加：范围与边界、依赖关系、当前事实状态快照、测试框架速查等。

对**迁移或重构类 Plan**（涉及 10+ 文件的批量修改），还应包含：

6. **影响范围** — 先定义操作类型，再按目录分组列出受影响文件及其操作组合（详见下方"影响范围章节"）

## 任务条目

- 所有可执行任务用 `- [ ]` checkbox + 唯一编号（`P1`、`P1.1`）。
- **每个可执行任务后必须紧跟一条 `📦 Git 提交` checkbox**，防止忘记提交。
- 紧密关联的小步骤可合并提交，但最后一步仍必须跟提交 checkbox。

### 条目详情要求

每个可执行任务不应只有一句标题，**必须在缩进列表中补充这个任务的来龙去脉、要达到什么效果、以及打算怎么实现**，使执行者无需反复查阅外部文档即可动手。不要使用显式标签（如"背景信息："、"实现目标："），直接用自然语言描述即可。

```markdown
- [ ] **P2.2** 对齐 template function 的编译与调用链路
  - 当前本地 `Functions.Template` 仍是负例，但上游 `2.38` 已支持"脚本实例化并调用已注册模板函数"；这正是需要翻转的主能力边界
  - 让 parser/builder 识别模板函数调用语法，compiler 完成 subtype 推导，最终脚本中可调用至少一个注册模板函数
  - 先拆旧负例为"非目标语法仍失败 + 注册模板正例待通过"的双边界结构，再在 `as_parser.cpp` / `as_builder.cpp` / `as_compiler.cpp` 中接通最小识别路径，先覆盖 `T -> T` 模式再扩展
- [ ] **P2.2** 📦 Git 提交：`[ThirdParty/AS238] Feat: compile and invoke registered template functions`
```

## 影响范围章节

对涉及批量文件修改的迁移或重构类 Plan，在"分阶段执行计划"之前增加"影响范围"章节，包含两部分：

### 操作类型定义

先列出本次迁移涉及的所有操作类型，给每种操作一个简短名称。

```markdown
本次迁移涉及以下操作（按需组合）：
- **脚本基类替换**：`class AMyActor : AOldBase` → `class AMyActor : ANewBase`
- **include 替换**：`#include "Old.h"` → 删除或替换为 `#include "New.h"`
- **Cast 替换**：`Cast<AOldBase>(...)` → `Cast<ANewBase>(...)`
- **断言文案更新**：更新含旧名称的 `TEXT("...")` 断言消息
```

### 按目录分组的文件清单

按目录分组列出每个受影响文件，标注其所需的操作组合。每组标注文件数量。

```markdown
Actor/（4 个）：
- FileA.cpp — 脚本基类替换 + include 替换 + Cast 替换
- FileB.cpp — 仅脚本基类替换

Component/（1 个）：
- FileC.cpp — 脚本基类替换 + include 替换
```

涉及文件少于 10 个时可用扁平列表或表格替代。

## 风险与已知行为变化

"风险与注意事项"章节内应区分两类内容：

- **风险**：不确定性，需要评估和缓解策略。描述可能出错的场景和应对方案。
- **已知行为变化**：确定性的副作用，执行时**必须**处理，否则会导致编译失败或测试回归。应具体到文件名和（尽可能）行号。

```markdown
## 风险与注意事项

### 风险

1. **序列化格式差异**：2.33 和 2.38 的 bytecode 格式可能有其他差异...
   - **缓解**：此部分最后做，且依赖充分的测试验证

### 已知行为变化

1. **`bCanEverTick` 默认值**：移除后脚本 Actor 继承 AActor 默认
   `bCanEverTick = false`，涉及 Tick 的测试脚本需在构造函数中显式设置
   - 影响文件：`Template_WorldTick.cpp`、`Template_BlueprintWorldTick.cpp`
2. **双分发消除后断言可收紧**：之前因双分发放宽为 `TestTrue(>=1)`，
   移除后可恢复为 `TestEqual(1)`
   - 影响文件：`Template_WorldTick.cpp` (line 85)、`Template_BlueprintWorldTick.cpp` (line 72)
```

## 禁止事项

- 没有编号的任务条目。
- 没有 checkbox 的执行项。
- 不可执行的模糊表述（"准备重构""后续优化"）。
- 把多个不相关系统揉成一个 Phase。
- 遗漏受影响文档与测试的同步更新。
- 只有一句标题没有任何补充说明的条目（纯配置改动除外）。
