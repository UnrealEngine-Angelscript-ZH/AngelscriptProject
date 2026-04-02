# Plan 编写规则

`Documents/Plans/` 下计划文档的编写约束。

## 基本约定

- 文件名：`Plan_<Topic>.md`，Topic 用英文 CamelCase。
- 默认中文撰写；代码符号、类名、路径保持英文。
- commit message 遵守 `Documents/Rules/GitCommitRule_ZH.md`。
- 不要在 Plan 中写死本地路径；路径配置引用 `AgentConfig.ini`。

## 结构

一份 Plan 至少包含：

1. **标题** — `# <计划标题>`
2. **背景与目标** — 为什么做、要达到什么状态
3. **分阶段执行计划** — 按 Phase 组织
4. **验收标准**
5. **风险与注意事项**

根据需要可增加：范围与边界、依赖关系、当前事实状态快照、测试框架速查等。

## 任务条目

- 所有可执行任务用 `- [ ]` checkbox + 唯一编号（`P1`、`P1.1`）。
- **每个可执行任务后必须紧跟一条 `📦 Git 提交` checkbox**，防止忘记提交。
- 紧密关联的小步骤可合并提交，但最后一步仍必须跟提交 checkbox。

```markdown
- [ ] **P1.1** 修改 `as_objecttype.cpp`：启用 IsInterface()
  - 条件：`(flags & asOBJ_SCRIPT_OBJECT) && size == 0`
- [ ] **P1.1** 📦 Git 提交：`[ThirdParty] Feat: enable AS native interface semantics`

- [ ] **P2.1** 注册双重类型
- [ ] **P2.2** 注册接口方法
- [ ] **P2.3** 编译验证 + 测试通过
- [ ] **P2.3** 📦 Git 提交：`[Interface] Feat: C++ UInterface binding`
```

## 阶段组织

- 统一使用 **Phase** 划分阶段（Phase 1、Phase 2...）。
- 每个 Phase 开头用引用块写明目标。
- 条目按依赖顺序排列，阻塞项排前面。

## 禁止事项

- 没有编号的任务条目。
- 没有 checkbox 的执行项。
- 不可执行的模糊表述（"准备重构""后续优化"）。
- 把多个不相关系统揉成一个 Phase。
- 遗漏受影响文档与测试的同步更新。
