#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Shared/AngelscriptNativeInterfaceTestTypes.h"
#include "Shared/AngelscriptScenarioTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

namespace
{
}

namespace
{
	void TestCallInterfaceMethod(asIScriptGeneric* Generic)
	{
		FInterfaceMethodSignature* Signature = static_cast<FInterfaceMethodSignature*>(Generic->GetFunction()->GetUserData());
		UObject* Object = static_cast<UObject*>(Generic->GetObject());
		if (Signature == nullptr || Object == nullptr)
		{
			return;
		}

		UFunction* RealFunc = Object->FindFunction(Signature->FunctionName);
		if (RealFunc == nullptr)
		{
			return;
		}

		uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(RealFunc->ParmsSize));
		FMemory::Memzero(Buffer, RealFunc->ParmsSize);

		int32 ArgIndex = 0;
		for (TFieldIterator<FProperty> It(RealFunc); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			void* Source = Generic->GetAddressOfArg(ArgIndex++);
			Property->CopySingleValue(Property->ContainerPtrToValuePtr<void>(Buffer), Source);
		}

		Object->ProcessEvent(RealFunc, Buffer);

		if (RealFunc->ReturnValueOffset != MAX_uint16)
		{
			if (FProperty* ReturnProperty = RealFunc->GetReturnProperty())
			{
				void* ReturnSource = ReturnProperty->ContainerPtrToValuePtr<void>(Buffer);
				void* ReturnDestination = Generic->GetAddressOfReturnLocation();
				ReturnProperty->CopySingleValue(ReturnDestination, ReturnSource);
			}
		}

		for (TFieldIterator<FProperty> It(RealFunc); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->DestroyValue(It->ContainerPtrToValuePtr<void>(Buffer));
		}
	}

	void BindNativeInterfaceMethod(FAngelscriptBinds& Binds, const TCHAR* Declaration, const TCHAR* FunctionName)
	{
		FInterfaceMethodSignature* Signature = FAngelscriptEngine::Get().RegisterInterfaceMethodSignature(FName(FunctionName));
		Binds.GenericMethod(FString(Declaration), TestCallInterfaceMethod, Signature);
	}

	void EnsureNativeInterfaceBoundForTests(UClass* InterfaceClass)
	{
		if (InterfaceClass == nullptr)
		{
			return;
		}

		auto* ScriptEngine = FAngelscriptEngine::Get().Engine;
		const FString TypeName = FAngelscriptType::GetBoundClassName(InterfaceClass);
		if (ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) != nullptr)
		{
			return;
		}

		FAngelscriptBinds Binds = FAngelscriptBinds::ReferenceClass(TypeName, InterfaceClass);
		auto* TypeInfo = (asCTypeInfo*)Binds.GetTypeInfo();
		if (TypeInfo != nullptr)
		{
			TypeInfo->plainUserData = (SIZE_T)InterfaceClass;
		}

		if (InterfaceClass == UAngelscriptNativeParentInterface::StaticClass())
		{
			BindNativeInterfaceMethod(Binds, TEXT("int GetNativeValue() const"), TEXT("GetNativeValue"));
			BindNativeInterfaceMethod(Binds, TEXT("void SetNativeMarker(FName Marker)"), TEXT("SetNativeMarker"));
		}
		else if (InterfaceClass == UAngelscriptNativeChildInterface::StaticClass())
		{
			BindNativeInterfaceMethod(Binds, TEXT("int GetChildValue() const"), TEXT("GetChildValue"));
		}
	}

	void EnsureNativeInterfaceFixturesBound()
	{
		EnsureNativeInterfaceBoundForTests(UAngelscriptNativeParentInterface::StaticClass());
		EnsureNativeInterfaceBoundForTests(UAngelscriptNativeChildInterface::StaticClass());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceNativeImplementTest,
	"Angelscript.TestModule.Interface.NativeImplement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceNativeInheritedImplementTest,
	"Angelscript.TestModule.Interface.NativeInheritedImplement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioInterfaceNativeImplementTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireFreshSharedCloneEngine();
	FAngelscriptEngineScope EngineScope(Engine);
	static const FName ModuleName(TEXT("ScenarioInterfaceNativeImplement"));
	EnsureNativeInterfaceFixturesBound();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceNativeImplement.as"),
		TEXT(R"AS(
UCLASS()
class AScenarioInterfaceNativeImplement : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int NativeValue = 123;

	UPROPERTY()
	FName NativeMarker = NAME_None;

	UPROPERTY()
	int ParentCastWorked = 0;

	UFUNCTION()
	int GetNativeValue() const
	{
		return NativeValue;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
		NativeMarker = Marker;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef != nullptr)
		{
			ParentCastWorked = 1;
			NativeValue = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"FromScript");
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceNativeImplement"));
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

	TestTrue(TEXT("Script actor should implement native parent interface"), ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()));

	int32 ParentCastWorked = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ParentCastWorked"), ParentCastWorked))
	{
		return false;
	}
	TestEqual(TEXT("Cast to native parent interface should succeed in script"), ParentCastWorked, 1);

	int32 NativeValue = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("NativeValue"), NativeValue))
	{
		return false;
	}
	TestEqual(TEXT("Script-side native interface call should preserve the returned value"), NativeValue, 123);

	FName NativeMarker = NAME_None;
	if (!ReadPropertyValue<FNameProperty>(*this, Actor, TEXT("NativeMarker"), NativeMarker))
	{
		return false;
	}
	TestEqual(TEXT("Script-side native interface setter should run through the interface reference"), NativeMarker, FName(TEXT("FromScript")));

	TestEqual(TEXT("C++ Execute_ bridge should call the script implementation of GetNativeValue"),
		IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor), 123);
	IAngelscriptNativeParentInterface::Execute_SetNativeMarker(Actor, TEXT("FromCpp"));

	if (!ReadPropertyValue<FNameProperty>(*this, Actor, TEXT("NativeMarker"), NativeMarker))
	{
		return false;
	}
	TestEqual(TEXT("C++ Execute_ bridge should call the script implementation of SetNativeMarker"), NativeMarker, FName(TEXT("FromCpp")));

	return true;
}

