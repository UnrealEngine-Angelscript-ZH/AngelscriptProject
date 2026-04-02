#pragma once

class FAutomationTestBase;

namespace AngelscriptScriptExamples
{
	struct FScriptExampleSource
	{
		const wchar_t* ExampleFileName;
		const wchar_t* ScriptText;
		const wchar_t* DependencyFileName;
		const wchar_t* DependencyScriptText;
	};

	const FScriptExampleSource& GetScriptExampleEnumSource();
	bool RunScriptExampleCompileTest(FAutomationTestBase& Test, const FScriptExampleSource& Example);
}
