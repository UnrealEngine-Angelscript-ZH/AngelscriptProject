#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptBindConfigTestAccess
{
	static void CallBinds(const TSet<FName>& DisabledBindNames)
	{
		FAngelscriptBinds::CallBinds(DisabledBindNames);
	}

	static TSet<FName> CollectDisabledBindNames(const FAngelscriptEngine& Engine)
	{
		return Engine.CollectDisabledBindNames();
	}
};

namespace
{
	struct FBindExecutionRecorder
	{
		static TMap<FName, int32>& GetCounts()
		{
			static TMap<FName, int32> Counts;
			return Counts;
		}

		static void Reset(const FName CounterKey)
		{
			GetCounts().FindOrAdd(CounterKey) = 0;
		}

		static void Increment(const FName CounterKey)
		{
			++GetCounts().FindOrAdd(CounterKey);
		}

		static int32 Get(const FName CounterKey)
		{
			return GetCounts().FindRef(CounterKey);
		}
	};

	FName MakeUniqueBindTestName(const TCHAR* Prefix)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	TArray<FName> FindNewBindNames(const TArray<FName>& BeforeNames, const TArray<FName>& AfterNames)
	{
		TSet<FName> ExistingNames;
		for (const FName& BeforeName : BeforeNames)
		{
			ExistingNames.Add(BeforeName);
		}

		TArray<FName> NewNames;
		for (const FName& AfterName : AfterNames)
		{
			if (!ExistingNames.Contains(AfterName))
			{
				NewNames.Add(AfterName);
			}
		}

		return NewNames;
	}

	TSet<FName> BuildDisabledSetExcluding(const TArray<FName>& AllBindNames, const TSet<FName>& AllowedNames)
	{
		TSet<FName> DisabledBindNames;
		for (const FName& BindName : AllBindNames)
		{
			if (!AllowedNames.Contains(BindName))
			{
				DisabledBindNames.Add(BindName);
			}
		}

		return DisabledBindNames;
	}

	void ExecuteIsolatedBinds(const TSet<FName>& DisabledBindNames)
	{
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		FAngelscriptBindConfigTestAccess::CallBinds(DisabledBindNames);
		UE_SET_LOG_VERBOSITY(Angelscript, Log);
	}

