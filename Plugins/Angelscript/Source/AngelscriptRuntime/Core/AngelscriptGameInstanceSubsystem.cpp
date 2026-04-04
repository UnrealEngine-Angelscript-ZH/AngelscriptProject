#include "AngelscriptGameInstanceSubsystem.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

int32 UAngelscriptGameInstanceSubsystem::ActiveTickOwners = 0;

UAngelscriptGameInstanceSubsystem::~UAngelscriptGameInstanceSubsystem() = default;

void UAngelscriptGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bInitialized = true;
	PrimaryEngine = FAngelscriptEngine::TryGetGlobalEngine();
	if (PrimaryEngine == nullptr)
	{
		OwnedPrimaryEngine = MakeUnique<FAngelscriptEngine>();
		PrimaryEngine = OwnedPrimaryEngine.Get();
		FAngelscriptEngine::SetGlobalEngine(PrimaryEngine);
		PrimaryEngine->Initialize();
		bOwnsPrimaryEngine = true;
	}

	if (PrimaryEngine != nullptr)
	{
		++ActiveTickOwners;
	}
}

void UAngelscriptGameInstanceSubsystem::Deinitialize()
{
	if (PrimaryEngine != nullptr)
	{
		ActiveTickOwners = FMath::Max(0, ActiveTickOwners - 1);
	}

	if (bOwnsPrimaryEngine)
	{
		if (FAngelscriptEngine::TryGetGlobalEngine() == PrimaryEngine)
		{
			FAngelscriptEngine::SetGlobalEngine(nullptr);
		}

		OwnedPrimaryEngine.Reset();
		bOwnsPrimaryEngine = false;
	}

	PrimaryEngine = nullptr;
	bInitialized = false;

	Super::Deinitialize();
}

UWorld* UAngelscriptGameInstanceSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

ETickableTickType UAngelscriptGameInstanceSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType();
}

bool UAngelscriptGameInstanceSubsystem::IsAllowedToTick() const
{
	return !IsTemplate() && bInitialized && PrimaryEngine != nullptr;
}

bool UAngelscriptGameInstanceSubsystem::IsTickableInEditor() const
{
	return true;
}

bool UAngelscriptGameInstanceSubsystem::IsTickableWhenPaused() const
{
	return true;
}

void UAngelscriptGameInstanceSubsystem::Tick(float DeltaTime)
{
	if (PrimaryEngine != nullptr && PrimaryEngine->ShouldTick())
	{
		PrimaryEngine->Tick(DeltaTime);
	}
}

TStatId UAngelscriptGameInstanceSubsystem::GetStatId() const
{
	return GetStatID();
}

UAngelscriptGameInstanceSubsystem* UAngelscriptGameInstanceSubsystem::GetCurrent()
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::CurrentWorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
}

bool UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()
{
	return ActiveTickOwners > 0;
}
