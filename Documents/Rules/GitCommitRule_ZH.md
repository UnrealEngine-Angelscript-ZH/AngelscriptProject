# Git 提交指南

## 提交格式

```text
[<Scope>] <Type>: <description>

<body optional>

<footer optional>
```

## 格式说明

- `Scope`：可选，表示本次提交影响的模块、系统或功能范围，例如 `UI`、`Build`、`Physics`、`Inventory`。
- `Type`：必填，表示变更类型，建议使用清晰且统一的动词或类别，如 `Fix`、`Feat`、`Refactor`、`Docs`、`Test`、`Chore`。
- `description`：必填，用一句简洁的话说明本次提交的核心改动，聚焦结果，不写空泛描述。
- `body`：可选，用于补充为什么改、改了什么背景、有哪些额外影响。
- `footer`：可选，用于关联任务、Issue、Breaking Change 或其他补充信息。

## 编写要求

- 标题行尽量简洁，优先说明最重要的改动。
- `Type` 建议统一使用首字母大写，保持与 UE 常见提交风格一致。
- `Docs` 表示变更类型，不表示作用范围；如果能明确具体模块，建议同时填写 `Scope`。
- `description` 使用明确动作和结果，避免使用“更新一下”“调整内容”等模糊表达。
- 如果改动跨多个模块，可省略 `Scope`，或使用最主要的影响范围。
- 如果提交需要上下文才能理解，补充 `body`；如果不需要，可只保留标题行。

## 示例

```text
[Physics] Fix: prevent character teleport jitter on steep slopes
[UI] Feat: add inventory hotkey hints for enhanced input
[Build] Chore: update plugin compatibility to UE 5.5
[Inventory] Refactor: simplify item stack merge flow
[Angelscript] Docs: clarify module setup steps
```

## 带正文示例

```text
[UI] Feat: add inventory hotkey hints for enhanced input

Show the current hotkey bindings directly in the inventory panel so
players can discover the enhanced input mappings without leaving gameplay.
```

## 带页脚示例

```text
[Build] Chore: update plugin compatibility to UE 5.5

Refs: TASK-128
```
