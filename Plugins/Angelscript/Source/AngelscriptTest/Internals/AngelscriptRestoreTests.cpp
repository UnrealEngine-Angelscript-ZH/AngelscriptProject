#include "../Angelscript/AngelscriptTestSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_restore.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FMemoryBinaryStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			const int32 StartIndex = Bytes.Num();
			Bytes.AddUninitialized(static_cast<int32>(Size));
			FMemory::Memcpy(Bytes.GetData() + StartIndex, Ptr, static_cast<SIZE_T>(Size));
			return asSUCCESS;
		}

		int Read(void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			const int32 RemainingBytes = Bytes.Num() - ReadOffset;
			if (RemainingBytes < static_cast<int32>(Size))
			{
				return asERROR;
			}

			FMemory::Memcpy(Ptr, Bytes.GetData() + ReadOffset, static_cast<SIZE_T>(Size));
			ReadOffset += static_cast<int32>(Size);
			return asSUCCESS;
		}

		void ResetReadOffset()
		{
			ReadOffset = 0;
		}

		void Truncate(int32 NewSize)
		{
			Bytes.SetNum(FMath::Max(NewSize, 0), EAllowShrinking::No);
			ReadOffset = FMath::Min(ReadOffset, Bytes.Num());
		}

		int32 Num() const
		{
			return Bytes.Num();
		}

	private:
		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	asCModule* CreateRestoreModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	asCModule* BuildRestoreModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName)
	{
		asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
			Test,
			Engine,
			ModuleName,
			TEXT("const int GlobalValue = 41; int Test() { return GlobalValue + 1; }"));
		return static_cast<asCModule*>(Module);
	}

	bool ExecuteRestoreFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCModule& Module, int32& OutValue)
	{
		asIScriptFunction* Function = AngelscriptTestSupport::GetFunctionByDecl(Test, Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return false;
		}

		return AngelscriptTestSupport::ExecuteIntFunction(Test, Engine, *Function, OutValue);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRestoreRoundTripTest,
	"Angelscript.TestModule.Internals.Restore.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRestoreStripDebugInfoRoundTripTest,
	"Angelscript.TestModule.Internals.Restore.StripDebugInfoRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRestoreEmptyStreamFailsTest,
	"Angelscript.TestModule.Internals.Restore.EmptyStreamFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRestoreTruncatedStreamFailsTest,
	"Angelscript.TestModule.Internals.Restore.TruncatedStreamFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptRestoreRoundTripTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> SourceEngineOwner = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Restore roundtrip should create an isolated clone test engine"), SourceEngineOwner.Get()))
	{
		return false;
	}

	FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
	ON_SCOPE_EXIT
	{
		SourceEngine.DiscardModule(TEXT("RestoreSourceModule"));
	};

	asCModule* SourceModule = BuildRestoreModule(*this, SourceEngine, "RestoreSourceModule");
	if (!TestNotNull(TEXT("Restore roundtrip should compile a source module"), SourceModule))
	{
		return false;
	}

	int32 SourceValue = 0;
	if (!ExecuteRestoreFunction(*this, SourceEngine, *SourceModule, SourceValue))
	{
		return false;
	}
	if (!TestEqual(TEXT("Restore roundtrip should execute before serialization"), SourceValue, 42))
	{
		return false;
	}

	FMemoryBinaryStream Stream;
	const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
	if (!TestEqual(TEXT("Restore roundtrip should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Restore roundtrip should emit bytecode bytes"), Stream.Num() > 0))
	{
		return false;
	}

	Stream.ResetReadOffset();
	SourceModule->Discard();
	bool bWasDebugInfoStripped = true;
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
	const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
	ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
	ON_SCOPE_EXIT
	{
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
	};
	asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreSourceModule");
	if (!TestNotNull(TEXT("Restore roundtrip should create a destination module"), RestoredModule))
	{
		return false;
	}

	const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
	if (!TestEqual(TEXT("Restore roundtrip should load bytecode successfully"), LoadResult, static_cast<int>(asSUCCESS)))
	{
		return false;
	}
	if (!TestFalse(TEXT("Restore roundtrip should preserve debug info when not stripping"), bWasDebugInfoStripped))
	{
		return false;
	}

	return true;
}

