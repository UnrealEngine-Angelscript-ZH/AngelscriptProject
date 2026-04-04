#pragma once

#include "../Shared/AngelscriptTestEngineHelper.h"
#include "../Shared/AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace AngelscriptScenarioTestUtils
{
	inline UClass* CompileScriptModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const FString& Filename,
		const FString& ScriptSource,
		FName GeneratedClassName)
	{
		FAngelscriptEngineScope EngineScope(Engine);

		if (!Test.TestTrue(
			*FString::Printf(TEXT("Scenario module '%s' should compile"), *ModuleName.ToString()),
			AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource)))
		{
			return nullptr;
		}

		UClass* ScriptClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, GeneratedClassName);
		Test.TestNotNull(
			*FString::Printf(TEXT("Scenario class '%s' should be generated"), *GeneratedClassName.ToString()),
			ScriptClass);
		return ScriptClass;
	}

	inline void TickWorld(FAngelscriptEngine& Engine, UWorld& World, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope WorldScope(Engine);
			World.Tick(ELevelTick::LEVELTICK_All, DeltaTime);

			for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
			{
				if (AActor* Actor = *ActorIt)
				{
					FAngelscriptEngineScope ActorScope(Engine, Actor);
					Actor->Tick(DeltaTime);

					TArray<UActorComponent*> Components;
					Actor->GetComponents(Components);
					for (UActorComponent* Component : Components)
					{
						if (Component != nullptr && Component->IsRegistered() && Component->IsComponentTickEnabled())
						{
							FAngelscriptEngineScope ComponentScope(Engine, Component);
							Component->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, &Component->PrimaryComponentTick);
						}
					}
				}
			}
		}
	}

	inline void BeginPlayActor(FAngelscriptEngine& Engine, AActor& Actor)
	{
		FAngelscriptEngineScope ActorScope(Engine, &Actor);

		if (!Actor.HasActorBegunPlay())
		{
			Actor.DispatchBeginPlay();
		}
	}

	template <typename ActorType = AActor>
	inline ActorType* SpawnScriptActor(
		FAutomationTestBase& Test,
		FActorTestSpawner& Spawner,
		UClass* ScriptClass,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(),
		const FVector& Location = FVector::ZeroVector,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (!Test.TestNotNull(TEXT("Scenario actor class should be valid for spawning"), ScriptClass))
		{
			return nullptr;
		}

		return &Spawner.SpawnActorAt<ActorType>(Location, Rotation, SpawnParameters, ScriptClass);
	}

	template <typename PropertyType, typename ValueType>
	inline bool ReadPropertyValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		ValueType& OutValue)
	{
		if (!Test.TestNotNull(TEXT("Scenario object should be valid for reflected property reads"), Object))
		{
			return false;
		}

		PropertyType* Property = FindFProperty<PropertyType>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Scenario property '%s' should exist"), *PropertyName.ToString()),
			Property))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}
}
