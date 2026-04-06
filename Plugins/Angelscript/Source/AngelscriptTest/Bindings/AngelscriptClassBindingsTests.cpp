#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"
#include "../Shared/AngelscriptTestEngineHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptClassLookupBindingsTest,
	"Angelscript.TestModule.Bindings.ClassLookupCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTSubclassOfBindingsTest,
	"Angelscript.TestModule.Bindings.TSubclassOfCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTSoftClassPtrBindingsTest,
	"Angelscript.TestModule.Bindings.TSoftClassPtrCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticClassCompatBindingsTest,
	"Angelscript.TestModule.Bindings.StaticClassCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeStaticClassNamespaceBindingTest,
	"Angelscript.TestModule.Bindings.NativeStaticClassNamespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeStaticTypeGlobalBindingTest,
	"Angelscript.TestModule.Bindings.NativeStaticTypeGlobal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptClassLookupBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASClassLookupCompat",
		TEXT(R"(
int Entry()
{
	UClass ActorClass = FindClass("AActor");
	if (ActorClass == null)
		return 10;

	TArray<UClass> AllClasses;
	GetAllClasses(AllClasses);
	if (AllClasses.Num() <= 0)
		return 20;

	bool bFoundActor = false;
	for (int Index = 0; Index < AllClasses.Num(); ++Index)
	{
		if (AllClasses[Index] == ActorClass)
		{
			bFoundActor = true;
			break;
		}
	}

	if (!bFoundActor)
		return 30;

	return 1;
}
)"));
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

	TestEqual(TEXT("Class lookup helper operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptTSubclassOfBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTSubclassOfCompat",
		TEXT(R"(
TSubclassOf<AActor> EchoSubclass(TSubclassOf<AActor> Value)
{
	return Value;
}

UClass EchoSubclassClass(TSubclassOf<AActor> Value)
{
	return Value;
}

int Entry()
{
	TSubclassOf<AActor> Empty;
	if (Empty.IsValid())
		return 10;
	if (!(Empty.Get() == null))
		return 20;

	Empty = AActor::StaticClass();
	if (!Empty.IsValid())
		return 30;
	if (!(Empty.Get() == AActor::StaticClass()))
		return 40;
	if (!(Empty == AActor::StaticClass()))
		return 50;

	TSubclassOf<AActor> ImplicitFromClassArg = EchoSubclass(AActor::StaticClass());
	if (!(ImplicitFromClassArg == AActor::StaticClass()))
		return 55;

	UClass ImplicitClassArg = EchoSubclassClass(ACameraActor::StaticClass());
	if (!(ImplicitClassArg == ACameraActor::StaticClass()))
		return 56;

	TSubclassOf<AActor> Narrowed = ACameraActor::StaticClass();
	TSubclassOf<AActor> Copy = Narrowed;
	if (!(Copy == ACameraActor::StaticClass()))
		return 60;

	AActor DefaultActor = Copy.GetDefaultObject();
	if (!IsValid(DefaultActor))
		return 70;
	if (!DefaultActor.IsA(ACameraActor::StaticClass()))
		return 80;

	TArray<TSubclassOf<AActor>> LiteralSubclassHistory;
	LiteralSubclassHistory.Add(AActor::StaticClass());
	LiteralSubclassHistory.Add(ACameraActor::StaticClass());
	if (LiteralSubclassHistory.Num() != 2)
		return 90;
	if (!(LiteralSubclassHistory[1] == ACameraActor::StaticClass()))
		return 100;

	return 1;
}
)"));
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

	TestEqual(TEXT("TSubclassOf compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptTSoftClassPtrBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTSoftClassPtrCompat",
		TEXT(R"(
TSoftClassPtr<AActor> EchoSoftClass(TSoftClassPtr<AActor> Value)
{
	return Value;
}

UClass EchoSoftClassClass(TSoftClassPtr<AActor> Value)
{
	return Value.Get();
}

int Entry()
{
	TSoftClassPtr<AActor> Empty;
	if (!Empty.IsNull())
		return 10;
	if (Empty.IsValid())
		return 20;

	TSoftClassPtr<AActor> Constructed(AActor::StaticClass());
	if (!(Constructed == AActor::StaticClass()))
		return 30;
	if (!(Constructed.Get() == AActor::StaticClass()))
		return 40;
	if (!(Constructed.Get() == AActor::StaticClass()))
		return 50;
	if (!Constructed.IsValid())
		return 60;
	if (Constructed.ToString().IsEmpty())
		return 70;

	TSoftClassPtr<AActor> ImplicitFromClassArg = EchoSoftClass(TSoftClassPtr<AActor>(AActor::StaticClass()));
	if (!(ImplicitFromClassArg == AActor::StaticClass()))
		return 80;

	UClass ImplicitClassArg = EchoSoftClassClass(TSoftClassPtr<AActor>(ACameraActor::StaticClass()));
	if (!(ImplicitClassArg == ACameraActor::StaticClass()))
		return 90;

	TSoftClassPtr<AActor> Assigned;
	Assigned = AActor::StaticClass();
	if (!(Assigned == AActor::StaticClass()))
		return 100;

	TArray<TSoftClassPtr<AActor>> LiteralHistory;
	LiteralHistory.Add(TSoftClassPtr<AActor>(AActor::StaticClass()));
	LiteralHistory.Add(TSoftClassPtr<AActor>(ACameraActor::StaticClass()));
	if (LiteralHistory.Num() != 2)
		return 110;
	if (!(LiteralHistory[1] == ACameraActor::StaticClass()))
		return 120;

	Assigned.Reset();
	if (!Assigned.IsNull())
		return 130;

	return 1;
}
)"));
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

	TestEqual(TEXT("TSoftClassPtr compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptStaticClassCompatBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE

	asIScriptModule* PlainModule = BuildModule(
		*this,
		Engine,
		"ASStaticClassCompat",
		TEXT(R"(
int Entry()
{
	UClass ActorClass = AActor::StaticClass();
	TSubclassOf<AActor> CompatClass = AActor::StaticClass();

	if (!IsValid(ActorClass) || !CompatClass.IsValid())
		return 10;
	if (!ActorClass.IsChildOf(CompatClass) || !CompatClass.IsChildOf(ActorClass))
		return 20;
	if (!IsValid(ActorClass.GetDefaultObject()))
		return 30;

	return 1;
}
)"));
	if (PlainModule == nullptr)
	{
		return false;
	}

	asIScriptFunction* PlainFunction = GetFunctionByDecl(*this, *PlainModule, TEXT("int Entry()"));
	if (PlainFunction == nullptr)
	{
		return false;
	}

	int32 PlainResult = 0;
	if (!ExecuteIntFunction(*this, Engine, *PlainFunction, PlainResult))
	{
		return false;
	}
	if (!TestEqual(TEXT("Plain module StaticClass and TSubclassOf compat syntax should behave as expected"), PlainResult, 1))
	{
		return false;
	}
	const bool bAnnotatedCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASAnnotatedStaticClassCompat"),
		TEXT("ASAnnotatedStaticClassCompat.as"),
		TEXT(R"(
UCLASS()
class ABindingStaticClassActor : AActor
{
	UFUNCTION()
	int ReadStaticClassCompat()
	{
		UClass SelfClass = ABindingStaticClassActor::StaticClass();
		TSubclassOf<ABindingStaticClassActor> CompatClass = ABindingStaticClassActor::StaticClass();

		if (!IsValid(SelfClass) || !CompatClass.IsValid())
			return 10;
		if (!SelfClass.IsChildOf(GetClass()) || !GetClass().IsChildOf(SelfClass))
			return 20;
		if (!CompatClass.IsChildOf(SelfClass) || !SelfClass.IsChildOf(CompatClass))
			return 30;

		return IsValid(SelfClass.GetDefaultObject()) ? 1 : 40;
	}
}

)"));
	if (!TestTrue(TEXT("Annotated StaticClass compat module should compile"), bAnnotatedCompiled))
	{
		return false;
	}

	UClass* RuntimeActorClass = FindGeneratedClass(&Engine, TEXT("ABindingStaticClassActor"));
	if (!TestNotNull(TEXT("Generated actor class for StaticClass compat should exist"), RuntimeActorClass))
	{
		return false;
	}

	UFunction* ReadStaticClassCompatFunction = FindGeneratedFunction(RuntimeActorClass, TEXT("ReadStaticClassCompat"));
	if (!TestNotNull(TEXT("StaticClass compat function should exist"), ReadStaticClassCompatFunction))
	{
		return false;
	}
	AActor* RuntimeActor = NewObject<AActor>(GetTransientPackage(), RuntimeActorClass);
	if (!TestNotNull(TEXT("Generated actor for StaticClass compat should instantiate"), RuntimeActor))
	{
		return false;
	}

	int32 AnnotatedResult = 0;
	if (!TestTrue(TEXT("StaticClass compat reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeActor, ReadStaticClassCompatFunction, AnnotatedResult)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Annotated module StaticClass and TSubclassOf compat syntax should behave as expected"), AnnotatedResult, 1))
	{
		return false;
	}

	asIScriptModule* QueryModule = BuildModule(
		*this,
		Engine,
		"ASGeneratedStaticClassQuery",
		TEXT(R"(
int Entry()
{
	UClass SelfClass = FindClass("ABindingStaticClassActor");
	TSubclassOf<AActor> CompatClass = FindClass("ABindingStaticClassActor");

	if (!IsValid(SelfClass) || !CompatClass.IsValid())
		return 10;
	if (!SelfClass.IsChildOf(AActor::StaticClass()))
		return 20;
	if (!CompatClass.IsChildOf(SelfClass) || !SelfClass.IsChildOf(CompatClass))
		return 30;

	return 1;
}
)"));
	if (QueryModule == nullptr)
	{
		return false;
	}

	asIScriptFunction* QueryFunction = GetFunctionByDecl(*this, *QueryModule, TEXT("int Entry()"));
	if (QueryFunction == nullptr)
	{
		return false;
	}

	int32 QueryResult = 0;
	if (!ExecuteIntFunction(*this, Engine, *QueryFunction, QueryResult))
	{
		return false;
	}

	TestEqual(TEXT("Follow-up plain module should resolve generated classes into TSubclassOf compat flow"), QueryResult, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptNativeStaticClassNamespaceBindingTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptEngine* ScriptEngine = Engine.Engine;
	if (!TestNotNull(TEXT("AngelScript engine should exist"), ScriptEngine))
	{
		return false;
	}

	if (!TestTrue(TEXT("Set namespace to AActor should succeed"), ScriptEngine->SetDefaultNamespace("AActor") >= 0))
	{
		return false;
	}

	asIScriptFunction* StaticClassFunction = ScriptEngine->GetGlobalFunctionByDecl("UClass StaticClass()");
	const bool bHasFunction = TestNotNull(TEXT("Native class namespace should expose StaticClass"), StaticClassFunction);

	if (StaticClassFunction != nullptr)
	{
		TestEqual(TEXT("Native class namespace StaticClass should keep bound UClass userdata"), static_cast<UClass*>(StaticClassFunction->GetUserData()), AActor::StaticClass());
	}

	TestTrue(TEXT("Restore global namespace should succeed"), ScriptEngine->SetDefaultNamespace("") >= 0);
	bPassed = bHasFunction;
	ASTEST_END_SHARE

	return bPassed;
}

bool FAngelscriptNativeStaticTypeGlobalBindingTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASNativeStaticTypeGlobal",
		TEXT(R"(
int Entry()
{
	if (!__StaticType_AActor.IsValid())
		return 5;
	if (!(__StaticType_AActor.Get() == AActor::StaticClass()))
		return 7;
	if (!(__StaticType_AActor == AActor::StaticClass()))
		return 10;
	if (!__StaticType_AActor.IsChildOf(AActor::StaticClass()))
		return 20;
	return IsValid(__StaticType_AActor.GetDefaultObject()) ? 1 : 30;
}
)"));
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

	TestEqual(TEXT("Native static type globals should expose usable UClass values"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

#endif
