#include "../Shared/AngelscriptTestUtilities.h"
#include "../Shared/AngelscriptTestMacros.h"

#include "GameplayTagsManager.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameplayTagBindingsTest,
	"Angelscript.TestModule.Bindings.GameplayTagCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameplayTagContainerBindingsTest,
	"Angelscript.TestModule.Bindings.GameplayTagContainerCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameplayTagQueryBindingsTest,
	"Angelscript.TestModule.Bindings.GameplayTagQueryCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGameplayTagBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE

	FGameplayTagContainer AllTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
	if (!TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
	{
		return false;
	}

	FString GameplayTagIdentifier = AllTags.First().ToString();
	for (TCHAR& Character : GameplayTagIdentifier)
	{
		if (!(FChar::IsAlnum(Character) || Character == TEXT('_')))
		{
			Character = TEXT('_');
		}
	}

	const FString EscapedTagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
	const FString Script = FString::Printf(TEXT(R"(
int Entry()
{
	FGameplayTag GlobalTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	if (!GlobalTag.IsValid())
		return 5;

	FGameplayTag EmptyDefault;
	if (EmptyDefault.IsValid())
		return 10;
	if (!(EmptyDefault == FGameplayTag::EmptyTag))
		return 20;
	if (!EmptyDefault.GetTagName().IsNone())
		return 30;
	if (!(EmptyDefault.ToString() == FGameplayTag::EmptyTag.ToString()))
		return 40;

	FGameplayTag RequestedInvalid = FGameplayTag::RequestGameplayTag(NAME_None, false);
	if (RequestedInvalid.IsValid())
		return 50;
	if (!(RequestedInvalid == FGameplayTag::EmptyTag))
		return 60;

	return 1;
}
)"), *EscapedTagName);

	asIScriptModule* Module = BuildModule(*this, Engine, "ASGameplayTagCompat", Script);
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

	TestEqual(TEXT("GameplayTag compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptGameplayTagContainerBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE

	FGameplayTagContainer AllTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
	if (!TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
	{
		return false;
	}

	const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
	const FString Script = FString::Printf(TEXT(R"(
int Entry()
{
	FGameplayTagContainer EmptyDefault;
	if (!EmptyDefault.IsEmpty())
		return 10;
	if (!(EmptyDefault == FGameplayTagContainer::EmptyContainer))
		return 20;

	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	if (!ValidTag.IsValid())
		return 25;

	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	if (!Tags.IsValid())
		return 30;
	if (Tags.Num() != 1)
		return 40;
	if (!Tags.HasTag(ValidTag))
		return 50;
	if (!Tags.HasTagExact(ValidTag))
		return 60;
	if (!(Tags.First() == ValidTag))
		return 70;

	FGameplayTagContainer Others;
	Others.AddTag(ValidTag);
	if (!Tags.HasAny(Others))
		return 80;
	if (!Tags.HasAnyExact(Others))
		return 85;
	if (!Tags.HasAll(Others))
		return 90;
	if (!Tags.HasAllExact(Others))
		return 95;

	FGameplayTagContainer Combined;
	Combined.AppendTags(Tags);
	if (!Combined.HasTag(ValidTag))
		return 100;

	if (!Combined.RemoveTag(ValidTag))
		return 110;
	if (!Combined.IsEmpty())
		return 120;

	Combined.AppendTags(Tags);
	Combined.Reset();
	if (!Combined.IsEmpty())
		return 130;

	return 1;
}
)"), *TagName);

	asIScriptModule* Module = BuildModule(*this, Engine, "ASGameplayTagContainerCompat", Script);
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

	TestEqual(TEXT("GameplayTagContainer compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

bool FAngelscriptGameplayTagQueryBindingsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_SHARE();
	ASTEST_BEGIN_SHARE

	FGameplayTagContainer AllTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
	if (!TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
	{
		return false;
	}

	const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
	const FString Script = FString::Printf(TEXT(R"(
int Entry()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	if (!ValidTag.IsValid())
		return 10;

	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);

	FGameplayTagQuery EmptyDefault;
	if (!EmptyDefault.IsEmpty())
		return 20;
	if (!(EmptyDefault == FGameplayTagQuery::EmptyQuery))
		return 30;

	FGameplayTagQuery MatchAny = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	if (MatchAny.IsEmpty())
		return 40;

	FGameplayTagQuery MatchAll = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
	if (MatchAll.IsEmpty())
		return 50;

	FGameplayTagQuery MatchNone = FGameplayTagQuery::MakeQuery_MatchNoTags(Tags);
	if (MatchNone.IsEmpty())
		return 60;

	FGameplayTagQuery MatchTag = FGameplayTagQuery::MakeQuery_MatchTag(ValidTag);
	if (MatchTag.IsEmpty())
		return 70;

	if (MatchAny == MatchAll)
		return 80;

	FGameplayTagQuery Copy = MatchAny;
	if (!(Copy == MatchAny))
		return 90;

	FGameplayTagQuery MatchAnyExact = FGameplayTagQuery::MakeQuery_ExactMatchAnyTags(Tags);
	if (MatchAnyExact.IsEmpty())
		return 100;

	FGameplayTagQuery MatchAllExact = FGameplayTagQuery::MakeQuery_ExactMatchAllTags(Tags);
	if (MatchAllExact.IsEmpty())
		return 110;

	if (!Tags.MatchesQuery(MatchAny))
		return 120;
	if (!Tags.MatchesQuery(MatchAll))
		return 130;
	if (!Tags.MatchesQuery(MatchAnyExact))
		return 140;
	if (!Tags.MatchesQuery(MatchAllExact))
		return 150;
	if (Tags.MatchesQuery(MatchNone))
		return 160;

	return 1;
}
)"), *TagName);

	asIScriptModule* Module = BuildModule(*this, Engine, "ASGameplayTagQueryCompat", Script);
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

	TestEqual(TEXT("GameplayTagQuery compat operations should behave as expected"), Result, 1);
	ASTEST_END_SHARE

	return true;
}

#endif
