#include "AngelscriptTestAdapter.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptASSDKEngineCreateTest,
	"Angelscript.TestModule.Native.ASSDK.Engine.Create",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptASSDKEngineCreateTest::RunTest(const FString& Parameters)
{
	FASSDKBufferedOutStream BufferedOutStream;
	asIScriptEngine* PrimaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if (!TestNotNull(TEXT("ASSDK engine-create test should create the primary engine"), PrimaryEngine))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (PrimaryEngine != nullptr)
		{
			PrimaryEngine->ShutDownAndRelease();
		}
	};

	const int PrimaryCallbackResult = PrimaryEngine->SetMessageCallback(
		asMETHODPR(FASSDKBufferedOutStream, Callback, (asSMessageInfo*), void),
		&BufferedOutStream,
		asCALL_THISCALL);
	if (!TestEqual(TEXT("ASSDK engine-create test should install the primary engine callback"), PrimaryCallbackResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	asIScriptEngine* SecondaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if (!TestNotNull(TEXT("ASSDK engine-create test should create the secondary engine"), SecondaryEngine))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (SecondaryEngine != nullptr)
		{
			SecondaryEngine->ShutDownAndRelease();
		}
	};

	asSFuncPtr MessageCallback;
	void* CallbackObject = nullptr;
	asDWORD CallConv = 0;
	const int GetCallbackResult = PrimaryEngine->GetMessageCallback(&MessageCallback, &CallbackObject, &CallConv);
	if (!TestEqual(TEXT("ASSDK engine-create test should read back the primary callback"), GetCallbackResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	const int ReuseCallbackResult = SecondaryEngine->SetMessageCallback(MessageCallback, CallbackObject, CallConv);
	if (!TestEqual(TEXT("ASSDK engine-create test should reuse the primary callback on the secondary engine"), ReuseCallbackResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	const int WriteMessageResult = SecondaryEngine->WriteMessage("test", 0, 0, asMSGTYPE_INFORMATION, "Hello from engine2");
	if (!TestEqual(TEXT("ASSDK engine-create test should emit a callback message from the secondary engine"), WriteMessageResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	TestTrue(TEXT("ASSDK engine-create test should preserve the upstream callback payload"), BufferedOutStream.Buffer.find("Hello from engine2") != std::string::npos);
	TestTrue(TEXT("ASSDK engine-create test should preserve the upstream callback section"), BufferedOutStream.Buffer.find("test (0, 0)") != std::string::npos);
	return true;
}

#endif
