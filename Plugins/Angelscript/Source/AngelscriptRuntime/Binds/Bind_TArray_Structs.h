#pragma once
#include "CoreMinimal.h"
#include "Containers/ScriptArray.h"

struct ANGELSCRIPTRUNTIME_API FArrayIterator
{
	FScriptArray* Array;
	uint32 Stride;
	uint32 Index;
	bool bCanProceed;

	FArrayIterator() {}
	FArrayIterator& operator=(const FArrayIterator& Other) = default;

	static void CopyConstruct(FArrayIterator& Iterator, const FArrayIterator& Other);
	static void Destruct(FArrayIterator& Iterator);
	static FArrayIterator Create(FScriptArray& Array, class asCObjectType* Meta);
	static FArrayIterator CreateForEach(FScriptArray& Array, class asCObjectType* Meta);

	FArrayIterator& Assignment(const FArrayIterator& Other);
	void* Proceed();
	void* GetCurrent() const;
	void Advance();

	static void AddArrayToIteratorDebugging(void* Array);
	static void VerifyArrayIteratorDebuggingModification(void* Array);
};
