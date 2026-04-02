#include "../Shared/AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectPtrBindingsTest,
	"Angelscript.TestModule.Bindings.ObjectPtrCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSoftObjectPtrBindingsTest,
	"Angelscript.TestModule.Bindings.SoftObjectPtrCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectPtrBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASObjectPtrCompat",
		TEXT(R"(
int Entry()
{
	TObjectPtr<UTexture2D> Empty;
	if (!(Empty.Get() == null))
		return 10;

	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	if (!IsValid(Texture))
		return 20;

	TObjectPtr<UTexture2D> Constructed(Texture);
	if (!(Constructed == Texture))
		return 30;
	if (!(Constructed.Get() == Texture))
		return 40;

	TObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	if (!(Assigned == Texture))
		return 50;
	if (!(Assigned.Get() == Texture))
		return 60;

	UTexture2D Converted = Assigned;
	if (!(Converted == Texture))
		return 70;

	TObjectPtr<UTexture2D> Copy = Assigned;
	if (!(Copy == Assigned))
		return 80;

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

	TestEqual(TEXT("TObjectPtr compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptSoftObjectPtrBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSoftObjectPtrCompat",
		TEXT(R"(
int Entry()
{
	TSoftObjectPtr<UTexture2D> Empty;
	if (!Empty.IsNull())
		return 10;
	if (!(Empty.Get() == null))
		return 20;
	if (Empty.IsValid())
		return 30;

	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	if (!IsValid(Texture))
		return 40;

	TSoftObjectPtr<UTexture2D> Constructed(Texture);
	if (!(Constructed == Texture))
		return 50;
	if (!(Constructed.Get() == Texture))
		return 60;
	if (!(Constructed.ToSoftObjectPath().ToString() == Constructed.ToString()))
		return 70;
	if (Constructed.IsPending())
		return 80;
	if (!Constructed.IsValid())
		return 90;
	if (Constructed.GetAssetName().IsEmpty())
		return 100;
	if (Constructed.GetLongPackageName().IsEmpty())
		return 110;
	if (Constructed.ToString().IsEmpty())
		return 120;

	FSoftObjectPath ConstructedPath = Constructed.ToSoftObjectPath();
	if (ConstructedPath.ToString().IsEmpty())
		return 125;

	TSoftObjectPtr<UTexture2D> FromPath(ConstructedPath);
	if (!(FromPath == Texture))
		return 126;

	TSoftObjectPtr<UTexture2D> AssignedFromPath;
	AssignedFromPath = ConstructedPath;
	if (!(AssignedFromPath == Texture))
		return 127;

	TSoftObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	if (!(Assigned == Texture))
		return 130;
	if (!(Assigned.Get() == Texture))
		return 140;

	UTexture2D Converted = Assigned.Get();
	if (!(Converted == Texture))
		return 150;

	TSoftObjectPtr<UTexture2D> Copy = Assigned;
	if (!(Copy == Assigned))
		return 160;

	TArray<TSoftObjectPtr<UTexture2D>> History;
	History.Add(Constructed);
	History.Add(Copy);
	if (History.Num() != 2)
		return 170;
	if (!(History[0] == Texture))
		return 180;
	if (!History.Contains(Copy))
		return 190;

	Assigned.Reset();
	if (!Assigned.IsNull())
		return 200;
	if (!(Assigned.Get() == null))
		return 210;

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

	TestEqual(TEXT("TSoftObjectPtr compat operations should behave as expected"), Result, 1);
	return true;
}

#endif
