#include "AngelscriptTestAdapter.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptASSDKSmokeTest,
	"Angelscript.TestModule.Native.ASSDK.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptASSDKSmokeTest::RunTest(const FString& Parameters)
{
	FAngelscriptSDKTestAdapter Adapter(*this);
	FASSDKBufferedOutStream BufferedOutStream;
	asIScriptEngine* ScriptEngine = CreateASSDKTestEngine(Adapter, &BufferedOutStream);
	if (!TestNotNull(TEXT("ASSDK smoke test should create a standalone script engine"), ScriptEngine))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
	};

	ScriptEngine->WriteMessage("ASSDKSmoke", 0, 0, asMSGTYPE_INFORMATION, "Smoke callback ready");
	if (!TestTrue(TEXT("ASSDK smoke test should capture engine callback messages"), BufferedOutStream.Buffer.find("Smoke callback ready") != std::string::npos))
	{
		return false;
	}

	const int ExecuteResult = ASSDKExecuteString(
		ScriptEngine,
		"bool ExecuteSmoke() { assert(true); return true; }");

	if (!TestEqual(TEXT("ASSDK smoke test should compile and execute a simple snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		return false;
	}

	TestFalse(TEXT("ASSDK smoke test should not latch an adapter failure for Assert(true)"), Adapter.bFailed);
	return !Adapter.bFailed;
}

#endif
