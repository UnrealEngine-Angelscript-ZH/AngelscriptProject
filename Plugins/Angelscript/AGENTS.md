# AGENTS.md

- `Source/AngelscriptTest/Native/` 是 Native Core 测试层，只使用 `AngelscriptInclude.h` / `angelscript.h` 暴露的公共 API；不要把 `FAngelscriptEngine` 或 `source/as_*.h` 带进这个目录。
- `Source/AngelscriptRuntime/Tests/` 只放运行时内部 C++ 测试，Automation 前缀统一用 `Angelscript.CppTests.*`。
- `Source/AngelscriptEditor/Private/Tests/` 只放 Editor 内部测试，Automation 前缀统一用 `Angelscript.Editor.*`。
- `Source/AngelscriptTest/` 使用 `Angelscript.TestModule.*` 前缀；`Native/` 与 `Learning/` 采用层级优先命名，主题目录（如 `Actor/`、`Component/`、`Delegate/`）采用主题优先命名，不要在 Automation 路径里重复追加 `Scenario`。
- 新增测试源文件统一以 `Angelscript` 开头；ASSDK 适配层测试文件显式包含 `ASSDK` 标记，例如 `AngelscriptASSDKExecuteTests.cpp`。
- 测试放置、命名与典型流程以 `Documents/Guides/Test.md` 和 `Documents/Guides/TestConventions.md` 为准；新增测试先定层级，再定目录、前缀和运行入口。
