#include "Shared/AngelscriptScenarioTestUtils.h"

#include "Core/AngelscriptActor.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

// Test Layer: UE Scenario
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceInheritedInterfaceTest,
	"Angelscript.TestModule.Interface.InheritedInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceMissingMethodTest,
	"Angelscript.TestModule.Interface.MissingMethod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceNoPropertyTest,
	"Angelscript.TestModule.Interface.NoProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceGCSafeTest,
	"Angelscript.TestModule.Interface.GCSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#if 1
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceHotReloadTest,
	"Angelscript.TestModule.Interface.HotReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceCppInterfaceTest,
	"Angelscript.TestModule.Interface.CppInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceInheritedMethodDispatchTest,
	"Angelscript.TestModule.Interface.InheritedMethodDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceMultipleInheritanceChainTest,
	"Angelscript.TestModule.Interface.MultipleInheritanceChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceMultipleInheritanceDispatchTest,
	"Angelscript.TestModule.Interface.MultipleInheritanceDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioInterfaceInheritedInterfaceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceInherited"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceInherited.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableParent
{
	void TakeDamage(float Amount);
}

UINTERFACE()
interface UIKillableChild : UIDamageableParent
{
	void Kill();
}

UCLASS()
class AScenarioInterfaceInherited : AAngelscriptActor, UIKillableChild
{
	UPROPERTY()
	float DamageReceived = 0.0;

	UPROPERTY()
	int Killed = 0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		DamageReceived = Amount;
	}

	UFUNCTION()
	void Kill()
	{
		Killed = 1;
	}
}
)AS"),
		TEXT("AScenarioInterfaceInherited"));
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

	UClass* ParentInterface = FindGeneratedClass(&Engine, TEXT("UIDamageableParent"));
	UClass* ChildInterface = FindGeneratedClass(&Engine, TEXT("UIKillableChild"));

	TestNotNull(TEXT("Parent interface class should exist"), ParentInterface);
	TestNotNull(TEXT("Child interface class should exist"), ChildInterface);

	if (ChildInterface != nullptr)
	{
		TestTrue(TEXT("Actor should implement child interface UIKillableChild"), Actor->GetClass()->ImplementsInterface(ChildInterface));
	}
	if (ParentInterface != nullptr)
	{
		TestTrue(TEXT("Actor implementing child interface should also satisfy parent UIDamageableParent"), Actor->GetClass()->ImplementsInterface(ParentInterface));
	}
	return true;
}

bool FAngelscriptScenarioInterfaceMissingMethodTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceMissingMethod"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	AddExpectedError(TEXT("missing required method"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		ModuleName,
		TEXT("ScenarioInterfaceMissingMethod.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableMissing
{
	void TakeDamage(float Amount);
	float GetHealth();
}

UCLASS()
class AScenarioInterfaceMissingMethod : AAngelscriptActor, UIDamageableMissing
{
	UFUNCTION()
	void TakeDamage(float Amount) {}
}

)AS"));

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UIDamageableMissing"));
	TestNotNull(TEXT("Interface class UIDamageableMissing should exist"), InterfaceClass);

	if (InterfaceClass != nullptr)
	{
		UFunction* TakeDamageFunc = InterfaceClass->FindFunctionByName(TEXT("TakeDamage"));
		UFunction* GetHealthFunc = InterfaceClass->FindFunctionByName(TEXT("GetHealth"));

		TestNotNull(TEXT("Interface should have TakeDamage UFunction"), TakeDamageFunc);
		TestNotNull(TEXT("Interface should have GetHealth UFunction"), GetHealthFunc);

		int32 FuncCount = 0;
		for (TFieldIterator<UFunction> FuncIt(InterfaceClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (Func->GetOuter() != UInterface::StaticClass())
				FuncCount++;
		}
		TestEqual(TEXT("Interface should declare exactly 2 methods"), FuncCount, 2);
	}

	return bCompiled;
}

bool FAngelscriptScenarioInterfaceNoPropertyTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceNoProperty"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* InterfaceClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceNoProperty.as"),
		TEXT(R"AS(
UINTERFACE()
interface UINoProperty
{
	void DoSomething();
}
)AS"),
		TEXT("UINoProperty"));

	if (InterfaceClass == nullptr)
	{
		return false;
	}

	int32 PropertyCount = 0;
	for (TFieldIterator<FProperty> PropIt(InterfaceClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		PropertyCount++;
	}

	TestEqual(TEXT("Interface should have no UPROPERTY members"), PropertyCount, 0);
	return true;
}

