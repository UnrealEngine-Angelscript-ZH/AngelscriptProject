#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/ASClass.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

namespace AngelscriptTestSupport
{
	namespace
	{

		TSharedRef<FAngelscriptModuleDesc> MakeModuleDesc(FName ModuleName, FString Filename, FString Script)
		{
			const FString ModuleNameString = ModuleName.ToString();
			const FString RelativeFilename = FPaths::IsRelative(Filename)
				? FPaths::Combine(TEXT("Automation"), Filename)
				: FPaths::GetCleanFilename(Filename);
			const FString AbsoluteFilename = FPaths::IsRelative(Filename)
				? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Filename)
				: Filename;

			TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
			ModuleDesc->ModuleName = ModuleNameString;

			FAngelscriptModuleDesc::FCodeSection& Section = ModuleDesc->Code.AddDefaulted_GetRef();
			Section.RelativeFilename = RelativeFilename;
			Section.AbsoluteFilename = AbsoluteFilename;
			Section.Code = MoveTemp(Script);
			Section.CodeHash = static_cast<int64>(FCrc::StrCrc32(*Section.Code));
			ModuleDesc->CodeHash ^= Section.CodeHash;
			return ModuleDesc;
		}

		bool CompileModuleInternal(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult* OutCompileResult = nullptr)
		{
			if (Engine == nullptr)
			{
				if (OutCompileResult != nullptr)
				{
					*OutCompileResult = ECompileResult::Error;
				}
				return false;
			}

			TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeModuleDesc(ModuleName, MoveTemp(Filename), MoveTemp(Script));

			TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
			ModulesToCompile.Add(ModuleDesc);

			TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
			TGuardValue<bool> AutomaticImportGuard(FAngelscriptEngine::bUseAutomaticImportMethod, false);
			FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
			FScopedGlobalEngineOverride GlobalScope(Engine);
			const ECompileResult CompileResult = Engine->CompileModules(CompileType, ModulesToCompile, CompiledModules);
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = CompileResult;
			}
			return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
		}

		bool PreprocessAndCompile(FAngelscriptEngine* Engine, ECompileType CompileType, FString Filename, FString Script, ECompileResult* OutCompileResult = nullptr)
		{
			if (Engine == nullptr)
			{
				if (OutCompileResult != nullptr)
				{
					*OutCompileResult = ECompileResult::Error;
				}
				return false;
			}

			FString AbsoluteFilename;

			if (FPaths::IsRelative(Filename))
			{
				const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
				AbsoluteFilename = FPaths::Combine(TempDir, Filename);
				IFileManager::Get().MakeDirectory(*TempDir, true);
				if (!FFileHelper::SaveStringToFile(Script, *AbsoluteFilename))
				{
					if (OutCompileResult != nullptr)
					{
						*OutCompileResult = ECompileResult::Error;
					}
					return false;
				}
			}
			else
			{
				AbsoluteFilename = Filename;
			}

			FAngelscriptPreprocessor Preprocessor;
			Preprocessor.AddFile(Filename, AbsoluteFilename);
			if (!Preprocessor.Preprocess())
			{
				if (OutCompileResult != nullptr)
				{
					*OutCompileResult = ECompileResult::Error;
				}
				return false;
			}

			TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
			if (ModulesToCompile.Num() == 0)
			{
				if (OutCompileResult != nullptr)
				{
					*OutCompileResult = ECompileResult::Error;
				}
				return false;
			}

			TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
			TGuardValue<bool> AutomaticImportGuard(FAngelscriptEngine::bUseAutomaticImportMethod, false);
			FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
			FScopedGlobalEngineOverride GlobalScope(Engine);
			const ECompileResult CompileResult = Engine->CompileModules(CompileType, ModulesToCompile, CompiledModules);
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = CompileResult;
			}
			return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
		}

		asIScriptFunction* FindFunctionByDecl(asIScriptModule& Module, const FString& Declaration)
		{
			FTCHARToUTF8 DeclarationUtf8(*Declaration);
			if (asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get()))
			{
				return Function;
			}

			FString FunctionName;
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
				}
			}

			if (FunctionName.IsEmpty())
			{
				return nullptr;
			}

			FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
			if (asIScriptFunction* Function = Module.GetFunctionByName(FunctionNameUtf8.Get()))
			{
				return Function;
			}

			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
				{
					return CandidateFunction;
				}
			}

			return nullptr;
		}
	}

	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult)
	{
		const bool bHasAnnotations = Script.Contains(TEXT("UCLASS(")) || Script.Contains(TEXT("USTRUCT(")) || Script.Contains(TEXT("UENUM("));
		if (bHasAnnotations)
		{
			return PreprocessAndCompile(Engine, CompileType, MoveTemp(Filename), MoveTemp(Script), &OutCompileResult);
		}
		return CompileModuleInternal(Engine, CompileType, ModuleName, MoveTemp(Filename), MoveTemp(Script), &OutCompileResult);
	}

	bool AnalyzeReloadFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script, FAngelscriptClassGenerator::EReloadRequirement& OutReloadRequirement, bool& bOutWantsFullReload, bool& bOutNeedsFullReload)
	{
		ECompileResult CompileResult = ECompileResult::Error;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		const bool bCompiled = PreprocessAndCompile(Engine, ECompileType::SoftReloadOnly, MoveTemp(Filename), MoveTemp(Script), &CompileResult);
		UE_SET_LOG_VERBOSITY(Angelscript, Log);

		bOutWantsFullReload = false;
		bOutNeedsFullReload = false;

		switch (CompileResult)
		{
		case ECompileResult::FullyHandled:
			OutReloadRequirement = FAngelscriptClassGenerator::SoftReload;
			return bCompiled;

		case ECompileResult::PartiallyHandled:
			OutReloadRequirement = FAngelscriptClassGenerator::FullReloadSuggested;
			bOutWantsFullReload = true;
			return true;

		case ECompileResult::ErrorNeedFullReload:
			OutReloadRequirement = FAngelscriptClassGenerator::FullReloadRequired;
			bOutWantsFullReload = true;
			bOutNeedsFullReload = true;
			return true;

		case ECompileResult::Error:
		default:
			OutReloadRequirement = FAngelscriptClassGenerator::Error;
			return false;
		}
	}

	bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script)
	{
		return CompileModuleInternal(Engine, ECompileType::SoftReloadOnly, ModuleName, MoveTemp(Filename), MoveTemp(Script));
	}

	bool CompileAnnotatedModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script)
	{
		if (Engine == nullptr)
		{
			return false;
		}
		return PreprocessAndCompile(Engine, ECompileType::FullReload, MoveTemp(Filename), MoveTemp(Script));
	}

	bool ExecuteIntFunction(FAngelscriptEngine* Engine, FName ModuleName, FString Decl, int32& OutResult)
	{
		if (Engine == nullptr)
		{
			return false;
		}

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine->GetModuleByModuleName(ModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			return false;
		}

		asIScriptModule* Module = ModuleDesc->ScriptModule;

		asIScriptFunction* Function = FindFunctionByDecl(*Module, Decl);
		if (Function == nullptr)
		{
			return false;
		}

		FScopedTestEngineGlobalScope GlobalScope(Engine);

		asIScriptContext* Context = Engine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (PrepareResult != asSUCCESS)
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			return false;
		}

		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	bool ExecuteGeneratedIntEventOnGameThread(UObject* Object, UFunction* Function, int32& OutResult)
	{
		if (Object == nullptr || Function == nullptr)
		{
			return false;
		}

		auto Invoke = [Object, Function, &OutResult]()
		{
			FScopedTestWorldContextScope WorldContextScope(Object);

			if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
			{
				ScriptFunction->RuntimeCallEvent(Object, &OutResult);
			}
			else
			{
				Object->ProcessEvent(Function, &OutResult);
			}
		};

		if (IsInGameThread())
		{
			Invoke();
			return true;
		}

		FEvent* CompletedEvent = FPlatformProcess::GetSynchEventFromPool(true);
		AsyncTask(ENamedThreads::GameThread, [Invoke, CompletedEvent]() mutable
		{
			Invoke();
			CompletedEvent->Trigger();
		});

		CompletedEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
		return true;
	}

	UClass* FindGeneratedClass(FAngelscriptEngine* Engine, FName ClassName)
	{
		if (Engine == nullptr)
		{
			return nullptr;
		}

		UPackage* Package = Engine->GetPackageInstance();
		if (Package == nullptr)
		{
			return nullptr;
		}

		const FString ClassNameStr = ClassName.ToString();
		if (UClass* Found = FindObject<UClass>(Package, *ClassNameStr))
		{
			if (UASClass* ScriptClass = Cast<UASClass>(Found))
			{
				return ScriptClass->GetMostUpToDateClass();
			}
			return Found;
		}

		if (ClassNameStr.Len() >= 2 && (ClassNameStr[0] == TEXT('U') || ClassNameStr[0] == TEXT('A')) && FChar::IsUpper(ClassNameStr[1]))
		{
			if (UClass* Found = FindObject<UClass>(Package, *ClassNameStr.Mid(1)))
			{
				if (UASClass* ScriptClass = Cast<UASClass>(Found))
				{
					return ScriptClass->GetMostUpToDateClass();
				}
				return Found;
			}
		}

		return nullptr;
	}

	UFunction* FindGeneratedFunction(UClass* OwnerClass, FName FuncName)
	{
		if (UASClass* ScriptClass = Cast<UASClass>(OwnerClass))
		{
			OwnerClass = ScriptClass->GetMostUpToDateClass();
		}
		return OwnerClass != nullptr ? OwnerClass->FindFunctionByName(FuncName) : nullptr;
	}
}
