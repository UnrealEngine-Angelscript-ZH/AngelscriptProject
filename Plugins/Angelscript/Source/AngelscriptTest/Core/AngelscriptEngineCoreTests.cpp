#include "AngelscriptEngine.h"
#include "Angelscript/AngelscriptTestSupport.h"
#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestModuleLifecycleTest,
	"Angelscript.TestModule.Engine.CreateDestroy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestModuleCompileSnippetTest,
	"Angelscript.TestModule.Engine.CompileSnippet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestModuleExecuteSnippetTest,
	"Angelscript.TestModule.Engine.ExecuteSnippet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTestModuleLifecycleTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngineConfig Config;
	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::CreateForTesting(Config, Dependencies);
	if (!TestNotNull(TEXT("Test module should create an angelscript engine instance"), Engine.Get()))
	{
		return false;
	}

	Engine.Reset();
	TestTrue(TEXT("Resetting the test-owned engine should clear the pointer"), !Engine.IsValid());
	return true;
}

bool FAngelscriptTestModuleCompileSnippetTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetOrCreateSharedCloneEngine();
	if (!TestNotNull(TEXT("Compile test should create an initialized engine"), &Engine))
	{
		return false;
	}

	asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("CompileSnippet", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Compile test should create a script module"), Module))
	{
		return false;
	}

	const char* Source = "int CompileOnly() { return 7; }";
	asIScriptFunction* Function = nullptr;
	const int CompileResult = Module->CompileFunction("CompileSnippet", Source, 0, 0, &Function);
	TestEqual(TEXT("Compile test should compile the snippet successfully"), CompileResult, asSUCCESS);
	const bool bCompiled = TestNotNull(TEXT("Compile test should receive a compiled function"), Function);
	if (Function != nullptr)
	{
		Function->Release();
	}
	return CompileResult == asSUCCESS && bCompiled;
}

bool FAngelscriptTestModuleExecuteSnippetTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetOrCreateSharedCloneEngine();
	if (!TestNotNull(TEXT("Execute test should create an initialized engine"), &Engine))
	{
		return false;
	}

	asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("ExecuteSnippet", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Execute test should create a script module"), Module))
	{
		return false;
	}

	const char* Source = "int ReturnFortyTwo() { return 42; }";
	asIScriptFunction* Function = nullptr;
	const int CompileResult = Module->CompileFunction("ExecuteSnippet", Source, 0, 0, &Function);
	if (!TestEqual(TEXT("Execute test should compile the snippet successfully"), CompileResult, asSUCCESS))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Execute test should find the compiled function"), Function))
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execute test should create a script context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
	TestEqual(TEXT("Execute test should prepare the function successfully"), PrepareResult, asSUCCESS);
	TestEqual(TEXT("Execute test should finish successfully"), ExecuteResult, asEXECUTION_FINISHED);
	TestEqual(TEXT("Execute test should receive the script return value"), static_cast<int>(Context->GetReturnDWord()), 42);
	Context->Release();
	Function->Release();
	return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
}

#endif
