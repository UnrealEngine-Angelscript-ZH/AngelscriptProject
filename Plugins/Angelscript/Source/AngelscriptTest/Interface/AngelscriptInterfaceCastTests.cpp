#include "Shared/AngelscriptScenarioTestUtils.h"

#include "Core/AngelscriptActor.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_callfunc.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: UE Scenario
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

namespace
{
	void AddGlobalCastDiagnostics(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (asUINT FunctionIndex = 0; FunctionIndex < ScriptEngine->GetGlobalFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* GlobalFunction = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
			if (GlobalFunction == nullptr)
			{
				continue;
			}

			const char* Declaration = GlobalFunction->GetDeclaration();
			if (Declaration != nullptr && FCStringAnsi::Strstr(Declaration, "Cast") != nullptr)
			{
				Test.AddInfo(FString::Printf(TEXT("Global cast diagnostic: %s"), ANSI_TO_TCHAR(Declaration)));
			}
		}
	}

	void AddFunctionBytecodeDiagnostics(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptFunction* Function, const TCHAR* Label)
	{
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		Test.AddInfo(FString::Printf(
			TEXT("%s declaration=%s bytecodeLength=%u"),
			Label,
			ANSI_TO_TCHAR(Function->GetDeclaration()),
			BytecodeLength));

		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return;
		}

		auto DescribeSystemFunctionTarget = [&Test, Label](int32 InstructionIndex, const FString& CallKind, asCScriptFunction* TargetFunction)
		{
			if (TargetFunction == nullptr)
			{
				Test.AddInfo(FString::Printf(TEXT("%s bytecode[%d]: %s -> <null>"), Label, InstructionIndex, *CallKind));
				return;
			}

			const asSSystemFunctionInterface* SystemFunction = TargetFunction->sysFuncIntf;
			Test.AddInfo(FString::Printf(
				TEXT("%s bytecode[%d]: %s -> %s funcType=%d callConv=%d callerType=%d passMeta=%d"),
				Label,
				InstructionIndex,
				*CallKind,
				ANSI_TO_TCHAR(TargetFunction->GetDeclaration()),
				static_cast<int32>(TargetFunction->funcType),
				SystemFunction != nullptr ? static_cast<int32>(SystemFunction->callConv) : -1,
				SystemFunction != nullptr ? SystemFunction->caller.type : -1,
				SystemFunction != nullptr ? static_cast<int32>(SystemFunction->passFirstParamMetaData) : -1));
		};

		int32 InstructionIndex = 0;
		for (asDWORD* Instruction = Bytecode; Instruction < Bytecode + BytecodeLength && InstructionIndex < 64; )
		{
			const asEBCInstr Op = static_cast<asEBCInstr>(*reinterpret_cast<asBYTE*>(Instruction));
			const FString DebugString = ANSI_TO_TCHAR(asBCInfo[Op].name);
			switch (Op)
			{
			case asBC_CALL:
			case asBC_CALLINTF:
				{
					const int32 FunctionId = asBC_INTARG(Instruction);
					asIScriptFunction* TargetFunction = ScriptEngine->GetFunctionById(FunctionId);
					Test.AddInfo(FString::Printf(
						TEXT("%s bytecode[%d]: %s -> %s"),
						Label,
						InstructionIndex,
						*DebugString,
						TargetFunction != nullptr ? ANSI_TO_TCHAR(TargetFunction->GetDeclaration()) : TEXT("<null>")));
				}
				break;

			case asBC_CALLBND:
				{
					auto* NativeEngine = static_cast<asCScriptEngine*>(ScriptEngine);
					const int32 BindId = asBC_INTARG(Instruction);
					asCScriptFunction* TargetFunction = nullptr;
					if (NativeEngine != nullptr
						&& BindId >= 0
						&& (BindId & FUNC_IMPORTED) != 0
						&& NativeEngine->importedFunctions[(int)BindId & ~FUNC_IMPORTED] != nullptr)
					{
						const int32 FunctionId = NativeEngine->importedFunctions[(int)BindId & ~FUNC_IMPORTED]->boundFunctionId;
						TargetFunction = FunctionId >= 0 ? NativeEngine->GetScriptFunction(FunctionId) : nullptr;
					}

					DescribeSystemFunctionTarget(InstructionIndex, DebugString, TargetFunction);
				}
				break;

			case asBC_CALLSYS:
			case asBC_Thiscall1:
				DescribeSystemFunctionTarget(
					InstructionIndex,
					DebugString,
					reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(Instruction)));
				break;

			case asBC_Cast:
				Test.AddInfo(FString::Printf(
					TEXT("%s bytecode[%d]: %s -> %s"),
					Label,
					InstructionIndex,
					*DebugString,
					ANSI_TO_TCHAR(ScriptEngine->GetTypeDeclaration(asBC_INTARG(Instruction)))));
				break;

			case asBC_TYPEID:
				Test.AddInfo(FString::Printf(
					TEXT("%s bytecode[%d]: %s -> %s (%d)"),
					Label,
					InstructionIndex,
					*DebugString,
					ANSI_TO_TCHAR(ScriptEngine->GetTypeDeclaration(asBC_INTARG(Instruction))),
					asBC_INTARG(Instruction)));
				break;

			default:
				Test.AddInfo(FString::Printf(
					TEXT("%s bytecode[%d]: %s"),
					Label,
					InstructionIndex,
					*DebugString));
				break;
			}

