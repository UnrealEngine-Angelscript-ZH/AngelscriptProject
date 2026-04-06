#pragma once

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Shared/AngelscriptTestUtilities.h"

namespace AngelscriptTestSupport
{
	struct FAngelscriptCompileTraceDiagnosticSummary
	{
		FString Section;
		int32 Row = 0;
		int32 Column = 0;
		bool bIsError = false;
		bool bIsInfo = false;
		FString Message;
	};

	struct FAngelscriptCompileTraceSummary
	{
		bool bCompileSucceeded = false;
		ECompileType CompileType = ECompileType::SoftReloadOnly;
		ECompileResult CompileResult = ECompileResult::Error;
		bool bUsedPreprocessor = false;
		int32 ModuleDescCount = 0;
		int32 CompiledModuleCount = 0;
		TArray<FString> ModuleNames;
		TArray<FString> AbsoluteFilenames;
		TArray<FAngelscriptCompileTraceDiagnosticSummary> Diagnostics;
	};

	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	bool CompileModuleWithSummary(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, bool bUsePreprocessor, FAngelscriptCompileTraceSummary& OutSummary, bool bSuppressCompileErrorLogs = false);
	bool AnalyzeReloadFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script, FAngelscriptClassGenerator::EReloadRequirement& OutReloadRequirement, bool& bOutWantsFullReload, bool& bOutNeedsFullReload);
	bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	bool CompileAnnotatedModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	bool ExecuteIntFunction(FAngelscriptEngine* Engine, FString Filename, FName ModuleName, FString Decl, int32& OutResult);
	bool ExecuteGeneratedIntEventOnGameThread(FAngelscriptEngine* Engine, UObject* Object, UFunction* Function, int32& OutResult);
	inline bool ExecuteGeneratedIntEventOnGameThread(UObject* Object, UFunction* Function, int32& OutResult)
	{
		return ExecuteGeneratedIntEventOnGameThread(FAngelscriptEngine::TryGetCurrentEngine(), Object, Function, OutResult);
	}
	bool ExecuteIntFunction(FAngelscriptEngine* Engine, FName ModuleName, FString Decl, int32& OutResult);
	UClass* FindGeneratedClass(FAngelscriptEngine* Engine, FName ClassName);
	UFunction* FindGeneratedFunction(UClass* OwnerClass, FName FuncName);
}
