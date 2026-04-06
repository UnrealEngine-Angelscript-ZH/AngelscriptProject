# Angelscript Test Macro Notes

Chinese companion: `README_MACROS_ZH.md`

## Status

This file previously documented an abandoned `ANGELSCRIPT_TEST` / `ANGELSCRIPT_ISOLATED_TEST` wrapper family.

Those names are not the active macro API for this repository.

The current authoritative macro system is:

- `Plugins/Angelscript/Source/AngelscriptTest/Shared/AngelscriptTestMacros.h`
- `Plugins/Angelscript/Source/AngelscriptTest/TESTING_GUIDE.md`

## Current Macro Entry Points

Use the current `ASTEST_*` system instead of the older `ANGELSCRIPT_*` names:

- Engine creation: `ASTEST_CREATE_ENGINE_FULL()` / `ASTEST_CREATE_ENGINE_SHARE()` / `ASTEST_CREATE_ENGINE_SHARE_CLEAN()` / `ASTEST_CREATE_ENGINE_SHARE_FRESH()` / `ASTEST_CREATE_ENGINE_CLONE()` / `ASTEST_CREATE_ENGINE_NATIVE()`
- Lifecycle: `ASTEST_BEGIN_FULL` / `ASTEST_END_FULL`, `ASTEST_BEGIN_SHARE` / `ASTEST_END_SHARE`, `ASTEST_BEGIN_CLONE` / `ASTEST_END_CLONE`, `ASTEST_BEGIN_NATIVE` / `ASTEST_END_NATIVE`
- Helper macros: `ASTEST_COMPILE_RUN_INT`, `ASTEST_COMPILE_RUN_INT64`, `ASTEST_BUILD_MODULE`

## Quick Example

```cpp
#include "Shared/AngelscriptTestMacros.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExampleMacroTest,
	"Angelscript.TestModule.Validation.ExampleMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExampleMacroTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASExampleMacro",
		TEXT("int Run() { return 42; }"),
		TEXT("int Run()"),
		Result);

	bPassed = TestEqual(TEXT("Example macro test should return the expected result"), Result, 42);

	ASTEST_END_FULL
	return bPassed;
}
```

## Migration Guidance

- Treat historical `ANGELSCRIPT_*` macro references as deprecated design artifacts.
- Use `TESTING_GUIDE.md` for active examples and macro-selection guidance.
- Keep the terminal `return` after `ASTEST_END_*`; treat `BEGIN` / `END` as the explicit lifecycle boundary in source.
- Use `MACRO_MIGRATION_GUIDE.txt` only as a current redirect unless it has been updated to the same `ASTEST_*` terminology.
