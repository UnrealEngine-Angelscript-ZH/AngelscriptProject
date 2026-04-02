#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestEngineHelper.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScriptDelegateBindingsTest,
	"Angelscript.TestModule.Bindings.ScriptDelegateCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSoftPathBindingsTest,
	"Angelscript.TestModule.Bindings.SoftPathCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSourceMetadataBindingsTest,
	"Angelscript.TestModule.Bindings.SourceMetadataCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFileHelperBindingsTest,
	"Angelscript.TestModule.Bindings.FileHelperCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScriptDelegateBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASScriptDelegateCompat",
		TEXT(R"(
delegate int FNativeCallback(int Value, const FString& Label);
event void FNativeEvent(int Value, const FString& Label);

int Entry()
{
	UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();

	FNativeCallback Single;
	if (Single.IsBound())
		return 5;

	Single.BindUFunction(TestObject, n"NativeIntStringEvent");
	if (!Single.IsBound())
		return 10;
	if (!Single.GetFunctionName().IsEqual(n"NativeIntStringEvent"))
		return 15;

	FNativeEvent Multi;
	if (Multi.IsBound())
		return 20;

	Multi.AddUFunction(TestObject, n"SetIntStringFromDelegate");
	if (!Multi.IsBound())
		return 25;

	Multi.Unbind(TestObject, n"SetIntStringFromDelegate");
	if (Multi.IsBound())
		return 30;

	Single.Clear();
	if (Single.IsBound())
		return 35;

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

	TestEqual(TEXT("Script delegate compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptSoftPathBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSoftPathCompat",
		TEXT(R"(
int Entry()
{
	FSoftObjectPath EmptyPath;
	if (!EmptyPath.IsNull())
		return 10;

	FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
	if (!ObjectPath.IsValid())
		return 20;
	if (ObjectPath.GetAssetName().IsEmpty())
		return 30;
	if (ObjectPath.GetLongPackageName().IsEmpty())
		return 40;
	if (ObjectPath.IsSubobject())
		return 50;
	if (!(ObjectPath == FSoftObjectPath(AActor::StaticClass().GetPathName())))
		return 60;

	FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
	if (!ClassPath.IsValid())
		return 70;
	if (ClassPath.GetAssetName().IsEmpty())
		return 80;
	if (ClassPath.GetLongPackageName().IsEmpty())
		return 90;
	if (ClassPath.IsSubobject())
		return 100;

	FSoftClassPath Copy = ClassPath;
	if (!(Copy.ToString() == ClassPath.ToString()))
		return 110;
	if (Copy.ToString().IsEmpty())
		return 120;

	FSoftClassPath FromString(ClassPath.ToString());
	if (!(FromString.ToString() == ClassPath.ToString()))
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

	TestEqual(TEXT("SoftPath compat operations should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptSourceMetadataBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();

	const FString Script = TEXT(R"AS(
UCLASS()
class UBindingSourceMetadataCarrier : UObject
{
	UFUNCTION()
	int ComputeValue()
	{
		return 7;
	}
}
)AS");
	const FString ScriptDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Script/Automation"));
	IFileManager::Get().MakeDirectory(*ScriptDirectory, true);
	const FString ScriptPath = ScriptDirectory / TEXT("RuntimeSourceMetadataBindingsTest.as");
	if (!TestTrue(TEXT("Write source metadata script file should succeed"), FFileHelper::SaveStringToFile(Script, *ScriptPath)))
	{
		return false;
	}

	const bool bAnnotatedCompiled = CompileAnnotatedModuleFromMemory(&Engine, TEXT("RuntimeSourceMetadataBindingsTest"), ScriptPath, Script);
	if (!TestTrue(TEXT("Compile annotated source metadata module should succeed"), bAnnotatedCompiled))
	{
		IFileManager::Get().Delete(*ScriptPath, false, true, true);
		return false;
	}

	FString RuntimeScript = TEXT(R"AS(
int Entry()
{
	UClass Type = FindClass("UBindingSourceMetadataCarrier");
	if (Type == null)
		return 10;
	if (!(Type.GetSourceFilePath() == "__SCRIPT_PATH__"))
		return 20;
	if (!Type.GetScriptModuleName().Contains("RuntimeSourceMetadataBindingsTest"))
		return 30;
	if (Type.GetScriptTypeDeclaration().IsEmpty())
		return 35;
	if (!Type.IsFunctionImplementedInScript(n"ComputeValue"))
		return 37;

	UFunction Func = Type.FindFunctionByName(n"ComputeValue");
	if (Func == null)
		return 40;
	if (!(Func.GetSourceFilePath() == "__SCRIPT_PATH__"))
		return 50;
	if (Func.GetSourceLineNumber() != 6)
		return 60;
	if (!(Func.GetScriptFunctionDeclaration() == "int ComputeValue()"))
		return 70;

	return 1;
}
)AS");
	RuntimeScript.ReplaceInline(TEXT("__SCRIPT_PATH__"), *ScriptPath.ReplaceCharWithEscapedChar());

	asIScriptModule* Module = BuildModule(*this, Engine, "ASSourceMetadataQuery", RuntimeScript);
	if (Module == nullptr)
	{
		IFileManager::Get().Delete(*ScriptPath, false, true, true);
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		IFileManager::Get().Delete(*ScriptPath, false, true, true);
		return false;
	}

	int32 Result = 0;
	const bool bExecuted = ExecuteIntFunction(*this, Engine, *Function, Result);
	IFileManager::Get().Delete(*ScriptPath, false, true, true);
	if (!TestTrue(TEXT("Execute source metadata accessor function should succeed"), bExecuted))
	{
		return false;
	}

	TestEqual(TEXT("Source metadata accessors should behave as expected"), Result, 1);
	return true;
}

bool FAngelscriptFileHelperBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFileHelperCompat",
		TEXT(R"(
int Entry()
{
	FString Filename = FPaths::CombinePaths(FPaths::ProjectSavedDir(), "AngelscriptFileHelperCompat.txt");
	if (!FFileHelper::SaveStringToFile("HelloFileHelper", Filename))
		return 10;

	FString Loaded;
	if (!FFileHelper::LoadFileToString(Loaded, Filename))
		return 20;
	if (!(Loaded == "HelloFileHelper"))
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

	TestEqual(TEXT("FileHelper compat operations should behave as expected"), Result, 1);
	return true;
}

#endif
