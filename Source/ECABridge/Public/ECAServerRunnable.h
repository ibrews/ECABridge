// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class UECABridge;
class FSocket;

/**
 * TCP Server Runnable
 * 
 * Handles incoming TCP connections from ECA clients.
 * Runs on a separate thread and marshals commands to the game thread for execution.
 */
class ECABRIDGE_API FECAServerRunnable : public FRunnable
{
public:
	FECAServerRunnable(UECABridge* InBridge, int32 InPort);
	virtual ~FECAServerRunnable();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	/** Handle a client connection */
	void HandleClient(FSocket* ClientSocket);
	
	/** Read a complete message from the socket (handles length-prefixed protocol) */
	bool ReadMessage(FSocket* ClientSocket, FString& OutMessage);
	
	/** Send a response to the client */
	bool SendResponse(FSocket* ClientSocket, const FString& Response);

	/** The bridge to route commands to */
	UECABridge* Bridge;
	
	/** Listener socket */
	FSocket* ListenerSocket;
	
	/** Port to listen on */
	int32 Port;
	
	/** Flag to signal shutdown */
	TAtomic<bool> bShouldStop;
	
	/** Buffer for receiving data */
	static constexpr int32 BufferSize = 1024 * 1024; // 1MB buffer for large commands
};
