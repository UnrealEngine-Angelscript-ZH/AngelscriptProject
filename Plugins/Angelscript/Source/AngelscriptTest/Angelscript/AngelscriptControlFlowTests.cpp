#include "Angelscript/AngelscriptTestSupport.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace
{
	bool ContainsWarningDiagnostic(const FAngelscriptEngine& Engine, const FString& Needle)
	{
		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& Pair : Engine.Diagnostics)
		{
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Pair.Value.Diagnostics)
			{
				if (!Diagnostic.bIsError && Diagnostic.Message.Contains(Needle))
				{
					return true;
				}
			}
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowForLoopTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.ForLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowForLoopTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowForLoop",
		TEXT("int Run() { int Sum = 0; for (int Index = 0; Index < 5; ++Index) { Sum += Index; } return Sum; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("For-loop control flow should sum the expected values"), Result, 10);

	ASTEST_END_FULL
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowSwitchTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.SwitchAndConditional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowSwitchTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowSwitch",
		TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Run() { int Base = Pick(1); return Base > 3 ? Base + 3 : Base - 1; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Switch/conditional flow should return the expected branch result"), Result, 7);

	ASTEST_END_FULL
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowConditionTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.Condition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowConditionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowCondition",
		TEXT("int Evaluate(int Value) { return (Value > 0) ? ((Value > 10) ? 2 : 1) : 0; } int Run() { return Evaluate(15) * 100 + Evaluate(5) * 10 + Evaluate(-3); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Condition control flow should preserve nested ternary branches"), Result, 210);

	ASTEST_END_FULL
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNeverVisitedTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.NeverVisited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNeverVisitedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	asIScriptModule* Module = nullptr;
	ASTEST_BUILD_MODULE(Engine, "ASControlFlowNeverVisited",
		TEXT("void Run(bool bCondition) { if (bCondition) { return; } int Value = 42; }"),
		Module);

	if (GetFunctionByDecl(*this, *Module, TEXT("void Run(bool)")) == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("NeverVisited should compile code with a potentially unreachable block"), true);

	ASTEST_END_FULL
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNotInitializedTest,
	"Angelscript.TestModule.Angelscript.ControlFlow.NotInitialized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNotInitializedTest::RunTest(const FString& Parameters)
{
	bool bFoundUninitializedWarning = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	asIScriptModule* Module = nullptr;
	ASTEST_BUILD_MODULE(Engine, "ASControlFlowNotInitialized",
		TEXT("int Run() { int Value; return Value; }"),
		Module);

	if (GetFunctionByDecl(*this, *Module, TEXT("int Run()")) == nullptr)
	{
		return false;
	}

	bFoundUninitializedWarning = ContainsWarningDiagnostic(Engine, TEXT("may not be initialized"));

	TestTrue(TEXT("NotInitialized should preserve the compiler warning for reading an uninitialized variable"), bFoundUninitializedWarning);

	ASTEST_END_FULL
	return bFoundUninitializedWarning;
}

#endif
