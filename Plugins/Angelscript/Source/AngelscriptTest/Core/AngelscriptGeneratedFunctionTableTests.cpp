#include "../../AngelscriptRuntime/Core/AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "Shared/AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GeneratedFunctionTableTests
{
	struct FBindingRateStats
	{
		FString ModuleName;
		int32 TotalEntries = 0;
		int32 DirectEntries = 0;
		int32 StubEntries = 0;

		double GetDirectRatePercent() const
		{
			return TotalEntries > 0 ? (static_cast<double>(DirectEntries) * 100.0) / static_cast<double>(TotalEntries) : 0.0;
		}
	};

	static FString GetGeneratedDirectory()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
	}

	static bool TryParseModuleName(const FString& FileName, FString& OutModuleName)
	{
		FString Stem = FileName;
		if (!Stem.RemoveFromStart(TEXT("AS_FunctionTable_")) || !Stem.RemoveFromEnd(TEXT(".cpp")))
		{
			return false;
		}

		int32 LastUnderscoreIndex = INDEX_NONE;
		if (!Stem.FindLastChar(TEXT('_'), LastUnderscoreIndex) || LastUnderscoreIndex <= 0)
		{
			return false;
		}

		OutModuleName = Stem.Left(LastUnderscoreIndex);
		return !OutModuleName.IsEmpty();
	}

	static void AccumulateFileStats(const FString& FileContents, FBindingRateStats& Stats)
	{
		TArray<FString> Lines;
		FileContents.ParseIntoArrayLines(Lines, false);

		for (const FString& Line : Lines)
		{
			if (Line.Contains(TEXT("AddFunctionEntry("), ESearchCase::CaseSensitive))
			{
				Stats.TotalEntries++;
			}

			if (Line.Contains(TEXT("ERASE_NO_FUNCTION("), ESearchCase::CaseSensitive))
			{
				Stats.StubEntries++;
			}
		}
	}

	static bool CollectBindingRateStats(TArray<FBindingRateStats>& OutModuleStats, FBindingRateStats& OutOverallStats, FString& OutFailureReason)
	{
		const FString GeneratedDirectory = GetGeneratedDirectory();
		TArray<FString> GeneratedFiles;
		IFileManager::Get().FindFiles(GeneratedFiles, *FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_*.cpp")), true, false);

		if (GeneratedFiles.Num() == 0)
		{
			OutFailureReason = FString::Printf(TEXT("No generated function table files found under %s"), *GeneratedDirectory);
			return false;
		}

		TMap<FString, FBindingRateStats> StatsByModule;
		for (const FString& GeneratedFile : GeneratedFiles)
		{
			FString ModuleName;
			if (!TryParseModuleName(GeneratedFile, ModuleName))
			{
				OutFailureReason = FString::Printf(TEXT("Unable to parse module name from generated function table file %s"), *GeneratedFile);
				return false;
			}

			FString FileContents;
			const FString FullPath = FPaths::Combine(GeneratedDirectory, GeneratedFile);
			if (!FFileHelper::LoadFileToString(FileContents, *FullPath))
			{
				OutFailureReason = FString::Printf(TEXT("Unable to load generated function table file %s"), *FullPath);
				return false;
			}

			FBindingRateStats& ModuleStats = StatsByModule.FindOrAdd(ModuleName);
			ModuleStats.ModuleName = ModuleName;
			AccumulateFileStats(FileContents, ModuleStats);
		}

		StatsByModule.GenerateValueArray(OutModuleStats);
		OutModuleStats.Sort([](const FBindingRateStats& Left, const FBindingRateStats& Right)
		{
			return Left.ModuleName < Right.ModuleName;
		});

		OutOverallStats.ModuleName = TEXT("Overall");
		for (FBindingRateStats& ModuleStats : OutModuleStats)
		{
			ModuleStats.DirectEntries = ModuleStats.TotalEntries - ModuleStats.StubEntries;
			OutOverallStats.TotalEntries += ModuleStats.TotalEntries;
			OutOverallStats.DirectEntries += ModuleStats.DirectEntries;
			OutOverallStats.StubEntries += ModuleStats.StubEntries;
		}

		return true;
	}

	static void LogBindingRateStats(FAutomationTestBase& Test, const FBindingRateStats& OverallStats, const TArray<FBindingRateStats>& ModuleStats)
	{
		Test.AddInfo(FString::Printf(
			TEXT("Generated binding export rate (overall): direct=%d/%d (%.2f%%), stub=%d"),
			OverallStats.DirectEntries,
			OverallStats.TotalEntries,
			OverallStats.GetDirectRatePercent(),
			OverallStats.StubEntries));

		for (const FBindingRateStats& ModuleStatsEntry : ModuleStats)
		{
			Test.AddInfo(FString::Printf(
				TEXT("Generated binding export rate (%s): direct=%d/%d (%.2f%%), stub=%d"),
				*ModuleStatsEntry.ModuleName,
				ModuleStatsEntry.DirectEntries,
				ModuleStatsEntry.TotalEntries,
				ModuleStatsEntry.GetDirectRatePercent(),
				ModuleStatsEntry.StubEntries));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGeneratedFunctionTablePopulatesClassFuncMapsTest,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable.PopulatesClassFuncMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGeneratedFunctionTablePreservesHandwrittenGASEntriesTest,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable.PreservesHandwrittenGASEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGeneratedFunctionTableEditorOutputsUseWithEditorGuardTest,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable.EditorOutputsUseWithEditorGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGeneratedFunctionTableRepresentativeCoverageTest,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable.RepresentativeCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGeneratedFunctionTableMinimalApiFunctionLevelExportTest,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable.MinimalApiFunctionLevelExports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGeneratedFunctionTablePopulatesClassFuncMapsTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Generated function table test requires the runtime engine to be initialized in editor automation"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	(void)Engine;

	const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
	int32 TotalFunctionEntryCount = 0;
	for (const TPair<UClass*, TMap<FString, FFuncEntry>>& ClassEntry : ClassFuncMaps)
	{
		TotalFunctionEntryCount += ClassEntry.Value.Num();
	}

	if (!TestTrue(TEXT("Generated function table startup pass should populate many ClassFuncMaps entries beyond the legacy handwritten baseline"), TotalFunctionEntryCount > 1000))
	{
		return false;
	}

	const TMap<FString, FFuncEntry>* ActorFunctionMap = ClassFuncMaps.Find(AActor::StaticClass());
	if (!TestNotNull(TEXT("Generated function table startup pass should register an entry map for AActor"), ActorFunctionMap))
	{
		return false;
	}

	const FFuncEntry* ActorHasTagEntry = ActorFunctionMap->Find(TEXT("ActorHasTag"));
	if (!TestNotNull(TEXT("Generated function table startup pass should register the generated AActor::ActorHasTag entry"), ActorHasTagEntry))
	{
		return false;
	}

	FGenericFuncPtr ActorHasTagPointer = ActorHasTagEntry->FuncPtr;
	TestTrue(TEXT("Generated function table startup pass should produce a bound direct-call pointer for AActor::ActorHasTag"), ActorHasTagPointer.IsBound());
	return true;
}

bool FAngelscriptGeneratedFunctionTablePreservesHandwrittenGASEntriesTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Generated GAS compatibility test requires the runtime engine to be initialized in editor automation"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	UClass* AbilityAsyncLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/AngelscriptRuntime.AngelscriptAbilityAsyncLibrary"));
	if (!TestNotNull(TEXT("Generated GAS compatibility test should locate UAngelscriptAbilityAsyncLibrary"), AbilityAsyncLibraryClass))
	{
		return false;
	}

	const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
	const TMap<FString, FFuncEntry>* AsyncLibraryFunctionMap = ClassFuncMaps.Find(AbilityAsyncLibraryClass);
	if (!TestNotNull(TEXT("Generated GAS compatibility test should expose the async ability helper class in ClassFuncMaps"), AsyncLibraryFunctionMap))
	{
		return false;
	}

	const FFuncEntry* WaitForAttributeChangedEntry = AsyncLibraryFunctionMap->Find(TEXT("WaitForAttributeChanged"));
	if (!TestNotNull(TEXT("Generated GAS compatibility test should keep the handwritten WaitForAttributeChanged function entry"), WaitForAttributeChangedEntry))
	{
		return false;
	}

	FGenericFuncPtr WaitForAttributeChangedPointer = WaitForAttributeChangedEntry->FuncPtr;
	if (!TestTrue(TEXT("Generated GAS compatibility test should preserve the handwritten direct-call pointer for WaitForAttributeChanged"), WaitForAttributeChangedPointer.IsBound()))
	{
		return false;
	}

	const FFuncEntry* WaitGameplayTagRemoveEntry = AsyncLibraryFunctionMap->Find(TEXT("WaitGameplayTagRemoveFromActor"));
	if (!TestNotNull(TEXT("Generated GAS compatibility test should expose WaitGameplayTagRemoveFromActor under its own key"), WaitGameplayTagRemoveEntry))
	{
		return false;
	}

	FGenericFuncPtr WaitGameplayTagRemovePointer = WaitGameplayTagRemoveEntry->FuncPtr;
	TestTrue(TEXT("Generated GAS compatibility test should keep WaitGameplayTagRemoveFromActor bound after handwritten registration"), WaitGameplayTagRemovePointer.IsBound());
	return true;
}

