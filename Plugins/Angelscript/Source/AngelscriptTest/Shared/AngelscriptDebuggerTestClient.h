#pragma once

#include "CoreMinimal.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

class FSocket;

namespace AngelscriptTestSupport
{
	class FAngelscriptDebuggerTestClient
	{
	public:
		FAngelscriptDebuggerTestClient() = default;
		~FAngelscriptDebuggerTestClient();

		bool Connect(const FString& Host, int32 Port);
		void Disconnect();

		bool IsConnected() const;
		const FString& GetLastError() const
		{
			return LastError;
		}

		bool SendRawEnvelope(EDebugMessageType MessageType, const TArray<uint8>& Body);

		template <typename T>
		bool SendTypedMessage(EDebugMessageType MessageType, T Message)
		{
			TArray<uint8> Body;
			FMemoryWriter Writer(Body);
			Writer << Message;
			return SendRawEnvelope(MessageType, Body);
		}

		TOptional<FAngelscriptDebugMessageEnvelope> ReceiveEnvelope();
		TOptional<FAngelscriptDebugMessageEnvelope> WaitForMessage(float TimeoutSeconds);
		TOptional<FAngelscriptDebugMessageEnvelope> WaitForMessageType(EDebugMessageType ExpectedType, float TimeoutSeconds);
		TArray<FAngelscriptDebugMessageEnvelope> DrainPendingMessages();

		template <typename T>
		static TOptional<T> DeserializeMessage(const FAngelscriptDebugMessageEnvelope& Envelope)
		{
			T Value;
			TArray<uint8> Body = Envelope.Body;
			FMemoryReader Reader(Body);
			Reader << Value;
			if (Reader.IsError())
			{
				return {};
			}
			return Value;
		}

		template <typename T>
		TOptional<T> WaitForTypedMessage(EDebugMessageType ExpectedType, float TimeoutSeconds)
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = WaitForMessageType(ExpectedType, TimeoutSeconds);
			if (!Envelope.IsSet())
			{
				return {};
			}
			return DeserializeMessage<T>(Envelope.GetValue());
		}

		bool SendStartDebugging(int32 AdapterVersion);
		bool SendContinue();
		bool SendStopDebugging();
		bool SendDisconnect();
		bool SendStepIn();
		bool SendStepOver();
		bool SendStepOut();
		bool SendRequestCallStack();
		bool SendRequestBreakFilters();
		bool SendRequestVariables(const FString& ScopePath);
		bool SendRequestEvaluate(const FString& Path, int32 DefaultFrame = 0);
		bool SendSetBreakpoint(const FAngelscriptBreakpoint& Breakpoint);
		bool SendClearBreakpoints(const FAngelscriptClearBreakpoints& Breakpoints);

	private:
		bool AppendReceivedData();
		bool ConsumeEnvelope(FAngelscriptDebugMessageEnvelope& OutEnvelope, bool& bOutHasEnvelope);
		bool SetError(const FString& ErrorMessage);

		FSocket* Socket = nullptr;
		TArray<uint8> ReceiveBuffer;
		FString LastError;
	};
}
