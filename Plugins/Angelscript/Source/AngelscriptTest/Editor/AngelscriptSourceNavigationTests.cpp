#include "../Shared/AngelscriptTestEngineHelper.h"
#include "../Shared/AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SourceCodeNavigation.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionSourceNavigationTest,
	"Angelscript.TestModule.Editor.SourceNavigation.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionSourceNavigationTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(*this, TEXT("Source navigation tests require a production engine."));
	if (ProductionEngine == nullptr)
	{
		return false;
	}

	FAngelscriptEngine& Engine = *ProductionEngine;

	const FString Script = TEXT(R"AS(
UCLASS()
class UFunctionNavigationCarrier : UObject
{
	UFUNCTION()
	int ComputeValue()
	{
		return 7;
	}
}
)AS");
	const FString RelativeScriptPath = TEXT("Automation/RuntimeFunctionNavigationTest.as");
	const FString ScriptPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeScriptPath);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("Automation.RuntimeFunctionNavigationTest"));
	};
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("RuntimeFunctionNavigationTest"),
		ScriptPath,

		Script);
	if (!TestTrue(TEXT("Compile annotated function navigation module should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UFunctionNavigationCarrier"));
	if (!TestNotNull(TEXT("Generated function navigation class should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* RuntimeFunction = FindGeneratedFunction(RuntimeClass, TEXT("ComputeValue"));
	if (!TestNotNull(TEXT("Generated function navigation function should exist"), RuntimeFunction))
	{
		return false;
	}

	UASFunction* RuntimeASFunction = Cast<UASFunction>(RuntimeFunction);
	if (!TestNotNull(TEXT("Generated function should materialize as UASFunction for source navigation"), RuntimeASFunction))
	{
		return false;
	}

	TestEqual(TEXT("Generated function should preserve source file path"), RuntimeASFunction->GetSourceFilePath(), ScriptPath);
	TestEqual(TEXT("Generated function should preserve source line number"), RuntimeASFunction->GetSourceLineNumber(), 6);
	TestTrue(TEXT("Source navigation should recognize generated script class"), FSourceCodeNavigation::CanNavigateToClass(RuntimeClass));
	TestTrue(TEXT("Source navigation should recognize generated script function"), FSourceCodeNavigation::CanNavigateToFunction(RuntimeFunction));

	return true;
}

#endif
