#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokenizer.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTokenizerAccessor : asCTokenizer
	{
		using asCTokenizer::GetToken;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTokenizerBasicTokenTest,
	"Angelscript.TestModule.Internals.Tokenizer.BasicTokens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTokenizerKeywordTest,
	"Angelscript.TestModule.Internals.Tokenizer.Keywords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTokenizerCommentStringTest,
	"Angelscript.TestModule.Internals.Tokenizer.CommentsAndStrings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTokenizerErrorRecoveryTest,
	"Angelscript.TestModule.Internals.Tokenizer.ErrorRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTokenizerBasicTokenTest::RunTest(const FString& Parameters)
{
	FTokenizerAccessor Tokenizer;
	size_t TokenLength = 0;

	TestEqual(TEXT("Identifier token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("Identifier123", 13, &TokenLength)), static_cast<int32>(ttIdentifier));
	TestEqual(TEXT("Identifier token length should be returned"), static_cast<int32>(TokenLength), 13);

	TestEqual(TEXT("Integer literal token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("12345", 5, &TokenLength)), static_cast<int32>(ttIntConstant));
	TestEqual(TEXT("Integer literal token length should be returned"), static_cast<int32>(TokenLength), 5);

	TestEqual(TEXT("String literal token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("\"abc\"", 5, &TokenLength)), static_cast<int32>(ttStringConstant));
	TestEqual(TEXT("String literal token length should be returned"), static_cast<int32>(TokenLength), 5);

	TestEqual(TEXT("Operator token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("+", 1, &TokenLength)), static_cast<int32>(ttPlus));
	TestEqual(TEXT("Operator token length should be returned"), static_cast<int32>(TokenLength), 1);
	return true;
}

bool FAngelscriptTokenizerKeywordTest::RunTest(const FString& Parameters)
{
	FTokenizerAccessor Tokenizer;
	size_t TokenLength = 0;

	TestEqual(TEXT("class should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("class", 5, &TokenLength)), static_cast<int32>(ttClass));
	TestEqual(TEXT("void should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("void", 4, &TokenLength)), static_cast<int32>(ttVoid));
	TestEqual(TEXT("int should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("int", 3, &TokenLength)), static_cast<int32>(ttInt));
	TestEqual(TEXT("float32 should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("float32", 7, &TokenLength)), static_cast<int32>(ttFloat32));
	return true;
}

bool FAngelscriptTokenizerCommentStringTest::RunTest(const FString& Parameters)
{
	FTokenizerAccessor Tokenizer;
	size_t TokenLength = 0;

	TestEqual(TEXT("Single line comment should be recognized"), static_cast<int32>(Tokenizer.GetToken("// hello\n", 9, &TokenLength)), static_cast<int32>(ttOnelineComment));
	TestEqual(TEXT("Multi line comment should be recognized"), static_cast<int32>(Tokenizer.GetToken("/* hi */", 8, &TokenLength)), static_cast<int32>(ttMultilineComment));
	TestEqual(TEXT("Multiline string should be recognized"), static_cast<int32>(Tokenizer.GetToken("\"first\\nsecond\"", 15, &TokenLength)), static_cast<int32>(ttStringConstant));
	return true;
}

bool FAngelscriptTokenizerErrorRecoveryTest::RunTest(const FString& Parameters)
{
	FTokenizerAccessor Tokenizer;
	size_t TokenLength = 0;

	TestEqual(TEXT("Unterminated string should produce the dedicated token type"), static_cast<int32>(Tokenizer.GetToken("\"unterminated", 13, &TokenLength)), static_cast<int32>(ttNonTerminatedStringConstant));
	TestEqual(TEXT("Unknown characters should produce an unrecognized token"), static_cast<int32>(Tokenizer.GetToken("`", 1, &TokenLength)), static_cast<int32>(ttUnrecognizedToken));
	return true;
}

#endif
