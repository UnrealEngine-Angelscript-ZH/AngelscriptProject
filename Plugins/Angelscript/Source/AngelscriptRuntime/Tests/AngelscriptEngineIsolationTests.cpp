#include "AngelscriptEngine.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "AngelscriptBinds.h"
#include "AngelscriptBindDatabase.h"
#include "Binds/Helper_ToString.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Curves/CurveFloat.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptEngineIsolationTestAccess
{
	static bool DestroyGlobalEngine()
	{
		return FAngelscriptEngine::DestroyGlobal();
	}

	static UObject* GetCurrentWorldContextObject()
	{
		return FAngelscriptEngine::TryGetCurrentWorldContextObject();
	}

	static int32 GetToStringCount(const FAngelscriptEngine& Engine)
	{
		return Engine.GetToStringEntryCountForTesting();
	}

	static int32 GetBindDatabaseClassCount(const FAngelscriptEngine& Engine)
	{
		return Engine.GetBindDatabaseForTesting().Classes.Num();
	}

	static void SetAutomaticImportMethod(FAngelscriptEngine& Engine, bool bEnabled)
	{
		Engine.SetAutomaticImportMethodForTesting(bEnabled);
	}

	static void SetUseEditorScripts(FAngelscriptEngine& Engine, bool bEnabled)
	{
		Engine.SetUseEditorScriptsForTesting(bEnabled);
	}
};

static void ResetIsolationRuntime()
{
	if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
	{
		FAngelscriptEngineIsolationTestAccess::DestroyGlobalEngine();
	}
}

static FString MakeIsolationName(const TCHAR* Prefix)
{
	return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptContextStackScopedResolutionTest,
	"Angelscript.CppTests.Engine.Isolation.ContextStack.ScopedResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEngineScopeRestoresWorldContextTest,
	"Angelscript.CppTests.Engine.Isolation.EngineScope.RestoresWorldContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCurrentWorldContextAccessorTracksActiveScopeTest,
	"Angelscript.CppTests.Engine.Isolation.WorldContext.AccessorTracksActiveScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInstanceScopedFlagsStaySeparatedTest,
	"Angelscript.CppTests.Engine.Isolation.Flags.InstanceScopedFlagsStaySeparated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFullEnginesKeepStateSeparateTest,
	"Angelscript.CppTests.Engine.Isolation.SharedState.FullEnginesKeepStateSeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCloneSharesSourceStateTest,
	"Angelscript.CppTests.Engine.Isolation.SharedState.CloneSharesSourceState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMultipleFullEnginesCanCoexistTest,
	"Angelscript.CppTests.Engine.Isolation.MultiFull.CanCoexist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptContextStackScopedResolutionTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptEngine::CreateCloneFrom(*PrimaryEngine, Config);

	if (!TestNotNull(TEXT("Context stack scoped resolution should create a primary engine"), PrimaryEngine.Get())
		|| !TestNotNull(TEXT("Context stack scoped resolution should create a secondary engine"), SecondaryEngine.Get()))
	{
		return false;
	}

	TestTrue(TEXT("Context stack should start empty"), FAngelscriptEngineContextStack::IsEmpty());

	{
		FAngelscriptEngineScope PrimaryScope(*PrimaryEngine);
		TestTrue(TEXT("Scoped resolution should return the primary engine while its scope is active"), &FAngelscriptEngine::Get() == PrimaryEngine.Get());
		TestTrue(TEXT("Context stack should expose the active primary engine"), FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get());

		{
			FAngelscriptEngineScope SecondaryScope(*SecondaryEngine);
			TestTrue(TEXT("Nested scoped resolution should prefer the nested engine"), &FAngelscriptEngine::Get() == SecondaryEngine.Get());
			TestTrue(TEXT("Context stack should update its top entry for nested scopes"), FAngelscriptEngineContextStack::Peek() == SecondaryEngine.Get());
		}

		TestTrue(TEXT("Nested scope teardown should restore the previous engine"), &FAngelscriptEngine::Get() == PrimaryEngine.Get());
		TestTrue(TEXT("Context stack should restore the previous engine after nested scope teardown"), FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get());
	}

	return TestTrue(TEXT("Context stack should be empty after all scopes leave"), FAngelscriptEngineContextStack::IsEmpty());
}

