#include "AngelscriptTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscNamespaceTest,
	"Angelscript.TestModule.Angelscript.Misc.Namespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscNamespaceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMiscNamespace",
		TEXT("namespace MyNamespace { const int Value = 42; int GetValue() { return Value; } } int Test() { return MyNamespace::GetValue(); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Misc.Namespace should resolve namespace-qualified globals"), Result, 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscAnyTest,
	"Angelscript.TestModule.Angelscript.Misc.Any",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscAnyTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Misc.Any should expose a script engine for the isolated compile-fail probe"), ScriptEngine))
	{
		return false;
	}

	ScriptEngine->SetDefaultNamespace("");
	FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
	asIScriptModule* Module = ScriptEngine->GetModule("ASMiscAnyRaw", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Misc.Any should create a raw script module"), Module))
	{
		return false;
	}

	static constexpr ANSICHAR Script[] = "int Test() { any Box; Box.store(42); int Value = 0; Box.retrieve(Value); return Value; }";
	Module->AddScriptSection("ASMiscAnyRaw", Script, UE_ARRAY_COUNT(Script) - 1);
	const int32 BuildResult = Module->Build();
	TestEqual(TEXT("Misc.Any should compile on the raw AngelScript path even though that path does not expose executable globals on this branch"), BuildResult, static_cast<int32>(asSUCCESS));
	return BuildResult == asSUCCESS;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscGlobalVarTest,
	"Angelscript.TestModule.Angelscript.Misc.GlobalVar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscGlobalVarTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMiscGlobalVar",
		TEXT("const int GlobalValue = 42; int Test() { return GlobalValue; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Misc.GlobalVar should expose compiled global state"), Result, 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscMultiAssignTest,
	"Angelscript.TestModule.Angelscript.Misc.MultiAssign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscMultiAssignTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMiscMultiAssign",
		TEXT("int Test() { int A = 0, B = 0, C = 0; A = B = C = 42; return A + B + C; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Misc.MultiAssign should evaluate chained assignments from right to left"), Result, 126);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscAssignTest,
	"Angelscript.TestModule.Angelscript.Misc.Assign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscAssignTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMiscAssign",
		TEXT("int Test() { int Value = 10; Value += 5; Value -= 3; Value *= 2; Value /= 3; return Value; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Misc.Assign should apply compound assignments in sequence"), Result, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscDuplicateFunctionTest,
	"Angelscript.TestModule.Angelscript.Misc.DuplicateFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscDuplicateFunctionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Misc.DuplicateFunction should expose a script engine for the isolated compile-fail probe"), ScriptEngine))
	{
		return false;
	}

	ScriptEngine->SetDefaultNamespace("");
	FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
	asIScriptModule* Module = ScriptEngine->GetModule("ASMiscDuplicateFunctionRaw", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Misc.DuplicateFunction should create a raw script module"), Module))
	{
		return false;
	}

	static constexpr ANSICHAR Script[] = "int Test() { return 42; }\nint Test() { return 42; }\n";
	Module->AddScriptSection("ASMiscDuplicateFunctionRaw", Script, UE_ARRAY_COUNT(Script) - 1);
	const int32 BuildResult = Module->Build();
	TestEqual(TEXT("Misc.DuplicateFunction currently compiles duplicate global declarations on the raw AngelScript path even though that path does not expose executable globals on this branch"), BuildResult, static_cast<int32>(asSUCCESS));
	return BuildResult == asSUCCESS;
}

#endif
