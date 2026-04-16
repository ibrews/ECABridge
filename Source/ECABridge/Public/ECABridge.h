// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "ECABridge.generated.h"

class FECAMCPServer;

/**
 * ECA Bridge
 * 
 * Main coordinator for Epic Code Assistant integration.
 * Manages the MCP HTTP server and routes commands to the appropriate handlers.
 */
UCLASS()
class ECABRIDGE_API UECABridge : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize the bridge and start the MCP server */
	void Initialize();
	
	/** Shutdown the bridge and stop the MCP server */
	void Shutdown();
	
	/** Process a JSON command string and return the response */
	FString ProcessCommand(const FString& JsonCommand);
	
	/** Check if the server is running */
	bool IsRunning() const;
	
	/** Get the port the MCP server is listening on */
	int32 GetPort() const;
	
	/** Get the number of commands processed */
	int32 GetCommandsProcessed() const { return CommandsProcessed; }
	
	/** Log all registered commands to the output log */
	void LogRegisteredCommands() const;

private:
	/** The MCP HTTP server */
	TSharedPtr<FECAMCPServer> MCPServer;
	
	/** Counter for processed commands */
	TAtomic<int32> CommandsProcessed{0};
	
	/** Parse JSON command and extract command name and params */
	bool ParseCommand(const FString& JsonCommand, FString& OutCommandName, TSharedPtr<FJsonObject>& OutParams);
};
