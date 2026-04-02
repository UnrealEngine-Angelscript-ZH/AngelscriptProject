#include "AngelscriptScenarioTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioActorUPropertyTest,
	"Angelscript.TestModule.Scenario.Actor.UProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioActorUFunctionTest,
	"Angelscript.TestModule.Scenario.Actor.UFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioActorDefaultValuesTest,
	"Angelscript.TestModule.Scenario.Actor.DefaultValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioActorUPropertyTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioActorUProperty"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioActorUProperty.as"),
		TEXT(R"AS(
UCLASS()
class AScenarioActorUProperty : AAngelscriptActor
{
	UPROPERTY()
	int Health = 100;

	UPROPERTY()
	FString DisplayName = "TestActor";
}
)AS"),
		TEXT("AScenarioActorUProperty"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	int32 Health = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("Health"), Health))
	{
		return false;
	}

	FString DisplayName;
	if (!ReadPropertyValue<FStrProperty>(*this, Actor, TEXT("DisplayName"), DisplayName))
	{
		return false;
	}

	TestEqual(TEXT("Scenario actor reflected int UPROPERTY should keep its default value"), Health, 100);
	TestEqual(TEXT("Scenario actor reflected FString UPROPERTY should keep its default value"), DisplayName, FString(TEXT("TestActor")));
	return true;
}

bool FAngelscriptScenarioActorUFunctionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioActorUFunction"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioActorUFunction.as"),
		TEXT(R"AS(
UCLASS()
class AScenarioActorUFunction : AAngelscriptActor
{
	UPROPERTY()
	int Health = 100;

	UFUNCTION()
	int GetHealth()
	{
		return Health;
	}
}
)AS"),
		TEXT("AScenarioActorUFunction"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	UFunction* GetHealthFunction = FindGeneratedFunction(ScriptClass, TEXT("GetHealth"));
	if (!TestNotNull(TEXT("Scenario actor reflected UFUNCTION should exist"), GetHealthFunction))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Scenario actor reflected UFUNCTION should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(Actor, GetHealthFunction, Result)))
	{
		return false;
	}

	TestEqual(TEXT("Scenario actor reflected UFUNCTION should return the scripted property value"), Result, 100);
	return true;
}

bool FAngelscriptScenarioActorDefaultValuesTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioActorDefaultValues"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioActorDefaultValues.as"),
		TEXT(R"AS(
UCLASS()
class AScenarioActorDefaultValues : AAngelscriptActor
{
	default PrimaryActorTick.TickInterval = 0.5f;
}
)AS"),
		TEXT("AScenarioActorDefaultValues"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	TestTrue(TEXT("Scenario actor default values should enable ticking on the generated actor"), Actor->CanEverTick());
	TestTrue(TEXT("Scenario actor default values should apply the configured tick interval"), FMath::IsNearlyEqual(Actor->PrimaryActorTick.TickInterval, 0.5f));
	return true;
}

#endif
