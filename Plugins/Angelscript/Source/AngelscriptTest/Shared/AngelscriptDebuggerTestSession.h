#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "AngelscriptTestUtilities.h"

class FAngelscriptDebugServer;

namespace AngelscriptTestSupport
{
	struct FAngelscriptDebuggerSessionConfig
	{
		FAngelscriptEngine* ExistingEngine = nullptr;
		int32 DebugServerPort = 0;
		float DefaultTimeoutSeconds = 5.0f;
		bool bDisableDebugBreaks = false;
		bool bResetSeenEnsuresOnInitialize = true;
		bool bResetSeenEnsuresOnShutdown = true;
	};

	class FAngelscriptDebuggerTestSession
	{
	public:
		FAngelscriptDebuggerTestSession() = default;
		~FAngelscriptDebuggerTestSession();

		bool Initialize(const FAngelscriptDebuggerSessionConfig& Config = FAngelscriptDebuggerSessionConfig());
		void Shutdown();

		bool IsInitialized() const
		{
			return Engine != nullptr && DebugServer != nullptr;
		}

		bool PumpOneTick();
		bool PumpUntil(TFunctionRef<bool()> Predicate, float TimeoutSeconds = 0.0f);

		FAngelscriptEngine& GetEngine() const;
		FAngelscriptDebugServer& GetDebugServer() const;
		int32 GetPort() const
		{
			return Port;
		}

		float GetDefaultTimeoutSeconds() const
		{
			return DefaultTimeoutSeconds;
		}

	private:
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* Engine = nullptr;
		FAngelscriptDebugServer* DebugServer = nullptr;
		TUniquePtr<FAngelscriptEngineScope> GlobalScope;

		int32 Port = 0;
		float DefaultTimeoutSeconds = 5.0f;
		bool bResetSeenEnsuresOnShutdown = true;
		bool bRestoreDebugBreaksToEnabled = false;
		int32 PreviousDebugAdapterVersion = 0;
	};
}
