#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestEngineHelper.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGuidBindingsTest,
	"Angelscript.TestModule.Bindings.GuidCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPathsBindingsTest,
	"Angelscript.TestModule.Bindings.PathsCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNumberFormattingOptionsBindingsTest,
	"Angelscript.TestModule.Bindings.NumberFormattingOptionsCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGuidBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASGuidCompat",
		TEXT(R"(
int Entry()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	if (!ExplicitGuid.IsValid())
		return 10;

	FString GuidString = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
	if (GuidString.IsEmpty())
		return 20;

	FGuid ParsedGuid;
	if (!FGuid::Parse(GuidString, ParsedGuid))
		return 30;
	if (!(ParsedGuid == ExplicitGuid))
		return 40;
	if (ParsedGuid.opCmp(ExplicitGuid) != 0)
		return 50;

	FGuid ParsedExactGuid;
	if (!FGuid::ParseExact(GuidString, EGuidFormats::DigitsWithHyphens, ParsedExactGuid))
		return 60;
	if (!(ParsedExactGuid == ExplicitGuid))
		return 70;

	FGuid Copy = ExplicitGuid;
	if (!(Copy == ExplicitGuid))
		return 80;

	Copy.Invalidate();
	if (Copy.IsValid())
		return 90;

	FGuid NewGuid = FGuid::NewGuid();
	if (!NewGuid.IsValid())
		return 100;
	if (NewGuid.GetTypeHash() == 0)
		return 110;

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

	TestEqual(TEXT("Guid compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptPathsBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASPathsCompat",
		TEXT(R"(
int Entry()
{
	FString ProjectDir = FPaths::ProjectDir();
	if (ProjectDir.IsEmpty())
		return 10;

	FString Combined = FPaths::CombinePaths(ProjectDir, "Script/Test.as");
	if (Combined.IsEmpty())
		return 20;

	FString Relative = "Script/Test.as";
	if (!FPaths::IsRelative(Relative))
		return 30;

	FString FullPath = FPaths::ConvertRelativePathToFull(Relative);
	if (FullPath.IsEmpty())
		return 40;

	FString FullFromBase = FPaths::ConvertRelativePathToFull(ProjectDir, Relative);
	if (FullFromBase.IsEmpty())
		return 50;

	FString Extension = FPaths::GetExtension(Combined, true);
	if (!(Extension == ".as"))
		return 60;

	FString Clean = FPaths::GetCleanFilename(Combined);
	if (!(Clean == "Test.as"))
		return 70;

	FString Base = FPaths::GetBaseFilename(Combined, true);
	if (!(Base == "Test"))
		return 80;

	FString PathOnly = FPaths::GetPath(Combined);
	if (PathOnly.IsEmpty())
		return 90;

	if (!FPaths::DirectoryExists(ProjectDir))
		return 100;
	if (FPaths::FileExists(ProjectDir))
		return 110;

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

	TestEqual(TEXT("Paths compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptNumberFormattingOptionsBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASNumberFormattingOptionsCompat",
		TEXT(R"(
int Entry()
{
	FNumberFormattingOptions Options;
	Options.SetAlwaysSign(true)
		.SetUseGrouping(false)
		.SetMinimumIntegralDigits(2)
		.SetMaximumIntegralDigits(4)
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(3);

	FNumberFormattingOptions Copy = Options;
	if (!Options.IsIdentical(Copy))
		return 10;
	if (!(Options.GetTypeHash() == Copy.GetTypeHash()))
		return 20;

	FNumberFormattingOptions DefaultGrouped = FNumberFormattingOptions::DefaultWithGrouping();
	FNumberFormattingOptions DefaultUngrouped = FNumberFormattingOptions::DefaultNoGrouping();
	if (DefaultGrouped.IsIdentical(DefaultUngrouped))
		return 30;

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

	TestEqual(TEXT("NumberFormattingOptions compat operations should behave as expected"), Result, 1);
	return true;
}

#endif
