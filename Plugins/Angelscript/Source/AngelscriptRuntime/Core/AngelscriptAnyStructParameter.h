#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "AngelscriptAnyStructParameter.generated.h"

USTRUCT(BlueprintType)
struct ANGELSCRIPTRUNTIME_API FAngelscriptAnyStructParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Struct Data")
	FInstancedStruct InstancedStruct;
};
