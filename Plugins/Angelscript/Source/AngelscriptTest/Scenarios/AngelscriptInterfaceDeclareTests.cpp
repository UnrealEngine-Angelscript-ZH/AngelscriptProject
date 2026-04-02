#include "AngelscriptScenarioTestUtils.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceDeclareBasicTest,
	"Angelscript.TestModule.Scenario.Interface.DeclareBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptScenarioInterfaceDeclareInheritanceTest,
	"Angelscript.TestModule.Scenario.Interface.DeclareInheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptScenarioInterfaceDeclareBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceDeclareBasic"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* InterfaceClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceDeclareBasic.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageable
{
	void TakeDamage(float Amount);
}
)AS"),
		TEXT("UIDamageable"));

	TestNotNull(TEXT("Interface class UIDamageable should be generated"), InterfaceClass);
	if (InterfaceClass != nullptr)
	{
		TestTrue(TEXT("Interface class should have CLASS_Interface flag"), InterfaceClass->HasAnyClassFlags(CLASS_Interface));
	}
	return true;
}

bool FAngelscriptScenarioInterfaceDeclareInheritanceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetResetSharedTestEngine();
	static const FName ModuleName(TEXT("ScenarioInterfaceDeclareInheritance"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedInitializedTestEngine(Engine);
	};

	UClass* ChildInterface = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("ScenarioInterfaceDeclareInheritance.as"),
		TEXT(R"AS(
UINTERFACE()
interface UIDamageableInh
{
	void TakeDamage(float Amount);
}

UINTERFACE()
interface UIKillableInh : UIDamageableInh
{
	void Kill();
}
)AS"),
		TEXT("UIKillableInh"));

	TestNotNull(TEXT("Child interface class UIKillableInh should be generated"), ChildInterface);
	if (ChildInterface != nullptr)
	{
		TestTrue(TEXT("Child interface should have CLASS_Interface flag"), ChildInterface->HasAnyClassFlags(CLASS_Interface));
	}
	return true;
}

#endif
