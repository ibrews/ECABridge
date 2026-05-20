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
 * Singleton cache for oversize tool-call response chunks. When a single
 * tools/call result exceeds the configured size cap, the server stores the
 * remainder here under an opaque token; the client retrieves subsequent
 * chunks by invoking the `continue_response` meta-command with that token.
 *
 * Entries auto-expire after 5 minutes to bound memory.
 */
class ECABRIDGE_API FECAResponseChunkCache
{
public:
	static FECAResponseChunkCache& Get();

	/** Store text under a fresh token and return the token. */
	FString StoreRemainder(const FString& RemainingText);

	/** Fetch the next chunk for a token. Returns the next slice (up to MaxBytes
	 *  characters) and sets bOutFinal=true when no bytes remain after this fetch.
	 *  Unknown token -> empty string, bOutFinal=true. */
	FString FetchNext(const FString& Token, int32 MaxBytes, bool& bOutFinal);

	/** Drop entries older than MaxAgeSeconds. Called opportunistically. */
	void Purge(double MaxAgeSeconds = 300.0);

private:
	struct FEntry
	{
		FString Remaining;
		double StoredAt = 0.0;
	};

	TMap<FString, FEntry> Entries;
	FCriticalSection Lock;
	int32 NextId = 1;
};

/**
 * Per-session state carried by the `Mcp-Session-Id` header (MCP 2025-03-26
 * session management). The server stamps a fresh ID on initialize; clients
 * echo it back on subsequent requests so concurrent agents don't trip over
 * each other's category-load / chunked-response state.
 */
struct ECABRIDGE_API FECASessionState
{
	FString SessionId;
	double CreatedAt = 0.0;
	double LastAccessedAt = 0.0;
	/** Categories the client has explicitly opted into for tools/list. Layered on
	 *  top of the global lazy-mode set; ALL categories in either set become visible. */
	TSet<FString> LoadedCategories;
	/** Opaque key/value scratch space agents can use to stash per-session context
	 *  (e.g. last-used asset paths). Reserved keys may be added later. */
	TMap<FString, FString> Metadata;
};

/**
 * MCP Streamable HTTP Server
 *
 * Implements the MCP Streamable HTTP transport (2025-03-26 spec).
 * This is a simpler transport that uses standard HTTP request/response patterns.
 *
 * Endpoints:
 *   POST   /mcp    - Main MCP endpoint for all JSON-RPC messages
 *   GET    /mcp    - SSE endpoint for server-initiated messages (list_changed, progress)
 *   DELETE /mcp    - Terminate the session identified by Mcp-Session-Id
 *   GET    /health - Health check
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

	/** Handle GET /mcp — drains queued server-initiated notifications as a
	 *  text/event-stream response (MCP Streamable HTTP, 2025-03-26 spec).
	 *  Clients poll this to discover notifications/tools/list_changed and
	 *  notifications/progress events. */
	bool HandleMCPGet(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle DELETE /mcp — terminate the session identified by Mcp-Session-Id. */
	bool HandleMCPDelete(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Look up the Mcp-Session-Id request header. Returns empty if absent. */
	static FString ReadSessionHeader(const FHttpServerRequest& Request);

	/** Ensure a session exists for the request, generating a fresh ID when the
	 *  header is absent (e.g. on initialize). Returns the active session ID and
	 *  bumps LastAccessedAt. The caller writes this back as a response header. */
	FString TouchSession(const FString& IncomingId);

	/** Prune sessions inactive longer than MaxAgeSeconds. */
	void PurgeSessions(double MaxAgeSeconds = 3600.0);

public:
	/** Enqueue a JSON-RPC notification for delivery via GET /mcp SSE. */
	void EnqueueNotification(const TSharedPtr<FJsonObject>& Notification);

	/** Build + enqueue a `notifications/tools/list_changed` notification. */
	void BroadcastToolsListChanged();

	/** Number of currently tracked sessions. Surfaced on /health. */
	int32 GetSessionCount() const;

private:

	/** Process a single JSON-RPC request and return the response */
	TSharedPtr<FJsonObject> ProcessJsonRpcRequest(const TSharedPtr<FJsonObject>& Request);
	
	/** Handle MCP initialize request */
	TSharedPtr<FJsonObject> HandleInitialize(const TSharedPtr<FJsonObject>& Params);
	
	/** Handle tools/list request. Params may carry a `category` string to filter the
	 *  returned tool set (custom extension on top of the spec; clients without
	 *  awareness of it just see the lazy-mode default). */
	TSharedPtr<FJsonObject> HandleToolsList(const TSharedPtr<FJsonObject>& Params);

	/** Handle tools/call request */
	TSharedPtr<FJsonObject> HandleToolsCall(const TSharedPtr<FJsonObject>& Params);

	/** Build MCP tool definitions from registered commands. When CategoryFilter is
	 *  non-empty, only commands in that category are emitted (bypasses lazy mode). */
	TArray<TSharedPtr<FJsonValue>> BuildToolDefinitions(const FString& CategoryFilter = FString());
	
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
	FHttpRouteHandle MCPGetRouteHandle;
	FHttpRouteHandle HealthRouteHandle;

	/** Queue of server-initiated JSON-RPC notifications awaiting delivery via
	 *  GET /mcp. Bounded to MaxPendingNotifications; oldest entries are dropped. */
	TArray<TSharedPtr<FJsonObject>> PendingNotifications;
	FCriticalSection NotificationLock;
	static constexpr int32 MaxPendingNotifications = 256;

	/** Active sessions keyed by Mcp-Session-Id. */
	TMap<FString, FECASessionState> Sessions;
	mutable FCriticalSection SessionLock;
	
	/** Server state */
	bool bIsRunning = false;
	int32 ServerPort = 3000;

	/** Per-response size cap. Results larger than this are split and the
	 *  remainder is stashed in FECAResponseChunkCache. Default 64 KB. */
	int32 MaxResponseBytes = 65536;
	
	/** MCP protocol info */
	const FString ServerName = TEXT("eca-unreal-editor");
	const FString ServerVersion = TEXT("1.0.0");
	const FString ProtocolVersion = TEXT("2025-03-26");
};
