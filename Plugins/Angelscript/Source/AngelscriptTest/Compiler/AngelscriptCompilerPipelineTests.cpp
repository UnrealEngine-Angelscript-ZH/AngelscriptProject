#include "../Shared/AngelscriptTestEngineHelper.h"
#include "../Shared/AngelscriptTestUtilities.h"

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerDelegateEnumClassCompileTest,
	"Angelscript.TestModule.Compiler.EndToEnd.DelegateEnumClassCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerDelegateEnumClassCompileTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerDelegateEnumClassCompile"),
		TEXT("CompilerDelegateEnumClassCompile.as"),
		TEXT(R"(
UENUM(BlueprintType)
enum class ECompilerTransferState : uint16
{
	Alpha,
	Beta = 4,
	Gamma
}

delegate void FCompilerTransferDelegate(int Value);
event void FCompilerTransferEvent(UClass TypeValue, FString Label);

UCLASS(Abstract, BlueprintType)
class UCompilerTransferObject : UObject
{
	UPROPERTY()
	int Score;

	UFUNCTION()
	int GetScore()
	{
		return Score;
	}
}
)"));

	if (!TestTrue(TEXT("Fallback compiler validation should compile delegate/enum/class transfer input"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> SimpleDelegate = Engine.GetDelegate(TEXT("FCompilerTransferDelegate"));
	const TSharedPtr<FAngelscriptDelegateDesc> MultiDelegate = Engine.GetDelegate(TEXT("FCompilerTransferEvent"));
	if (!TestTrue(TEXT("Simple delegate metadata should be registered after compile"), SimpleDelegate.IsValid()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Multicast delegate metadata should be registered after compile"), MultiDelegate.IsValid()))
	{
		return false;
	}

	TestFalse(TEXT("Simple delegate should remain single-cast"), SimpleDelegate->bIsMulticast);
	TestTrue(TEXT("Event delegate should remain multicast"), MultiDelegate->bIsMulticast);

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UCompilerTransferObject"));
	if (!TestNotNull(TEXT("Generated class should exist for delegate/enum/class transfer input"), GeneratedClass))
	{
		return false;
	}

	TestTrue(TEXT("Generated class should preserve abstract flag"), GeneratedClass->HasAnyClassFlags(CLASS_Abstract));
	TestNotNull(TEXT("Generated Score property should exist"), FindFProperty<FProperty>(GeneratedClass, TEXT("Score")));
	TestNotNull(TEXT("Generated GetScore function should exist"), AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("GetScore")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerFunctionDefaultsAndClassLikeCompileTest,
	"Angelscript.TestModule.Compiler.EndToEnd.FunctionDefaultsAndClassLikeCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerFunctionDefaultsAndClassLikeCompileTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerFunctionDefaultsAndClassLikeCompile"),
		TEXT("CompilerFunctionDefaultsAndClassLikeCompile.as"),
		TEXT(R"(
int SumWithDefault(int Value = 21, int Extra = 21)
{
	return Value + Extra;
}

int Entry()
{
	return SumWithDefault();
}

UCLASS()
class UCompilerFunctionCarrier : UObject
{

	UFUNCTION()
	UClass EchoPlainClass(UClass Value)
	{
		return Value;
	}

	UFUNCTION()
	TSubclassOf<AActor> EchoActorClass(TSubclassOf<AActor> Value)
	{
		return Value;
	}

	UFUNCTION()
	TSoftClassPtr<AActor> EchoSoftActorClass(TSoftClassPtr<AActor> Value)
	{
		return Value;
	}
}
)"));

	if (!TestTrue(TEXT("Fallback compiler validation should compile function defaults and class-like signatures"), bCompiled))
	{
		return false;
	}

	int32 Result = 0;
	const bool bExecuted = AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("CompilerFunctionDefaultsAndClassLikeCompile"), TEXT("int Entry()"), Result);
	if (!TestTrue(TEXT("Function default validation should execute compiled entry point"), bExecuted))
	{
		return false;
	}

	TestEqual(TEXT("Function default values should be honored at runtime"), Result, 42);

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UCompilerFunctionCarrier"));
	if (!TestNotNull(TEXT("Generated class should exist for class-like signature input"), GeneratedClass))
	{
		return false;
	}

	TestNotNull(TEXT("EchoPlainClass function should exist"), AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass")));
	TestNotNull(TEXT("EchoActorClass function should exist"), AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass")));
	TestNotNull(TEXT("EchoSoftActorClass function should exist"), AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass")));
	return Result == 42;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerPropertyDefaultsCompileTest,
	"Angelscript.TestModule.Compiler.EndToEnd.PropertyDefaultsCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerPropertyDefaultsCompileTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerPropertyDefaultsCompile"),
		TEXT("CompilerPropertyDefaultsCompile.as"),
		TEXT(R"(
UCLASS()
class UCompilerDefaultsCarrier : UObject
{
	UPROPERTY()
	int Score = 7;

	UPROPERTY()
	TArray<FName> Tags;

	default Score = 21;
	default Tags.Add(n"Alpha");
}
)"));

	if (!TestTrue(TEXT("Fallback compiler validation should compile property default input"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UCompilerDefaultsCarrier"));
	if (!TestNotNull(TEXT("Generated class should exist for property default input"), GeneratedClass))
	{
		return false;
	}

	UObject* DefaultObject = GeneratedClass->GetDefaultObject();
	if (!TestNotNull(TEXT("Generated class default object should exist"), DefaultObject))
	{
		return false;
	}

	FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("Score"));
	if (!TestNotNull(TEXT("Generated Score property should exist"), ScoreProperty))
	{
		return false;
	}

	const int32 ScoreValue = ScoreProperty->GetPropertyValue_InContainer(DefaultObject);
	TestEqual(TEXT("Generated default object should materialize overridden int default"), ScoreValue, 21);
	return ScoreValue == 21;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerGeneratedClassConsistencyTest,
	"Angelscript.TestModule.Compiler.EndToEnd.GeneratedClassConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerGeneratedClassConsistencyTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerGeneratedClassConsistency"),
		TEXT("CompilerGeneratedClassConsistency.as"),
		TEXT(R"(
UCLASS(Abstract, BlueprintType)
class UCompilerConsistencyCarrier : UObject
{
	UPROPERTY()
	int Score;

	UFUNCTION()
	int GetScore()
	{
		return Score;
	}
}
)"));

	if (!TestTrue(TEXT("Generated class consistency input should compile"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UCompilerConsistencyCarrier"));
	if (!TestNotNull(TEXT("Generated consistency class should exist"), GeneratedClass))
	{
		return false;
	}

	TestTrue(TEXT("Generated consistency class should preserve abstract flag"), GeneratedClass->HasAnyClassFlags(CLASS_Abstract));
	TestNotNull(TEXT("Generated consistency class should expose Score property"), FindFProperty<FProperty>(GeneratedClass, TEXT("Score")));
	TestNotNull(TEXT("Generated consistency class should expose GetScore function"), AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("GetScore")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerModuleFunctionInspectionTest,
	"Angelscript.TestModule.Compiler.EndToEnd.ModuleFunctionInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerModuleFunctionInspectionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
		*this,
		Engine,
		"CompilerModuleFunctionInspection",
		TEXT(R"(
int SumWithDefault(int Value = 21, int Extra = 21)
{
	return Value + Extra;
}

int Entry()
{
	return SumWithDefault();
}
)"));
	if (!TestNotNull(TEXT("Fallback compiler inspection module should build"), Module))
	{
		return false;
	}

	asIScriptFunction* SumWithDefault = AngelscriptTestSupport::GetFunctionByDecl(*this, *Module, TEXT("int SumWithDefault(int, int)"));
	if (!TestNotNull(TEXT("Compiled module should expose SumWithDefault"), SumWithDefault))
	{
		return false;
	}

	TestEqual(TEXT("Compiled module should expose exactly two parameters for SumWithDefault"), static_cast<int32>(SumWithDefault->GetParamCount()), 2);

	int32 Result = 0;
	if (!TestTrue(TEXT("Compiled inspection module entry should execute"), AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("CompilerModuleFunctionInspection"), TEXT("int Entry()"), Result)))
	{
		return false;
	}

	TestEqual(TEXT("Compiled inspection module should preserve executable default values"), Result, 42);
	return Result == 42;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerEnumAvailabilityTest,
	"Angelscript.TestModule.Compiler.EndToEnd.EnumAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerEnumAvailabilityTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerEnumAvailability"),
		TEXT("CompilerEnumAvailability.as"),
		TEXT(R"(
UENUM(BlueprintType)
enum class ECompilerAvailabilityState : uint16
{
	Alpha,
	Beta = 4,
	Gamma
}
)"));

	if (!TestTrue(TEXT("Enum availability input should compile"), bCompiled))
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerDelegateSignatureConsistencyTest,
	"Angelscript.TestModule.Compiler.EndToEnd.DelegateSignatureConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerDelegateSignatureConsistencyTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerDelegateSignatureConsistency"),
		TEXT("CompilerDelegateSignatureConsistency.as"),
		TEXT(R"(
delegate void FCompilerSingleCastSignature(int Value);
event void FCompilerMultiCastSignature(UClass TypeValue, FString Label);

UCLASS()
class UCompilerDelegateCarrier : UObject
{
}
)"));

	if (!TestTrue(TEXT("Delegate signature consistency input should compile"), bCompiled))
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerClassLikeReflectionShapeTest,
	"Angelscript.TestModule.Compiler.EndToEnd.ClassLikeReflectionShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerClassLikeReflectionShapeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = AngelscriptTestSupport::GetSharedTestEngine();
FAngelscriptEngine& Engine = AngelscriptTestSupport::GetSharedTestEngine();
	const bool bCompiled = AngelscriptTestSupport::CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerClassLikeReflectionShape"),
		TEXT("CompilerClassLikeReflectionShape.as"),
		TEXT(R"(
UCLASS()
class UCompilerClassLikeShapeCarrier : UObject
{
	UFUNCTION()
	UClass EchoPlainClass(UClass Value)
	{
		return Value;
	}

	UFUNCTION()
	TSubclassOf<AActor> EchoActorClass(TSubclassOf<AActor> Value)
	{
		return Value;
	}

	UFUNCTION()
	TSoftClassPtr<AActor> EchoSoftActorClass(TSoftClassPtr<AActor> Value)
	{
		return Value;
	}
}
)"));

	if (!TestTrue(TEXT("Class-like reflection shape input should compile"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = AngelscriptTestSupport::FindGeneratedClass(&Engine, TEXT("UCompilerClassLikeShapeCarrier"));
	if (!TestNotNull(TEXT("Generated class should exist for class-like reflection shape input"), GeneratedClass))
	{
		return false;
	}

	UFunction* EchoPlainClass = AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass"));
	UFunction* EchoActorClass = AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass"));
	UFunction* EchoSoftActorClass = AngelscriptTestSupport::FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass"));
	if (!TestNotNull(TEXT("EchoPlainClass should exist"), EchoPlainClass) ||
		!TestNotNull(TEXT("EchoActorClass should exist"), EchoActorClass) ||
		!TestNotNull(TEXT("EchoSoftActorClass should exist"), EchoSoftActorClass))
	{
		return false;
	}

	TestNotNull(TEXT("Plain class return should materialize as FClassProperty"), CastField<FClassProperty>(EchoPlainClass->GetReturnProperty()));
	TestNotNull(TEXT("Plain class parameter should materialize as FClassProperty"), CastField<FClassProperty>(FindFProperty<FProperty>(EchoPlainClass, TEXT("Value"))));

	FClassProperty* ActorReturnProperty = CastField<FClassProperty>(EchoActorClass->GetReturnProperty());
	FClassProperty* ActorParamProperty = CastField<FClassProperty>(FindFProperty<FProperty>(EchoActorClass, TEXT("Value")));
	if (!TestNotNull(TEXT("Subclass return should materialize as FClassProperty"), ActorReturnProperty) ||
		!TestNotNull(TEXT("Subclass parameter should materialize as FClassProperty"), ActorParamProperty))
	{
		return false;
	}
	TestTrue(TEXT("Subclass return MetaClass should be AActor"), ActorReturnProperty->MetaClass == AActor::StaticClass());
	TestTrue(TEXT("Subclass parameter MetaClass should be AActor"), ActorParamProperty->MetaClass == AActor::StaticClass());

	FSoftClassProperty* SoftReturnProperty = CastField<FSoftClassProperty>(EchoSoftActorClass->GetReturnProperty());
	FSoftClassProperty* SoftParamProperty = CastField<FSoftClassProperty>(FindFProperty<FProperty>(EchoSoftActorClass, TEXT("Value")));
	if (!TestNotNull(TEXT("Soft class return should materialize as FSoftClassProperty"), SoftReturnProperty) ||
		!TestNotNull(TEXT("Soft class parameter should materialize as FSoftClassProperty"), SoftParamProperty))
	{
		return false;
	}
	TestTrue(TEXT("Soft class return MetaClass should be AActor"), SoftReturnProperty->MetaClass == AActor::StaticClass());
	TestTrue(TEXT("Soft class parameter MetaClass should be AActor"), SoftParamProperty->MetaClass == AActor::StaticClass());
	return true;
}

#endif
