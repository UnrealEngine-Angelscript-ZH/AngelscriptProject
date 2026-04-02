#include "Testing/AngelscriptBindExecutionObservation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCriticalSection GAngelscriptBindExecutionObservationMutex;
	FAngelscriptBindExecutionSnapshot GAngelscriptBindExecutionSnapshot;
	double GBindScriptTypesStartTime = 0.0;
	double GCallBindsStartTime = 0.0;
}

void FAngelscriptBindExecutionObservation::Reset()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GAngelscriptBindExecutionSnapshot = FAngelscriptBindExecutionSnapshot();
}

FAngelscriptBindExecutionSnapshot FAngelscriptBindExecutionObservation::GetLastSnapshot()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	return GAngelscriptBindExecutionSnapshot;
}

int32 FAngelscriptBindExecutionObservation::GetInvocationCount()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	return GAngelscriptBindExecutionSnapshot.InvocationCount;
}

void FAngelscriptBindExecutionObservation::BeginBindScriptTypesTiming()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GBindScriptTypesStartTime = FPlatformTime::Seconds();
	GAngelscriptBindExecutionSnapshot.BindScriptTypesDurationSeconds = 0.0;
}

void FAngelscriptBindExecutionObservation::EndBindScriptTypesTiming()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GBindScriptTypesStartTime > 0.0)
	{
		GAngelscriptBindExecutionSnapshot.BindScriptTypesDurationSeconds = FPlatformTime::Seconds() - GBindScriptTypesStartTime;
		GBindScriptTypesStartTime = 0.0;
	}
}

void FAngelscriptBindExecutionObservation::BeginObservationPass(const TSet<FName>& DisabledBindNames)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	++GAngelscriptBindExecutionSnapshot.InvocationCount;
	GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Reset();
	GAngelscriptBindExecutionSnapshot.DisabledBindNames = DisabledBindNames.Array();
	GAngelscriptBindExecutionSnapshot.DisabledBindNames.Sort(FNameLexicalLess());
	GCallBindsStartTime = FPlatformTime::Seconds();
	GAngelscriptBindExecutionSnapshot.CallBindsDurationSeconds = 0.0;
}

void FAngelscriptBindExecutionObservation::RecordExecutedBind(FName BindName)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Add(BindName);
}

void FAngelscriptBindExecutionObservation::EndObservationPass()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GCallBindsStartTime > 0.0)
	{
		GAngelscriptBindExecutionSnapshot.CallBindsDurationSeconds = FPlatformTime::Seconds() - GCallBindsStartTime;
		GCallBindsStartTime = 0.0;
	}
}

#endif