bool FAngelscriptScenarioInterfaceGCSafeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceGCSafe"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceGCSafe.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableGC
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceGCSafe : AAngelscriptActor, UIDamageableGC
{
	UFUNCTION()
	void TakeDamage(float Amount) {}
}
)AS"),
		TEXT("AScenarioInterfaceGCSafe"));
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

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UIDamageableGC"));
	if (InterfaceClass != nullptr)
	{
		TestTrue(TEXT("Actor should implement interface before destroy"), Actor->GetClass()->ImplementsInterface(InterfaceClass));
	}

	TWeakObjectPtr<AActor> WeakActor = Actor;
	Actor->Destroy();
	TickWorld(Spawner.GetWorld(), 0.0f, 1);
	CollectGarbage(RF_NoFlags, true);

	TestTrue(TEXT("Interface actor should be collected after destroy + GC"), !WeakActor.IsValid());
	return true;
}

#if 1
bool FAngelscriptScenarioInterfaceHotReloadTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceHotReload"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UINTERFACE()
interface UIDamageableHR
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceHotReload : AAngelscriptActor, UIDamageableHR
{
	UPROPERTY()
	float DamageReceived = 0.0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		DamageReceived = Amount;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UINTERFACE()
interface UIDamageableHR
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceHotReload : AAngelscriptActor, UIDamageableHR
{
	UPROPERTY()
	float DamageReceived = 0.0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		DamageReceived = Amount * 2.0;
	}
}
)AS");

	UClass* ClassV1 = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceHotReload.as"),
		ScriptV1,
		TEXT("AScenarioInterfaceHotReload"));
	if (ClassV1 == nullptr)
	{
		return false;
	}

	UClass* InterfaceV1 = FindGeneratedClass(&Engine, TEXT("UIDamageableHR"));
	TestNotNull(TEXT("Interface should exist after V1 compile"), InterfaceV1);
	if (InterfaceV1 != nullptr)
	{
		TestTrue(TEXT("V1 class should implement interface"), ClassV1->ImplementsInterface(InterfaceV1));
	}

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Interface hot reload should succeed on the full reload path"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("ScenarioInterfaceHotReload.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Interface hot reload should route through the full reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("AScenarioInterfaceHotReload"));
	if (ClassV2 == nullptr)
	{
		return false;
	}

	UClass* InterfaceV2 = FindGeneratedClass(&Engine, TEXT("UIDamageableHR"));
	TestNotNull(TEXT("Interface should exist after V2 hot reload"), InterfaceV2);
	if (InterfaceV2 != nullptr)
	{
		TestTrue(TEXT("V2 class should still implement interface after hot reload"), ClassV2->ImplementsInterface(InterfaceV2));
	}

	return true;
}
#endif

bool FAngelscriptScenarioInterfaceCppInterfaceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceCppInterface"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceCppInterface.as"),
		TEXT(R"AS(
UINTERFACE()
interface UICppTestInterface
{
	void DoWork();
}

UCLASS()
class AScenarioInterfaceCppBase : AAngelscriptActor, UICppTestInterface
{
	UPROPERTY()
	int WorkDone = 0;

	UFUNCTION()
	void DoWork()
	{
		WorkDone = 1;
	}
}
)AS"),
		TEXT("AScenarioInterfaceCppBase"));
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

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UICppTestInterface"));
	TestNotNull(TEXT("Interface class should exist"), InterfaceClass);
	if (InterfaceClass != nullptr)
	{
		TestTrue(TEXT("Class should implement its declared interface"), Actor->GetClass()->ImplementsInterface(InterfaceClass));
	}

	return true;
}

bool FAngelscriptScenarioInterfaceInheritedMethodDispatchTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceInheritedDispatch"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceInheritedDispatch.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableDispatch
{
	int GetDamageLevel();
}

UINTERFACE()
interface UIKillableDispatch : UIDamageableDispatch
{
	int GetKillCount();
}

UCLASS()
class AScenarioInterfaceInheritedDispatch : AAngelscriptActor, UIKillableDispatch
{
	UPROPERTY()
	int ParentCastWorked = 0;

	UPROPERTY()
	int ChildCastWorked = 0;

	UPROPERTY()
	int ParentResult = 0;

	UPROPERTY()
	int ChildResult = 0;

	UFUNCTION()
	int GetDamageLevel()
	{
		return 3;
	}

