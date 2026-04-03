#include "Angelscript/AngelscriptTestSupport.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Misc/ScopeExit.h"
// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateCompileExecuteTest,
	"Angelscript.TestModule.Angelscript.Core.CreateCompileExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreCreateCompileExecuteTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASCoreCreateCompileExecute",
		TEXT("int DoubleValue(int Value) { return Value * 2; } int Run() { return DoubleValue(21); }"));
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

	TestEqual(TEXT("Core create/compile/execute should return the expected value"), Result, 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreGlobalStateTest,
	"Angelscript.TestModule.Angelscript.Core.GlobalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreGlobalStateTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASCoreGlobalState",
		TEXT("const int g_Count = 3; int Step(int Value) { return Value + 4; } int Run() { return Step(g_Count); }"));
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

	TestEqual(TEXT("Const globals and helper calls should evaluate as expected"), Result, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateEngineTest,
	"Angelscript.TestModule.Angelscript.Core.CreateEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreCreateEngineTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngineConfig Config;
	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> LocalEngineA = FAngelscriptEngine::CreateForTesting(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> LocalEngineB = FAngelscriptEngine::CreateForTesting(Config, Dependencies);
	if (!TestNotNull(TEXT("Core.CreateEngine should create a first test engine wrapper"), LocalEngineA.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Core.CreateEngine should create a second test engine wrapper"), LocalEngineB.Get()))
	{
		return false;
	}

	asIScriptEngine* ScriptEngineA = LocalEngineA->GetScriptEngine();
	asIScriptEngine* ScriptEngineB = LocalEngineB->GetScriptEngine();
	TestNotNull(TEXT("Core.CreateEngine should create the first asIScriptEngine for the returned wrapper"), ScriptEngineA);
	TestNotNull(TEXT("Core.CreateEngine should create the second asIScriptEngine for the returned wrapper"), ScriptEngineB);
	TestNotEqual(TEXT("Core.CreateEngine should always assign a creation mode to the first engine"), LocalEngineA->GetCreationMode(), static_cast<EAngelscriptEngineCreationMode>(255));
	TestNotEqual(TEXT("Core.CreateEngine should always assign a creation mode to the second engine"), LocalEngineB->GetCreationMode(), static_cast<EAngelscriptEngineCreationMode>(255));

	TestEqual(TEXT("Core.CreateEngine should preserve the embedded AngelScript version"), ANGELSCRIPT_VERSION, 23300);
	return ScriptEngineA != nullptr && ScriptEngineB != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerBasicTest,
	"Angelscript.TestModule.Angelscript.Core.CompilerBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASCoreCompilerBasicSimple"));
		Engine.DiscardModule(TEXT("ASCoreCompilerBasicMulti"));
		Engine.DiscardModule(TEXT("ASCoreCompilerBasicGlobals"));
		Engine.DiscardModule(TEXT("ASCoreCompilerBasicInvalid"));
	};
	const bool bCompiledSimple = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicSimple"),
		TEXT("ASCoreCompilerBasicSimple.as"),

		TEXT("void Main() { int Value = 1; }"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile a simple function"), bCompiledSimple))
	{
		return false;
	}
	const bool bCompiledMulti = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicMulti"),
		TEXT("ASCoreCompilerBasicMulti.as"),

		TEXT("void Func1() {} void Func2() {} void Func3() {}"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile a module with multiple functions"), bCompiledMulti))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> MultiModuleDesc = Engine.GetModuleByModuleName(TEXT("ASCoreCompilerBasicMulti"));
	asIScriptModule* MultiModule = MultiModuleDesc.IsValid() ? MultiModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Core.CompilerBasic should register the multi-function module"), MultiModule))
	{
		return false;
	}
	if (!TestEqual(TEXT("Core.CompilerBasic should expose all compiled functions"), static_cast<int32>(MultiModule->GetFunctionCount()), 3))
	{
		return false;
	}
	const bool bCompiledGlobals = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicGlobals"),
		TEXT("ASCoreCompilerBasicGlobals.as"),

		TEXT("const int GlobalInt = 42; const float GlobalFloat = 3.14f; void Main() {}"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile global declarations"), bCompiledGlobals))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> GlobalsModuleDesc = Engine.GetModuleByModuleName(TEXT("ASCoreCompilerBasicGlobals"));
	asIScriptModule* GlobalsModule = GlobalsModuleDesc.IsValid() ? GlobalsModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Core.CompilerBasic should register the globals module"), GlobalsModule))
	{
		return false;
	}
	if (!TestEqual(TEXT("Core.CompilerBasic should preserve both global declarations"), static_cast<int32>(GlobalsModule->GetGlobalVarCount()), 2))
	{
		return false;
	}

	ECompileResult ErrorCompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiledInvalid = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASCoreCompilerBasicInvalid"),
		TEXT("ASCoreCompilerBasicInvalid.as"),
		TEXT("void Main( { int Value = 1; }"),
		ErrorCompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Core.CompilerBasic should fail to compile invalid syntax"), bCompiledInvalid))
	{
		return false;
	}
	TestEqual(TEXT("Core.CompilerBasic should report an error compile result for invalid syntax"), ErrorCompileResult, ECompileResult::Error);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerParserTest,
	"Angelscript.TestModule.Angelscript.Core.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerParserTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASCoreParserValid"));
		Engine.DiscardModule(TEXT("ASCoreParserNested"));
		Engine.DiscardModule(TEXT("ASCoreParserInvalid"));
	};
	const bool bCompiledValid = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreParserValid"),
		TEXT("ASCoreParserValid.as"),

		TEXT("void Test() { int A = 1 + 2; bool bFlag = true && false; if (A > 0) { A = A + 1; } }"));
	if (!TestTrue(TEXT("Core.Parser should compile valid syntax constructs"), bCompiledValid))
	{
		return false;
	}
	const bool bCompiledNested = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreParserNested"),
		TEXT("ASCoreParserNested.as"),

		TEXT("void Test() { { int A = 1; { int B = 2; } } }"));
	if (!TestTrue(TEXT("Core.Parser should compile nested blocks"), bCompiledNested))
	{
		return false;
	}

	ECompileResult InvalidCompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiledInvalid = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASCoreParserInvalid"),
		TEXT("ASCoreParserInvalid.as"),
		TEXT("void Test( { int A = 1; }"),
		InvalidCompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Core.Parser should reject invalid syntax"), bCompiledInvalid))
	{
		return false;
	}
	TestEqual(TEXT("Core.Parser should report an error compile result for invalid syntax"), InvalidCompileResult, ECompileResult::Error);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerOptimizeTest,
	"Angelscript.TestModule.Angelscript.Core.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerOptimizeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASCoreOptimizeConstant"));
		Engine.DiscardModule(TEXT("ASCoreOptimizeDeadCode"));
	};
	const bool bCompiledConstant = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreOptimizeConstant"),
		TEXT("ASCoreOptimizeConstant.as"),

		TEXT("int Test() { return 1 + 2 + 3; }"));
	if (!TestTrue(TEXT("Core.Optimize should compile the constant-folding case"), bCompiledConstant))
	{
		return false;
	}

	int32 ConstantResult = 0;
	if (!TestTrue(TEXT("Core.Optimize should execute the constant-folding case"), AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("ASCoreOptimizeConstant"), TEXT("int Test()"), ConstantResult)))
	{
		return false;
	}
	TestEqual(TEXT("Core.Optimize should preserve constant-folded results"), ConstantResult, 6);
	const bool bCompiledDeadCode = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreOptimizeDeadCode"),
		TEXT("ASCoreOptimizeDeadCode.as"),

		TEXT("int Test() { int Value = 1; return Value; Value = 2; }"));
	if (!TestTrue(TEXT("Core.Optimize should compile the dead-code case"), bCompiledDeadCode))
	{
		return false;
	}

	int32 DeadCodeResult = 0;
	if (!TestTrue(TEXT("Core.Optimize should execute the dead-code case"), AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("ASCoreOptimizeDeadCode"), TEXT("int Test()"), DeadCodeResult)))
	{
		return false;
	}
	TestEqual(TEXT("Core.Optimize should keep reachable results stable when dead code is present"), DeadCodeResult, 1);
	return true;
}

#endif
