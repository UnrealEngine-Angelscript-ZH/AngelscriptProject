#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestEngineHelper.h"

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptConsoleVariableBindingsTest,
	"Angelscript.TestModule.Bindings.ConsoleVariableCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptConsoleVariableExistingBindingsTest,
	"Angelscript.TestModule.Bindings.ConsoleVariableExistingCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FString MakeConsoleVariableName(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("Angelscript.Test.%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	void UnregisterConsoleObjectIfPresent(const FString& Name)
	{
		if (IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*Name))
		{
			IConsoleManager::Get().UnregisterConsoleObject(ConsoleObject, false);
		}
	}

	bool VerifyConsoleVariableInt(FAutomationTestBase& Test, const FString& Name, int32 ExpectedValue)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!Test.TestNotNull(TEXT("Console variable test should register the int cvar"), Variable))
		{
			return false;
		}

		return Test.TestEqual(TEXT("Console variable test should preserve the int value in IConsoleManager"), Variable->GetInt(), ExpectedValue);
	}

	bool VerifyConsoleVariableFloat(FAutomationTestBase& Test, const FString& Name, float ExpectedValue)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!Test.TestNotNull(TEXT("Console variable test should register the float cvar"), Variable))
		{
			return false;
		}

		return Test.TestTrue(
			TEXT("Console variable test should preserve the float value in IConsoleManager"),
			FMath::IsNearlyEqual(Variable->GetFloat(), ExpectedValue, 0.0001f));
	}

	bool VerifyConsoleVariableBool(FAutomationTestBase& Test, const FString& Name, bool bExpectedValue)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!Test.TestNotNull(TEXT("Console variable test should register the bool cvar"), Variable))
		{
			return false;
		}

		return Test.TestEqual(TEXT("Console variable test should preserve the bool value in IConsoleManager"), Variable->GetBool(), bExpectedValue);
	}

	bool VerifyConsoleVariableString(FAutomationTestBase& Test, const FString& Name, const FString& ExpectedValue)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!Test.TestNotNull(TEXT("Console variable test should register the string cvar"), Variable))
		{
			return false;
		}

		return Test.TestEqual(TEXT("Console variable test should preserve the string value in IConsoleManager"), Variable->GetString(), ExpectedValue);
	}
}

bool FAngelscriptConsoleVariableBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();

	const FString IntName = MakeConsoleVariableName(TEXT("Int"));
	const FString FloatName = MakeConsoleVariableName(TEXT("Float"));
	const FString BoolName = MakeConsoleVariableName(TEXT("Bool"));
	const FString StringName = MakeConsoleVariableName(TEXT("String"));

	ON_SCOPE_EXIT
	{
		UnregisterConsoleObjectIfPresent(IntName);
		UnregisterConsoleObjectIfPresent(FloatName);
		UnregisterConsoleObjectIfPresent(BoolName);
		UnregisterConsoleObjectIfPresent(StringName);
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASConsoleVariableCompat",
		FString::Printf(TEXT(R"(
int Entry()
{
	FConsoleVariable IntVar("%s", 5, "Test int cvar");
	if (IntVar.GetInt() != 5)
		return 10;
	IntVar.SetInt(42);
	if (IntVar.GetInt() != 42)
		return 20;

	FConsoleVariable FloatVar("%s", 1.5f, "Test float cvar");
	float32 CurrentFloat = FloatVar.GetFloat();
	if (CurrentFloat < 1.49f || CurrentFloat > 1.51f)
		return 30;
	FloatVar.SetFloat(3.25f);
	CurrentFloat = FloatVar.GetFloat();
	if (CurrentFloat < 3.24f || CurrentFloat > 3.26f)
		return 40;

	FConsoleVariable BoolVar("%s", true, "Test bool cvar");
	if (!BoolVar.GetBool())
		return 50;
	BoolVar.SetBool(false);
	if (BoolVar.GetBool())
		return 60;

	FConsoleVariable StringVar("%s", "DefaultValue", "Test string cvar");
	if (!(StringVar.GetString() == "DefaultValue"))
		return 70;
	StringVar.SetString("UpdatedValue");
	if (!(StringVar.GetString() == "UpdatedValue"))
		return 80;

	return 1;
}
)"), *IntName, *FloatName, *BoolName, *StringName));
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

	const bool bScriptPassed = TestEqual(TEXT("Console variable compat script should exercise all supported cvar types"), Result, 1);
	const bool bIntPassed = VerifyConsoleVariableInt(*this, IntName, 42);
	const bool bFloatPassed = VerifyConsoleVariableFloat(*this, FloatName, 3.25f);
	const bool bBoolPassed = VerifyConsoleVariableBool(*this, BoolName, false);
	const bool bStringPassed = VerifyConsoleVariableString(*this, StringName, TEXT("UpdatedValue"));
	return bScriptPassed && bIntPassed && bFloatPassed && bBoolPassed && bStringPassed;
}

bool FAngelscriptConsoleVariableExistingBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	const FString ExistingName = MakeConsoleVariableName(TEXT("Existing"));
	IConsoleVariable* ExistingVariable = IConsoleManager::Get().RegisterConsoleVariable(*ExistingName, 7, TEXT("Existing native cvar for bindings test"));
	if (!TestNotNull(TEXT("Console variable existing-value test should pre-register a native cvar"), ExistingVariable))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		UnregisterConsoleObjectIfPresent(ExistingName);
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASConsoleVariableExistingCompat",
		FString::Printf(TEXT(R"(
int Entry()
{
	FConsoleVariable ExistingVar("%s", 99, "Should reuse existing native cvar");
	if (ExistingVar.GetInt() != 7)
		return 10;
	ExistingVar.SetInt(21);
	if (ExistingVar.GetInt() != 21)
		return 20;
	return 1;
}
)"), *ExistingName));
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

	const bool bScriptPassed = TestEqual(TEXT("Console variable existing-value script should reuse the already-registered cvar"), Result, 1);
	const bool bNativeValuePassed = VerifyConsoleVariableInt(*this, ExistingName, 21);
	return bScriptPassed && bNativeValuePassed;
}

#endif
