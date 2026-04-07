#pragma once

#include "CoreMinimal.h"

class UFunction;
class UObject;
class asIScriptGeneric;
struct FAngelscriptType;
struct FFuncEntry;
struct FAngelscriptFunctionSignature;

enum class EAngelscriptReflectiveFallbackEligibility : uint8
{
	Eligible,
	RejectedNullFunction,
	RejectedMissingOwningClass,
	RejectedInterfaceClass,
	RejectedCustomThunk,
	RejectedTooManyArguments,
};

ANGELSCRIPTRUNTIME_API EAngelscriptReflectiveFallbackEligibility EvaluateReflectiveFallbackEligibility(const UFunction* Function);
ANGELSCRIPTRUNTIME_API bool ShouldBindBlueprintCallableReflectiveFallback(const UFunction* Function);
ANGELSCRIPTRUNTIME_API bool InvokeReflectiveUFunctionFromGenericCall(
	asIScriptGeneric* Generic,
	UObject* TargetObject,
	UFunction* Function,
	bool bInjectMixinObject = false);
bool IsScriptDeclarationAlreadyBound(TSharedRef<FAngelscriptType> InType, const FAngelscriptFunctionSignature& Signature);

ANGELSCRIPTRUNTIME_API bool BindBlueprintCallableReflectiveFallback(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptFunctionSignature& Signature,
	FFuncEntry& Entry);
