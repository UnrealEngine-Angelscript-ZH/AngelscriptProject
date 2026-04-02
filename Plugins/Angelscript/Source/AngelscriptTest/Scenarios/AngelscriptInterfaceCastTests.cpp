#include "AngelscriptScenarioTestUtils.h"

#include "Core/AngelscriptActor.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceCastSuccessTest,
	"Angelscript.TestModule.Scenario.Interface.CastSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceCastFailTest,
	"Angelscript.TestModule.Scenario.Interface.CastFail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceMethodCallTest,
	"Angelscript.TestModule.Scenario.Interface.MethodCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioInterfaceCastSuccessTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceCastSuccess"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceCastSuccess.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableCastOk
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceCastSuccess : AAngelscriptActor, UIDamageableCastOk
{
	UPROPERTY()
	int CastSucceeded = 0;

	UFUNCTION()
	void TakeDamage(float Amount) {}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableCastOk Casted = Cast<UIDamageableCastOk>(Self);
		if (Casted != nullptr)
		{
			CastSucceeded = 1;
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceCastSuccess"));
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

	int32 CastSucceeded = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastSucceeded"), CastSucceeded))
	{
		return false;
	}

	TestEqual(TEXT("Cast to interface should succeed for implementing actor"), CastSucceeded, 1);
	return true;
}

bool FAngelscriptScenarioInterfaceCastFailTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceCastFail"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceCastFail.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableCastFail
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceCastFail : AAngelscriptActor
{
	UPROPERTY()
	int CastReturnedNull = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableCastFail Casted = Cast<UIDamageableCastFail>(Self);
		if (Casted == nullptr)
		{
			CastReturnedNull = 1;
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceCastFail"));
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

	int32 CastReturnedNull = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastReturnedNull"), CastReturnedNull))
	{
		return false;
	}

	TestEqual(TEXT("Cast to interface should fail for non-implementing actor"), CastReturnedNull, 1);
	return true;
}

bool FAngelscriptScenarioInterfaceMethodCallTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceMethodCall"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceMethodCall.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableMethodCall
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceMethodCall : AAngelscriptActor, UIDamageableMethodCall
{
	UPROPERTY()
	int CastSucceeded = 0;

	UPROPERTY()
	int MethodCalled = 0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		MethodCalled = 1;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableMethodCall Casted = Cast<UIDamageableMethodCall>(Self);
		if (Casted != nullptr)
		{
			CastSucceeded = 1;
			Casted.TakeDamage(42.0);
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceMethodCall"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UIDamageableMethodCall"));
	TestNotNull(TEXT("Interface class should exist"), InterfaceClass);
	if (InterfaceClass != nullptr)
	{
		TestTrue(TEXT("ScriptClass should implement UIDamageableMethodCall"), ScriptClass->ImplementsInterface(InterfaceClass));
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}

	BeginPlayActor(*Actor);

	int32 CastSucceeded = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastSucceeded"), CastSucceeded))
	{
		return false;
	}
	TestEqual(TEXT("Cast to interface type should succeed"), CastSucceeded, 1);

	int32 MethodCalled = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("MethodCalled"), MethodCalled))
	{
		return false;
	}
	TestEqual(TEXT("Method should have been called via interface reference"), MethodCalled, 1);

	return true;
}

#endif
