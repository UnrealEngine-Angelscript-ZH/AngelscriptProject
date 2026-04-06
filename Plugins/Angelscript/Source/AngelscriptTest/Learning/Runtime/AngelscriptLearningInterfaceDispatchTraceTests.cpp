#include "../../Shared/AngelscriptLearningTrace.h"
#include "../../Shared/AngelscriptScenarioTestUtils.h"
#include "../../Shared/AngelscriptTestEngineHelper.h"
#include "../../Shared/AngelscriptTestUtilities.h"
#include "../../Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptScenarioTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningInterfaceDispatchTraceTest,
	"Angelscript.TestModule.Learning.Runtime.InterfaceDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningInterfaceDispatchTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	static const FName ModuleName(TEXT("LearningInterfaceDispatchModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UINTERFACE()
interface UILearningDamageable
{
	void TakeDamage(float Amount);
}

UCLASS()
class ALearningInterfaceDispatchActor : AActor, UILearningDamageable
{
	UPROPERTY()
	float DamageAccumulated = 0.0;

	UFUNCTION()
	void TakeDamage(float Amount)
	{
		DamageAccumulated += Amount;
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningInterfaceDispatch"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningInterfaceDispatchModule.as"),
		ScriptSource,
		TEXT("ALearningInterfaceDispatchActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileInterfaceAndImplementer"), TEXT("Compiled the UINTERFACE declaration and the actor class that implements it through the Angelscript type system"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());

	UClass* InterfaceClass = FindGeneratedClass(&Engine, TEXT("UILearningDamageable"));
	Trace.AddStep(TEXT("FindGeneratedInterface"), InterfaceClass != nullptr ? TEXT("Resolved the generated UInterface UClass from the script UINTERFACE declaration") : TEXT("Failed to find generated interface UClass"));
	Trace.AddKeyValue(TEXT("InterfaceClassName"), InterfaceClass != nullptr ? InterfaceClass->GetName() : TEXT("<null>"));

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnImplementerActor"), TEXT("Spawned an actor instance of the script class that implements the interface"));

	const bool bImplements = InterfaceClass != nullptr && Actor->GetClass()->ImplementsInterface(InterfaceClass);
	Trace.AddStep(TEXT("CheckImplementsInterface"), bImplements ? TEXT("Verified that the spawned actor's class reports ImplementsInterface for the generated UInterface") : TEXT("ImplementsInterface check failed"));
	Trace.AddKeyValue(TEXT("ImplementsInterface"), bImplements ? TEXT("true") : TEXT("false"));

	if (InterfaceClass != nullptr)
	{
		UFunction* TakeDamageFunc = InterfaceClass->FindFunctionByName(TEXT("TakeDamage"));
		Trace.AddStep(TEXT("InspectInterfaceUFunction"), TakeDamageFunc != nullptr ? TEXT("Located the UFunction representing the interface method in the generated UInterface") : TEXT("Failed to find interface UFunction"));
		Trace.AddKeyValue(TEXT("InterfaceUFunctionName"), TakeDamageFunc != nullptr ? TakeDamageFunc->GetName() : TEXT("<null>"));
		Trace.AddKeyValue(TEXT("InterfaceUFunctionFlags"), TakeDamageFunc != nullptr ? FString::Printf(TEXT("0x%08X"), TakeDamageFunc->FunctionFlags) : TEXT("<null>"));
	}

	Trace.AddStep(TEXT("InterfaceDispatchBoundary"), TEXT("Interface dispatch in Angelscript is handled by the engine's native bridge; the learning trace shows the structural visibility rather than duplicating internal callstack internals"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("Interface dispatch learning script should produce a script actor class"), ScriptClass);
	const bool bInterfaceGenerated = TestNotNull(TEXT("Interface dispatch learning script should generate the UInterface"), InterfaceClass);
	const bool bImplementsInterface = TestTrue(TEXT("Spawned actor should implement the generated interface"), bImplements);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsInterfaceKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("InterfaceClassName"));
	const bool bContainsImplementsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ImplementsInterface"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 5);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bInterfaceGenerated
		&& bImplementsInterface
		&& bPhaseSequenceOk
		&& bContainsInterfaceKeyword
		&& bContainsImplementsKeyword
		&& bMinimumEventsOk;

	ASTEST_END_SHARE_CLEAN
}

#endif