bool FAngelscriptEngineScopeRestoresWorldContextTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptEngine::CreateCloneFrom(*PrimaryEngine, Config);

	if (!TestNotNull(TEXT("Engine scope restore test should create a primary engine"), PrimaryEngine.Get())
		|| !TestNotNull(TEXT("Engine scope restore test should create a secondary engine"), SecondaryEngine.Get()))
	{
		return false;
	}

	UObject* OuterContext = NewObject<UCurveFloat>();
	UObject* InnerContext = NewObject<UCurveFloat>();
	if (!TestNotNull(TEXT("Engine scope restore test should create an outer context object"), OuterContext)
		|| !TestNotNull(TEXT("Engine scope restore test should create an inner context object"), InnerContext))
	{
		return false;
	}

	{
		FAngelscriptEngineScope OuterScope(*PrimaryEngine, OuterContext);
		TestTrue(TEXT("Outer scope should expose its world context through the active engine"), PrimaryEngine->GetCurrentWorldContextObject() == OuterContext);

		{
			FAngelscriptEngineScope InnerScope(*SecondaryEngine, InnerContext);
			TestTrue(TEXT("Inner scope should expose its world context through the nested engine"), SecondaryEngine->GetCurrentWorldContextObject() == InnerContext);
		}

		TestTrue(TEXT("Leaving the inner scope should restore the outer world context"), PrimaryEngine->GetCurrentWorldContextObject() == OuterContext);
	}

	return TestNull(TEXT("Leaving the outer scope should clear the world context"), PrimaryEngine->GetCurrentWorldContextObject());
}

bool FAngelscriptCurrentWorldContextAccessorTracksActiveScopeTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);

	if (!TestNotNull(TEXT("World-context accessor test should create a primary engine"), PrimaryEngine.Get())
		|| !TestNotNull(TEXT("World-context accessor test should create a secondary engine"), SecondaryEngine.Get()))
	{
		return false;
	}

	UObject* OuterContext = NewObject<UCurveFloat>();
	UObject* InnerContext = NewObject<UCurveFloat>();
	if (!TestNotNull(TEXT("World-context accessor test should create an outer context"), OuterContext)
		|| !TestNotNull(TEXT("World-context accessor test should create an inner context"), InnerContext))
	{
		return false;
	}

	TestNull(TEXT("World-context accessor should start empty when no engine scope is active"), FAngelscriptEngineIsolationTestAccess::GetCurrentWorldContextObject());

	{
		FAngelscriptEngineScope OuterScope(*PrimaryEngine, OuterContext);
		TestTrue(TEXT("World-context accessor should expose the outer scope context"), FAngelscriptEngineIsolationTestAccess::GetCurrentWorldContextObject() == OuterContext);

		{
			FAngelscriptEngineScope InnerScope(*SecondaryEngine, InnerContext);
			TestTrue(TEXT("World-context accessor should switch to the nested scope context"), FAngelscriptEngineIsolationTestAccess::GetCurrentWorldContextObject() == InnerContext);
		}

		TestTrue(TEXT("World-context accessor should restore the outer scope context after nested teardown"), FAngelscriptEngineIsolationTestAccess::GetCurrentWorldContextObject() == OuterContext);
	}

	return TestNull(TEXT("World-context accessor should clear after all scopes leave"), FAngelscriptEngineIsolationTestAccess::GetCurrentWorldContextObject());
}

