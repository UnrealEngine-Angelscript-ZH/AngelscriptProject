#include "../Shared/AngelscriptTestUtilities.h"
#include "../../AngelscriptRuntime/Binds/Bind_TMap.h"
#include "../../AngelscriptRuntime/Binds/Bind_TOptional.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSetCompareBindingsTest,
	"Angelscript.TestModule.Bindings.SetCompareCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMapCompareBindingsTest,
	"Angelscript.TestModule.Bindings.MapCompareCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOptionalTypeCompareTest,
	"Angelscript.TestModule.Bindings.OptionalTypeCompareCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMapDebuggerBindingsTest,
	"Angelscript.TestModule.Bindings.MapDebuggerCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptSetCompareBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSetCompareCompat",
		TEXT(R"(
int Entry()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> Right;
	Right.Add(4);
	Right.Add(1);

	if (!(Left == Right))
		return 10;

	Right.Add(7);
	if (Left == Right)
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

	TestEqual(TEXT("TSet compare operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptMapCompareBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMapCompareCompat",
		TEXT(R"(
int Entry()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> Right;
	Right.Add(FName("Beta"), 5);
	Right.Add(FName("Alpha"), 2);

	if (!(Left == Right))
		return 10;

	Right.Add(FName("Gamma"), 7);
	if (Left == Right)
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

	TestEqual(TEXT("TMap compare operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptOptionalTypeCompareTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	static_cast<void>(Engine);

	FAngelscriptTypeUsage IntUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
	FAngelscriptTypeUsage OptionalUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TOptional")));
	OptionalUsage.SubTypes.Add(IntUsage);

	if (!TestTrue(TEXT("TOptional<int> should report type-level compare support"), OptionalUsage.CanCompare()))
	{
		return false;
	}

	const SIZE_T OptionalSize = static_cast<SIZE_T>(OptionalUsage.GetValueSize());
	const uint32 OptionalAlignment = static_cast<uint32>(OptionalUsage.GetValueAlignment());

	void* LeftStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
	void* RightStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
	ON_SCOPE_EXIT
	{
		FMemory::Free(LeftStorage);
		FMemory::Free(RightStorage);
	};

	OptionalUsage.ConstructValue(LeftStorage);
	OptionalUsage.ConstructValue(RightStorage);

	FOptionalOperations OptionalOps(IntUsage);
	FAngelscriptOptional& LeftOptional = *static_cast<FAngelscriptOptional*>(LeftStorage);
	FAngelscriptOptional& RightOptional = *static_cast<FAngelscriptOptional*>(RightStorage);

	if (!TestTrue(TEXT("Two unset optionals should compare equal"), OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional)))
	{
		OptionalUsage.DestructValue(LeftStorage);
		OptionalUsage.DestructValue(RightStorage);
		return false;
	}

	int32 LeftValue = 7;
	int32 RightValue = 7;
	OptionalOps.Set(LeftOptional, &LeftValue);
	OptionalOps.Set(RightOptional, &RightValue);

	if (!TestTrue(TEXT("Two equal set optionals should compare equal"), OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional)))
	{
		OptionalUsage.DestructValue(LeftStorage);
		OptionalUsage.DestructValue(RightStorage);
		return false;
	}

	RightValue = 9;
	OptionalOps.Set(RightOptional, &RightValue);
	if (!TestFalse(TEXT("Different set optionals should compare unequal"), OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional)))
	{
		OptionalUsage.DestructValue(LeftStorage);
		OptionalUsage.DestructValue(RightStorage);
		return false;
	}

	OptionalOps.Reset(RightOptional);
	const bool bSetVsUnsetEqual = OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional);
	OptionalUsage.DestructValue(LeftStorage);
	OptionalUsage.DestructValue(RightStorage);

	TestFalse(TEXT("Set and unset optionals should compare unequal"), bSetVsUnsetEqual);
	return true;
}

bool FAngelscriptMapDebuggerBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptTypeUsage KeyUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("FName")));
	FAngelscriptTypeUsage ValueUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
	FAngelscriptTypeUsage MapUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TMap")));
	MapUsage.SubTypes.Add(KeyUsage);
	MapUsage.SubTypes.Add(ValueUsage);
	MapUsage.TypeIndex = 0;

	FScriptMap TestMap;
	FMapOperations MapOps(KeyUsage, ValueUsage);
	FName AlphaName(TEXT("Alpha"));
	FName BetaName(TEXT("Beta"));
	int32 AlphaValue = 2;
	int32 BetaValue = 5;
	MapOps.Add(TestMap, &AlphaName, &AlphaValue);
	MapOps.Add(TestMap, &BetaName, &BetaValue);

	FDebuggerValue SummaryValue;
	if (!TestTrue(TEXT("TMap debugger summary should be available"), MapUsage.GetDebuggerValue(&TestMap, SummaryValue)))
	{
		MapOps.Empty(TestMap, 0);
		return false;
	}
	TestEqual(TEXT("TMap debugger summary should show element count"), SummaryValue.Value, FString(TEXT("Num = 2")));
	TestTrue(TEXT("TMap debugger summary should report child members"), SummaryValue.bHasMembers);

	FDebuggerValue NumValue;
	if (!TestTrue(TEXT("TMap debugger should expose Num member"), MapUsage.GetDebuggerMember(&TestMap, TEXT("Num"), NumValue)))
	{
		MapOps.Empty(TestMap, 0);
		return false;
	}
	TestEqual(TEXT("TMap debugger Num member should match element count"), NumValue.Value, FString(TEXT("2")));

	FDebuggerValue AlphaDebugValue;
	const bool bAlphaFound = MapUsage.GetDebuggerMember(&TestMap, TEXT("[Alpha]"), AlphaDebugValue);
	MapOps.Empty(TestMap, 0);
	if (!TestTrue(TEXT("TMap debugger should expose FName-keyed members by string identifier"), bAlphaFound))
	{
		return false;
	}

	TestEqual(TEXT("TMap debugger key lookup should return the mapped value"), AlphaDebugValue.Value, FString(TEXT("2")));
	return true;
}

#endif
