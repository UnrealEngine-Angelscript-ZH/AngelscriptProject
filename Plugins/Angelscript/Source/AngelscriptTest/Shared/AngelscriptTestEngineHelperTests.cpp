#include "Shared/AngelscriptTestEngineHelper.h"

#include "Shared/AngelscriptNativeScriptTestObject.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperCompileModuleTest,
	"Angelscript.TestModule.Shared.EngineHelper.CompileModuleFromMemory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTestEngineHelperCompileModuleTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperCompileModule"));
	};

	const bool bCompiled = AngelscriptTestSupport::CompileModuleFromMemory(
		&Engine,
		TEXT("HelperCompileModule"),
		TEXT("HelperCompileModule.as"),
		TEXT("int Entry() { return 42; }"));

	TestTrue(TEXT("CompileModuleFromMemory should compile a trivial module"), bCompiled);
	return bCompiled;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperExecuteIntFunctionTest,
	"Angelscript.TestModule.Shared.EngineHelper.ExecuteIntFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTestEngineHelperExecuteIntFunctionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperExecuteInt"));
	};

	const bool bCompiled = AngelscriptTestSupport::CompileModuleFromMemory(
		&Engine,
		TEXT("HelperExecuteInt"),
		TEXT("HelperExecuteInt.as"),
		TEXT("int DoubleValue(int Value) { return Value * 2; } int Entry() { return DoubleValue(21); }"));
	if (!TestTrue(TEXT("ExecuteIntFunction test module compiles"), bCompiled))
	{
		return false;
	}

	int32 Result = 0;
	const bool bExecuted = AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("HelperExecuteInt"), TEXT("int Entry()"), Result);
	if (!TestTrue(TEXT("ExecuteIntFunction should execute the compiled entry point"), bExecuted))
	{
		return false;
	}

	TestEqual(TEXT("ExecuteIntFunction returns the expected result"), Result, 42);
	return Result == 42;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperGeneratedSymbolLookupTest,
	"Angelscript.TestModule.Shared.EngineHelper.GeneratedSymbolLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperFailedAnnotatedIsolationTest,
	"Angelscript.TestModule.Shared.EngineHelper.FailedAnnotatedModuleDoesNotPolluteLaterCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperSharedEngineNeverAttachesToProductionTest,
	"Angelscript.TestModule.Shared.EngineHelper.SharedTestEngineNeverSilentlyAttachesToProductionEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperProductionHelperRejectsMissingProductionTest,
	"Angelscript.TestModule.Shared.EngineHelper.ProductionHelperRejectsMissingProductionEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperCompileRestoresScopedGlobalEngineTest,
	"Angelscript.TestModule.Shared.EngineHelper.CompileUsesScopedGlobalEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperNestedGlobalScopeRestoreTest,
	"Angelscript.TestModule.Shared.EngineHelper.NestedGlobalEngineScopeRestoresPreviousEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperWorldContextScopeRestoreTest,
	"Angelscript.TestModule.Shared.EngineHelper.WorldContextScopeRestoresPreviousContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperContextIsolationAcrossEnginesTest,
	"Angelscript.TestModule.Shared.EngineHelper.ExecutingOneTestEngineDoesNotLeakContextIntoNextTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestEngineHelperProductionSubsystemDoesNotHijackIsolatedEngineTest,
	"Angelscript.TestModule.Shared.EngineHelper.SubsystemAttachedProductionEngineDoesNotHijackIsolatedTestEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


