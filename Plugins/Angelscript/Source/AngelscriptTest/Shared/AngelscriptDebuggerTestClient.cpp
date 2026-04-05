#include "Shared/AngelscriptDebuggerTestClient.h"

#include "HAL/PlatformProcess.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace AngelscriptTestSupport
{
	namespace
	{
		constexpr int32 DebuggerEnvelopeHeaderSize = sizeof(int32) + sizeof(uint8);

		bool ParseHostAddress(const FString& Host, FIPv4Address& OutAddress)
		{
			if (Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
			{
				OutAddress = FIPv4Address(127, 0, 0, 1);
				return true;
			}

			return FIPv4Address::Parse(Host, OutAddress);
		}
	}

	FAngelscriptDebuggerTestClient::~FAngelscriptDebuggerTestClient()
	{
		Disconnect();
	}

	bool FAngelscriptDebuggerTestClient::Connect(const FString& Host, int32 Port)
	{
		Disconnect();

		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem == nullptr)
		{
			return SetError(TEXT("Socket subsystem is unavailable."));
		}

		FIPv4Address Address;
		if (!ParseHostAddress(Host, Address))
		{
			return SetError(FString::Printf(TEXT("Failed to parse IPv4 host '%s' for debugger test client."), *Host));
		}

		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("AngelscriptDebuggerTestClient"), false);
		if (Socket == nullptr)
		{
			return SetError(TEXT("Failed to create debugger test client socket."));
		}

		Socket->SetNonBlocking(true);
		Socket->SetNoDelay(true);

		const TSharedRef<FInternetAddr> InternetAddr = SocketSubsystem->CreateInternetAddr();
		InternetAddr->SetIp(Address.Value);
		InternetAddr->SetPort(Port);

		if (!Socket->Connect(*InternetAddr))
		{
			ESocketErrors ErrorCode = SocketSubsystem->GetLastErrorCode();
			if (ErrorCode != SE_NO_ERROR && ErrorCode != SE_EWOULDBLOCK && ErrorCode != SE_EINPROGRESS)
			{
				Disconnect();
				return SetError(FString::Printf(TEXT("Failed to connect debugger test client to %s:%d (socket error %d)."), *Host, Port, static_cast<int32>(ErrorCode)));
			}
		}

		ReceiveBuffer.Reset();
		LastError.Reset();
		return true;
	}

	void FAngelscriptDebuggerTestClient::Disconnect()
	{
		if (Socket != nullptr)
		{
			Socket->Close();
			ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSubsystem != nullptr)
			{
				SocketSubsystem->DestroySocket(Socket);
			}
			Socket = nullptr;
		}

		ReceiveBuffer.Reset();
	}

	bool FAngelscriptDebuggerTestClient::IsConnected() const
	{
		return Socket != nullptr && Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
	}

	bool FAngelscriptDebuggerTestClient::SendRawEnvelope(EDebugMessageType MessageType, const TArray<uint8>& Body)
	{
		if (Socket == nullptr)
		{
			return SetError(TEXT("Cannot send debugger message without an active socket connection."));
		}

		TArray<uint8> Buffer;
		if (!SerializeDebugMessageEnvelope(MessageType, Body, Buffer))
		{
			return SetError(FString::Printf(TEXT("Failed to serialize debugger message type %d."), static_cast<int32>(MessageType)));
		}

		int32 TotalSent = 0;
		while (TotalSent < Buffer.Num())
		{
			int32 BytesSent = 0;
			if (!Socket->Send(Buffer.GetData() + TotalSent, Buffer.Num() - TotalSent, BytesSent))
			{
				return SetError(FString::Printf(TEXT("Failed to send debugger message type %d after %d/%d bytes."), static_cast<int32>(MessageType), TotalSent, Buffer.Num()));
			}

			if (BytesSent <= 0)
			{
				return SetError(FString::Printf(TEXT("Debugger message type %d did not send any bytes."), static_cast<int32>(MessageType)));
			}

			TotalSent += BytesSent;
		}

		LastError.Reset();
		return true;
	}

	TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::ReceiveEnvelope()
	{
		if (Socket == nullptr)
		{
			SetError(TEXT("Cannot receive debugger messages without an active socket connection."));
			return {};
		}

		if (!AppendReceivedData())
		{
			return {};
		}

		FAngelscriptDebugMessageEnvelope Envelope;
		bool bHasEnvelope = false;
		if (!ConsumeEnvelope(Envelope, bHasEnvelope))
		{
			return {};
		}

		if (!bHasEnvelope)
		{
			return {};
		}

		LastError.Reset();
		return Envelope;
	}

	TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::WaitForMessage(float TimeoutSeconds)
	{
		const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < EndTime)
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
			if (Envelope.IsSet())
			{
				return Envelope;
			}

			if (!LastError.IsEmpty())
			{
				return {};
			}

			FPlatformProcess::Sleep(0.0f);
		}

		SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for any debugger message."), TimeoutSeconds));
		return {};
	}

	TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::WaitForMessageType(EDebugMessageType ExpectedType, float TimeoutSeconds)
	{
		const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
		EDebugMessageType LastMessageType = EDebugMessageType::Disconnect;
		bool bSawUnexpectedMessage = false;

		while (FPlatformTime::Seconds() < EndTime)
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
			if (Envelope.IsSet())
			{
				if (Envelope->MessageType == ExpectedType)
				{
					return Envelope;
				}

				bSawUnexpectedMessage = true;
				LastMessageType = Envelope->MessageType;
			}

			if (!LastError.IsEmpty())
			{
				return {};
			}

			FPlatformProcess::Sleep(0.0f);
		}

		if (bSawUnexpectedMessage)
		{
			SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for debugger message type %d. Last received type was %d."), TimeoutSeconds, static_cast<int32>(ExpectedType), static_cast<int32>(LastMessageType)));
		}
		else
		{
			SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for debugger message type %d without receiving any complete envelopes."), TimeoutSeconds, static_cast<int32>(ExpectedType)));
		}

		return {};
	}

	TArray<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::DrainPendingMessages()
	{
		TArray<FAngelscriptDebugMessageEnvelope> Messages;
		while (true)
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
			if (!Envelope.IsSet())
			{
				break;
			}

			Messages.Add(MoveTemp(Envelope.GetValue()));
		}

		LastError.Reset();
		return Messages;
	}

	bool FAngelscriptDebuggerTestClient::SendStartDebugging(int32 AdapterVersion)
	{
		FStartDebuggingMessage Message;
		Message.DebugAdapterVersion = AdapterVersion;
		return SendTypedMessage(EDebugMessageType::StartDebugging, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendContinue()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::Continue, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendStopDebugging()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::StopDebugging, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendDisconnect()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::Disconnect, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendStepIn()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::StepIn, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendStepOver()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::StepOver, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendStepOut()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::StepOut, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendRequestCallStack()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::RequestCallStack, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendRequestBreakFilters()
	{
		FEmptyMessage Message;
		return SendTypedMessage(EDebugMessageType::RequestBreakFilters, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendRequestVariables(const FString& ScopePath)
	{
		FString Message = ScopePath;
		return SendTypedMessage(EDebugMessageType::RequestVariables, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendRequestEvaluate(const FString& Path, int32 DefaultFrame)
	{
		TArray<uint8> Body;
		FMemoryWriter Writer(Body);
		FString MessagePath = Path;
		Writer << MessagePath;
		Writer << DefaultFrame;
		return SendRawEnvelope(EDebugMessageType::RequestEvaluate, Body);
	}

	bool FAngelscriptDebuggerTestClient::SendSetBreakpoint(const FAngelscriptBreakpoint& Breakpoint)
	{
		FAngelscriptBreakpoint Message = Breakpoint;
		return SendTypedMessage(EDebugMessageType::SetBreakpoint, Message);
	}

	bool FAngelscriptDebuggerTestClient::SendClearBreakpoints(const FAngelscriptClearBreakpoints& Breakpoints)
	{
		FAngelscriptClearBreakpoints Message = Breakpoints;
		return SendTypedMessage(EDebugMessageType::ClearBreakpoints, Message);
	}

	bool FAngelscriptDebuggerTestClient::AppendReceivedData()
	{
		if (Socket == nullptr)
		{
			return false;
		}

		uint32 PendingDataSize = 0;
		while (Socket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
		{
			TArray<uint8> Chunk;
			Chunk.SetNumUninitialized(static_cast<int32>(PendingDataSize));
			int32 BytesRead = 0;
			if (!Socket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead))
			{
				return SetError(TEXT("Failed to receive debugger envelope bytes from the test socket."));
			}

			if (BytesRead <= 0)
			{
				return SetError(TEXT("Debugger test socket reported pending data but did not return any bytes."));
			}

			Chunk.SetNum(BytesRead, EAllowShrinking::No);
			ReceiveBuffer.Append(Chunk);
		}

		return true;
	}

	bool FAngelscriptDebuggerTestClient::ConsumeEnvelope(FAngelscriptDebugMessageEnvelope& OutEnvelope, bool& bOutHasEnvelope)
	{
		return TryDeserializeDebugMessageEnvelope(ReceiveBuffer, OutEnvelope, bOutHasEnvelope, &LastError);
	}

	bool FAngelscriptDebuggerTestClient::SetError(const FString& ErrorMessage)
	{
		LastError = ErrorMessage;
		return false;
	}
}
