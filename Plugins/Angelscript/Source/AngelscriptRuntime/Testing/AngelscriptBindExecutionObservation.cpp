#include "Testing/AngelscriptBindExecutionObservation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCriticalSection GAngelscriptBindExecutionObservationMutex;
	FAngelscriptBindExecutionSnapshot GAngelscriptBindExecutionSnapshot;
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

void FAngelscriptBindExecutionObservation::BeginObservationPass(const TSet<FName>& DisabledBindNames)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	++GAngelscriptBindExecutionSnapshot.InvocationCount;
	GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Reset();
	GAngelscriptBindExecutionSnapshot.DisabledBindNames = DisabledBindNames.Array();
	GAngelscriptBindExecutionSnapshot.DisabledBindNames.Sort(FNameLexicalLess());
}

void FAngelscriptBindExecutionObservation::RecordExecutedBind(FName BindName)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Add(BindName);
}

#endif
