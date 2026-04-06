#pragma once

#include "CoreMinimal.h"

#include "Shared/AngelscriptTestEngineHelper.h"

namespace AngelscriptTestSupport
{
	struct FAngelscriptDebuggerScriptFixture
	{
		static FAngelscriptDebuggerScriptFixture CreateBreakpointFixture();
		static FAngelscriptDebuggerScriptFixture CreateSteppingFixture();
		static FAngelscriptDebuggerScriptFixture CreateCallstackFixture();
		static FAngelscriptDebuggerScriptFixture CreateBindingFixture();

		bool Compile(FAngelscriptEngine& Engine) const;
		int32 GetLine(FName Marker) const;
		FString GetEvalPath(FName Marker) const;
		UClass* FindGeneratedClass(FAngelscriptEngine& Engine) const;
		UFunction* FindGeneratedFunction(FAngelscriptEngine& Engine, FName FunctionName) const;

		FName ModuleName;
		FName GeneratedClassName;
		FName EntryFunctionName;
		FString EntryFunctionDeclaration;
		FString Filename;
		FString ScriptSource;
		TMap<FName, int32> LineMarkers;
		TMap<FName, FString> EvalPaths;
		bool bUseAnnotatedCompilation = true;
	};
}
