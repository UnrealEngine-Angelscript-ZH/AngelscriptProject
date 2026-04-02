#include "AngelscriptRuntimeModule.h"
#include "AngelscriptEngine.h"
#include "AngelscriptGameInstanceSubsystem.h"

IMPLEMENT_MODULE(FAngelscriptRuntimeModule, AngelscriptRuntime);

void FAngelscriptRuntimeModule::StartupModule()
{
	if (GIsEditor || IsRunningCommandlet())
	{
		InitializeAngelscript();
	}

	if (GIsEditor)
	{
		FallbackTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FAngelscriptRuntimeModule::TickFallbackPrimaryEngine));
	}
}

void FAngelscriptRuntimeModule::ShutdownModule()
{
	if (FallbackTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FallbackTickHandle);
		FallbackTickHandle.Reset();
	}
}

FAngelscriptGetDynamicSpawnLevel& FAngelscriptRuntimeModule::GetDynamicSpawnLevel()
{
	static FAngelscriptGetDynamicSpawnLevel Delegate;
	return Delegate;
}

FAngelscriptDebugCheckBreakOptions& FAngelscriptRuntimeModule::GetDebugCheckBreakOptions()
{
	static FAngelscriptDebugCheckBreakOptions Delegate;
	return Delegate;
}

FAngelscriptGetDebugBreakFilters& FAngelscriptRuntimeModule::GetDebugBreakFilters()
{
	static FAngelscriptGetDebugBreakFilters Delegate;
	return Delegate;
}

FAngelscriptDebugObjectSuffix& FAngelscriptRuntimeModule::GetDebugObjectSuffix()
{
	static FAngelscriptDebugObjectSuffix Delegate;
	return Delegate;
}

FAngelscriptComponentCreated& FAngelscriptRuntimeModule::GetComponentCreated()
{
	static FAngelscriptComponentCreated Delegate;
	return Delegate;
}

FAngelscriptCompilationDelegate& FAngelscriptRuntimeModule::GetPreCompile()
{
	static FAngelscriptCompilationDelegate Delegate;
	return Delegate;
}

FAngelscriptCompilationDelegate& FAngelscriptRuntimeModule::GetPostCompile()
{
	static FAngelscriptCompilationDelegate Delegate;
	return Delegate;
}

FAngelscriptCompilationDelegate& FAngelscriptRuntimeModule::GetOnInitialCompileFinished()
{
	static FAngelscriptCompilationDelegate Delegate;
	return Delegate;
}

FAngelscriptClassAnalyzeDelegate& FAngelscriptRuntimeModule::GetClassAnalyze()
{
	static FAngelscriptClassAnalyzeDelegate Delegate;
	return Delegate;
}

FAngelscriptPostCompileClassCollection& FAngelscriptRuntimeModule::GetPostCompileClassCollection()
{
	static FAngelscriptPostCompileClassCollection Delegate;
	return Delegate;
}

FAngelscriptPreGenerateClasses& FAngelscriptRuntimeModule::GetPreGenerateClasses()
{
	static FAngelscriptPreGenerateClasses Delegate;
	return Delegate;
}

FAngelscriptLiteralAssetCreated& FAngelscriptRuntimeModule::GetOnLiteralAssetCreated()
{
	static FAngelscriptLiteralAssetCreated Delegate;
	return Delegate;
}

FAngelscriptLiteralAssetCreated& FAngelscriptRuntimeModule::GetPostLiteralAssetSetup()
{
	static FAngelscriptLiteralAssetCreated Delegate;
	return Delegate;
}

FAngelscriptDebugListAssets& FAngelscriptRuntimeModule::GetDebugListAssets()
{
	static FAngelscriptDebugListAssets Delegate;
	return Delegate;
}

FAngelscriptEditorCreateBlueprint& FAngelscriptRuntimeModule::GetEditorCreateBlueprint()
{
	static FAngelscriptEditorCreateBlueprint Delegate;
	return Delegate;
}

FAngelscriptEditorGetCreateBlueprintDefaultAssetPath& FAngelscriptRuntimeModule::GetEditorGetCreateBlueprintDefaultAssetPath()
{
	static FAngelscriptEditorGetCreateBlueprintDefaultAssetPath Delegate;
	return Delegate;
}

void FAngelscriptRuntimeModule::InitializeAngelscript()
{
	static bool bInitialized = false;
	if (bInitialized)
		return;

	bInitialized = true;
	FModuleManager::Get().LoadModuleChecked(TEXT("AngelscriptRuntime"));
	FAngelscriptEngine::GetOrCreate().Initialize();
}

bool FAngelscriptRuntimeModule::TickFallbackPrimaryEngine(float DeltaTime)
{
	if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
	{
		if (FAngelscriptEngine* GlobalEngine = FAngelscriptEngine::TryGetGlobalEngine())
		{
			if (GlobalEngine->ShouldTick())
			{
				GlobalEngine->Tick(DeltaTime);
			}
		}
	}

	return true;
}
