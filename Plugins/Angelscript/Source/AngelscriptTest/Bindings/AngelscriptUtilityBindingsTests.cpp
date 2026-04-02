#include "../Shared/AngelscriptTestUtilities.h"

#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHashBindingsTest,
	"Angelscript.TestModule.Bindings.HashCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUtilityBindingsTest,
	"Angelscript.TestModule.Bindings.UtilityCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptParseBindingsTest,
	"Angelscript.TestModule.Bindings.ParseCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRandomStreamBindingsTest,
	"Angelscript.TestModule.Bindings.RandomStreamCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStringRemoveAtBindingsTest,
	"Angelscript.TestModule.Bindings.StringRemoveAtCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHashBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASHashCompat",
		TEXT(R"(
int Entry()
{
	uint Hash32A = Hash::CityHash32("Alpha");
	uint Hash32B = Hash::CityHash32("Alpha");
	if (Hash32A != Hash32B)
		return 10;

	uint64 Hash64A = Hash::CityHash64("Alpha");
	uint64 Hash64B = Hash::CityHash64("Alpha");
	if (Hash64A != Hash64B)
		return 20;

	uint64 SeededA = Hash::CityHash64WithSeed("Alpha", 123);
	uint64 SeededB = Hash::CityHash64WithSeed("Alpha", 123);
	if (SeededA != SeededB)
		return 30;

	uint64 SeededC = Hash::CityHash64WithSeeds("Alpha", 1, 2);
	uint64 SeededD = Hash::CityHash64WithSeeds("Alpha", 1, 2);
	if (SeededC != SeededD)
		return 40;

	if (Hash64A == SeededA)
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

	TestEqual(TEXT("Hash compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptUtilityBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();

	TArray<FString> ExpectedTokens;
	TArray<FString> ExpectedSwitches;
	FCommandLine::Parse(TEXT("-foo Alpha Beta"), ExpectedTokens, ExpectedSwitches);
	const FString ProjectName = FApp::GetProjectName();

	const FString Script = FString::Printf(
		TEXT(R"(
int Entry()
{
	FString CmdLine = FCommandLine::Get();
	if (CmdLine.IsEmpty())
		return 10;

	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse("-foo Alpha Beta", Tokens, Switches);
	if (Tokens.Num() != %d)
		return 20;
	if (Switches.Num() != %d)
		return 30;
	if (Switches.Num() > 0 && !(Switches[0] == "foo"))
		return 40;

	FString RuntimeProjectName = FApp::GetProjectName();
	if (!(RuntimeProjectName == "%s"))
		return 50;

	FString PathEnv = FPlatformMisc::GetEnvironmentVariable("PATH");
	if (PathEnv.IsEmpty())
		return 60;

	return 1;
}
)"),
		ExpectedTokens.Num(),
		ExpectedSwitches.Num(),
		*ProjectName);

	asIScriptModule* Module = BuildModule(*this, Engine, "ASUtilityCompat", Script);
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

	TestEqual(TEXT("Utility compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptParseBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASParseCompat",
		TEXT(R"(
int Entry()
{
	FString Source = "Count=12 Ratio=3.5 Name=Alpha Enabled=true";

	int Count = 0;
	if (!FParse::Value(Source, "Count=", Count))
		return 10;
	if (Count != 12)
		return 20;

	float32 Ratio = 0.0f;
	if (!FParse::Value(Source, "Ratio=", Ratio))
		return 30;
	if (Ratio != 3.5f)
		return 40;

	FString Name;
	if (!FParse::Value(Source, "Name=", Name))
		return 50;
	if (!(Name == "Alpha"))
		return 60;

	bool bEnabled = false;
	if (!FParse::Bool(Source, "Enabled=", bEnabled))
		return 70;
	if (!bEnabled)
		return 80;

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

	TestEqual(TEXT("Parse compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptRandomStreamBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASRandomStreamCompat",
		TEXT(R"(
int Entry()
{
	FRandomStream Stream(123);
	if (Stream.GetInitialSeed() != 123)
		return 10;

	int FirstValue = Stream.RandRange(1, 1000);
	Stream.Reset();
	int SecondValue = Stream.RandRange(1, 1000);
	if (FirstValue != SecondValue)
		return 20;

	if (Stream.GetCurrentSeed() == 0)
		return 30;

	float Fraction = Stream.GetFraction();
	if (Fraction < 0.0 || Fraction > 1.0)
		return 40;

	double DoubleValue = Stream.RandRange(0.0, 10.0);
	if (DoubleValue < 0.0 || DoubleValue > 10.0)
		return 50;

	FRandomStream Copy = Stream;
	if (Copy.GetCurrentSeed() != Stream.GetCurrentSeed())
		return 60;

	Stream.GenerateNewSeed();
	if (Stream.GetCurrentSeed() == 0)
		return 70;
	if (Stream.ToString().IsEmpty())
		return 80;

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

	TestEqual(TEXT("RandomStream compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptStringRemoveAtBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASStringRemoveAtCompat",
		TEXT(R"(
int Entry()
{
	FString Value = "ABCDE";
	Value.RemoveAt(1, 2);
	if (!(Value == "ADE"))
		return 10;

	Value.RemoveAt(0, 1);
	if (!(Value == "DE"))
		return 20;

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

	TestEqual(TEXT("FString RemoveAt(Index, Count) should behave as expected"), Result, 1);
	return true;
}

#endif