bool FAngelscriptTestEngineHelperGeneratedSymbolLookupTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperAnnotatedModule"));
	};

	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("HelperAnnotatedModule"),
		TEXT("HelperAnnotatedModule.as"),
		TEXT(R"(
UCLASS()
class UAnnotatedHelperObject : UObject
{
    UFUNCTION()
    int GetValue()
    {
        return 42;
    }
}
)") );
	if (!TestTrue(TEXT("CompileAnnotatedModuleFromMemory should compile an annotated class module"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UAnnotatedHelperObject"));
	if (!TestNotNull(TEXT("FindGeneratedClass should locate the generated class"), GeneratedClass))
	{
		return false;
	}

	UFunction* GeneratedFunction = AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("GetValue"));
	TestNotNull(TEXT("FindGeneratedFunction should locate the generated UFunction"), GeneratedFunction);
	return GeneratedFunction != nullptr;
}

bool FAngelscriptTestEngineHelperFailedAnnotatedIsolationTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperBrokenAnnotated"));
		Engine.DiscardModule(TEXT("HelperRecoveredAnnotated"));
	};

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bBrokenCompiled = AngelscriptTestSupport::AnalyzeReloadFromMemory(
		&Engine,
		TEXT("HelperBrokenAnnotated"),
		TEXT("HelperBrokenAnnotated.as"),
		TEXT(R"(
UCLASS()
class UBrokenHelperObject : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
	)") ,
		ReloadRequirement,
		bWantsFullReload,
		bNeedsFullReload);
	if (!TestFalse(TEXT("Invalid annotated helper module should fail to compile"), bBrokenCompiled))
	{
		return false;
	}
	TestEqual(TEXT("Broken annotated helper module should report an error reload requirement"), ReloadRequirement, FAngelscriptClassGenerator::Error);
	TestFalse(TEXT("Broken annotated helper module should not suggest a full reload"), bWantsFullReload);
	TestFalse(TEXT("Broken annotated helper module should not require a full reload"), bNeedsFullReload);

	const bool bRecoveredCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("HelperRecoveredAnnotated"),
		TEXT("HelperRecoveredAnnotated.as"),
		TEXT(R"(
UCLASS()
class URecoveredHelperObject : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 7;
	}
}
)") );
	if (!TestTrue(TEXT("A later valid annotated helper module should compile after a failed one"), bRecoveredCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("URecoveredHelperObject"));
	if (!TestNotNull(TEXT("Recovered generated class should be discoverable"), GeneratedClass))
	{
		return false;
	}

	UFunction* GeneratedFunction = AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("GetValue"));
	TestNotNull(TEXT("Recovered generated function should be discoverable"), GeneratedFunction);
	return GeneratedFunction != nullptr;
}

bool FAngelscriptTestEngineHelperSharedEngineNeverAttachesToProductionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& SharedEngine = AngelscriptTestSupport::GetSharedTestEngine();
	return TestTrue(
		TEXT("Shared test engine helper should resolve to the shared test engine instance in non-production test context"),
		&AngelscriptTestSupport::GetSharedInitializedTestEngine() == &SharedEngine);
}

bool FAngelscriptTestEngineHelperProductionHelperRejectsMissingProductionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine* ProductionEngine = AngelscriptTestSupport::TryGetCurrentProductionEngine();
	if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
	{
		return TestTrue(
			TEXT("Production-engine probe should resolve the subsystem-attached engine when one is available"),
			ProductionEngine == Subsystem->GetEngine());
	}

	if (FAngelscriptEngine::IsInitialized())
	{
		return TestTrue(
			TEXT("Production-engine probe should resolve the global runtime engine when the runtime is initialized without a subsystem owner"),
			ProductionEngine == &FAngelscriptEngine::Get());
	}

	return TestNull(
		TEXT("Production-engine probe should return null when no production engine is attached"),
		ProductionEngine);
}

bool FAngelscriptTestEngineHelperCompileRestoresScopedGlobalEngineTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& SharedEngine = AngelscriptTestSupport::GetSharedTestEngine();
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Compile restore test should create an isolated engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	FScopedTestEngineGlobalScope GlobalScope(&SharedEngine);
	const bool bCompiled = AngelscriptTestSupport::CompileModuleFromMemory(
		IsolatedEngine.Get(),
		TEXT("HelperScopedGlobalRestore"),
		TEXT("HelperScopedGlobalRestore.as"),
		TEXT("int Entry() { return 1; }"));

	if (!TestTrue(TEXT("Scoped global restore test module should compile"), bCompiled))
	{
		return false;
	}

	return TestTrue(
		TEXT("Compiling through helper should restore the previous scoped global engine"),
		FAngelscriptTestEngineScopeAccess::GetGlobalEngine() == &SharedEngine);
}

