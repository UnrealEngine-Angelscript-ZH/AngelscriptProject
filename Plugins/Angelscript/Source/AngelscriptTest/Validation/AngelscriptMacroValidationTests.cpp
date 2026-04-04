#include "../Shared/AngelscriptTestMacros.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

// Ported from AngelscriptGlobalBindingsTests.cpp using the new ANGELSCRIPT_TEST macro
ANGELSCRIPT_TEST(
	FAngelscriptGlobalBindingsMacroValidationTest,
	"Angelscript.TestModule.Validation.GlobalBindingsMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASGlobalVariableCompatMacro",
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

	return TestEqual(TEXT("Global variable compat operations via macro should preserve bound namespace globals and defaults"), Result, 1);
}

#endif
