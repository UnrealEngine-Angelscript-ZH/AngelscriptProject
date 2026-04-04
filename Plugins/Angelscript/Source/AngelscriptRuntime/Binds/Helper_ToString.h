#pragma once
#include "CoreMinimal.h"

class asITypeInfo;

struct FToStringHelper
{
	typedef void(*FToStringFunction)(void*, FString&);

	template<typename T>
	static void Register(const FString& TypeName, T ToString, bool bImplicitConversion = false, bool bIsHandleType = false)
	{
		Register(TypeName, (FToStringFunction)ToString, bImplicitConversion, bIsHandleType);
	}

	static void ANGELSCRIPTRUNTIME_API Register(const FString& TypeName, FToStringFunction ToString, bool bImplicitConversion = false, bool bIsHandleType = false);
	static void ANGELSCRIPTRUNTIME_API Generic_AppendToString(FString& AppendTo, void* ValuePtr, int TypeId);
	static void ANGELSCRIPTRUNTIME_API Reset();
	static void ANGELSCRIPTRUNTIME_API ResetForKey(const void* StateKey);
#if WITH_DEV_AUTOMATION_TESTS
	static int32 ANGELSCRIPTRUNTIME_API GetRegisteredTypeCountForTesting();
#endif
};

struct FToStringType
{
	FString TypeName;
	asITypeInfo* TypeInfo = nullptr;
	FToStringHelper::FToStringFunction ToString;
	bool bImplicitConversion;
	bool bIsHandleType;
};
