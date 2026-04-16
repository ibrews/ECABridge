// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpResultCallback.h"

class UECABridge;

/**
 * MCP Streamable HTTP Server
 * 
 * Implements the MCP Streamable HTTP transport (2025-03-26 spec).
 * This is a simpler transport that uses standard HTTP request/response patterns.
 * 
 * Endpoints:
 *   POST /mcp    - Main MCP endpoint for all JSON-RPC messages
 *   GET  /mcp    - Optional SSE endpoint for server-initiated messages (not implemented)
 *   GET  /health - Health check
 */
class ECABRIDGE_API FECAMCPServer
{
public:
	FECAMCPServer(UECABridge* InBridge);
	~FECAMCPServer();

	/** Start the HTTP server */
	bool Start(int32 Port = 3000);
	
	/** Stop the HTTP server */
	void Stop();
	
	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }
	
	/** Get the port */
	int32 GetPort() const { return ServerPort; }

private:
	/** Handle MCP POST request (JSON-RPC messages) */
	bool HandleMCPPost(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	
	/** Handle health check */
	bool HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	
	/** Handle CORS preflight (OPTIONS) requests */
	bool HandleCORSPreflight(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Process a single JSON-RPC request and return the response */
	TSharedPtr<FJsonObject> ProcessJsonRpcRequest(const TSharedPtr<FJsonObject>& Request);
	
	/** Handle MCP initialize request */
	TSharedPtr<FJsonObject> HandleInitialize(const TSharedPtr<FJsonObject>& Params);
	
	/** Handle tools/list request */
	TSharedPtr<FJsonObject> HandleToolsList();
	
	/** Handle tools/call request */
	TSharedPtr<FJsonObject> HandleToolsCall(const TSharedPtr<FJsonObject>& Params);
	
	/** Build MCP tool definitions from registered commands */
	TArray<TSharedPtr<FJsonValue>> BuildToolDefinitions();
	
	/** Create JSON-RPC response wrapper */
	TSharedPtr<FJsonObject> CreateJsonRpcResponse(const TSharedPtr<FJsonObject>& Result, const TSharedPtr<FJsonValue>& RequestId);
	
	/** Create JSON-RPC error response */
	TSharedPtr<FJsonObject> CreateJsonRpcError(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& RequestId);
	
	/** Serialize JSON object to string */
	FString SerializeJson(const TSharedPtr<FJsonObject>& JsonObject);

	/** The bridge to execute commands */
	UECABridge* Bridge;
	
	/** HTTP router */
	TSharedPtr<IHttpRouter> HttpRouter;
	
	/** Route handles for cleanup */
	FHttpRouteHandle MCPRouteHandle;
	FHttpRouteHandle HealthRouteHandle;
	
	/** Server state */
	bool bIsRunning = false;
	int32 ServerPort = 3000;
	
	/** MCP protocol info */
	const FString ServerName = TEXT("eca-unreal-editor");
	const FString ServerVersion = TEXT("1.0.0");
	const FString ProtocolVersion = TEXT("2025-03-26");
};