bool FAngelscriptInstanceScopedFlagsStaySeparatedTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);

	if (!TestNotNull(TEXT("Instance-scoped flags test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Instance-scoped flags test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	FAngelscriptEngineIsolationTestAccess::SetUseEditorScripts(*EngineA, true);
	FAngelscriptEngineIsolationTestAccess::SetAutomaticImportMethod(*EngineA, true);
	FAngelscriptEngineIsolationTestAccess::SetUseEditorScripts(*EngineB, false);
	FAngelscriptEngineIsolationTestAccess::SetAutomaticImportMethod(*EngineB, false);

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		TestTrue(TEXT("Engine A should keep editor scripts enabled on its own instance"), EngineA->ShouldUseEditorScripts());
		TestTrue(TEXT("Engine A should keep automatic imports enabled on its own instance"), EngineA->ShouldUseAutomaticImportMethod());
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		TestFalse(TEXT("Engine B should keep editor scripts disabled on its own instance"), EngineB->ShouldUseEditorScripts());
		TestFalse(TEXT("Engine B should keep automatic imports disabled on its own instance"), EngineB->ShouldUseAutomaticImportMethod());
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		TestTrue(TEXT("Engine A should remain unchanged after visiting engine B"), EngineA->ShouldUseEditorScripts());
		TestTrue(TEXT("Engine A should keep its automatic import setting after visiting engine B"), EngineA->ShouldUseAutomaticImportMethod());
	}

	return true;
}

bool FAngelscriptFullEnginesKeepStateSeparateTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);

	if (!TestNotNull(TEXT("Full engine isolation test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Full engine isolation test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	const FString AliasName = MakeIsolationName(TEXT("Alias"));
	const FString ToStringName = MakeIsolationName(TEXT("ToString"));
	FAngelscriptClassBind BindClass;
	BindClass.TypeName = MakeIsolationName(TEXT("BindDb"));
	int32 BaselineToStringCountA = 0;
	int32 BaselineBindDatabaseClassCountA = 0;
	int32 BaselineToStringCountB = 0;
	int32 BaselineBindDatabaseClassCountB = 0;

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		BaselineToStringCountA = FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineA);
		BaselineBindDatabaseClassCountA = FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineA);
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		BaselineToStringCountB = FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineB);
		BaselineBindDatabaseClassCountB = FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineB);
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		TSharedPtr<FAngelscriptType> IntType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int"));
		if (!TestTrue(TEXT("Full engine isolation test should resolve the built-in int type inside engine A"), IntType.IsValid()))
		{
			return false;
		}

		FAngelscriptType::RegisterAlias(AliasName, IntType.ToSharedRef());
		FAngelscriptBinds::AddSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA")));
		FToStringHelper::Register(ToStringName, +[](void*, FString& OutString)
		{
			OutString = TEXT("EngineA");
		});
		FAngelscriptBindDatabase::Get().Classes.Add(BindClass);
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		TestNull(TEXT("Engine B should not see aliases registered through engine A"), FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		TestFalse(TEXT("Engine B should not inherit skip entries registered through engine A"), FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))));
		TestEqual(TEXT("Engine B should keep its original ToString registry baseline"), FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineB), BaselineToStringCountB);
		TestEqual(TEXT("Engine B should keep its original bind database baseline"), FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineB), BaselineBindDatabaseClassCountB);
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		TestNotNull(TEXT("Engine A should keep its alias registration"), FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		TestTrue(TEXT("Engine A should keep its skip entry registration"), FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))));
		TestEqual(TEXT("Engine A should retain its extra ToString registry entry"), FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineA), BaselineToStringCountA + 1);
		TestEqual(TEXT("Engine A should retain its extra bind database class"), FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineA), BaselineBindDatabaseClassCountA + 1);
	}

	return true;
}

bool FAngelscriptCloneSharesSourceStateTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	if (!TestNotNull(TEXT("Clone shared-state test should create a source engine"), SourceEngine.Get()))
	{
		return false;
	}

	const FString AliasName = MakeIsolationName(TEXT("CloneAlias"));
	{
		FAngelscriptEngineScope SourceScope(*SourceEngine);
		TSharedPtr<FAngelscriptType> IntType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int"));
		if (!TestTrue(TEXT("Clone shared-state test should resolve the built-in int type inside the source engine"), IntType.IsValid()))
		{
			return false;
		}

		FAngelscriptType::RegisterAlias(AliasName, IntType.ToSharedRef());
		FAngelscriptBinds::AddSkipEntry(FName(TEXT("CloneIsolationActor")), FName(TEXT("SharedSkip")));
		FToStringHelper::Register(MakeIsolationName(TEXT("CloneToString")), +[](void*, FString& OutString)
		{
			OutString = TEXT("CloneShared");
		});
	}

	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);
	if (!TestNotNull(TEXT("Clone shared-state test should create the clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	{
		FAngelscriptEngineScope CloneScope(*CloneEngine);
		TestNotNull(TEXT("Clone engine should see aliases registered on the source engine"), FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		TestTrue(TEXT("Clone engine should share skip entries with the source engine"), FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("CloneIsolationActor")), FName(TEXT("SharedSkip"))));
		TestTrue(TEXT("Clone engine should inherit the shared ToString registry"), FAngelscriptEngineIsolationTestAccess::GetToStringCount(*CloneEngine) > 0);
	}

	return true;
}

bool FAngelscriptMultipleFullEnginesCanCoexistTest::RunTest(const FString& Parameters)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);

	if (!TestNotNull(TEXT("Multiple full engines test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Multiple full engines test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	TestNotEqual(TEXT("Multiple full engines test should create distinct engine wrappers"), EngineA.Get(), EngineB.Get());
	TestNotEqual(TEXT("Multiple full engines test should allocate distinct script engines"), EngineA->GetScriptEngine(), EngineB->GetScriptEngine());
	return TestNull(TEXT("Leaving the test without any engine scope should leave no ambient context engine"), FAngelscriptEngineContextStack::Peek());
}

#endif
