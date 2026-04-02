#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindExecutionSnapshot
{
	int32 InvocationCount = 0;
	TArray<FName> DisabledBindNames;
	TArray<FName> ExecutedBindNames;
	double BindScriptTypesDurationSeconds = 0.0;
	double CallBindsDurationSeconds = 0.0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindExecutionObservation
{
	static void Reset();
	static FAngelscriptBindExecutionSnapshot GetLastSnapshot();
	static int32 GetInvocationCount();

	static void BeginBindScriptTypesTiming();
	static void EndBindScriptTypesTiming();
	static void BeginObservationPass(const TSet<FName>& DisabledBindNames);
	static void RecordExecutedBind(FName BindName);
	static void EndObservationPass();
};

#endif
