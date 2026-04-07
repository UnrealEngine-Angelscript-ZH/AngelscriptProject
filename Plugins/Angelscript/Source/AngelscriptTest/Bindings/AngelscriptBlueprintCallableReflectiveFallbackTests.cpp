#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"
#include "../Shared/AngelscriptTestEngineHelper.h"
#include "../../AngelscriptRuntime/Binds/BlueprintCallableReflectiveFallback.h"
#include "../../AngelscriptRuntime/Binds/Helper_FunctionSignature.h"

#include "GameplayTagsManager.h"
#include "BlueprintGameplayTagLibrary.h"
#include "GameplayTagAssetInterface.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Perception/PawnSensingComponent.h"
#include "Components/CheckBox.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptBlueprintCallableReflectiveFallbackUmgTest,
	"Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback.UMG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptBlueprintCallableReflectiveFallbackAiModuleTest,
	"Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback.AIModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptBlueprintCallableReflectiveFallbackGameplayTagsTest,
	"Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback.GameplayTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptBlueprintCallableReflectiveFallbackEligibilityTest,
	"Angelscript.TestModule.Bindings.BlueprintCallableReflectiveFallback.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptBlueprintCallableReflectiveFallbackUmgTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASReflectiveFallbackUMG"),
		TEXT("ASReflectiveFallbackUMG.as"),
		TEXT(R"(
UCLASS()
class UReflectiveCheckBoxBinding : UCheckBox
{
	UFUNCTION()
	int RunReflectiveFallback()
	{
		SetIsChecked(true);
		if (GetCheckedState() != ECheckBoxState::Checked)
			return 10;

		SetCheckedState(ECheckBoxState::Undetermined);
		return GetCheckedState() == ECheckBoxState::Undetermined ? 1 : 20;
	}
}
)"));
	if (!TestTrue(TEXT("Reflective fallback should compile a UCheckBox-derived script class that uses unresolved UMG methods"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UReflectiveCheckBoxBinding"));
	if (!TestNotNull(TEXT("Reflective fallback UMG test should generate a checkbox subclass"), RuntimeClass))
	{
		return false;
	}

	UFunction* RunFunction = FindGeneratedFunction(RuntimeClass, TEXT("RunReflectiveFallback"));
	if (!TestNotNull(TEXT("Reflective fallback UMG test should generate the execution function"), RunFunction))
	{
		return false;
	}

	UCheckBox* RuntimeObject = NewObject<UCheckBox>(GetTransientPackage(), RuntimeClass, TEXT("ReflectiveCheckBox"));
	if (!TestNotNull(TEXT("Reflective fallback UMG test should create the checkbox instance"), RuntimeObject))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Reflective fallback UMG test should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, RunFunction, Result)))
	{
		return false;
	}

	TestEqual(TEXT("Reflective fallback should invoke unresolved UMG methods correctly"), Result, 1);
	ASTEST_END_FULL
	return true;
}

bool FAngelscriptBlueprintCallableReflectiveFallbackAiModuleTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASReflectiveFallbackAI"),
		TEXT("ASReflectiveFallbackAI.as"),
		TEXT(R"(
UCLASS()
class UReflectivePawnSensingBinding : UPawnSensingComponent
{
	UFUNCTION()
	int RunReflectiveFallback()
	{
		SetPeripheralVisionAngle(42.0f);
		float CurrentAngle = GetPeripheralVisionAngle();
		if (CurrentAngle < 41.99f || CurrentAngle > 42.01f)
			return 10;

		SetSensingInterval(0.25f);
		return 1;
	}
}
)"));
	if (!TestTrue(TEXT("Reflective fallback should compile a UPawnSensingComponent-derived script class that uses unresolved AIModule methods"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UReflectivePawnSensingBinding"));
	if (!TestNotNull(TEXT("Reflective fallback AIModule test should generate a pawn sensing subclass"), RuntimeClass))
	{
		return false;
	}

	UFunction* RunFunction = FindGeneratedFunction(RuntimeClass, TEXT("RunReflectiveFallback"));
	if (!TestNotNull(TEXT("Reflective fallback AIModule test should generate the execution function"), RunFunction))
	{
		return false;
	}

	UPawnSensingComponent* RuntimeObject = NewObject<UPawnSensingComponent>(GetTransientPackage(), RuntimeClass, TEXT("ReflectivePawnSensing"));
	if (!TestNotNull(TEXT("Reflective fallback AIModule test should create the pawn sensing instance"), RuntimeObject))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Reflective fallback AIModule test should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, RunFunction, Result)))
	{
		return false;
	}

	TestEqual(TEXT("Reflective fallback should invoke unresolved AIModule methods correctly"), Result, 1);
	ASTEST_END_FULL
	return true;
}

