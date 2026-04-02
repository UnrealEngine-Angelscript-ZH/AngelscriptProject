#include "AngelscriptScriptExampleTestSupport.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptScriptExamples
{
	const FScriptExampleSource& GetScriptExampleEnumSource()
	{
		static const FScriptExampleSource Example = {
			TEXT("Example_Enum.as"),
			TEXT(R"ANGELSCRIPT(/*
 * All enums from angelscript get bound to unreal by default,
 * nothing special is needed.
 */
enum EExampleEnum
{
	A,
	B,
	C
};

/*
 * Enums can of course be taken as arguments to functions,
 * set as properties, whatever.
 */
UFUNCTION()
void TestExampleEnum(EExampleEnum Input)
{
	switch (Input)
	{
	case EExampleEnum::A:
		Print("You selected A!", Duration=30);
	break;
	case EExampleEnum::B:
		Print("You shouldn't select B.", Duration=30);
	break;
	case EExampleEnum::C:
		Print("What is this even?", Duration=30);
	break;
	}

	// You can cast an int to the enum thus:
	EExampleEnum NumberAsEnum = EExampleEnum(0);
})ANGELSCRIPT"),
			nullptr,
			nullptr,
		};

		return Example;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAngelscriptScriptExampleEnumTest, "Angelscript.TestModule.ScriptExamples.Enum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScriptExampleEnumTest::RunTest(const FString& Parameters)
{
	return AngelscriptScriptExamples::RunScriptExampleCompileTest(*this, AngelscriptScriptExamples::GetScriptExampleEnumSource());
}

#endif
