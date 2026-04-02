#include "../Shared/AngelscriptTestUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UScriptStruct* BuildScriptStruct(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName, const FString& Source, const char* TypeName)
	{
		asIScriptModule* Module = AngelscriptTestSupport::BuildModule(Test, Engine, ModuleName, Source);
		if (Module == nullptr)
		{
			return nullptr;
		}

		FString UnrealName = UTF8_TO_TCHAR(TypeName);
		if (UnrealName.Len() >= 2 && UnrealName[0] == 'F' && FChar::IsUpper(UnrealName[1]))
		{
			UnrealName.RightChopInline(1, EAllowShrinking::No);
		}

		UScriptStruct* Struct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *UnrealName);
		if (!Test.TestNotNull(TEXT("Compiled script struct should have a backing UScriptStruct"), Struct))
		{
			return nullptr;
		}
		return Struct;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStructNotBlueprintTypeByDefaultTest,
	"Angelscript.TestModule.Internals.StructCppOps.NotBlueprintTypeByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStructNotBlueprintTypeByDefaultTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetResetSharedTestEngine();
	UScriptStruct* Struct = BuildScriptStruct(
		*this,
		Engine,
		"StructCppOpsScopeModule",
		TEXT(R"ANGELSCRIPT(
struct FScopeConstructStruct
{
	int Value = 7;
}
)ANGELSCRIPT"),
		"FScopeConstructStruct");
	if (Struct == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("Script structs should not be BlueprintType by default"), Struct->GetBoolMetaData(TEXT("BlueprintType")));
	return !Struct->GetBoolMetaData(TEXT("BlueprintType"));
}

#endif