bool FAngelscriptBlueprintCallableReflectiveFallbackGameplayTagsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	ASTEST_BEGIN_FULL

	FGameplayTagContainer AllTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
	if (!TestTrue(TEXT("Reflective fallback GameplayTags test requires at least one registered gameplay tag"), AllTags.Num() > 0))
	{
		return false;
	}

	const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
	TSharedPtr<FAngelscriptType> GameplayTagLibraryType = FAngelscriptType::GetByClass(UBlueprintGameplayTagLibrary::StaticClass());
	if (!TestTrue(TEXT("Reflective fallback GameplayTags test should resolve the BlueprintGameplayTagLibrary script type"), GameplayTagLibraryType.IsValid()))
	{
		return false;
	}

	UFunction* GetTagNameFunction = UBlueprintGameplayTagLibrary::StaticClass()->FindFunctionByName(TEXT("GetTagName"));
	if (!TestNotNull(TEXT("Reflective fallback GameplayTags test should locate BlueprintGameplayTagLibrary::GetTagName"), GetTagNameFunction))
	{
		return false;
	}

	const FString GameplayTagLibraryNamespace = FAngelscriptFunctionSignature::GetScriptNamespaceForClass(GameplayTagLibraryType.ToSharedRef(), GetTagNameFunction);
	const FString GameplayTagLibraryCallPrefix = GameplayTagLibraryNamespace.IsEmpty()
		? FString()
		: GameplayTagLibraryNamespace + TEXT("::");
	const FString Script = FString::Printf(TEXT(R"(
int Entry()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	if (!ValidTag.IsValid())
		return 10;

	FName ReflectedName = %sGetTagName(ValidTag);
	if (!(ReflectedName == ValidTag.GetTagName()))
		return 20;

	FGameplayTagContainer Container = %sMakeGameplayTagContainerFromTag(ValidTag);
	if (%sGetNumGameplayTagsInContainer(Container) != 1)
		return 30;

	return 1;
}
)"), *TagName, *GameplayTagLibraryCallPrefix, *GameplayTagLibraryCallPrefix, *GameplayTagLibraryCallPrefix);

	asIScriptModule* Module = BuildModule(*this, Engine, "ASReflectiveFallbackGameplayTags", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Reflective fallback should invoke unresolved GameplayTags library functions correctly"), Result, 1);
	ASTEST_END_FULL
	return true;
}

bool FAngelscriptBlueprintCallableReflectiveFallbackEligibilityTest::RunTest(const FString& Parameters)
{
	const UFunction* InterfaceFunction = UGameplayTagAssetInterface::StaticClass()->FindFunctionByName(TEXT("HasMatchingGameplayTag"));
	if (!TestNotNull(TEXT("Reflective fallback eligibility test should locate GameplayTagAssetInterface::HasMatchingGameplayTag"), InterfaceFunction))
	{
		return false;
	}

	const UFunction* CustomThunkFunction = UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Add"));
	if (!TestNotNull(TEXT("Reflective fallback eligibility test should locate KismetArrayLibrary::Array_Add"), CustomThunkFunction))
	{
		return false;
	}

	TestEqual(
		TEXT("Reflective fallback should reject interface-class functions explicitly"),
		EvaluateReflectiveFallbackEligibility(InterfaceFunction),
		EAngelscriptReflectiveFallbackEligibility::RejectedInterfaceClass);

	TestEqual(
		TEXT("Reflective fallback should reject CustomThunk functions explicitly"),
		EvaluateReflectiveFallbackEligibility(CustomThunkFunction),
		EAngelscriptReflectiveFallbackEligibility::RejectedCustomThunk);

	return true;
}

#endif
