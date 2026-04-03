#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"

#include "Engine/ScopedMovementUpdate.h"

#include "UObject/UObjectThreadContext.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#include "StartAngelscriptHeaders.h"
//#include "as_objecttype.h"
//#include "as_scriptengine.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"


/**
 * Binds default methods that all USceneComponents have
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_USceneComponent_Base((int32)FAngelscriptBinds::EOrder::Late-1, []
{
	auto USceneComponent_ = FAngelscriptBinds::ExistingClass("USceneComponent");
	USceneComponent_.Method("int32 GetNumChildrenComponents() const", METHOD_TRIVIAL(USceneComponent, GetNumChildrenComponents));
	auto USphereComponent_ = FAngelscriptBinds::ExistingClass("USphereComponent");
	USphereComponent_.Method("void SetSphereRadius(float32 InSphereRadius, bool bUpdateOverlaps = true)", METHOD_TRIVIAL(USphereComponent, SetSphereRadius));

	USceneComponent_.Method("USceneComponent GetChildComponentByClass(TSubclassOf<USceneComponent> ComponentClass)",
		[](USceneComponent* ParentComp, TSubclassOf<USceneComponent> ComponentClass) -> USceneComponent*
	{
		if (ComponentClass == nullptr)
			return nullptr;

		int ChildCount = ParentComp->GetNumChildrenComponents();
		for (int i = 0; i < ChildCount; ++i)
		{
			USceneComponent* Child = ParentComp->GetChildComponent(i);
			if (Child != nullptr && Child->IsA(ComponentClass))
				return Child;
		}
		
		return nullptr;
	});
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

	USceneComponent_.Method("void GetChildrenComponentsByClass(UClass ComponentClass, bool bIncludeAllDescendants, ?& OutChildren)",
	[](USceneComponent* ParentComp, UClass* ComponentClass, bool bIncludeAllDescendants, TArray<USceneComponent*>& OutComponents, int TypeId)
	{
		auto& Manager = FAngelscriptEngine::Get();
		asCTypeInfo* ScriptType = (asCTypeInfo*)Manager.Engine->GetTypeInfoById(TypeId);
		if (ScriptType == nullptr
			|| (ScriptType->flags & asOBJ_VALUE) == 0)
		{
			FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
			return;
		}

		asCObjectType* ObjectType = (asCObjectType*)(ScriptType);
		if (ObjectType->templateBaseType != FAngelscriptType::GetArrayTemplateTypeInfo())
		{
			FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
			return;
		}

		auto* SubTypeInfo = ObjectType->templateSubTypes[0].GetTypeInfo();
		if (SubTypeInfo == nullptr
			|| (SubTypeInfo->GetFlags() & asOBJ_REF) == 0
			|| (SubTypeInfo->plainUserData == 0))
		{
			FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
			return;
		}

		UClass* SubClass = (UClass*)SubTypeInfo->plainUserData;
		if (!SubClass->IsChildOf<USceneComponent>())
		{
			FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
			return;
		}

		if (ParentComp == nullptr)
		{
			FAngelscriptEngine::Throw("Scene component was null.");
			return;
		}

		if (ComponentClass == nullptr)
		{
			FAngelscriptEngine::Throw("Component class was null.");
			return;
		}

		if (!ComponentClass->IsChildOf(SubClass))
		{
			FAngelscriptEngine::Throw("Class specified to GetChildrenComponentsByClass is not a child of array element class.");
			return;
		}

		TArray<USceneComponent*> Children;
		ParentComp->GetChildrenComponents(bIncludeAllDescendants, Children);
		for (USceneComponent* Comp : Children)
		{
			if (Comp == nullptr)
				continue;
			if (Comp->IsA(ComponentClass))
				OutComponents.Add(Comp);
		}
	});

	USceneComponent_.Method("FTransform GetComponentTransform() const", [](USceneComponent* Component) -> FTransform
	{
		if (Component == nullptr)
		{
			return FTransform::Identity;
		}

		if (!Component->IsRegistered())
		{
			const FTransform RelativeTransform = Component->GetRelativeTransform();
			if (USceneComponent* AttachParent = Component->GetAttachParent())
			{
				return RelativeTransform * AttachParent->GetComponentTransform();
			}
			return RelativeTransform;
		}

		return Component->GetComponentTransform();
	});
	USceneComponent_.Method("void SetRelativeLocation(FVector NewLocation)", [](USceneComponent* Component, FVector NewLocation)
	{
		Component->SetRelativeLocation(NewLocation);
		Component->UpdateComponentToWorld();
	});
	USceneComponent_.Method("void SetComponentVelocity(const FVector& Velocity)", [](USceneComponent* SceneComponent, const FVector& Velocity)
	{
		SceneComponent->ComponentVelocity = Velocity;
	});
	USceneComponent_.Method("FVector GetComponentVelocity() const", METHOD_TRIVIAL(USceneComponent, GetComponentVelocity));

	struct FScriptScopedMovementUpdate : public FScopedMovementUpdate
	{
		void* operator new(std::size_t Count, void* Ptr)
		{
			return Ptr;
		}

		void operator delete(void*) {}

		explicit FScriptScopedMovementUpdate(USceneComponent* Component)
			: FScopedMovementUpdate(Component)
		{
		}

		~FScriptScopedMovementUpdate() {}
	};

	auto FScopedMovementUpdate_ = FAngelscriptBinds::ValueClass<FScriptScopedMovementUpdate>("FScopedMovementUpdate", FBindFlags());
	FScopedMovementUpdate_.Constructor("void f(USceneComponent Component)", [](FScriptScopedMovementUpdate* Address, USceneComponent* SceneComp)
	{
		new(Address) FScriptScopedMovementUpdate(SceneComp);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FScopedMovementUpdate_.Destructor("void f()", [](FScriptScopedMovementUpdate& Scope)
	{
		Scope.~FScriptScopedMovementUpdate();
	});

	#if WITH_EDITOR
	// Small hack: We want to have GetSocketQuaternion in script, but the BP version is deprecated.
	// To avoid having two binds, one deprecated and one manual, we mark the function as not bound automatically here:
	UFunction* Function_GetSocketQuaternion = FindObject<UFunction>(nullptr, TEXT("/Script/Engine.SceneComponent:GetSocketQuaternion"));
	if (Function_GetSocketQuaternion != nullptr)
		Function_GetSocketQuaternion->SetMetaData(TEXT("NotInAngelscript"), TEXT("true"));
#endif
});
