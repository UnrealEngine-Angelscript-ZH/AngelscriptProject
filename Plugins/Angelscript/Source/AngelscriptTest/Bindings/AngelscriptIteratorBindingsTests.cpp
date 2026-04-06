#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSetIteratorBindingsTest,
	"Angelscript.TestModule.Bindings.SetIteratorCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMapIteratorBindingsTest,
	"Angelscript.TestModule.Bindings.MapIteratorCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptSetIteratorBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASSetIteratorCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSetIteratorCompat",
		TEXT(R"(
int Entry()
{
	TSet<int> Values;
	Values.Add(2);
	Values.Add(5);

	TSetIterator<int> It = Values.Iterator();
	int Sum = 0;
	while (It.CanProceed)
	{
		Sum += It.Proceed();
	}

	return Sum == 7 ? 1 : 10;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("TSet iterator helpers should behave as expected"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptMapIteratorBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASMapIteratorCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMapIteratorCompat",
		TEXT(R"(
int Entry()
{
	TMap<FName, int> Values;
	Values.Add(FName("Alpha"), 2);
	Values.Add(FName("Beta"), 5);

	TMapIterator<FName, int> It = Values.Iterator();
	int Sum = 0;
	int KeyCount = 0;
	while (It.CanProceed)
	{
		It.Proceed();
		if (It.GetKey() == FName("Alpha") || It.GetKey() == FName("Beta"))
			KeyCount += 1;
		Sum += It.GetValue();
	}

	return (Sum == 7 && KeyCount == 2) ? 1 : 10;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("TMap iterator helpers should behave as expected"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

#endif
