#pragma once

#include "AngelscriptEngine.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "HAL/FileManager.h"
#include "Containers/StringConv.h"
#include "Misc/Crc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

struct FAngelscriptTestEngineScopeAccess
{
	static FAngelscriptEngine* GetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static bool DestroyGlobalEngine()
	{
		return FAngelscriptEngine::DestroyGlobal();
	}
	};

namespace AngelscriptTestSupport
{
	using FAngelscriptTestEngineScopeAccess = ::FAngelscriptTestEngineScopeAccess;
	using FScopedGlobalEngineOverride = ::FScopedGlobalEngineOverride;

	inline TUniquePtr<FAngelscriptEngine>& GetSharedTestEngineStorage()
	{
		static TUniquePtr<FAngelscriptEngine> Storage;
		return Storage;
	}

	struct FScopedTestWorldContextScope
	{
		explicit FScopedTestWorldContextScope(UObject* WorldContextObject)
		{
			if (WorldContextObject != nullptr)
			{
				Scope = MakeUnique<FAngelscriptGameThreadScopeWorldContext>(WorldContextObject);
			}
		}

	private:
		TUniquePtr<FAngelscriptGameThreadScopeWorldContext> Scope;
	};

	inline FAngelscriptEngine* TryGetRunningProductionEngine()
	{
		if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
		{
			if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
			{
				return AttachedEngine;
			}
		}

		if (FAngelscriptEngine::IsInitialized())
		{
			return &FAngelscriptEngine::Get();
		}

		return nullptr;
	}

	inline TUniquePtr<FAngelscriptEngine> CreateIsolatedFullEngine()
	{
		FAngelscriptEngineConfig Config;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::CreateTestingFullEngine(Config, Dependencies);
		return Engine;
	}

	inline TUniquePtr<FAngelscriptEngine> CreateIsolatedCloneEngine()
	{
		FAngelscriptEngineConfig Config;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::CreateForTesting(Config, Dependencies, EAngelscriptEngineCreationMode::Clone);
	}

	inline FAngelscriptEngine& GetOrCreateSharedCloneEngine()
	{
		static TUniquePtr<FAngelscriptEngine> SharedCloneEngine;
		if (!SharedCloneEngine.IsValid())
		{
			SharedCloneEngine = CreateIsolatedCloneEngine();
		}

		check(SharedCloneEngine.IsValid());
		return *SharedCloneEngine;
	}

	inline void ResetSharedCloneEngine(FAngelscriptEngine& Engine)
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}

		CollectGarbage(RF_NoFlags, true);
	}

	inline FAngelscriptEngine& AcquireCleanSharedCloneEngine()
	{
		FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
		ResetSharedCloneEngine(Engine);
		return Engine;
	}

	inline FAngelscriptEngine& AcquireCleanSharedCloneEngineAndOverrideGlobal(FScopedGlobalEngineOverride& OutScope)
	{
		FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
		ResetSharedCloneEngine(Engine);
		OutScope = FScopedGlobalEngineOverride(&Engine);
		return Engine;
	}

	inline FAngelscriptEngine* RequireRunningProductionEngine(FAutomationTestBase& Test, const TCHAR* ErrorContext)
	{
		if (FAngelscriptEngine* ProductionEngine = TryGetRunningProductionEngine())
		{
			return ProductionEngine;
		}

		Test.AddError(ErrorContext);
		return nullptr;
	}

	inline void ReportCompileDiagnostics(FAutomationTestBase& Test, const FAngelscriptEngine& Engine, const FString& AbsoluteFilename)
	{
		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr || Diagnostics->Diagnostics.IsEmpty())
		{
			return;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			if (!Diagnostic.bIsError)
			{
				continue;
			}

			Test.AddError(FString::Printf(
				TEXT("%s:%d:%d: %s"),
				*AbsoluteFilename,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}
	}

	struct FScopedAutomaticImportsOverride
	{
		explicit FScopedAutomaticImportsOverride(asIScriptEngine* InScriptEngine)
			: ScriptEngine(InScriptEngine)
			, PreviousValue(InScriptEngine != nullptr ? InScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS) : 0)
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 0);
			}
		}

		~FScopedAutomaticImportsOverride()
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousValue);
			}
		}

		asIScriptEngine* ScriptEngine = nullptr;
		asPWORD PreviousValue = 0;
	};

	inline asIScriptModule* BuildModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName, const FString& Source)
	{
		const FString RequestedModuleName = ANSI_TO_TCHAR(ModuleName);
		const FString UniqueFilename = FString::Printf(TEXT("%s_%s.as"), *RequestedModuleName, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString RelativeFilename = FString::Printf(TEXT("%s.as"), *RequestedModuleName);
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), UniqueFilename);
		const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		TArray<FString> ExistingAutomationFiles;
		IFileManager::Get().FindFiles(ExistingAutomationFiles, *(AutomationDirectory / (RequestedModuleName + TEXT("*.as"))), true, false);
		for (const FString& ExistingFilename : ExistingAutomationFiles)
		{
			IFileManager::Get().Delete(*(AutomationDirectory / ExistingFilename), false, true, true);
		}

		if (!FFileHelper::SaveStringToFile(Source, *AbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write script module '%s' to '%s'"), *RequestedModuleName, *AbsoluteFilename));
			return nullptr;
		}

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to preprocess script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(FAngelscriptEngine::bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to compile script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		if (!Test.TestTrue(TEXT("Exactly one Angelscript test module should compile"), CompiledModules.Num() == 1))
		{
			return nullptr;
		}

		asIScriptModule* Module = CompiledModules[0]->ScriptModule;
		const FString ModuleContext = FString::Printf(TEXT("Compiled script module '%s' should have a backing asIScriptModule"), *RequestedModuleName);
		if (!Test.TestNotNull(*ModuleContext, Module))
		{
			return nullptr;
		}

		return Module;
	}

	inline asIScriptFunction* GetFunctionByDecl(FAutomationTestBase& Test, asIScriptModule& Module, const FString& Declaration)
	{
		FString FunctionName;
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
					if (!FunctionName.IsEmpty())
					{
						FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
						Function = Module.GetFunctionByName(FunctionNameUtf8.Get());
					}
				}
			}
		}

		if (Function == nullptr && !FunctionName.IsEmpty())
		{
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
				{
					Function = CandidateFunction;
					break;
				}
			}
		}

		if (Function == nullptr)
		{
			FString AvailableFunctions;
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction == nullptr)
				{
					continue;
				}

				if (!AvailableFunctions.IsEmpty())
				{
					AvailableFunctions += TEXT(", ");
				}

				AvailableFunctions += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
			}

			if (AvailableFunctions.IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; module exposes no global functions"), *Declaration));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; available functions: %s"), *Declaration, *AvailableFunctions));
			}
		}

		return Function;
	}

	inline bool ExecuteIntFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int32& OutValue)
	{
		FScopedTestEngineGlobalScope GlobalScope(&Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}

	inline bool ExecuteInt64Function(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int64& OutValue)
	{
		FScopedTestEngineGlobalScope GlobalScope(&Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare int64 function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute int64 function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int64>(Context->GetReturnQWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}

}
