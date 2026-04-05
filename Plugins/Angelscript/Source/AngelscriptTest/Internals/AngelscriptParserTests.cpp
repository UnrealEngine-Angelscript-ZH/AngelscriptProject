#include "Angelscript/AngelscriptTestSupport.h"
#include "../Shared/AngelscriptTestMacros.h"
#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

	namespace
	{
		struct FParserAccessor : asCParser
		{
			explicit FParserAccessor(asCBuilder* Builder)
				: asCParser(Builder)
			{
			}

			asCScriptNode* ParseExpressionSnippet(asCScriptCode* InScript)
			{
				Reset();
				script = InScript;
				return ParseExpression();
			}

			asCScriptNode* ParseStatementSnippet(asCScriptCode* InScript)
			{
				Reset();
				script = InScript;
				return ParseStatement();
			}
		};

		bool ContainsNodeType(const asCScriptNode* Node, eScriptNode ExpectedType)
		{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == ExpectedType)
			{
				return true;
			}

			if (Current->firstChild != nullptr && ContainsNodeType(Current->firstChild, ExpectedType))
			{
				return true;
			}
		}

		return false;
	}

	asCModule* CreateParserModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptParserDeclarationTest,
	"Angelscript.TestModule.Internals.Parser.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptParserExpressionAstTest,
	"Angelscript.TestModule.Internals.Parser.ExpressionAst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptParserControlFlowTest,
	"Angelscript.TestModule.Internals.Parser.ControlFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptParserSyntaxErrorTest,
	"Angelscript.TestModule.Internals.Parser.SyntaxErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptParserDeclarationTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	asCModule* Module = CreateParserModule(ScriptEngine, "ParserDeclarations");
	if (!TestNotNull(TEXT("Parser declaration test should create a backing module"), Module))
	{
		return false;
	}

	asCBuilder Builder(ScriptEngine, Module);
	asCScriptCode Code;
	Code.SetCode("ParserDeclarations", "int GlobalValue = 7; class FSample { int Value; }", true);

	asCParser Parser(&Builder);
	const int ParseResult = Parser.ParseScript(&Code);
	if (!TestEqual(TEXT("Parser should successfully parse declarations"), ParseResult, 0))
	{
		return false;
	}

	asCScriptNode* Root = Parser.GetScriptNode();
	if (!TestNotNull(TEXT("Parser should produce a root script node"), Root))
	{
		return false;
	}

	TestEqual(TEXT("Root node should be a script node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snScript));
	TestTrue(TEXT("Parser should emit a declaration node for the global variable"), ContainsNodeType(Root, snDeclaration));
	TestTrue(TEXT("Parser should emit a class node for the class declaration"), ContainsNodeType(Root, snClass));
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptParserExpressionAstTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	asCModule* Module = CreateParserModule(ScriptEngine, "ParserExpressions");
	if (!TestNotNull(TEXT("Parser expression test should create a backing module"), Module))
	{
		return false;
	}

	asCBuilder Builder(ScriptEngine, Module);
	asCScriptCode Code;
	Code.SetCode("ParserExpressions", "1 + 2 * 3", true);

	FParserAccessor Parser(&Builder);
	asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
	if (!TestNotNull(TEXT("Parser should produce an AST for expression input"), Root))
	{
		return false;
	}

	TestEqual(TEXT("Expression root should be an expression node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snExpression));
	TestTrue(TEXT("Parser should emit an expression operator node"), ContainsNodeType(Root, snExprOperator));
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptParserControlFlowTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	asCModule* Module = CreateParserModule(ScriptEngine, "ParserControlFlow");
	if (!TestNotNull(TEXT("Parser control-flow test should create a backing module"), Module))
	{
		return false;
	}

	asCBuilder Builder(ScriptEngine, Module);
	asCScriptCode Code;
	Code.SetCode("ParserControlFlow", "if (true) { for (int i = 0; i < 3; i++) { while(false) { } } }", true);

	FParserAccessor Parser(&Builder);
	asCScriptNode* Root = Parser.ParseStatementSnippet(&Code);
	if (!TestNotNull(TEXT("Parser should produce an AST for control flow input"), Root))
	{
		return false;
	}

	TestEqual(TEXT("Control-flow root should be an if node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snIf));
	TestTrue(TEXT("Parser should emit an if node"), ContainsNodeType(Root, snIf));
	TestTrue(TEXT("Parser should emit a for node"), ContainsNodeType(Root, snFor));
	TestTrue(TEXT("Parser should emit a while node"), ContainsNodeType(Root, snWhile));
	ASTEST_END_SHARE_CLEAN

	return true;
}

bool FAngelscriptParserSyntaxErrorTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE_CLEAN();
	ASTEST_BEGIN_SHARE_CLEAN
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	asCModule* Module = CreateParserModule(ScriptEngine, "ParserSyntaxErrors");
	if (!TestNotNull(TEXT("Parser syntax-error test should create a backing module"), Module))
	{
		return false;
	}

	asCBuilder Builder(ScriptEngine, Module);
	Builder.silent = true;
	asCScriptCode Code;
	Code.SetCode("ParserSyntaxErrors", "void Broken( { return; }", true);

	asCParser Parser(&Builder);
	const int ParseResult = Parser.ParseScript(&Code);
	TestTrue(TEXT("Parser should reject malformed syntax"), ParseResult < 0);
	bPassed = ParseResult < 0;
	ASTEST_END_SHARE_CLEAN

	return bPassed;
}

#endif
