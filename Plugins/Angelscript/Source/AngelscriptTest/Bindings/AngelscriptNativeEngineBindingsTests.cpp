#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestEngineHelper.h"
#include "ClassGenerator/ASClass.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeActorBindingsTest,
	"Angelscript.TestModule.Bindings.NativeActorMethods",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeComponentBindingsTest,
	"Angelscript.TestModule.Bindings.NativeComponentMethods",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptComponentDestroyBindingsTest,
	"Angelscript.TestModule.Bindings.ComponentDestroyCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNativeActorBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASNativeActorBindingTest"),
		TEXT("ASNativeActorBindingTest.as"),
		TEXT(R"(
UCLASS()
class ABindingExampleActor : AActor
{
	UFUNCTION()
	int ReadNativeBindings()
	{
		FVector Location = GetActorLocation();
		FRotator Rotation = GetActorRotation();
		UClass RuntimeType = GetClass();
		FName ClassName = RuntimeType.GetName();
		FString Path = GetPathName();
		FString FullName = GetFullName();
		bool bActorType = IsA(RuntimeType);

		if (Path.Len() < 0 || FullName.Len() < 0 || !bActorType)
			return 0;
		return 1;
	}
}
)"));
	if (!TestTrue(TEXT("Compile annotated actor module using native bindings should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("ABindingExampleActor"));
	if (!TestNotNull(TEXT("Generated actor class should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* ReadNativeBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadNativeBindings"));
	if (!TestNotNull(TEXT("Native actor binding test function should exist"), ReadNativeBindingsFunction))
	{
		return false;
	}

	AActor* RuntimeActor = RuntimeClass->GetDefaultObject<AActor>();
	if (!TestNotNull(TEXT("Generated actor default object should exist"), RuntimeActor))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Native actor binding reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(RuntimeActor, ReadNativeBindingsFunction, Result)))
	{
		return false;
	}
	TestEqual(TEXT("Script class should call bridged native AActor and UObject methods"), Result, 1);
	return Result == 1;
}

bool FAngelscriptNativeComponentBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASNativeComponentBindingTest"),
		TEXT("ASNativeComponentBindingTest.as"),
		TEXT(R"(
UCLASS()
class UBindingSceneComponent : USceneComponent
{
	UFUNCTION()
	int ReadComponentBindings()
	{
		FScopedMovementUpdate ScopedMove(this);
		if (!IsValid(GetOwner()))
			return 10;
		if (!IsValid(GetPackage()) || !IsValid(GetOutermost()))
			return 20;

		Deactivate();
		Activate();

		SetRelativeLocation(FVector(1.0, 2.0, 3.0));
		SetComponentVelocity(FVector(4.0, 5.0, 6.0));
		FVector Relative = GetRelativeLocation();
		FTransform Transform = GetComponentTransform();
		FVector Velocity = GetComponentVelocity();

		if (!Relative.Equals(FVector(1.0, 2.0, 3.0)))
			return 30;
		if (!Transform.GetTranslation().Equals(Relative))
			return 40;
		if (!Velocity.Equals(FVector(4.0, 5.0, 6.0)))
			return 45;
		if (GetNumChildrenComponents() != 0)
			return 50;
		UActorComponent FoundByClass = GetOwner().GetComponent(USceneComponent::StaticClass());
		if (!IsValid(FoundByClass))
			return 80;
		if (!(FoundByClass.GetName() == n"ScriptScene"))
			return 90;
		UActorComponent FoundByName = GetOwner().GetComponent(USceneComponent::StaticClass(), n"ScriptScene");
		if (!IsValid(FoundByName))
			return 100;
		if (!(FoundByName.GetName() == n"ScriptScene"))
			return 110;
		if (!IsValid(USceneComponent::Get(GetOwner())))
			return 120;
		if (!(USceneComponent::Get(GetOwner()).GetName() == n"ScriptScene"))
			return 130;
		if (!IsValid(USceneComponent::Get(GetOwner(), n"ScriptScene")))
			return 140;
		if (!(USceneComponent::Get(GetOwner(), n"ScriptScene").GetName() == n"ScriptScene"))
			return 150;

		TArray<USceneComponent> SceneComponents;
		SceneComponents.Add(USceneComponent::Get(GetOwner()));
		SceneComponents.Empty();
		GetOwner().GetComponentsByClass(SceneComponents);
		if (SceneComponents.Num() != 1)
			return 160;
		if (!IsValid(SceneComponents[0]) || !(SceneComponents[0].GetName() == n"ScriptScene"))
			return 170;

		TArray<UActorComponent> AllComponents;
		GetOwner().GetComponentsByClass(AllComponents);
		if (AllComponents.Num() != 1)
			return 180;
		if (!IsValid(AllComponents[0]) || !(AllComponents[0].GetName() == n"ScriptScene"))
			return 190;

		return ComponentHasTag(NAME_None) ? 0 : 1;
	}
}
)"));
	if (!TestTrue(TEXT("Compile annotated scene component module using native bindings should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UBindingSceneComponent"));
	if (!TestNotNull(TEXT("Generated scene component class should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* ReadComponentBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadComponentBindings"));
	if (!TestNotNull(TEXT("Native component binding test function should exist"), ReadComponentBindingsFunction))
	{
		return false;
	}

	AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
	if (!TestNotNull(TEXT("Transient outer actor should be created"), OuterActor))
	{
		return false;
	}

	USceneComponent* RuntimeComponent = NewObject<USceneComponent>(OuterActor, RuntimeClass, TEXT("ScriptScene"));
	if (!TestNotNull(TEXT("Generated scene component instance should be created"), RuntimeComponent))
	{
		return false;
	}

	OuterActor->AddOwnedComponent(RuntimeComponent);

	int32 Result = 0;
	if (!TestTrue(TEXT("Native component binding reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(RuntimeComponent, ReadComponentBindingsFunction, Result)))
	{
		return false;
	}
	TestEqual(TEXT("Script component should call bridged native component methods"), Result, 1);
	return Result == 1;
}

bool FAngelscriptComponentDestroyBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("ASComponentDestroyCompat"),
		TEXT("ASComponentDestroyCompat.as"),
		TEXT(R"(
UCLASS()
class UDestroyBindingComponent : UActorComponent
{
	UFUNCTION()
	int DestroySelf()
	{
		DestroyComponent();
		return 1;
	}
}
)"));
	if (!TestTrue(TEXT("Compile annotated destroy component module should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UDestroyBindingComponent"));
	if (!TestNotNull(TEXT("Generated destroy component class should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* DestroySelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("DestroySelf"));
	if (!TestNotNull(TEXT("Destroy component function should exist"), DestroySelfFunction))
	{
		return false;
	}

	AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
	if (!TestNotNull(TEXT("Transient actor should be created for destroy component test"), OuterActor))
	{
		return false;
	}

	UActorComponent* RuntimeComponent = NewObject<UActorComponent>(OuterActor, RuntimeClass, TEXT("DestroyBindingComponent"));
	if (!TestNotNull(TEXT("Destroy binding component should be created"), RuntimeComponent))
	{
		return false;
	}

	OuterActor->AddOwnedComponent(RuntimeComponent);

	int32 Result = 0;
	if (!TestTrue(TEXT("Destroy component reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(RuntimeComponent, DestroySelfFunction, Result)))
	{
		return false;
	}

	if (!TestEqual(TEXT("Destroy component function should return success"), Result, 1))
	{
		return false;
	}

	TestTrue(TEXT("DestroyComponent binding should mark the component as being destroyed"), RuntimeComponent->IsBeingDestroyed());
	return RuntimeComponent->IsBeingDestroyed();
}

#endif
