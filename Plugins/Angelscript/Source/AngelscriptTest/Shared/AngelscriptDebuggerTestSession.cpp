#include "AngelscriptDebuggerTestSession.h"

#include "Binds/Bind_Debugging.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"

namespace AngelscriptTestSupport
{
	namespace
	{
		int32 MakeUniqueDebugServerPort()
		{
			static int32 NextOffset = 0;
			constexpr int32 BasePort = 30000;
			constexpr int32 PortWindow = 10000;
			const int32 ProcessBucket = (FPlatformProcess::GetCurrentProcessId() % 500) * 10;
			const int32 Port = BasePort + ProcessBucket + NextOffset;
			NextOffset = (NextOffset + 1) % 10;
			return FMath::Clamp(Port, BasePort, BasePort + PortWindow - 1);
		}
	}

	FAngelscriptDebuggerTestSession::~FAngelscriptDebuggerTestSession()
	{
		Shutdown();
	}

	bool FAngelscriptDebuggerTestSession::Initialize(const FAngelscriptDebuggerSessionConfig& Config)
	{
		Shutdown();

		DefaultTimeoutSeconds = Config.DefaultTimeoutSeconds > 0.0f ? Config.DefaultTimeoutSeconds : 5.0f;
		bResetSeenEnsuresOnShutdown = Config.bResetSeenEnsuresOnShutdown;
		PreviousDebugAdapterVersion = AngelscriptDebugServer::DebugAdapterVersion;

		if (Config.bResetSeenEnsuresOnInitialize)
		{
			AngelscriptForgetSeenEnsures();
		}

		if (Config.bDisableDebugBreaks)
		{
			AngelscriptDisableDebugBreaks();
			bRestoreDebugBreaksToEnabled = true;
		}

		if (Config.ExistingEngine != nullptr)
		{
			Engine = Config.ExistingEngine;
		}
		else
		{
			FAngelscriptEngineConfig EngineConfig;
			EngineConfig.DebugServerPort = Config.DebugServerPort > 0 ? Config.DebugServerPort : MakeUniqueDebugServerPort();
			FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
			OwnedEngine = FAngelscriptEngine::CreateTestingFullEngine(EngineConfig, Dependencies);
			Engine = OwnedEngine.Get();
		}

		if (Engine == nullptr)
		{
			Shutdown();
			return false;
		}

		GlobalScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
		DebugServer = Engine->DebugServer;
		Port = Engine->GetRuntimeConfig().DebugServerPort;

		if (DebugServer == nullptr)
		{
			Shutdown();
			return false;
		}

		return true;
	}

	void FAngelscriptDebuggerTestSession::Shutdown()
	{
		if (bResetSeenEnsuresOnShutdown)
		{
			AngelscriptForgetSeenEnsures();
		}

		if (bRestoreDebugBreaksToEnabled)
		{
			AngelscriptEnableDebugBreaks();
			bRestoreDebugBreaksToEnabled = false;
		}

		AngelscriptDebugServer::DebugAdapterVersion = PreviousDebugAdapterVersion;

		DebugServer = nullptr;
		Port = 0;
		GlobalScope.Reset();
		Engine = nullptr;
		OwnedEngine.Reset();
	}

	bool FAngelscriptDebuggerTestSession::PumpOneTick()
	{
		if (!IsInitialized())
		{
			return false;
		}

		if (IsInGameThread())
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		}

		DebugServer->Tick();
		FPlatformProcess::Sleep(0.0f);
		return true;
	}

	bool FAngelscriptDebuggerTestSession::PumpUntil(TFunctionRef<bool()> Predicate, float TimeoutSeconds)
	{
		if (!IsInitialized())
		{
			return false;
		}

		if (Predicate())
		{
			return true;
		}

		const double Timeout = TimeoutSeconds > 0.0f ? TimeoutSeconds : DefaultTimeoutSeconds;
		const double EndTime = FPlatformTime::Seconds() + Timeout;

		while (FPlatformTime::Seconds() < EndTime)
		{
			if (!PumpOneTick())
			{
				return false;
			}

			if (Predicate())
			{
				return true;
			}
		}

		return Predicate();
	}

	FAngelscriptEngine& FAngelscriptDebuggerTestSession::GetEngine() const
	{
		check(Engine != nullptr);
		return *Engine;
	}

	FAngelscriptDebugServer& FAngelscriptDebuggerTestSession::GetDebugServer() const
	{
		check(DebugServer != nullptr);
		return *DebugServer;
	}
}
