#pragma once

#include "Engine/LevelStreaming.h"
#include "AngelscriptLevelStreamingLibrary.generated.h"

UCLASS(meta = (ScriptMixin = "ULevelStreaming"))
class ANGELSCRIPTRUNTIME_API UAngelscriptLevelStreamingLibrary : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable)
	static bool GetShouldBeVisibleInEditor(const ULevelStreaming* LevelStreaming)
	{
		return LevelStreaming->GetShouldBeVisibleInEditor();
	}
#endif
};
