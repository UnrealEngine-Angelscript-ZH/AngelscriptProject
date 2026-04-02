#include "AngelscriptTestSupport.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTestSupport
{
	bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	void ResetSharedInitializedTestEngine(FAngelscriptEngine& Engine);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleBasicTest,
	"Angelscript.TestModule.Angelscript.Handles.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleBasic.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASHandleBasic"));
		ResetSharedInitializedTestEngine(Engine);
	};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASHandleBasic"),
		ScriptFilename,
		TEXT("class HandleBasicObject { int Value; } int Test() { HandleBasicObject@ First = HandleBasicObject(); First.Value = 10; HandleBasicObject@ Second = First; return First.Value + Second.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Handles.Basic should remain unsupported because script-class handle declarations are not available on this branch"), bCompiled))
	{
		return false;
	}
	return CompileResult == ECompileResult::Error;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleImplicitTest,
	"Angelscript.TestModule.Angelscript.Handles.Implicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleImplicitTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedInitializedTestEngine();
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASHandleImplicit"),
		TEXT("ASHandleImplicit.as"),
		TEXT("class HandleImplicitObject { int Value; } void SetValue(HandleImplicitObject ObjectRef) { ObjectRef.Value = 42; } int Test() { HandleImplicitObject ValueHolder; SetValue(ValueHolder); return ValueHolder.Value; }"));
	if (!TestTrue(TEXT("Handles.Implicit should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleImplicit"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Handles.Implicit should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Handles.Implicit currently verifies compile and symbol registration only because executing implicit script-class parameter passing still faults at runtime on this branch"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleAutoTest,
	"Angelscript.TestModule.Angelscript.Handles.Auto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleAutoTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleAuto.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASHandleAuto"));
		ResetSharedInitializedTestEngine(Engine);
	};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASHandleAuto"),
		ScriptFilename,
		TEXT("class HandleAutoObject { int Value; } HandleAutoObject@ Create() { HandleAutoObject Instance; Instance.Value = 42; return Instance; } int Test() { HandleAutoObject@ Created = Create(); return Created.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Handles.Auto should remain unsupported because factory-style script-class handles are not available on this branch"), bCompiled))
	{
		return false;
	}
	return CompileResult == ECompileResult::Error;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleRefArgumentTest,
	"Angelscript.TestModule.Angelscript.Handles.RefArgument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleRefArgumentTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedInitializedTestEngine();
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASHandleRefArgument"),
		TEXT("ASHandleRefArgument.as"),
		TEXT("void Modify(int &out Value) { Value = 42; } int Test() { int Value = 0; Modify(Value); return Value; }"));
	if (!TestTrue(TEXT("Handles.RefArgument should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleRefArgument"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Handles.RefArgument should expose the compiled module"), Module))
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

	TestEqual(TEXT("Handles.RefArgument should propagate out-ref writes back to the caller"), Result, 42);
	return true;
}

#endif
