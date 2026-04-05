#include "Testing/AngelscriptUhtOverloadCoverageTypes.h"

int32 UAngelscriptUhtOverloadCoverageLibrary::ResolveCoverageOverload(UObject* Target, int32 Value)
{
	return Target != nullptr ? Value : INDEX_NONE;
}

float UAngelscriptUhtOverloadCoverageLibrary::ResolveCoverageOverload(UObject* Target, float Value)
{
	return Target != nullptr ? Value : -1.0f;
}
