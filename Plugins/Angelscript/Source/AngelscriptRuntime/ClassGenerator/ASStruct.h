#pragma once

#include "CoreMinimal.h"

#include "AngelscriptEngine.h"

#include "ASStruct.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UASStruct : public UScriptStruct
{
	GENERATED_BODY()
public:
	UASStruct* NewerVersion = nullptr;
	class asITypeInfo* ScriptType = nullptr;
	FGuid Guid;
	bool bIsScriptStruct;

	UScriptStruct* GetNewestVersion()
	{
#if !AS_CAN_HOTRELOAD
		return this;
#else
		if (NewerVersion == nullptr)
			return this;

		UASStruct* NewerStruct = NewerVersion;
		while (NewerStruct->NewerVersion != nullptr)
			NewerStruct = NewerStruct->NewerVersion;
		return NewerStruct;
#endif
	}

	class asIScriptFunction* GetToStringFunction() const;

	FGuid GetCustomGuid() const override
	{
		return Guid;
	}

	void SetGuid(FName FromName);

	UASStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void UpdateScriptType();

	void PrepareCppStructOps() override;
	ICppStructOps* CreateCppStructOps();

	void SetCppStructOps(ICppStructOps* Ops)
	{
		CppStructOps = Ops;
	} 
};

USTRUCT(BlueprintType)
struct FScriptStructWildcard
{
	GENERATED_BODY()
};
