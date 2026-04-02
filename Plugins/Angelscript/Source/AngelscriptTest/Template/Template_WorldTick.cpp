#include "../Scenarios/AngelscriptScenarioTestUtils.h"
#include "../Shared/AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Core/AngelscriptActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

namespace TemplateWorldTickTest
{
	void BeginPlayActor(AActor& Actor)
	{
		if (!Actor.HasActorBegunPlay())
		{
			Actor.DispatchBeginPlay();
		}
	}

	void TickWorld(UWorld& World, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			World.Tick(ELevelTick::LEVELTICK_All, DeltaTime);

			for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
			{
				if (AActor* Actor = *ActorIt)
				{
					Actor->Tick(DeltaTime);

					TArray<UActorComponent*> Components;
					Actor->GetComponents(Components);
					for (UActorComponent* Component : Components)
					{
						if (Component != nullptr && Component->IsRegistered() && Component->IsComponentTickEnabled())
						{
							Component->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, &Component->PrimaryComponentTick);
						}
					}
				}
			}
		}
	}

	struct FScopedGameplayWorld
	{
		explicit FScopedGameplayWorld(FAutomationTestBase& InTest)
			: Test(InTest)
		{
			Spawner.InitializeGameSubsystems();
			World = &Spawner.GetWorld();
			bIsValid = Test.TestNotNull(TEXT("Template gameplay world should be created"), World);
		}

		bool IsValid() const
		{
			return bIsValid && World != nullptr;
		}

		void BeginPlay(AActor& Actor) const
		{
			BeginPlayActor(Actor);
		}

		void Tick(float DeltaTime, int32 NumTicks) const
		{
			if (IsValid())
			{
				TickWorld(*World, DeltaTime, NumTicks);
			}
		}

		template <typename ActorType = AActor>
		ActorType* SpawnActorOfClass(
			UClass* ActorClass,
			const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(),
			const FVector& Location = FVector::ZeroVector,
			const FRotator& Rotation = FRotator::ZeroRotator)
		{
			if (!Test.TestNotNull(TEXT("Template actor class should be valid for spawning"), ActorClass) || !IsValid())
			{
				return nullptr;
			}

			return &Spawner.SpawnActorAt<ActorType>(Location, Rotation, SpawnParameters, ActorClass);
		}

		UWorld& GetWorld() const
		{
			return *World;
		}

	private:
		FAutomationTestBase& Test;
		FActorTestSpawner Spawner;
		UWorld* World = nullptr;
		bool bIsValid = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplateWorldTickScriptActorTest,
	"Angelscript.Template.WorldTick.ScriptActorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplateWorldTickScriptActorTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("TemplateWorldTickScriptActor"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("TemplateWorldTickScriptActor.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateWorldTickScriptActor : AAngelscriptActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
		TEXT("ATemplateWorldTickScriptActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	TemplateWorldTickTest::FScopedGameplayWorld WorldTemplate(*this);
	if (!WorldTemplate.IsValid())
	{
		return false;
	}

	AActor* Actor = WorldTemplate.SpawnActorOfClass<AActor>(ScriptClass);
	if (!TestNotNull(TEXT("WorldTick template should spawn the generated script actor"), Actor))
	{
		return false;
	}

	WorldTemplate.BeginPlay(*Actor);
	WorldTemplate.Tick(0.016f, 3);

	int32 BeginPlayCount = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BeginPlayCount"), BeginPlayCount))
	{
		return false;
	}

	int32 TickCount = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("TickCount"), TickCount))
	{
		return false;
	}

	TestEqual(TEXT("WorldTick template should drive BeginPlay exactly once"), BeginPlayCount, 1);
	TestEqual(TEXT("WorldTick template should drive Tick the requested number of times"), TickCount, 3);
	return true;
}

#endif
