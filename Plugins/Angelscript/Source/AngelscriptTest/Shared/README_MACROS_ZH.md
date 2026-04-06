# Angelscript 测试宏说明（中文补充）

本文是 `README_MACROS.md` 的中文补充，聚焦当前生效的 `ASTEST_*` 宏体系与生命周期边界规则。

## 当前有效入口

- 宏定义：`AngelscriptTestMacros.h`
- 测试指南：`../TESTING_GUIDE.md`
- 中文补充：`../TESTING_GUIDE_ZH.md`

## 关键结论

- `ASTEST_END_*` 不是“主动执行清理”的运行时代码点，它主要是 `ASTEST_BEGIN_*` 对应的源码级作用域闭合。
- 清理动作来自 `ASTEST_BEGIN_*` 内创建的 `FAngelscriptEngineScope` 和 `ON_SCOPE_EXIT`。
- 因此，提前 `return` 时清理仍然会发生，但终结 `return` 仍然应该写在 `ASTEST_END_*` 之后，让源码里的生命周期边界保持显式。

## 迁移规则

- 不再使用旧的 `ANGELSCRIPT_*` 包装宏命名。
- 终结 `return` 放在 `ASTEST_END_*` 之后。
- 如果返回值依赖作用域内局部变量，先保存到外层变量，再结束生命周期宏，再返回。
