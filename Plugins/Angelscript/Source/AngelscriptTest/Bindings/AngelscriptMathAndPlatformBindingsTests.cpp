#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"
#include "../Shared/AngelscriptTestEngineHelper.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMathExtendedBindingsTest,
	"Angelscript.TestModule.Bindings.MathExtendedCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPlatformProcessBindingsTest,
	"Angelscript.TestModule.Bindings.PlatformProcessCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLoggingBindingsTest,
	"Angelscript.TestModule.Bindings.Logging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMathExtendedBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMathExtendedCompat",
		TEXT(R"(
int Entry()
{
	int Helper = Math::RandHelper(10);
	if (Helper < 0 || Helper >= 10)
		return 10;

	if (!Math::IsPowerOfTwo(8))
		return 20;
	if (Math::IsPowerOfTwo(7))
		return 30;

	FVector RandomUnit = Math::VRand();
	if (RandomUnit.IsNearlyZero())
		return 40;

	FVector RandomCone = Math::VRandCone(FVector::ForwardVector, 0.5f);
	if (RandomCone.IsNearlyZero())
		return 50;

	FVector2D PointInCircle = Math::RandPointInCircle(10.0f);
	if (PointInCircle.Size() > 10.01f)
		return 60;

	if (Math::ClampAngle(370.0f, -180.0f, 180.0f) < -180.0f || Math::ClampAngle(370.0f, -180.0f, 180.0f) > 180.0f)
		return 70;
	if (Math::Clamp(int64(20), int64(0), int64(5)) != 5)
		return 75;
	if (Math::Clamp(uint64(20), uint64(0), uint64(5)) != 5)
		return 77;

	if (Math::FindDeltaAngleDegrees(10.0f, 20.0f) != 10.0f)
		return 80;
	if (Math::UnwindDegrees(370.0f) != 10.0f)
		return 90;

	float Pulse = Math::MakePulsatingValue(1.25, 2.0f, 0.0f);
	if (Pulse < 0.0f || Pulse > 1.0f)
		return 100;

	FVector Reflected = Math::GetReflectionVector(FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f));
	if (Reflected.IsNearlyZero())
		return 110;

	FVector Interped = Math::VInterpTo(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 0.1f, 5.0f);
	if (Interped.IsNearlyZero())
		return 120;

	FRotator RotInterp = Math::RInterpTo(FRotator::ZeroRotator, FRotator(0.0f, 90.0f, 0.0f), 0.1f, 5.0f);
	if (RotInterp.IsNearlyZero())
		return 130;

	float ScalarInterp = Math::FInterpTo(0.0f, 100.0f, 0.1f, 5.0f);
	if (ScalarInterp <= 0.0f)
		return 140;

	FVector RandomPoint = Math::RandomPointInBoundingBox(FVector::ZeroVector, FVector(10.0f, 10.0f, 10.0f));
	if (Math::Abs(RandomPoint.X) > 10.01f || Math::Abs(RandomPoint.Y) > 10.01f || Math::Abs(RandomPoint.Z) > 10.01f)
		return 150;

	FRotator RandomRot = Math::RandomRotator(false);
	if (RandomRot.IsNearlyZero())
		return 160;

	float32 ScalarCubic = Math::CubicInterp(0.0f, 1.0f, 10.0f, 0.0f, 0.5f);
	if (ScalarCubic <= 0.0f)
		return 170;
	float32 ScalarDerivative = Math::CubicInterpDerivative(0.0f, 1.0f, 10.0f, 0.0f, 0.5f);
	if (ScalarDerivative <= 0.0f)
		return 180;
	float64 ScalarCubic64 = Math::CubicInterp(0.0, 1.0, 10.0, 0.0, 0.5);
	if (ScalarCubic64 <= 0.0)
		return 190;
	float64 ScalarDerivative64 = Math::CubicInterpDerivative(0.0, 1.0, 10.0, 0.0, 0.5);
	if (ScalarDerivative64 <= 0.0)
		return 200;

	FVector2f Direction;
	float32 DirectionLength = 0.0f;
	FVector2f(3.0f, 4.0f).ToDirectionAndLength(Direction, DirectionLength);
	if (!Direction.Equals(FVector2f(0.6f, 0.8f), 0.001f))
		return 205;
	if (!Math::IsNearlyEqual(DirectionLength, 5.0f, 0.001f))
		return 210;

	if (Math::Abs(int64(-7)) != 7)
		return 215;
	if (Math::Sign(int64(-7)) != -1 || Math::Sign(int64(0)) != 0 || Math::Sign(int64(7)) != 1)
		return 220;
	if (Math::Min(int64(7), int64(-3)) != -3)
		return 225;
	if (Math::Max(int64(7), int64(-3)) != 7)
		return 230;
	if (Math::Square(int64(9)) != 81)
		return 235;

	FVector PlaneIntersection = Math::LinePlaneIntersection(FVector(0.0f, 0.0f, -5.0f), FVector(0.0f, 0.0f, 5.0f), FPlane(FVector::ZeroVector, FVector::UpVector));
	if (!PlaneIntersection.Equals(FVector::ZeroVector, 0.001f))
		return 240;

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

	TestEqual(TEXT("Extended Math helpers should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptPlatformProcessBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASPlatformProcessCompat",
		TEXT(R"(
int Entry()
{
	if (FPlatformProcess::UserDir().IsEmpty())
		return 10;
	if (FPlatformProcess::UserSettingsDir().IsEmpty())
		return 20;
	if (FPlatformProcess::UserTempDir().IsEmpty())
		return 30;
	if (FPlatformProcess::ApplicationSettingsDir().IsEmpty())
		return 40;
	if (FPlatformProcess::ExecutablePath().IsEmpty())
		return 50;
	if (FPlatformProcess::ExecutableName().IsEmpty())
		return 60;
	if (FPlatformProcess::CurrentWorkingDirectory().IsEmpty())
		return 70;
	if (FPlatformProcess::ComputerName().IsEmpty())
		return 80;
	if (FPlatformProcess::UserName().IsEmpty())
		return 90;
	if (FPlatformProcess::CanLaunchURL("https://example.com") == false)
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

	TestEqual(TEXT("PlatformProcess compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptLoggingBindingsTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Test error message"), EAutomationExpectedErrorFlags::Contains, 1);

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASLoggingCompat",
		TEXT(R"(
int Entry()
{
	Log("Test log message");
	LogDisplay("Test display message");
	Warning("Test warning message");
	Error("Test error message");
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

	TestEqual(TEXT("Headless-safe logging helpers should execute successfully"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

#endif