bool FAngelscriptGeneratedFunctionTableEditorOutputsUseWithEditorGuardTest::RunTest(const FString& Parameters)
{
	TArray<GeneratedFunctionTableTests::FBindingRateStats> ModuleStats;
	GeneratedFunctionTableTests::FBindingRateStats OverallStats;
	FString BindingRateFailureReason;
	if (!TestTrue(TEXT("Generated strategy test should collect binding export rate stats from generated UHT outputs"), GeneratedFunctionTableTests::CollectBindingRateStats(ModuleStats, OverallStats, BindingRateFailureReason)))
	{
		AddError(BindingRateFailureReason);
		return false;
	}

	GeneratedFunctionTableTests::LogBindingRateStats(*this, OverallStats, ModuleStats);

	const FString GeneratedDirectory = GeneratedFunctionTableTests::GetGeneratedDirectory();

	const FString EditorOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_UMGEditor_000.cpp"));
	const FString RuntimeOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Engine_000.cpp"));

	FString EditorOutput;
	if (!TestTrue(TEXT("Generated strategy test should find the editor-only UHT output"), FFileHelper::LoadFileToString(EditorOutput, *EditorOutputPath)))
	{
		return false;
	}

	FString RuntimeOutput;
	if (!TestTrue(TEXT("Generated strategy test should find the runtime UHT output"), FFileHelper::LoadFileToString(RuntimeOutput, *RuntimeOutputPath)))
	{
		return false;
	}

	TestTrue(TEXT("Generated strategy test should wrap editor-only outputs with #if WITH_EDITOR"), EditorOutput.StartsWith(TEXT("#if WITH_EDITOR")));
	TestFalse(TEXT("Generated strategy test should not wrap runtime outputs with #if WITH_EDITOR"), RuntimeOutput.StartsWith(TEXT("#if WITH_EDITOR")));
	return true;
}

