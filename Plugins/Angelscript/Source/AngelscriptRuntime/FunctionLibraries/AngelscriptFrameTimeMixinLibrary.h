#pragma once

#include "CoreMinimal.h"
#include "Misc/QualifiedFrameTime.h"
#include "AngelscriptFrameTimeMixinLibrary.generated.h"

UCLASS(meta = (ScriptMixin = "FQualifiedFrameTime"))
class ANGELSCRIPTRUNTIME_API UAngelscriptFrameTimeMixinLibrary : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FrameTime")
	static double AsSeconds(const FQualifiedFrameTime& Target)
	{
		return Target.AsSeconds();
	}
};