bool FAngelscriptTestEngineHelperNestedGlobalScopeRestoreTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> EngineA = AngelscriptTestSupport::CreateCloneTestEngine();
	TUniquePtr<FAngelscriptEngine> EngineB = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Nested scope restore test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Nested scope restore test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	{
		FScopedTestEngineGlobalScope ScopeA(EngineA.Get());
		if (!TestTrue(TEXT("Outer scope should install engine A"), FAngelscriptTestEngineScopeAccess::GetGlobalEngine() == EngineA.Get()))
		{
			return false;
		}

		{
			FScopedTestEngineGlobalScope ScopeB(EngineB.Get());
			if (!TestTrue(TEXT("Inner scope should temporarily install engine B"), FAngelscriptTestEngineScopeAccess::GetGlobalEngine() == EngineB.Get()))
			{
				return false;
			}
		}

		if (!TestTrue(TEXT("Leaving inner scope should restore engine A"), FAngelscriptTestEngineScopeAccess::GetGlobalEngine() == EngineA.Get()))
		{
			return false;
		}
	}

	return true;
}

bool FAngelscriptTestEngineHelperWorldContextScopeRestoreTest::RunTest(const FString& Parameters)
{
	UObject* PreviousWorldContext = FAngelscriptEngine::CurrentWorldContext;
	UObject* DummyContext = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
	if (!TestNotNull(TEXT("World context scope restore test should create a dummy context object"), DummyContext))
	{
		return false;
	}

	{
		AngelscriptTestSupport::FScopedTestWorldContextScope WorldContextScope(DummyContext);
		if (!TestTrue(TEXT("World context scope should install the dummy context"), FAngelscriptEngine::CurrentWorldContext == DummyContext))
		{
			return false;
		}
	}

	return TestTrue(TEXT("World context scope should restore the previous context"), FAngelscriptEngine::CurrentWorldContext == PreviousWorldContext);
}

bool FAngelscriptTestEngineHelperContextIsolationAcrossEnginesTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> EngineA = AngelscriptTestSupport::CreateCloneTestEngine();
	TUniquePtr<FAngelscriptEngine> EngineB = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Context isolation test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Context isolation test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	const bool bCompiledA = AngelscriptTestSupport::CompileModuleFromMemory(
		EngineA.Get(),
		TEXT("HelperIsolationA"),
		TEXT("HelperIsolationA.as"),
		TEXT("int EntryA() { return 11; }"));
	const bool bCompiledB = AngelscriptTestSupport::CompileModuleFromMemory(
		EngineB.Get(),
		TEXT("HelperIsolationB"),
		TEXT("HelperIsolationB.as"),
		TEXT("int EntryB() { return 22; }"));
	if (!TestTrue(TEXT("Context isolation test should compile module A"), bCompiledA)
		|| !TestTrue(TEXT("Context isolation test should compile module B"), bCompiledB))
	{
		return false;
	}

	int32 ResultA = 0;
	int32 ResultB = 0;
	if (!TestTrue(TEXT("Engine A should execute its own module"), AngelscriptTestSupport::ExecuteIntFunction(EngineA.Get(), TEXT("HelperIsolationA"), TEXT("int EntryA()"), ResultA))
		|| !TestTrue(TEXT("Engine B should execute its own module"), AngelscriptTestSupport::ExecuteIntFunction(EngineB.Get(), TEXT("HelperIsolationB"), TEXT("int EntryB()"), ResultB)))
	{
		return false;
	}

	TestEqual(TEXT("Engine A should return its own result"), ResultA, 11);
	return TestEqual(TEXT("Engine B should return its own result without context leakage"), ResultB, 22);
}

bool FAngelscriptTestEngineHelperProductionSubsystemDoesNotHijackIsolatedEngineTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& SharedEngine = AngelscriptTestSupport::GetSharedTestEngine();
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Subsystem hijack test should create an isolated engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	const bool bCompiled = AngelscriptTestSupport::CompileModuleFromMemory(
		IsolatedEngine.Get(),
		TEXT("HelperIsolationHijack"),
		TEXT("HelperIsolationHijack.as"),
		TEXT("int Entry() { return 5; }"));
	if (!TestTrue(TEXT("Subsystem hijack test module should compile"), bCompiled))
	{
		return false;
	}

	TestTrue(TEXT("Isolated engine should keep its own module record"), IsolatedEngine->GetModuleByModuleName(TEXT("HelperIsolationHijack")).IsValid());
	return TestTrue(TEXT("Shared test engine should not receive isolated engine modules"), !SharedEngine.GetModuleByModuleName(TEXT("HelperIsolationHijack")).IsValid());
}

#endif
