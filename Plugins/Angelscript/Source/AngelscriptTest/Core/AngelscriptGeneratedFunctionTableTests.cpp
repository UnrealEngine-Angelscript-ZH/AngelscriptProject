#include "../../AngelscriptRuntime/Core/AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "Shared/AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

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

	const FFuncEntry* ActorTimeDilationEntry = ActorFunctionMap->Find(TEXT("GetActorTimeDilation"));
	if (!TestNotNull(TEXT("Generated function table startup pass should register the generated AActor::GetActorTimeDilation entry"), ActorTimeDilationEntry))
	{
		return false;
	}

	FGenericFuncPtr ActorTimeDilationPointer = ActorTimeDilationEntry->FuncPtr;
	TestTrue(TEXT("Generated function table startup pass should produce a bound direct-call pointer for AActor::GetActorTimeDilation"), ActorTimeDilationPointer.IsBound());
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
	const FString GeneratedDirectory = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Angelscript"),
		TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));

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
#endif
