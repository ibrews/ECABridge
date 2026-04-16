// Copyright Epic Games, Inc. All Rights Reserved.

#include "ECABridge.h"
#include "ECAMCPServer.h"
#include "ECABridgeSettings.h"
#include "Commands/ECACommand.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UECABridge::Initialize()
{
	const UECABridgeSettings* Settings = UECABridgeSettings::Get();
	
	if (!Settings->bAutoStart)
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Auto-start disabled in settings"));
		return;
	}
	
	// Start MCP HTTP server
	MCPServer = MakeShared<FECAMCPServer>(this);
	if (MCPServer->Start(Settings->ServerPort))
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] ========================================"));
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Server running on http://localhost:%d"), Settings->ServerPort);
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] ========================================"));
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Add to Claude Desktop config:"));
		UE_LOG(LogTemp, Log, TEXT("[ECABridge]   \"unreal-editor\": { \"url\": \"http://localhost:%d/sse\" }"), Settings->ServerPort);
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] ========================================"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to start MCP server on port %d"), Settings->ServerPort);
	}
}

void UECABridge::Shutdown()
{
	if (MCPServer.IsValid())
	{
		MCPServer->Stop();
		MCPServer.Reset();
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Server stopped"));
}

bool UECABridge::IsRunning() const
{
	return MCPServer.IsValid() && MCPServer->IsRunning();
}

int32 UECABridge::GetPort() const
{
	return UECABridgeSettings::Get()->ServerPort;
}

FString UECABridge::ProcessCommand(const FString& JsonCommand)
{
	const UECABridgeSettings* Settings = UECABridgeSettings::Get();
	
	if (Settings->bLogCommands)
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Command: %s"), *JsonCommand);
	}
	
	// Parse the command
	FString CommandName;
	TSharedPtr<FJsonObject> Params;
	
	if (!ParseCommand(JsonCommand, CommandName, Params))
	{
		return FECACommandResult::Error(TEXT("Invalid JSON or missing command field")).ToJsonString();
	}
	
	if (Settings->bVerboseLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Executing: %s"), *CommandName);
	}
	
	// Execute registered command
	FECACommandResult Result = FECACommandRegistry::Get().ExecuteCommand(CommandName, Params);
	
	CommandsProcessed++;
	
	if (Settings->bVerboseLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] %s: %s"), 
			*CommandName, 
			Result.bSuccess ? TEXT("Success") : *Result.ErrorMessage);
	}
	
	return Result.ToJsonString();
}

bool UECABridge::ParseCommand(const FString& JsonCommand, FString& OutCommandName, TSharedPtr<FJsonObject>& OutParams)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonCommand);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}
	
	if (!JsonObject->TryGetStringField(TEXT("command"), OutCommandName))
	{
		return false;
	}
	
	const TSharedPtr<FJsonObject>* ParamsPtr;
	if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr))
	{
		OutParams = *ParamsPtr;
	}
	else
	{
		OutParams = MakeShared<FJsonObject>();
	}
	
	return true;
}

void UECABridge::LogRegisteredCommands() const
{
	FECACommandRegistry::Get().LogCommands();
}