bool FAngelscriptRestoreStripDebugInfoRoundTripTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> SourceEngineOwner = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Restore strip roundtrip should create an isolated clone test engine"), SourceEngineOwner.Get()))
	{
		return false;
	}

	FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
	ON_SCOPE_EXIT
	{
		SourceEngine.DiscardModule(TEXT("RestoreStripSourceModule"));
	};

	asCModule* SourceModule = BuildRestoreModule(*this, SourceEngine, "RestoreStripSourceModule");
	if (!TestNotNull(TEXT("Restore strip roundtrip should compile a source module"), SourceModule))
	{
		return false;
	}

	FMemoryBinaryStream Stream;
	const int SaveResult = SourceModule->SaveByteCode(&Stream, true);
	if (!TestEqual(TEXT("Restore strip roundtrip should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
	{
		return false;
	}

	Stream.ResetReadOffset();
	SourceModule->Discard();
	bool bWasDebugInfoStripped = false;
	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
	const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
	ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
	ON_SCOPE_EXIT
	{
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
	};
	asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreStripSourceModule");
	if (!TestNotNull(TEXT("Restore strip roundtrip should create a destination module"), RestoredModule))
	{
		return false;
	}

	const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
	if (!TestEqual(TEXT("Restore strip roundtrip should load bytecode successfully"), LoadResult, static_cast<int>(asSUCCESS)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Restore strip roundtrip should report stripped debug info"), bWasDebugInfoStripped))
	{
		return false;
	}

	return true;
}

bool FAngelscriptRestoreEmptyStreamFailsTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> EngineOwner = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Restore empty stream test should create an isolated clone test engine"), EngineOwner.Get()))
	{
		return false;
	}

	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(EngineOwner->GetScriptEngine());
	asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreEmptyStream");
	if (!TestNotNull(TEXT("Restore empty stream test should create a destination module"), RestoredModule))
	{
		return false;
	}

	FMemoryBinaryStream Stream;
	bool bWasDebugInfoStripped = false;
	AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
	const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
	return TestNotEqual(TEXT("Restore should reject an empty bytecode stream"), LoadResult, static_cast<int>(asSUCCESS));
}

bool FAngelscriptRestoreTruncatedStreamFailsTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FAngelscriptEngine> SourceEngineOwner = AngelscriptTestSupport::CreateCloneTestEngine();
	if (!TestNotNull(TEXT("Restore truncated stream test should create an isolated clone test engine"), SourceEngineOwner.Get()))
	{
		return false;
	}

	FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
	ON_SCOPE_EXIT
	{
		SourceEngine.DiscardModule(TEXT("RestoreTruncatedSourceModule"));
	};

	asCModule* SourceModule = BuildRestoreModule(*this, SourceEngine, "RestoreTruncatedSourceModule");
	if (!TestNotNull(TEXT("Restore truncated stream test should compile a source module"), SourceModule))
	{
		return false;
	}

	FMemoryBinaryStream Stream;
	const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
	if (!TestEqual(TEXT("Restore truncated stream test should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Restore truncated stream test should emit enough bytes to truncate"), Stream.Num() > 16))
	{
		return false;
	}

	Stream.Truncate(Stream.Num() - 16);
	Stream.ResetReadOffset();
	SourceModule->Discard();

	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
	const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
	ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
	ON_SCOPE_EXIT
	{
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
	};

	asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreTruncatedSourceModule");
	if (!TestNotNull(TEXT("Restore truncated stream test should create a destination module"), RestoredModule))
	{
		return false;
	}

	bool bWasDebugInfoStripped = false;
	AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
	const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
	return TestNotEqual(TEXT("Restore should reject a truncated bytecode stream"), LoadResult, static_cast<int>(asSUCCESS));
}

#endif
