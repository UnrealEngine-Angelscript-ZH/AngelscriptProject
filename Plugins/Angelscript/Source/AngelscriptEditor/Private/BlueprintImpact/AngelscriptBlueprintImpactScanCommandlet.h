#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "AngelscriptBlueprintImpactScanCommandlet.generated.h"

UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptBlueprintImpactScanCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
