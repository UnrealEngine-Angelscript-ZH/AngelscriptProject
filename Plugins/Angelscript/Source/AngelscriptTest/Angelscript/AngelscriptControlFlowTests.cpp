#include "Angelscript/AngelscriptTestSupport.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace
{
	class FAngelscriptWarningCapture final : public FOutputDevice
	{
	public:
		void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("Angelscript") && Verbosity == ELogVerbosity::Warning)
			{
				Warnings.Add(V);
			}
		}

		bool ContainsWarningText(const FString& Needle) const
		{
			for (const FString& Warning : Warnings)
			{
				if (Warning.Contains(Needle))
				{
					return true;
				}
			}
			return false;
		}

	private:
		TArray<FString> Warnings;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowForLoopTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.ForLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowForLoopTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASControlFlowForLoop",
		TEXT("int Run() { int Sum = 0; for (int Index = 0; Index < 5; ++Index) { Sum += Index; } return Sum; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("For-loop control flow should sum the expected values"), Result, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowSwitchTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.SwitchAndConditional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowSwitchTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASControlFlowSwitch",
		TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Run() { int Base = Pick(1); return Base > 3 ? Base + 3 : Base - 1; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Switch/conditional flow should return the expected branch result"), Result, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowConditionTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.Condition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowConditionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASControlFlowCondition",
		TEXT("int Evaluate(int Value) { return (Value > 0) ? ((Value > 10) ? 2 : 1) : 0; } int Run() { return Evaluate(15) * 100 + Evaluate(5) * 10 + Evaluate(-3); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Condition control flow should preserve nested ternary branches"), Result, 210);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNeverVisitedTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.NeverVisited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNeverVisitedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASControlFlowNeverVisited"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASControlFlowNeverVisited",
		TEXT("void Run(bool bCondition) { if (bCondition) { return; } int Value = 42; }"));
	if (Module == nullptr)
	{
		return false;
	}
	if (GetFunctionByDecl(*this, *Module, TEXT("void Run(bool)")) == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("NeverVisited should compile code with a potentially unreachable block"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNotInitializedTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.NotInitialized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNotInitializedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	FAngelscriptWarningCapture WarningCapture;
	GLog->AddOutputDevice(&WarningCapture);
	ON_SCOPE_EXIT
	{
		GLog->RemoveOutputDevice(&WarningCapture);
	};

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASControlFlowNotInitialized"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASControlFlowNotInitialized",
		TEXT("int Run() { int Value; return Value; }"));
	if (Module == nullptr)
	{
		return false;
	}

	if (GetFunctionByDecl(*this, *Module, TEXT("int Run()")) == nullptr)
	{
		return false;
	}

	const bool bFoundUninitializedWarning = WarningCapture.ContainsWarningText(TEXT("may not be initialized"));

	TestTrue(TEXT("NotInitialized should preserve the compiler warning for reading an uninitialized variable"), bFoundUninitializedWarning);
	return bFoundUninitializedWarning;
}

#endif
