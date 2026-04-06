#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGlobalVariableBindingsTest,
	"Angelscript.TestModule.Bindings.GlobalVariableCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGlobalVariableBindingsTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASGlobalVariableCompat",
		TEXT(R"(
int Entry()
{
	if (CollisionProfile::BlockAllDynamic.Compare(FName("BlockAllDynamic")) != 0)
		return 10;

	FComponentQueryParams FreshParams;
	if (FComponentQueryParams::DefaultComponentQueryParams.ShapeCollisionMask.Bits != FreshParams.ShapeCollisionMask.Bits)
		return 20;

	FGameplayTag EmptyTagCopy = FGameplayTag::EmptyTag;
	if (EmptyTagCopy.IsValid())
		return 30;
	if (!FGameplayTagContainer::EmptyContainer.IsEmpty())
		return 40;
	if (!FGameplayTagQuery::EmptyQuery.IsEmpty())
		return 50;

	return 1;
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

	bPassed = TestEqual(TEXT("Global variable compat operations should preserve bound namespace globals and defaults"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return bPassed;
}

#endif
