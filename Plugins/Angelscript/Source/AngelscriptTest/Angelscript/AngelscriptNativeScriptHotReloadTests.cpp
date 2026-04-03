#include "AngelscriptTestSupport.h"
#include "../Shared/AngelscriptTestEngineHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace
{
	bool VerifyNativeScriptHotReload(
		FAutomationTestBase& Test,
		const TCHAR* GroupLabel,
		const TArray<FString>& RelativeScriptPaths)
	{
		FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(Test, TEXT("Native script hot reload tests require a production engine."));
		if (ProductionEngine == nullptr)
		{
			return false;
		}

		FAngelscriptEngine& Engine = *ProductionEngine;

		for (int32 Index = 0; Index < RelativeScriptPaths.Num(); ++Index)
		{
			const FString& RelativeScriptPath = RelativeScriptPaths[Index];
			const FString CompileRelativePath = RelativeScriptPath.StartsWith(TEXT("Script/"))
				? RelativeScriptPath.Mid(7)
				: RelativeScriptPath;
			const FString AbsoluteScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeScriptPath);
			FString Source;
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should load source from %s"), GroupLabel, *RelativeScriptPath),
				FFileHelper::LoadFileToString(Source, *AbsoluteScriptPath)))
			{
				return false;
			}

			const FName ModuleName(*FString::Printf(TEXT("NativeHotReload_%s_%d"), GroupLabel, Index));
			ECompileResult InitialCompileResult = ECompileResult::Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should compile %s before reload"), GroupLabel, *RelativeScriptPath),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, AbsoluteScriptPath, Source, InitialCompileResult)))
			{
				return false;
			}

			FString ReloadSource = Source;
			ReloadSource += TEXT("\n// hot reload verification marker\n");
			ECompileResult ReloadCompileResult = ECompileResult::Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should hot reload %s through the compile wrapper"), GroupLabel, *RelativeScriptPath),
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, AbsoluteScriptPath, ReloadSource, ReloadCompileResult)))
			{
				return false;
			}

			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should keep %s on a handled reload path"), GroupLabel, *RelativeScriptPath),
				ReloadCompileResult == ECompileResult::FullyHandled || ReloadCompileResult == ECompileResult::PartiallyHandled))
			{
				return false;
			}

			Engine.DiscardModule(*ModuleName.ToString());
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptHotReloadPhase2ATest,
	"Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2A",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptHotReloadPhase2BTest,
	"Angelscript.TestModule.Angelscript.NativeScriptHotReload.Phase2B",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNativeScriptHotReloadPhase2ATest::RunTest(const FString& Parameters)
{
	return VerifyNativeScriptHotReload(
		*this,
		TEXT("Phase2A"),
		{
			TEXT("Script/Tests/Test_Enums.as"),
			TEXT("Script/Tests/Test_Inheritance.as"),
			TEXT("Script/Tests/Test_Handles.as"),
		});
}

bool FAngelscriptNativeScriptHotReloadPhase2BTest::RunTest(const FString& Parameters)
{
	return VerifyNativeScriptHotReload(
		*this,
		TEXT("Phase2B"),
		{
			TEXT("Script/Tests/Test_GameplayTags.as"),
			TEXT("Script/Tests/Test_SystemUtils.as"),
			TEXT("Script/Tests/Test_ActorLifecycle.as"),
			TEXT("Script/Tests/Test_MathNamespace.as"),
		});
}

#endif
