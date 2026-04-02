#include "Angelscript/AngelscriptTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace
{
	bool ReadExpectedFloatResult(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, double ExpectedValue)
	{
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!Test.TestNotNull(TEXT("Float helper should expose a script engine"), ScriptEngine))
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Float helper should create an execution context"), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (!Test.TestEqual(TEXT("Float helper should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)) ||
			!Test.TestEqual(TEXT("Float helper should execute the function"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		bool bMatches = false;
		if (bFloatUsesFloat64)
		{
			double ReturnValue = 0.0;
			const asQWORD EncodedReturnValue = Context->GetReturnQWord();
			FMemory::Memcpy(&ReturnValue, &EncodedReturnValue, sizeof(ReturnValue));
			bMatches = FMath::IsNearlyEqual(ReturnValue, ExpectedValue, 0.001);
			Test.TestTrue(TEXT("Float helper should preserve float64-compatible return values"), bMatches);
		}
		else
		{
			const float ReturnValue = Context->GetReturnFloat();
			bMatches = FMath::IsNearlyEqual(ReturnValue, static_cast<float>(ExpectedValue), 0.001f);
			Test.TestTrue(TEXT("Float helper should preserve float return values"), bMatches);
		}

		Context->Release();
		return bMatches;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPrimitiveTypeTest,
	"Angelscript.TestModule.Angelscript.Types.PrimitiveAndEnum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptPrimitiveTypeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypePrimitiveAndEnum",
		TEXT("enum EState { Idle = 2, Running = 4 } int Run() { bool bFlag = true; float Value = 1.5f + 2.5f; return (bFlag ? 1 : 0) + int(Value) + int(EState::Running); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Primitive and enum math should preserve the expected result"), Result, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInt64TypedefTest,
	"Angelscript.TestModule.Angelscript.Types.Int64AndTypedef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInt64TypedefTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeInt64AndTypedef",
		TEXT("int64 Run() { int64 Value = 1; Value <<= 40; Value += 7; return Value; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int64 Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int64 Result = 0;
	if (!ExecuteInt64Function(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("int64 arithmetic should preserve wide integer precision"), Result, static_cast<int64>(1099511627783LL));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeBoolTest,
	"Angelscript.TestModule.Angelscript.Types.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeBoolTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeBool",
		TEXT("int Run() { bool A = true; bool B = false; return (A && !B) ? 1 : 0; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Bool expressions should preserve logical truthiness"), Result, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatTest,
	"Angelscript.TestModule.Angelscript.Types.Float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatDebuggerFormattingTest,
	"Angelscript.TestModule.Angelscript.Types.FloatDebuggerFormatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeFloatTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Float should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("double Run() { double A = 3.14; double B = 2.0; return A * B; }")
		: TEXT("float Run() { float A = 3.14f; float B = 2.0f; return A * B; }");
	const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeFloat", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	return ReadExpectedFloatResult(*this, Engine, *Function, 6.28);
}

bool FAngelscriptTypeFloatDebuggerFormattingTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	static_cast<void>(Engine);

	FAngelscriptTypeUsage FloatUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("float")));
	if (!TestTrue(TEXT("Float debugger formatting test should resolve the float type"), FloatUsage.IsValid()))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = Engine.GetScriptEngine()->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	FDebuggerValue DebugValue;
	if (bFloatUsesFloat64)
	{
		double SmallValue = 0.000000123456;
		if (!FloatUsage.GetDebuggerValue(&SmallValue, DebugValue))
		{
			AddError(TEXT("Float debugger formatting should read a small float64 value"));
			return false;
		}
	}
	else
	{
		float SmallValue = 0.000000123456f;
		if (!FloatUsage.GetDebuggerValue(&SmallValue, DebugValue))
		{
			AddError(TEXT("Float debugger formatting should read a small float value"));
			return false;
		}
	}

	TestTrue(TEXT("Small float debugger values should use scientific notation"), DebugValue.Value.Contains(TEXT("e")) || DebugValue.Value.Contains(TEXT("E")));
	return DebugValue.Value.Contains(TEXT("e")) || DebugValue.Value.Contains(TEXT("E"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeInt8Test,
	"Angelscript.TestModule.Angelscript.Types.Int8",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeInt8Test::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeInt8",
		TEXT("int Run() { int8 A = 100; int8 B = 50; return int(A + B); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Int8 arithmetic should survive promotion back to int"), Result, 150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeBitsTest,
	"Angelscript.TestModule.Angelscript.Types.Bits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeBitsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeBits",
		TEXT("int Run() { int A = 0x0F; int B = 0xF0; return ((A | B) == 0xFF && (A & B) == 0 && (A ^ B) == 0xFF) ? 1 : 0; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Bitwise operations should preserve expected masks"), Result, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeEnumTest,
	"Angelscript.TestModule.Angelscript.Types.Enum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeEnumTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeEnum",
		TEXT("enum Color { Red, Green, Blue } int Run() { Color Value = Color::Green; return int(Value); }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Enums should preserve ordinal values"), Result, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeAutoTest,
	"Angelscript.TestModule.Angelscript.Types.Auto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeAutoTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTypeAuto",
		TEXT("int Run() { auto Value = 42; return Value; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Auto should infer integer literal types"), Result, 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeConversionTest,
	"Angelscript.TestModule.Angelscript.Types.Conversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeConversionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Conversion should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("int Run() { double Value = 3.7; return int(Value); }")
		: TEXT("int Run() { float Value = 3.7f; return int(Value); }");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeConversion", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Explicit numeric conversion should truncate toward zero"), Result, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeImplicitCastTest,
	"Angelscript.TestModule.Angelscript.Types.ImplicitCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeImplicitCastTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetSharedTestEngine();
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.ImplicitCast should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("double Run() { int Value = 42; double Converted = Value; return Converted; }")
		: TEXT("float Run() { int Value = 42; float Converted = Value; return Converted; }");
	const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeImplicitCast", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	return ReadExpectedFloatResult(*this, Engine, *Function, 42.0);
}

#endif
