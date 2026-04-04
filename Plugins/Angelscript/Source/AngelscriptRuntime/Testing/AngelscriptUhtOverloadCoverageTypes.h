#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AngelscriptUhtOverloadCoverageTypes.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptUhtOverloadCoverageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Angelscript|UHT|Coverage")
	static int32 ResolveCoverageOverload(UObject* Target, int32 Value);

	static float ResolveCoverageOverload(UObject* Target, float Value);
};