			Instruction += asBCTypeSize[asBCInfo[Op].type];
			++InstructionIndex;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceCastSuccessTest,
	"Angelscript.TestModule.Interface.CastSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceCastFailTest,
	"Angelscript.TestModule.Interface.CastFail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceMethodCallTest,
	"Angelscript.TestModule.Interface.MethodCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioInterfaceCastSuccessTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireFreshSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceCastSuccess"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceCastSuccess.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableCastOk
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceCastSuccess : AAngelscriptActor, UIDamageableCastOk
{
	UPROPERTY()
	int CastSucceeded = 0;

	UFUNCTION()
	void TakeDamage(float Amount) {}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableCastOk Casted = Cast<UIDamageableCastOk>(Self);
		if (Casted != nullptr)
		{
			CastSucceeded = 1;
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceCastSuccess"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}

	asITypeInfo* InterfaceScriptType = Engine.Engine->GetTypeInfoByName("UIDamageableCastOk");
	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UIDamageableCastOk"));
	UClass* AssociatedClass = InterfaceScriptType != nullptr ? static_cast<UClass*>(InterfaceScriptType->GetUserData()) : nullptr;
	const bool bActorImplementsInterface = InterfaceClass != nullptr && Actor->GetClass()->ImplementsInterface(InterfaceClass);
	const bool bAssociatedClassIsInterface = AssociatedClass != nullptr && AssociatedClass->HasAnyClassFlags(CLASS_Interface);
	UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
	asITypeInfo* ScriptClassType = ScriptASClass != nullptr ? static_cast<asITypeInfo*>(ScriptASClass->ScriptTypePtr) : nullptr;
	const bool bScriptTypeImplementsInterface = ScriptClassType != nullptr && InterfaceScriptType != nullptr && ScriptClassType->Implements(InterfaceScriptType);
	const bool bScriptTypeDerivesFromInterface = ScriptClassType != nullptr && InterfaceScriptType != nullptr && ScriptClassType->DerivesFrom(InterfaceScriptType);

	for (asUINT FunctionIndex = 0; FunctionIndex < Engine.Engine->GetGlobalFunctionCount(); ++FunctionIndex)
	{
		asIScriptFunction* GlobalFunction = Engine.Engine->GetGlobalFunctionByIndex(FunctionIndex);
		if (GlobalFunction != nullptr && FCStringAnsi::Strcmp(GlobalFunction->GetName(), "Cast") == 0)
		{
			AddInfo(FString::Printf(TEXT("Global Cast declaration: %s"), ANSI_TO_TCHAR(GlobalFunction->GetDeclaration())));
		}
	}

	AddGlobalCastDiagnostics(*this, Engine.Engine);

	if (ScriptClassType != nullptr)
	{
		for (asUINT MethodIndex = 0; MethodIndex < ScriptClassType->GetMethodCount(); ++MethodIndex)
		{
			asIScriptFunction* Method = ScriptClassType->GetMethodByIndex(MethodIndex);
			if (Method == nullptr || Method->GetObjectType() != ScriptClassType)
			{
				continue;
			}

			asUINT MethodBytecodeLength = 0;
			Method->GetByteCode(&MethodBytecodeLength);
			AddInfo(FString::Printf(
				TEXT("Script method diagnostic: %s bytecodeLength=%u"),
				ANSI_TO_TCHAR(Method->GetDeclaration()),
				MethodBytecodeLength));

			if (FCStringAnsi::Strstr(Method->GetDeclaration(), "BeginPlay") != nullptr)
			{
				AddFunctionBytecodeDiagnostics(*this, Engine.Engine, Method, TEXT("BeginPlay"));
			}
		}
	}

	AddInfo(FString::Printf(
		TEXT("Interface cast diagnostics: ScriptType=%p InterfaceClass=%s AssociatedClass=%s Implements=%s AssociatedIsInterface=%s ScriptTypeImplements=%s ScriptTypeDerives=%s"),
		InterfaceScriptType,
		InterfaceClass != nullptr ? *InterfaceClass->GetName() : TEXT("<null>"),
		AssociatedClass != nullptr ? *AssociatedClass->GetName() : TEXT("<null>"),
		bActorImplementsInterface ? TEXT("true") : TEXT("false"),
		bAssociatedClassIsInterface ? TEXT("true") : TEXT("false"),
		bScriptTypeImplementsInterface ? TEXT("true") : TEXT("false"),
		bScriptTypeDerivesFromInterface ? TEXT("true") : TEXT("false")));

	if (!TestNotNull(TEXT("Interface cast test should register a script type for the generated interface"), InterfaceScriptType))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Interface cast test should generate the interface UClass"), InterfaceClass))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Interface cast test should bind interface script type userdata to a UClass"), AssociatedClass))
	{
		return false;
	}

	TestTrue(TEXT("Spawned actor should report ImplementsInterface for the generated interface"), bActorImplementsInterface);
	TestTrue(TEXT("Interface script type userdata should point to an interface UClass"), bAssociatedClassIsInterface);
	TestEqual(TEXT("Interface script type userdata should match the generated interface class"), AssociatedClass, InterfaceClass);

	BeginPlayActor(Engine, *Actor);

	int32 CastSucceeded = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastSucceeded"), CastSucceeded))
	{
		return false;
	}

	TestEqual(TEXT("Cast to interface should succeed for implementing actor"), CastSucceeded, 1);
	return true;
}

