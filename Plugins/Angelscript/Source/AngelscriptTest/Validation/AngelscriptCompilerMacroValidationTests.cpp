#include "../Shared/AngelscriptTestMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

// Ported from AngelscriptCompilerPipelineTests.cpp - simpler test case
ANGELSCRIPT_TEST(
	FAngelscriptCompilerEnumMacroValidationTest,
	"Angelscript.TestModule.Validation.CompilerEnumMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerEnumAvailabilityMacro"),
		TEXT("CompilerEnumAvailabilityMacro.as"),
		TEXT(R"(
UENUM(BlueprintType)
enum class ECompilerAvailabilityState : uint16
{
	Alpha,
	Beta = 4,
	Gamma
}
)"));

	if (!TestTrue(TEXT("Enum availability input via macro should compile"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(TEXT("ECompilerAvailabilityState"));
	if (!TestTrue(TEXT("Compiled enum metadata should be registered"), EnumDesc.IsValid()))
	{
		return false;
	}

	TestEqual(TEXT("Compiled enum should have 3 declared values"), EnumDesc->ValueNames.Num(), 3);
	TestEqual(TEXT("Beta should have explicit value 4"), static_cast<int32>(EnumDesc->EnumValues[1]), 4);
	return true;
}

// Test delegate and class compilation
ANGELSCRIPT_TEST(
	FAngelscriptCompilerDelegateMacroValidationTest,
	"Angelscript.TestModule.Validation.CompilerDelegateMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerDelegateSignatureMacro"),
		TEXT("CompilerDelegateSignatureMacro.as"),
		TEXT(R"(
delegate void FCompilerSingleCastSignature(int Value);
event void FCompilerMultiCastSignature(UClass TypeValue, FString Label);

UCLASS()
class UCompilerDelegateCarrier : UObject
{
}
)"));

	if (!TestTrue(TEXT("Delegate signature compilation via macro should succeed"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> SingleCast = Engine.GetDelegate(TEXT("FCompilerSingleCastSignature"));
	const TSharedPtr<FAngelscriptDelegateDesc> MultiCast = Engine.GetDelegate(TEXT("FCompilerMultiCastSignature"));
	if (!TestTrue(TEXT("Single-cast delegate metadata should exist"), SingleCast.IsValid()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Multicast delegate metadata should exist"), MultiCast.IsValid()))
	{
		return false;
	}

	TestFalse(TEXT("Single-cast delegate should not be marked multicast"), SingleCast->bIsMulticast);
	TestTrue(TEXT("Multicast delegate should be marked multicast"), MultiCast->bIsMulticast);
	TestNotNull(TEXT("Single-cast delegate should materialize a UDelegateFunction"), SingleCast->Function);
	TestNotNull(TEXT("Multicast delegate should materialize a UDelegateFunction"), MultiCast->Function);
	return true;
}

#endif
