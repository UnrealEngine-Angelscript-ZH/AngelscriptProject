#include "Angelscript/AngelscriptTestSupport.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTestSupport
{
	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	void ResetSharedInitializedTestEngine(FAngelscriptEngine& Engine);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionDefaultArgumentsTest,
	"Angelscript.TestModule.Angelscript.Functions.DefaultArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionDefaultArgumentsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionDefaultArguments",
		TEXT("int Add(int A, int B = 5) { return A + B; } int Run() { return Add(7); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Default arguments should be applied when omitted"), Result, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionNamedArgumentsTest,
	"Angelscript.TestModule.Angelscript.Functions.NamedArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionNamedArgumentsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionNamedArguments",
		TEXT("int Mix(int A, int B, int C) { return A + B * 10 + C * 100; } int Run() { return Mix(C: 3, A: 1, B: 2); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Named arguments should bind to the intended parameters"), Result, 321);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionPointerAndOverloadTest,
	"Angelscript.TestModule.Angelscript.Functions.OverloadResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionPointerAndOverloadTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionPointerAndOverload",
		TEXT("int Convert(int Value) { return Value + 1; } int Convert(float Value) { return int(Value * 3.0f); } int Run() { return Convert(4) + Convert(2.0f); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Overload resolution should choose the expected function bodies"), Result, 11);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionPointerTest,
	"Angelscript.TestModule.Angelscript.Functions.Pointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionPointerTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionPointer.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASFunctionPointer"));
		ResetSharedInitializedTestEngine(Engine);
	};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionPointer"),
		ScriptFilename,
		TEXT("funcdef int FUNC(int Value); int Callback(int Value) { return Value * 2; } int Run() { FUNC@ FunctionRef = @Callback; return FunctionRef(21); }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Function pointer syntax should remain unsupported on the current branch"), bCompiled);
	return !bCompiled;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionConstructorTest,
	"Angelscript.TestModule.Angelscript.Functions.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionConstructorTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionConstructor",
		TEXT("class ConstructorCarrier { int Value; ConstructorCarrier() { Value = 42; } ConstructorCarrier(int InValue) { Value = InValue; } } int Run() { ConstructorCarrier DefaultCarrier; return DefaultCarrier.Value; }"));
	if (Module == nullptr)
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionDestructorTest,
	"Angelscript.TestModule.Angelscript.Functions.Destructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionDestructorTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionDestructor",
		TEXT("class DestructorCarrier { ~DestructorCarrier() {} } int Run() { DestructorCarrier Carrier; return 1; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Destructor declarations should compile and execute in local scope"), Result, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionTemplateTest,
	"Angelscript.TestModule.Angelscript.Functions.Template",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionTemplateTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionTemplate.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASFunctionTemplate"));
		ResetSharedInitializedTestEngine(Engine);
	};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionTemplate"),
		ScriptFilename,
		TEXT("class TemplateCarrier<T> { T Value; void Set(T InValue) { Value = InValue; } T Get() { return Value; } } int Run() { TemplateCarrier<int> Carrier; Carrier.Set(42); return Carrier.Get(); }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Template syntax should currently remain unsupported on this 2.33-based branch"), bCompiled);
	return !bCompiled;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionFactoryTest,
	"Angelscript.TestModule.Angelscript.Functions.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionFactoryTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionFactory.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASFunctionFactory"));
		ResetSharedInitializedTestEngine(Engine);
	};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionFactory"),
		ScriptFilename,
		TEXT("class FactoryCarrier { int Value; } FactoryCarrier @CreateCarrier(int InValue) { FactoryCarrier Carrier; Carrier.Value = InValue; return Carrier; } int Run() { FactoryCarrier@ Carrier = CreateCarrier(42); return Carrier.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Factory-style handle construction should remain unsupported on the current branch"), bCompiled);
	return !bCompiled;
}

#endif
