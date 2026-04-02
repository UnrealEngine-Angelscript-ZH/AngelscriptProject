#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AngelscriptGameInstanceSubsystem.generated.h"

struct FAngelscriptEngine;

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptGameInstanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override final;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	FAngelscriptEngine* GetEngine() const
	{
		return PrimaryEngine;
	}

	static UAngelscriptGameInstanceSubsystem* GetCurrent();
	static bool HasAnyTickOwner();

private:
	friend struct FAngelscriptTickBehaviorTestAccess;
	TUniquePtr<FAngelscriptEngine> OwnedPrimaryEngine;
	FAngelscriptEngine* PrimaryEngine = nullptr;
	bool bOwnsPrimaryEngine = false;
	bool bInitialized = false;
	static int32 ActiveTickOwners;
};
