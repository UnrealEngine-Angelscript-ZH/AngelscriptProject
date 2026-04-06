#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOptionalBindingsTest,
	"Angelscript.TestModule.Bindings.OptionalCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSetBindingsTest,
	"Angelscript.TestModule.Bindings.SetCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMapBindingsTest,
	"Angelscript.TestModule.Bindings.MapCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptArrayForeachBindingsTest,
	"Angelscript.TestModule.Bindings.ArrayForeach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSetForeachBindingsTest,
	"Angelscript.TestModule.Bindings.SetForeach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMapForeachBindingsTest,
	"Angelscript.TestModule.Bindings.MapForeach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptOptionalBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASOptionalCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASOptionalCompat",
		TEXT(R"(
int Entry()
{
	TOptional<int> Empty;
	if (Empty.IsSet())
		return 10;
	if (Empty.Get(7) != 7)
		return 20;

	Empty.Set(42);
	if (!Empty.IsSet())
		return 30;
	if (Empty.GetValue() != 42)
		return 40;

	TOptional<int> Copy(Empty);
	if (!(Copy == Empty))
		return 50;

	Copy = 19;
	if (Copy.GetValue() != 19)
		return 60;

	Copy.Reset();
	if (Copy.IsSet())
		return 70;

	TOptional<FName> OptionalName(FName("Alpha"));
	if (!OptionalName.IsSet())
		return 80;
	if (!(OptionalName.GetValue() == FName("Alpha")))
		return 90;
	if (!(OptionalName.Get(FName("Fallback")) == FName("Alpha")))
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

	TestEqual(TEXT("Optional compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptSetBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASSetCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSetCompat",
		TEXT(R"(
int Entry()
{
	TSet<int> Empty;
	if (!Empty.IsEmpty())
		return 10;

	Empty.Add(4);
	Empty.Add(4);
	if (Empty.Num() != 1)
		return 20;
	if (!Empty.Contains(4))
		return 30;

	TSet<int> Copy = Empty;
	if (!Copy.Contains(4) || Copy.Num() != Empty.Num())
		return 40;

	Copy.Add(7);
	if (Copy.Num() != 2)
		return 50;
	if (!Copy.Remove(4))
		return 60;
	if (Copy.Contains(4))
		return 70;

	Copy.Reset();
	if (!Copy.IsEmpty())
		return 80;

	TSet<FName> Names;
	Names.Add(FName("Alpha"));
	if (!Names.Contains(FName("Alpha")))
		return 90;

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

	TestEqual(TEXT("Set compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptMapBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASMapCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMapCompat",
		TEXT(R"(
int Entry()
{
	TMap<FName, int> Empty;
	if (!Empty.IsEmpty())
		return 10;

	Empty.Add(FName("Alpha"), 4);
	Empty.Add(FName("Alpha"), 7);
	if (Empty.Num() != 1)
		return 20;
	if (!Empty.Contains(FName("Alpha")))
		return 30;

	int Value = 0;
	if (!Empty.Find(FName("Alpha"), Value))
		return 40;
	if (Value != 7)
		return 50;

	int FoundOrAdded = Empty.FindOrAdd(FName("Alpha"));
	if (FoundOrAdded != 7)
		return 55;

	int AddedDefaulted = Empty.FindOrAdd(FName("Beta"), 11);
	if (AddedDefaulted != 11)
		return 56;
	if (Empty.Num() != 2)
		return 57;

	if (!Empty.Contains(FName("Alpha")) || !Empty.Contains(FName("Beta")) || Empty.Num() != 2)
		return 58;

	TMap<FName, int> Copy = Empty;
	if (Copy.Num() != 2)
		return 60;
	if (!Copy.Contains(FName("Alpha")) || !Copy.Contains(FName("Beta")))
		return 61;

	if (!Copy.Remove(FName("Alpha")))
		return 70;
	if (Copy.Contains(FName("Alpha")))
		return 80;

	Copy.Reset();
	if (!Copy.IsEmpty())
		return 90;

	TMap<FName, FName> Names;
	Names.Add(FName("A"), FName("Alpha"));
	FName NameValue;
	if (!Names.Find(FName("A"), NameValue))
		return 100;
	if (!(NameValue == FName("Alpha")))
		return 110;

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

	TestEqual(TEXT("Map compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptArrayForeachBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASArrayForeachCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASArrayForeachCompat",
		TEXT(R"(
int Entry()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(5);

	int Sum = 0;
	int IndexSum = 0;
	foreach (int Value, int Index : Values)
	{
		Sum += Value;
		IndexSum += Index;
	}

	return (Sum == 8 && IndexSum == 3) ? 1 : 10;
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

	TestEqual(TEXT("TArray should support foreach syntax with value and index"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptSetForeachBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASSetForeachCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASSetForeachCompat",
		TEXT(R"(
int Entry()
{
	TSet<int> Values;
	Values.Add(2);
	Values.Add(5);

	int Sum = 0;
	foreach (int Value : Values)
	{
		Sum += Value;
	}

	return Sum == 7 ? 1 : 10;
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

	TestEqual(TEXT("TSet should support foreach syntax with values"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptMapForeachBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASMapForeachCompat"));
	};

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASMapForeachCompat",
		TEXT(R"(
int Entry()
{
	TMap<FName, int> Values;
	Values.Add(FName("Alpha"), 2);
	Values.Add(FName("Beta"), 5);

	int Sum = 0;
	int KeyCount = 0;
	foreach (int Value, FName Key : Values)
	{
		Sum += Value;
		if (Key == FName("Alpha") || Key == FName("Beta"))
			KeyCount += 1;
	}

	return (Sum == 7 && KeyCount == 2) ? 1 : 10;
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

	TestEqual(TEXT("TMap should support foreach syntax with value and key"), Result, 1);
	ASTEST_END_SHARE_CLEAN

	return true;
}

#endif
