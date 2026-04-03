#include "Angelscript/AngelscriptTestSupport.h"
#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerBytecodeGenerationTest,
	"Angelscript.TestModule.Internals.Compiler.BytecodeGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerVariableScopeTest,
	"Angelscript.TestModule.Internals.Compiler.VariableScopes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerFunctionCallTest,
	"Angelscript.TestModule.Internals.Compiler.FunctionCalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerTypeConversionTest,
	"Angelscript.TestModule.Internals.Compiler.TypeConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerBytecodeGenerationTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
	asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
		*this,
		Engine,
		"CompilerBytecodeGeneration",
		TEXT("int Entry() { int A = 1; int B = 2; return A + B; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = AngelscriptTestSupport::GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	asUINT BytecodeLength = 0;
	asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	TestNotNull(TEXT("Compiled function should expose a bytecode buffer"), Bytecode);
	TestTrue(TEXT("Compiled function should emit at least one bytecode instruction"), BytecodeLength > 0);
	return Bytecode != nullptr && BytecodeLength > 0;
}

bool FAngelscriptCompilerVariableScopeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
	asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
		*this,
		Engine,
		"CompilerVariableScopes",
		TEXT("int Entry() { int Outer = 1; { int Inner = 2; Outer += Inner; } return Outer; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = AngelscriptTestSupport::GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("Compiled function should report local variables for scoped declarations"), Function->GetVarCount() >= 2);

	const char* FirstVarName = nullptr;
	Function->GetVar(0, &FirstVarName, nullptr);
	TestNotNull(TEXT("Compiler should record the first local variable name"), FirstVarName);
	return Function->GetVarCount() >= 2;
}

bool FAngelscriptCompilerFunctionCallTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
	asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
		*this,
		Engine,
		"CompilerFunctionCalls",
		TEXT("int Add(int A, int B) { return A + B; } int Entry() { return Add(7, 5); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = AngelscriptTestSupport::GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!AngelscriptTestSupport::ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Compiler should generate callable bytecode for function invocations"), Result, 12);
	return true;
}

bool FAngelscriptCompilerTypeConversionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
	asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
		*this,
		Engine,
		"CompilerTypeConversions",
		TEXT("float32 Entry() { int Value = 7; return float32(Value); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = AngelscriptTestSupport::GetFunctionByDecl(*this, *Module, TEXT("float32 Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Compiler conversion test should create a script context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
	const float Result = Context->GetReturnFloat();
	Context->Release();

	TestEqual(TEXT("Compiler conversion test should prepare successfully"), PrepareResult, asSUCCESS);
	TestEqual(TEXT("Compiler conversion test should execute successfully"), ExecuteResult, asEXECUTION_FINISHED);
	TestTrue(TEXT("Compiler should emit a numeric conversion that preserves the value"), FMath::IsNearlyEqual(Result, 7.0f));
	return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED && FMath::IsNearlyEqual(Result, 7.0f);
}

#endif