bool FAngelscriptScenarioInterfaceNativeInheritedImplementTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AcquireFreshSharedCloneEngine();
	FAngelscriptEngineScope EngineScope(Engine);
	static const FName ModuleName(TEXT("ScenarioInterfaceNativeInheritedImplement"));
	EnsureNativeInterfaceFixturesBound();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceNativeInheritedImplement.as"),
		TEXT(R"AS(
UCLASS()
class AScenarioInterfaceNativeInheritedImplement : AActor, UAngelscriptNativeChildInterface
{
	UPROPERTY()
	int ParentCastWorked = 0;

	UPROPERTY()
	int ChildCastWorked = 0;

	UPROPERTY()
	int ParentResult = 0;

	UPROPERTY()
	int ChildResult = 0;

	UPROPERTY()
	FName NativeMarker = NAME_None;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 7;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
		NativeMarker = Marker;
	}

	UFUNCTION()
	int GetChildValue() const
	{
		return 11;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef != nullptr)
		{
			ParentCastWorked = 1;
			ParentResult = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"ParentRoute");
		}

		UAngelscriptNativeChildInterface ChildRef = Cast<UAngelscriptNativeChildInterface>(Self);
		if (ChildRef != nullptr)
		{
			ChildCastWorked = 1;
			ChildResult = ChildRef.GetChildValue();
		}
	}
}
)AS"),
		TEXT("AScenarioInterfaceNativeInheritedImplement"));
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

	TestTrue(TEXT("Script actor should implement native child interface"), ScriptClass->ImplementsInterface(UAngelscriptNativeChildInterface::StaticClass()));
	TestTrue(TEXT("Script actor implementing child interface should also satisfy native parent interface"), ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()));

	int32 ParentCastWorked = 0;
	int32 ChildCastWorked = 0;
	int32 ParentResult = 0;
	int32 ChildResult = 0;
	FName NativeMarker = NAME_None;

	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ParentCastWorked"), ParentCastWorked)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ChildCastWorked"), ChildCastWorked)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ParentResult"), ParentResult)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ChildResult"), ChildResult)
		|| !ReadPropertyValue<FNameProperty>(*this, Actor, TEXT("NativeMarker"), NativeMarker))
	{
		return false;
	}

	TestEqual(TEXT("Script-side cast to native parent interface should succeed through child implementation"), ParentCastWorked, 1);
	TestEqual(TEXT("Script-side cast to native child interface should succeed"), ChildCastWorked, 1);
	TestEqual(TEXT("Parent native interface method should return the script implementation value"), ParentResult, 7);
	TestEqual(TEXT("Child native interface method should return the script implementation value"), ChildResult, 11);
	TestEqual(TEXT("Parent native interface setter should execute through the parent reference"), NativeMarker, FName(TEXT("ParentRoute")));

	TestEqual(TEXT("C++ Execute_ should dispatch parent interface method on child implementation"),
		IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor), 7);
	TestEqual(TEXT("C++ Execute_ should dispatch child interface method on child implementation"),
		IAngelscriptNativeChildInterface::Execute_GetChildValue(Actor), 11);

	return true;
}

#endif
