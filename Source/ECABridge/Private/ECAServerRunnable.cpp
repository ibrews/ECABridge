// Copyright Epic Games, Inc. All Rights Reserved.

#include "ECAServerRunnable.h"
#include "ECABridge.h"
#include "ECABridgeSettings.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Async/Async.h"

FECAServerRunnable::FECAServerRunnable(UECABridge* InBridge, int32 InPort)
	: Bridge(InBridge)
	, ListenerSocket(nullptr)
	, Port(InPort)
	, bShouldStop(false)
{
}

FECAServerRunnable::~FECAServerRunnable()
{
	if (ListenerSocket)
	{
		ListenerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
	}
}

bool FECAServerRunnable::Init()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to get socket subsystem"));
		return false;
	}
	
	// Create listener socket
	ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("ECAServer"), false);
	if (!ListenerSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to create listener socket"));
		return false;
	}
	
	// Configure socket
	ListenerSocket->SetReuseAddr(true);
	ListenerSocket->SetNonBlocking(true);
	ListenerSocket->SetNoDelay(true);
	
	// Bind to address
	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	
	const UECABridgeSettings* Settings = UECABridgeSettings::Get();
	if (Settings->bAllowRemoteConnections)
	{
		Addr->SetAnyAddress();
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] Allowing remote connections (security risk!)"));
	}
	else
	{
		// Localhost only
		bool bIsValid;
		Addr->SetIp(TEXT("127.0.0.1"), bIsValid);
	}
	Addr->SetPort(Port);
	
	if (!ListenerSocket->Bind(*Addr))
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to bind to port %d"), Port);
		return false;
	}
	
	if (!ListenerSocket->Listen(8))
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to listen on socket"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] TCP server listening on port %d"), Port);
	return true;
}

uint32 FECAServerRunnable::Run()
{
	while (!bShouldStop)
	{
		bool bHasPendingConnection = false;
		if (ListenerSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
		{
			FSocket* ClientSocket = ListenerSocket->Accept(TEXT("ECAClient"));
			if (ClientSocket)
			{
				HandleClient(ClientSocket);
				ClientSocket->Close();
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
			}
		}
		
		// Sleep to avoid spinning
		FPlatformProcess::Sleep(0.01f);
	}
	
	return 0;
}

void FECAServerRunnable::Stop()
{
	bShouldStop = true;
}

void FECAServerRunnable::HandleClient(FSocket* ClientSocket)
{
	ClientSocket->SetNonBlocking(false);
	
	// Read the command
	FString JsonCommand;
	if (!ReadMessage(ClientSocket, JsonCommand))
	{
		return;
	}
	
	if (JsonCommand.IsEmpty())
	{
		return;
	}
	
	// Execute on game thread and wait for result
	FString Response;
	FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool();
	
	AsyncTask(ENamedThreads::GameThread, [this, &JsonCommand, &Response, CompletionEvent]()
	{
		if (Bridge)
		{
			Response = Bridge->ProcessCommand(JsonCommand);
		}
		else
		{
			Response = TEXT("{\"success\":false,\"error\":\"Bridge not available\"}");
		}
		CompletionEvent->Trigger();
	});
	
	// Wait for completion with timeout
	const UECABridgeSettings* Settings = UECABridgeSettings::Get();
	bool bCompleted = CompletionEvent->Wait(Settings->CommandTimeoutSeconds * 1000);
	FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
	
	if (!bCompleted)
	{
		Response = TEXT("{\"success\":false,\"error\":\"Command execution timed out\"}");
	}
	
	// Send response
	SendResponse(ClientSocket, Response);
}

bool FECAServerRunnable::ReadMessage(FSocket* ClientSocket, FString& OutMessage)
{
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(BufferSize);
	
	int32 BytesRead = 0;
	
	// First, try to read a length prefix (4 bytes, big-endian)
	uint8 LengthBuffer[4];
	int32 LengthBytesRead = 0;
	
	// Try to peek if we have a length-prefixed message
	if (ClientSocket->Recv(LengthBuffer, 4, LengthBytesRead, ESocketReceiveFlags::Peek))
	{
		// Check if this looks like a length prefix (reasonable size)
		uint32 MessageLength = (LengthBuffer[0] << 24) | (LengthBuffer[1] << 16) | (LengthBuffer[2] << 8) | LengthBuffer[3];
		
		if (MessageLength > 0 && MessageLength < BufferSize && LengthBytesRead == 4)
		{
			// Looks like a length-prefixed message, consume the length bytes
			ClientSocket->Recv(LengthBuffer, 4, LengthBytesRead);
			
			// Now read exactly that many bytes
			TArray<uint8> MessageBuffer;
			MessageBuffer.SetNumUninitialized(MessageLength);
			
			int32 TotalRead = 0;
			while (TotalRead < (int32)MessageLength)
			{
				int32 ChunkRead = 0;
				if (!ClientSocket->Recv(MessageBuffer.GetData() + TotalRead, MessageLength - TotalRead, ChunkRead))
				{
					break;
				}
				TotalRead += ChunkRead;
			}
			
			if (TotalRead == (int32)MessageLength)
			{
				OutMessage = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(MessageBuffer.GetData())));
				OutMessage = OutMessage.Left(MessageLength);
				return true;
			}
		}
	}
	
	// Fallback: Read until we get valid JSON (for simple clients)
	if (ClientSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead))
	{
		if (BytesRead > 0)
		{
			OutMessage = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
			OutMessage = OutMessage.Left(BytesRead);
			return true;
		}
	}
	
	return false;
}

bool FECAServerRunnable::SendResponse(FSocket* ClientSocket, const FString& Response)
{
	FTCHARToUTF8 ResponseUtf8(*Response);
	const int32 ResponseLength = ResponseUtf8.Length();
	
	// Send length prefix (4 bytes, big-endian)
	uint8 LengthBuffer[4];
	LengthBuffer[0] = (ResponseLength >> 24) & 0xFF;
	LengthBuffer[1] = (ResponseLength >> 16) & 0xFF;
	LengthBuffer[2] = (ResponseLength >> 8) & 0xFF;
	LengthBuffer[3] = ResponseLength & 0xFF;
	
	int32 BytesSent = 0;
	if (!ClientSocket->Send(LengthBuffer, 4, BytesSent))
	{
		return false;
	}
	
	// Send message body
	BytesSent = 0;
	return ClientSocket->Send(reinterpret_cast<const uint8*>(ResponseUtf8.Get()), ResponseLength, BytesSent);
}