bool FAngelscriptGeneratedFunctionTableRepresentativeCoverageTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Generated representative coverage test requires the runtime engine to be initialized in editor automation"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();

	struct FRepresentativeClassExpectation
	{
		const TCHAR* ObjectPath;
		const TCHAR* DisplayName;
	};

	const FRepresentativeClassExpectation Expectations[] =
	{
		{ TEXT("/Script/Engine.Actor"), TEXT("AActor") },
		{ TEXT("/Script/Engine.World"), TEXT("UWorld") },
		{ TEXT("/Script/Engine.GameplayStatics"), TEXT("UGameplayStatics") },
		{ TEXT("/Script/Engine.PlayerController"), TEXT("APlayerController") },
		{ TEXT("/Script/Engine.ActorComponent"), TEXT("UActorComponent") },
		{ TEXT("/Script/Engine.SceneComponent"), TEXT("USceneComponent") },
		{ TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("UKismetSystemLibrary") },
		{ TEXT("/Script/UMG.UserWidget"), TEXT("UUserWidget") },
		{ TEXT("/Script/AssetRegistry.AssetRegistryHelpers"), TEXT("UAssetRegistryHelpers") },
		{ TEXT("/Script/AngelscriptRuntime.AngelscriptAbilityAsyncLibrary"), TEXT("UAngelscriptAbilityAsyncLibrary") },
	};

	for (const FRepresentativeClassExpectation& Expectation : Expectations)
	{
		UClass* ExpectedClass = FindObject<UClass>(nullptr, Expectation.ObjectPath);
		if (!TestNotNull(FString::Printf(TEXT("Representative coverage test should resolve %s"), Expectation.DisplayName), ExpectedClass))
		{
			return false;
		}

		const TMap<FString, FFuncEntry>* FunctionMap = ClassFuncMaps.Find(ExpectedClass);
		if (!TestNotNull(FString::Printf(TEXT("Representative coverage test should populate ClassFuncMaps for %s"), Expectation.DisplayName), FunctionMap))
		{
			return false;
		}

		if (!TestTrue(FString::Printf(TEXT("Representative coverage test should add at least one generated function entry for %s"), Expectation.DisplayName), FunctionMap->Num() > 0))
		{
			return false;
		}
	}

	return true;
}

bool FAngelscriptGeneratedFunctionTableMinimalApiFunctionLevelExportTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("MinimalAPI function export regression test requires the runtime engine to be initialized in editor automation"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
	const TMap<FString, FFuncEntry>* PlayerCameraManagerEntries = ClassFuncMaps.Find(APlayerCameraManager::StaticClass());
	if (!TestNotNull(TEXT("MinimalAPI function export regression test should expose generated entries for APlayerCameraManager"), PlayerCameraManagerEntries))
	{
		return false;
	}

	const TCHAR* ExpectedBoundFunctions[] =
	{
		TEXT("SetManualCameraFade"),
		TEXT("StartCameraFade"),
		TEXT("StopCameraFade"),
	};

	for (const TCHAR* ExpectedFunctionName : ExpectedBoundFunctions)
	{
		const FFuncEntry* Entry = PlayerCameraManagerEntries->Find(ExpectedFunctionName);
		if (!TestNotNull(FString::Printf(TEXT("MinimalAPI function export regression test should register %s"), ExpectedFunctionName), Entry))
		{
			return false;
		}

		FGenericFuncPtr FunctionPointer = Entry->FuncPtr;
		if (!TestTrue(FString::Printf(TEXT("MinimalAPI function export regression test should recover a direct-call pointer for %s"), ExpectedFunctionName), FunctionPointer.IsBound()))
		{
			return false;
		}
	}

	return true;
}
#endif