bool FAngelscriptScenarioInterfaceCastFailTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireFreshSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceCastFail"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceCastFail.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableCastFail
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceCastFail : AAngelscriptActor
{
	UPROPERTY()
	int CastReturnedNull = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableCastFail Casted = Cast<UIDamageableCastFail>(Self);
		if (Casted == nullptr)
		{
			CastReturnedNull = 1;
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceCastFail"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}

	BeginPlayActor(Engine, *Actor);

	int32 CastReturnedNull = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastReturnedNull"), CastReturnedNull))
	{
		return false;
	}

	TestEqual(TEXT("Cast to interface should fail for non-implementing actor"), CastReturnedNull, 1);
	return true;
}

bool FAngelscriptScenarioInterfaceMethodCallTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireFreshSharedCloneEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceMethodCall"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceMethodCall.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableMethodCall
{
	void TakeDamage(float Amount);
}

UCLASS()
class AScenarioInterfaceMethodCall : AAngelscriptActor, UIDamageableMethodCall
{
	UPROPERTY()
	int CastSucceeded = 0;

	UPROPERTY()
	int MethodCalled = 0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		MethodCalled = 1;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UIDamageableMethodCall Casted = Cast<UIDamageableMethodCall>(Self);
		if (Casted != nullptr)
		{
			CastSucceeded = 1;
			Casted.TakeDamage(42.0);
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceMethodCall"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UIDamageableMethodCall"));
	TestNotNull(TEXT("Interface class should exist"), InterfaceClass);
	if (InterfaceClass != nullptr)
	{
		TestTrue(TEXT("ScriptClass should implement UIDamageableMethodCall"), ScriptClass->ImplementsInterface(InterfaceClass));
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}

	BeginPlayActor(Engine, *Actor);

	int32 CastSucceeded = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("CastSucceeded"), CastSucceeded))
	{
		return false;
	}
	TestEqual(TEXT("Cast to interface type should succeed"), CastSucceeded, 1);

	int32 MethodCalled = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("MethodCalled"), MethodCalled))
	{
		return false;
	}
	TestEqual(TEXT("Method should have been called via interface reference"), MethodCalled, 1);

	return true;
}

#endif