	UFUNCTION()
	int GetKillCount()
	{
		return 5;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableDispatch ParentRef = Cast<UIDamageableDispatch>(Self);
		if (ParentRef != nullptr)
		{
			ParentCastWorked = 1;
			ParentResult = ParentRef.GetDamageLevel();
		}

		UIKillableDispatch ChildRef = Cast<UIKillableDispatch>(Self);
		if (ChildRef != nullptr)
		{
			ChildCastWorked = 1;
			ChildResult = ChildRef.GetKillCount();
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceInheritedDispatch"));
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

	int32 ParentCastWorked = 0;
	int32 ChildCastWorked = 0;
	int32 ParentResult = 0;
	int32 ChildResult = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ParentCastWorked"), ParentCastWorked)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ChildCastWorked"), ChildCastWorked)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ParentResult"), ParentResult)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ChildResult"), ChildResult))
	{
		return false;
	}

	TestEqual(TEXT("Parent interface cast should succeed for inherited script interface"), ParentCastWorked, 1);
	TestEqual(TEXT("Child interface cast should succeed for inherited script interface"), ChildCastWorked, 1);
	TestEqual(TEXT("Parent interface method should dispatch through inherited interface reference"), ParentResult, 3);
	TestEqual(TEXT("Child interface method should dispatch through child interface reference"), ChildResult, 5);
	return true;
}

bool FAngelscriptScenarioInterfaceMultipleInheritanceChainTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceMultiChain"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceMultiChain.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIBaseChain
{
	void BaseMethod();
}

UINTERFACE()
interface UIMidChain : UIBaseChain
{
	void MidMethod();
}

UINTERFACE()
interface UILeafChain : UIMidChain
{
	void LeafMethod();
}

UCLASS()
class AScenarioInterfaceMultiChain : AAngelscriptActor, UILeafChain
{
	UFUNCTION()
	void BaseMethod() {}

	UFUNCTION()
	void MidMethod() {}

	UFUNCTION()
	void LeafMethod() {}
}
)AS"),
		TEXT("AScenarioInterfaceMultiChain"));
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

	UClass* BaseInterface = FindGeneratedClass(&Engine, TEXT("UIBaseChain"));
	UClass* MidInterface = FindGeneratedClass(&Engine, TEXT("UIMidChain"));
	UClass* LeafInterface = FindGeneratedClass(&Engine, TEXT("UILeafChain"));

	TestNotNull(TEXT("Base interface should exist"), BaseInterface);
	TestNotNull(TEXT("Mid interface should exist"), MidInterface);
	TestNotNull(TEXT("Leaf interface should exist"), LeafInterface);

	if (BaseInterface != nullptr)
	{
		TestTrue(TEXT("Actor implementing leaf should satisfy base interface"), Actor->GetClass()->ImplementsInterface(BaseInterface));
	}
	if (MidInterface != nullptr)
	{
		TestTrue(TEXT("Actor implementing leaf should satisfy mid interface"), Actor->GetClass()->ImplementsInterface(MidInterface));
	}
	if (LeafInterface != nullptr)
	{
		TestTrue(TEXT("Actor implementing leaf should satisfy leaf interface"), Actor->GetClass()->ImplementsInterface(LeafInterface));
	}

	return true;
}

bool FAngelscriptScenarioInterfaceMultipleInheritanceDispatchTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceMultiDispatch"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceMultiDispatch.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIBaseDispatchChain
{
	int BaseValue();
}

UINTERFACE()
interface UIMidDispatchChain : UIBaseDispatchChain
{
	int MidValue();
}

UINTERFACE()
interface UILeafDispatchChain : UIMidDispatchChain
{
	int LeafValue();
}

UCLASS()
class AScenarioInterfaceMultiDispatch : AAngelscriptActor, UILeafDispatchChain
{
	UPROPERTY()
	int BaseResult = 0;

	UPROPERTY()
	int MidResult = 0;

	UPROPERTY()
	int LeafResult = 0;

	UFUNCTION()
	int BaseValue()
	{
		return 2;
	}

	UFUNCTION()
	int MidValue()
	{
		return 4;
	}

	UFUNCTION()
	int LeafValue()
	{
		return 8;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIBaseDispatchChain BaseRef = Cast<UIBaseDispatchChain>(Self);
		UIMidDispatchChain MidRef = Cast<UIMidDispatchChain>(Self);
		UILeafDispatchChain LeafRef = Cast<UILeafDispatchChain>(Self);

		if (BaseRef != nullptr)
		{
			BaseResult = BaseRef.BaseValue();
		}
		if (MidRef != nullptr)
		{
			MidResult = MidRef.MidValue();
		}
		if (LeafRef != nullptr)
		{
			LeafResult = LeafRef.LeafValue();
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceMultiDispatch"));
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

	int32 BaseResult = 0;
	int32 MidResult = 0;
	int32 LeafResult = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BaseResult"), BaseResult)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("MidResult"), MidResult)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("LeafResult"), LeafResult))
	{
		return false;
	}

	TestEqual(TEXT("Base interface method should dispatch through a leaf implementation"), BaseResult, 2);
	TestEqual(TEXT("Mid interface method should dispatch through a leaf implementation"), MidResult, 4);
	TestEqual(TEXT("Leaf interface method should dispatch through a leaf implementation"), LeafResult, 8);
	return true;
}

#endif
