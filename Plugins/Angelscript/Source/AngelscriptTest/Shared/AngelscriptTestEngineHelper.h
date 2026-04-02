#pragma once

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Shared/AngelscriptTestUtilities.h"

namespace AngelscriptTestSupport
{
	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	bool AnalyzeReloadFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script, FAngelscriptClassGenerator::EReloadRequirement& OutReloadRequirement, bool& bOutWantsFullReload, bool& bOutNeedsFullReload);
	bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	bool CompileAnnotatedModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	bool ExecuteGeneratedIntEventOnGameThread(UObject* Object, UFunction* Function, int32& OutResult);
	bool ExecuteIntFunction(FAngelscriptEngine* Engine, FName ModuleName, FString Decl, int32& OutResult);
	UClass* FindGeneratedClass(FAngelscriptEngine* Engine, FName ClassName);
	UFunction* FindGeneratedFunction(UClass* OwnerClass, FName FuncName);
}
