#include "CoreMinimal.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "Subsystems/Subsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#endif

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Subsystems((int32)FAngelscriptBinds::EOrder::Late + 150, []
{
	// Bind easy ::Get() accessor functions for all subsystem classes
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated))
			continue;
		if (!Class->HasAllClassFlags(CLASS_Native))
			continue;
		if (!Class->IsChildOf(USubsystem::StaticClass()))
			continue;

		auto Type = FAngelscriptType::GetByClass(Class);
		if (!Type.IsValid())
			continue;

		FString ClassName = Type->GetAngelscriptTypeName();
		FAngelscriptBinds::FNamespace ns(ClassName);

#if WITH_EDITOR
		if (Class->IsChildOf(UEditorSubsystem::StaticClass()))
		{
			if (FAngelscriptEngine::bUseEditorScripts)
			{
				FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
					[]() -> UEditorSubsystem*
					{
						UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
						return GEditor->GetEditorSubsystemBase(SubsystemClass);
					}, Class);
			}
		}
		else
#endif
		if (Class->IsChildOf(UEngineSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UEngineSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return GEngine->GetEngineSubsystemBase(SubsystemClass);
			}, Class);
		}
		else if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UGameInstanceSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::CurrentWorldContext, EGetWorldErrorMode::ReturnNull);
				if (World == nullptr)
					return nullptr;
				const UGameInstance* GameInstance = World->GetGameInstance();
				if (GameInstance == nullptr)
					return nullptr;

				return GameInstance->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
		else if (Class->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UWorldSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::CurrentWorldContext, EGetWorldErrorMode::ReturnNull);
				if (World == nullptr)
					return nullptr;

				return World->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
#if !WITH_ANGELSCRIPT_HAZE
		else if (Class->IsChildOf(ULocalPlayerSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get(ULocalPlayer LocalPlayer)"),
			[](ULocalPlayer* LocalPlayer) -> ULocalPlayerSubsystem*
			{
				if (LocalPlayer == nullptr)
					return nullptr;
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return LocalPlayer->GetSubsystemBase(SubsystemClass);
			}, Class);

			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get(APlayerController LocalPlayer)"),
			[](APlayerController* PlayerController) -> ULocalPlayerSubsystem*
			{
				if (PlayerController == nullptr)
					return nullptr;
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
				if (LocalPlayer == nullptr)
					return nullptr;
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return LocalPlayer->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
#endif
	}
});