	const FAngelscriptBinds::FBindInfo* FindBindInfoByName(const TArray<FAngelscriptBinds::FBindInfo>& BindInfos, const FName BindName)
	{
		for (const FAngelscriptBinds::FBindInfo& BindInfo : BindInfos)
		{
			if (BindInfo.BindName == BindName)
			{
				return &BindInfo;
			}
		}

		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGlobalDisabledBindNamesTest,
	"Angelscript.TestModule.Engine.BindConfig.GlobalDisabledBindNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEngineDisabledBindNamesTest,
	"Angelscript.TestModule.Engine.BindConfig.EngineDisabledBindNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUnnamedBindBackwardCompatibilityTest,
	"Angelscript.TestModule.Engine.BindConfig.UnnamedBindBackwardCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGlobalDisabledBindNamesTest::RunTest(const FString& Parameters)
{
	UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
	if (!TestNotNull(TEXT("BindConfig.GlobalDisabledBindNames should access mutable settings"), Settings))
	{
		return false;
	}

	const TArray<FName> PreviousDisabledBindNames = Settings->DisabledBindNames;
	ON_SCOPE_EXIT
	{
		Settings->DisabledBindNames = PreviousDisabledBindNames;
	};

	const FName NamedBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Global"));
	const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Global.Counter"));
	FBindExecutionRecorder::Reset(CounterKey);

	FAngelscriptBinds::FBind NamedBind(NamedBindName, [CounterKey]()
	{
		FBindExecutionRecorder::Increment(CounterKey);
	});

	const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
	TestTrue(TEXT("BindConfig.GlobalDisabledBindNames should expose newly registered named binds"), AllBindNames.Contains(NamedBindName));

	TSet<FName> AllowedBindNames;
	AllowedBindNames.Add(NamedBindName);

	ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
	TestEqual(TEXT("BindConfig.GlobalDisabledBindNames should execute the named bind when it is enabled"), FBindExecutionRecorder::Get(CounterKey), 1);

	FBindExecutionRecorder::Reset(CounterKey);
	Settings->DisabledBindNames = { NamedBindName };

	FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	const FAngelscriptEngine Engine(Config, Dependencies);
	const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
	TestTrue(TEXT("BindConfig.GlobalDisabledBindNames should merge the settings-level disabled bind name"), MergedDisabledBindNames.Contains(NamedBindName));

	TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
	DisabledBindNames.Append(MergedDisabledBindNames);
	ExecuteIsolatedBinds(DisabledBindNames);

	TestEqual(TEXT("BindConfig.GlobalDisabledBindNames should skip execution when disabled in settings"), FBindExecutionRecorder::Get(CounterKey), 0);

	const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList(MergedDisabledBindNames);
	const FAngelscriptBinds::FBindInfo* NamedBindInfo = FindBindInfoByName(BindInfos, NamedBindName);
	if (!TestNotNull(TEXT("BindConfig.GlobalDisabledBindNames should expose bind info for the named bind"), NamedBindInfo))
	{
		return false;
	}

	TestFalse(TEXT("BindConfig.GlobalDisabledBindNames should report the disabled named bind as disabled"), NamedBindInfo->bEnabled);
	return true;
}

bool FAngelscriptEngineDisabledBindNamesTest::RunTest(const FString& Parameters)
{
	UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
	if (!TestNotNull(TEXT("BindConfig.EngineDisabledBindNames should access mutable settings"), Settings))
	{
		return false;
	}

	const TArray<FName> PreviousDisabledBindNames = Settings->DisabledBindNames;
	ON_SCOPE_EXIT
	{
		Settings->DisabledBindNames = PreviousDisabledBindNames;
	};
	Settings->DisabledBindNames.Reset();

	const FName NamedBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Engine"));
	const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Engine.Counter"));
	FBindExecutionRecorder::Reset(CounterKey);

	FAngelscriptBinds::FBind NamedBind(NamedBindName, [CounterKey]()
	{
		FBindExecutionRecorder::Increment(CounterKey);
	});

	const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
	TestTrue(TEXT("BindConfig.EngineDisabledBindNames should expose the named bind through the query API"), AllBindNames.Contains(NamedBindName));

	TSet<FName> AllowedBindNames;
	AllowedBindNames.Add(NamedBindName);

	ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
	TestEqual(TEXT("BindConfig.EngineDisabledBindNames should execute the named bind before engine-level filtering is applied"), FBindExecutionRecorder::Get(CounterKey), 1);

	FBindExecutionRecorder::Reset(CounterKey);

	FAngelscriptEngineConfig Config;
	Config.DisabledBindNames.Add(NamedBindName);
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	const FAngelscriptEngine Engine(Config, Dependencies);
	const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
	TestTrue(TEXT("BindConfig.EngineDisabledBindNames should include the engine-level disabled bind name"), MergedDisabledBindNames.Contains(NamedBindName));

	TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
	DisabledBindNames.Append(MergedDisabledBindNames);
	ExecuteIsolatedBinds(DisabledBindNames);

	TestEqual(TEXT("BindConfig.EngineDisabledBindNames should skip execution when disabled in the engine config"), FBindExecutionRecorder::Get(CounterKey), 0);
	return true;
}

bool FAngelscriptUnnamedBindBackwardCompatibilityTest::RunTest(const FString& Parameters)
{
	const TArray<FName> BaselineBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
	const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Unnamed.Counter"));
	FBindExecutionRecorder::Reset(CounterKey);

	FAngelscriptBinds::FBind UnnamedBind([CounterKey]()
	{
		FBindExecutionRecorder::Increment(CounterKey);
	});

	const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
	const TArray<FName> NewBindNames = FindNewBindNames(BaselineBindNames, AllBindNames);

	FName GeneratedUnnamedBindName = NAME_None;
	for (const FName& NewBindName : NewBindNames)
	{
		if (NewBindName.ToString().StartsWith(TEXT("UnnamedBind_")))
		{
			GeneratedUnnamedBindName = NewBindName;
			break;
		}
	}

	if (!TestFalse(TEXT("BindConfig.UnnamedBindBackwardCompatibility should register at least one new bind name"), NewBindNames.IsEmpty()))
	{
		return false;
	}

	if (!TestTrue(TEXT("BindConfig.UnnamedBindBackwardCompatibility should auto-generate an unnamed bind name"), GeneratedUnnamedBindName != NAME_None))
	{
		return false;
	}

	const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
	const FAngelscriptBinds::FBindInfo* UnnamedBindInfo = FindBindInfoByName(BindInfos, GeneratedUnnamedBindName);
	if (!TestNotNull(TEXT("BindConfig.UnnamedBindBackwardCompatibility should expose bind info for the unnamed bind"), UnnamedBindInfo))
	{
		return false;
	}

	TestEqual(TEXT("BindConfig.UnnamedBindBackwardCompatibility should default unnamed bind order to zero"), UnnamedBindInfo->BindOrder, 0);
	TestTrue(TEXT("BindConfig.UnnamedBindBackwardCompatibility should report unnamed binds as enabled by default"), UnnamedBindInfo->bEnabled);

	TSet<FName> AllowedBindNames;
	AllowedBindNames.Add(GeneratedUnnamedBindName);
	ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));

	TestEqual(TEXT("BindConfig.UnnamedBindBackwardCompatibility should continue executing unnamed binds"), FBindExecutionRecorder::Get(CounterKey), 1);
	return true;
}

#endif
