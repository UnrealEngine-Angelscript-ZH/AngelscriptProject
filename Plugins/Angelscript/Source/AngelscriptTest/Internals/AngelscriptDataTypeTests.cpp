#include "Angelscript/AngelscriptTestSupport.h"
#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDataTypePrimitiveTest,
	"Angelscript.TestModule.Internals.DataType.Primitives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDataTypeComparisonTest,
	"Angelscript.TestModule.Internals.DataType.Comparisons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDataTypeObjectHandleTest,
	"Angelscript.TestModule.Internals.DataType.ObjectHandles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDataTypeSizeAlignmentTest,
	"Angelscript.TestModule.Internals.DataType.SizeAndAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptDataTypePrimitiveTest::RunTest(const FString& Parameters)
{
	asCDataType IntType = asCDataType::CreatePrimitive(ttInt, false);
	asCDataType FloatType = asCDataType::CreatePrimitive(ttFloat32, false);
	asCDataType BoolType = asCDataType::CreatePrimitive(ttBool, false);
	asCDataType VoidType = asCDataType::CreatePrimitive(ttVoid, false);
	asCDataType NullHandleType = asCDataType::CreateNullHandle();

	TestTrue(TEXT("int data type should be valid and primitive"), IntType.IsValid() && IntType.IsPrimitive() && IntType.IsIntegerType());
	TestTrue(TEXT("float32 data type should report float semantics"), FloatType.IsPrimitive() && FloatType.IsFloat32Type() && FloatType.IsMathType());
	TestTrue(TEXT("bool data type should report boolean semantics"), BoolType.IsPrimitive() && BoolType.IsBooleanType());
	TestFalse(TEXT("void data type should not be instantiable"), VoidType.CanBeInstantiated());
	TestTrue(TEXT("null handle data type should report object-handle semantics"), NullHandleType.IsNullHandle() && NullHandleType.IsObjectHandle());
	return true;
}

bool FAngelscriptDataTypeComparisonTest::RunTest(const FString& Parameters)
{
	asCDataType MutableInt = asCDataType::CreatePrimitive(ttInt, false);
	asCDataType ConstInt = asCDataType::CreatePrimitive(ttInt, true);
	asCDataType RefInt = asCDataType::CreatePrimitive(ttInt, false);
	RefInt.MakeReference(true);

	TestTrue(TEXT("Constness should be ignored by IsEqualExceptConst"), MutableInt.IsEqualExceptConst(ConstInt));
	TestFalse(TEXT("Constness should still matter for exact equality"), MutableInt == ConstInt);
	TestTrue(TEXT("Reference-ness should be ignored by IsEqualExceptRef"), MutableInt.IsEqualExceptRef(RefInt));
	TestFalse(TEXT("Reference-ness should still matter for exact equality"), MutableInt == RefInt);
	return true;
}

bool FAngelscriptDataTypeObjectHandleTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	asCTypeInfo* ActorType = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("AActor"));
	if (!TestNotNull(TEXT("AActor should exist in the script type system for data-type handle tests"), ActorType))
	{
		return false;
	}

	asCDataType ActorValueType = asCDataType::CreateType(ActorType, false);
	TestTrue(TEXT("AActor value type should be recognized as an object type"), ActorValueType.IsObject());
	TestTrue(TEXT("AActor value type should support handles"), ActorValueType.SupportHandles());

	asCDataType ActorHandleType = asCDataType::CreateObjectHandle(ActorType, false);
	TestTrue(TEXT("CreateObjectHandle should mark the type as an object handle"), ActorHandleType.IsObjectHandle());
	TestTrue(TEXT("Object handle should still be considered instantiable as a handle slot"), ActorHandleType.CanBeInstantiated());
	return true;
}

bool FAngelscriptDataTypeSizeAlignmentTest::RunTest(const FString& Parameters)
{
	asCDataType IntType = asCDataType::CreatePrimitive(ttInt, false);
	asCDataType Float64Type = asCDataType::CreatePrimitive(ttFloat64, false);
	asCDataType BoolType = asCDataType::CreatePrimitive(ttBool, false);

	TestEqual(TEXT("int should occupy one dword in memory"), IntType.GetSizeInMemoryDWords(), 1);
	TestEqual(TEXT("float64 should occupy eight bytes in memory"), Float64Type.GetSizeInMemoryBytes(), 8);
	TestEqual(TEXT("bool alignment should stay byte-sized"), BoolType.GetAlignment(), 1);
	return true;
}

#endif
