#include "../Shared/AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreValueBindingsTest,
	"Angelscript.TestModule.Bindings.ValueTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFNameArrayCompatBindingsTest,
	"Angelscript.TestModule.Bindings.FNameArrayCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTArrayMutationCompatBindingsTest,
	"Angelscript.TestModule.Bindings.TArrayMutationCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptForeachCompatBindingsTest,
	"Angelscript.TestModule.Bindings.ForeachCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTArrayIteratorCompatBindingsTest,
	"Angelscript.TestModule.Bindings.TArrayIteratorCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreValueBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASBindingValueTypes",
		TEXT(R"(
int Entry()
{
	int32 Count = 5;
	double Precise = 1.5;
	FString Text = "Alpha";
	FName Name(Text);
	FVector Vector = FVector(1.0, 2.0, 3.0) + FVector::OneVector;
	FRotator FacingRotation = FVector::RightVector.Rotation();
	FTransform Transform = FTransform(FRotator::ZeroRotator, FVector(10.0, 0.0, 0.0), FVector::OneVector);
	FVector Transformed = Transform.TransformPosition(FVector::ForwardVector);
	FText Label = FText::FromString("Alpha");

	if (Count != 5 || Precise < 1.49 || Precise > 1.51)
		return 10;
	if (!(Name == FName("Alpha")))
		return 20;
	if (!Vector.Equals(FVector(2.0, 3.0, 4.0)))
		return 30;
	if (!FacingRotation.Equals(FRotator(0.0, 90.0, 0.0), 0.01))
		return 40;
	if (!Transformed.Equals(FVector(11.0, 0.0, 0.0)))
		return 50;
	if (Label.IsEmpty() || !(Label.ToString() == "Alpha"))
		return 60;

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

	TestEqual(TEXT("Bound UE value types should behave as expected in script"), Result, 1);
	return true;
}

bool FAngelscriptFNameArrayCompatBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFNameArrayCompat",
		TEXT(R"(
struct FFNameMemberHolder
{
	UObject NativeSelf;
	FName Value;
}

int Entry()
{
	FName[] AliasValues;
	AliasValues.Add(n"Alpha");
	TArray<FName> ExplicitValues;
	ExplicitValues.Add(n"Beta");
	FName Copy = AliasValues[0];
	FFNameMemberHolder Holder;

	AliasValues.Add(n"Gamma");
	ExplicitValues.Add(Copy);
	Holder.Value = n"Delta";

	if (AliasValues.Num() != 2)
		return 10;
	if (ExplicitValues.Num() != 2)
		return 20;
	if (!(Copy == n"Alpha"))
		return 30;
	if (!(AliasValues[1] == n"Gamma"))
		return 40;
	if (!(ExplicitValues[0] == n"Beta"))
		return 50;
	if (!(ExplicitValues[1] == n"Alpha"))
		return 60;
	if (!(Holder.Value == n"Delta"))
		return 80;

	return AliasValues.Contains(n"Alpha") && ExplicitValues.Contains(n"Alpha") ? 1 : 70;
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

	TestEqual(TEXT("FName arrays should support copy, index, alias, and add operations"), Result, 1);
	return true;
}

bool FAngelscriptTArrayMutationCompatBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTArrayMutationCompat",
		TEXT(R"(
int Entry()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(2);
	Values.Add(3);
	if (Values.FindIndex(2) != 1)
		return 10;
	if (Values.AddUnique(2))
		return 20;
	if (!Values.AddUnique(4))
		return 30;

	Values.Insert(9, 1);
	if (!(Values.Num() == 6 && Values[1] == 9 && Values.Last() == 4))
		return 40;

	if (Values.RemoveSingle(2) != 1)
		return 50;
	if (Values.Remove(2) != 1)
		return 60;
	if (Values.FindIndex(2) != -1)
		return 70;

	Values.Reset(4);
	if (!Values.IsEmpty() || Values.Max() < 4)
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

	TestEqual(TEXT("TArray mutation helpers should match expected script behaviour"), Result, 1);
	return true;
}

bool FAngelscriptForeachCompatBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASForeachCompat",
		TEXT(R"(
int Entry()
{
	int[] AliasValues;
	AliasValues.Add(1);
	AliasValues.Add(2);
	AliasValues.Add(3);
	TArray<FName> ExplicitNames;
	ExplicitNames.Add(n"Alpha");
	ExplicitNames.Add(n"Beta");
	TArray<FVector> ExplicitVectors;
	ExplicitVectors.Add(FVector(1.0, 2.0, 3.0));
	ExplicitVectors.Add(FVector(4.0, 5.0, 6.0));
	int SumForeach = 0;
	int SumCompat = 0;
	int NameCount = 0;
	double VectorXSum = 0.0;

	for (int Value : AliasValues)
	{
		SumForeach += Value;
	}

	for (int Value : AliasValues)
	{
		SumCompat += Value;
	}

	for (FName Name : ExplicitNames)
	{
		if (Name == n"Alpha" || Name == n"Beta")
			NameCount += 1;
	}

	for (const FVector& VectorValue : ExplicitVectors)
	{
		VectorXSum += VectorValue.X;
	}

	AliasValues[1] = 5;

	int MutatedTotal = 0;
	for (int Value : AliasValues)
	{
		MutatedTotal += Value;
	}

	if (SumForeach != 6)
		return 10;
	if (SumCompat != 6)
		return 20;
	if (NameCount != 2)
		return 30;
	if (VectorXSum < 4.99 || VectorXSum > 5.01)
		return 35;

	return MutatedTotal == 9 ? 1 : 40;
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

	TestEqual(TEXT("TArray should support foreach and range-for compatibility syntax"), Result, 1);
	return true;
}

bool FAngelscriptTArrayIteratorCompatBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASTArrayIteratorCompat",
		TEXT(R"(
int Entry()
{
	int[] MutableValues;
	MutableValues.Add(1);
	MutableValues.Add(2);
	MutableValues.Add(3);
	TArrayIterator<int> MutableIt = MutableValues.Iterator();

	if (!MutableIt.CanProceed || MutableIt.Proceed() != 1)
		return 10;

	MutableIt.Proceed() = 20;
	if (MutableValues[1] != 20)
		return 20;

	int TailSum = 0;
	while (MutableIt.CanProceed)
	{
		TailSum += MutableIt.Proceed();
	}

	if (TailSum != 3 || MutableIt.CanProceed)
		return 30;

	int[] ConstSource;
	ConstSource.Add(4);
	ConstSource.Add(5);
	ConstSource.Add(6);
	const int[] ConstValues = ConstSource;
	TArrayConstIterator<int> ConstIt = ConstValues.Iterator();
	int ConstSum = 0;
	while (ConstIt.CanProceed)
	{
		ConstSum += ConstIt.Proceed();
	}

	if (ConstSum != 15)
		return 40;

	TArrayConstIterator<int> AliasIt = ConstIt;
	TArrayIterator<int> MutableAliasIt = MutableValues.Iterator();

	int AliasSum = 0;
	while (MutableAliasIt.CanProceed)
	{
		AliasSum += MutableAliasIt.Proceed();
	}

	return AliasSum == 24 ? 1 : 50;
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

	TestEqual(TEXT("TArray iterator helpers should match expected script behaviour"), Result, 1);
	return true;
}

#endif
