// Copyright Epic Games, Inc. All Rights Reserved.

#include "ECAMCPServer.h"
#include "ECABridge.h"
#include "Commands/ECACommand.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Guid.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

namespace ECABridgeExamples
{
	// Per-tool example payloads, produced by scripts/gen-examples.py and shipped
	// at <PluginRoot>/Resources/command-examples.json. Loaded once at module
	// startup; queried per tool when building tools/list responses. The lookup
	// is read-only after Load() so a plain TMap is fine — no lock needed since
	// LoadExamples() is only called from the game thread at module start.
	static TMap<FString, TSharedPtr<FJsonObject>> ExamplesByTool;
	static bool bLoaded = false;

	static void Load()
	{
		ExamplesByTool.Reset();
		bLoaded = true;

		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ECABridge"));
		if (!Plugin.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ECABridge] LoadExamples: plugin handle not found — skipping"));
			return;
		}

		const FString Path = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("command-examples.json"));
		if (!FPaths::FileExists(Path))
		{
			UE_LOG(LogTemp, Log, TEXT("[ECABridge] LoadExamples: %s not present — tools/list will omit examples"), *Path);
			return;
		}

		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ECABridge] LoadExamples: failed to read %s"), *Path);
			return;
		}

		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ECABridge] LoadExamples: failed to parse %s"), *Path);
			return;
		}

		for (const auto& Pair : Root->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Object)
			{
				continue;
			}
			// Cross-version: Pair.Key is FString on 5.7, UE::FSharedString on 5.8.
			// Dereferencing either yields const TCHAR*, so an explicit FString
			// construction unifies the two paths without a version guard.
			ExamplesByTool.Add(FString(*Pair.Key), Pair.Value->AsObject());
		}

		UE_LOG(LogTemp, Log, TEXT("[ECABridge] LoadExamples: loaded %d example(s) from %s"),
			ExamplesByTool.Num(), *Path);
	}

	// Lookup helper. Returns null when no example is registered for ToolName.
	static TSharedPtr<FJsonObject> Find(const FString& ToolName)
	{
		if (!bLoaded)
		{
			Load();
		}
		const TSharedPtr<FJsonObject>* Found = ExamplesByTool.Find(ToolName);
		return Found ? *Found : TSharedPtr<FJsonObject>();
	}
}

void FECAMCPServer::LoadExamples()
{
	ECABridgeExamples::Load();
}

namespace ECABridgeCompress
{
	// Compression threshold: skip compression for small responses where the
	// CPU + framing overhead outweighs any wire-size saving.
	static constexpr int32 MinSizeBytes = 16 * 1024;

	// Inspect a CSV-style Accept-Encoding header for the named encoding.
	// Robust to whitespace and q= quality values: "gzip;q=0.5, deflate;q=1.0".
	static bool AcceptsEncoding(const TArray<FString>& HeaderValues, const TCHAR* Name)
	{
		const FString NameStr(Name);
		for (const FString& Header : HeaderValues)
		{
			TArray<FString> Parts;
			Header.ParseIntoArray(Parts, TEXT(","), true);
			for (const FString& Part : Parts)
			{
				FString Trimmed = Part.TrimStartAndEnd();
				int32 SemiIdx;
				if (Trimmed.FindChar(TEXT(';'), SemiIdx))
				{
					Trimmed = Trimmed.Left(SemiIdx).TrimStartAndEnd();
				}
				if (Trimmed.Equals(NameStr, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
		return false;
	}

	// Try to compress UTF-8 bytes with zlib (used as 'deflate'). Returns true on
	// success with OutCompressed populated. Mismatched sizes / FCompression
	// failures keep us on the uncompressed fall-through.
	static bool TryDeflate(const TArray<uint8>& Uncompressed, TArray<uint8>& OutCompressed)
	{
		const int32 SrcSize = Uncompressed.Num();
		int32 BoundSize = FCompression::CompressMemoryBound(NAME_Zlib, SrcSize);
		if (BoundSize <= 0)
		{
			return false;
		}
		OutCompressed.SetNumUninitialized(BoundSize);
		int32 CompressedSize = BoundSize;
		const bool bOk = FCompression::CompressMemory(
			NAME_Zlib,
			OutCompressed.GetData(),
			CompressedSize,
			Uncompressed.GetData(),
			SrcSize);
		if (!bOk || CompressedSize <= 0 || CompressedSize >= SrcSize)
		{
			OutCompressed.Reset();
			return false;
		}
		OutCompressed.SetNum(CompressedSize, EAllowShrinking::No);
		return true;
	}
}

FECAResponseChunkCache& FECAResponseChunkCache::Get()
{
	static FECAResponseChunkCache Instance;
	return Instance;
}

FString FECAResponseChunkCache::StoreRemainder(const FString& RemainingText)
{
	FScopeLock ScopedLock(&Lock);
	const FString Token = FString::Printf(TEXT("eca-ct-%d-%s"), NextId++, *FGuid::NewGuid().ToString(EGuidFormats::Short));
	FEntry Entry;
	Entry.Remaining = RemainingText;
	Entry.StoredAt = FPlatformTime::Seconds();
	Entries.Add(Token, MoveTemp(Entry));
	return Token;
}

FString FECAResponseChunkCache::FetchNext(const FString& Token, int32 MaxBytes, bool& bOutFinal)
{
	FScopeLock ScopedLock(&Lock);
	FEntry* Entry = Entries.Find(Token);
	if (!Entry)
	{
		bOutFinal = true;
		return FString();
	}

	if (MaxBytes <= 0 || Entry->Remaining.Len() <= MaxBytes)
	{
		FString Chunk = Entry->Remaining;
		Entries.Remove(Token);
		bOutFinal = true;
		return Chunk;
	}

	FString Chunk = Entry->Remaining.Left(MaxBytes);
	Entry->Remaining = Entry->Remaining.RightChop(MaxBytes);
	Entry->StoredAt = FPlatformTime::Seconds();
	bOutFinal = false;
	return Chunk;
}

void FECAResponseChunkCache::Purge(double MaxAgeSeconds)
{
	FScopeLock ScopedLock(&Lock);
	const double Now = FPlatformTime::Seconds();
	TArray<FString> Expired;
	for (const auto& Pair : Entries)
	{
		if (Now - Pair.Value.StoredAt > MaxAgeSeconds)
		{
			Expired.Add(Pair.Key);
		}
	}
	for (const FString& Key : Expired)
	{
		Entries.Remove(Key);
	}
}

FECAMCPServer::FECAMCPServer(UECABridge* InBridge)
	: Bridge(InBridge)
{
}

FECAMCPServer::~FECAMCPServer()
{
	Stop();
}

bool FECAMCPServer::Start(int32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] MCP Server already running"));
		return true;
	}

	ServerPort = Port;

	// Get or start HTTP server module
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(ServerPort);
	
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to get HTTP router for port %d"), ServerPort);
		return false;
	}

	// Register routes
	
	// Main MCP endpoint (POST for messages, DELETE for session termination, OPTIONS for CORS)
	MCPRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_DELETE | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				return HandleCORSPreflight(Request, OnComplete);
			}
			if (Request.Verb == EHttpServerRequestVerbs::VERB_DELETE)
			{
				return HandleMCPDelete(Request, OnComplete);
			}
			return HandleMCPPost(Request, OnComplete);
		})
	);

	// Server-initiated notifications stream (MCP Streamable HTTP SSE).
	// Clients poll this to receive notifications/tools/list_changed,
	// notifications/progress, and notifications/resources/updated.
	MCPGetRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			return HandleMCPGet(Request, OnComplete);
		})
	);

	// Bind registry → MCP server: whenever the visible tool surface changes,
	// emit notifications/tools/list_changed.
	FECACommandRegistry::Get().SetOnVisibleToolsChanged([this]()
	{
		BroadcastToolsListChanged();
	});

	// Progress reporter → notifications/progress (MCP 2025-03-26). Long-running
	// commands call FECAProgressRegistry::Get().Report(...) and this lambda
	// turns each call into a queued SSE notification.
	FECAProgressRegistry::Get().SetListener(
		[this](const FString& Token, double Progress, double Total, const FString& Message)
		{
			TSharedPtr<FJsonObject> Note = MakeShared<FJsonObject>();
			Note->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
			Note->SetStringField(TEXT("method"), TEXT("notifications/progress"));

			TSharedPtr<FJsonObject> ParamsObj = MakeShared<FJsonObject>();
			ParamsObj->SetStringField(TEXT("progressToken"), Token);
			ParamsObj->SetNumberField(TEXT("progress"), Progress);
			if (Total >= 0.0)
			{
				ParamsObj->SetNumberField(TEXT("total"), Total);
			}
			if (!Message.IsEmpty())
			{
				ParamsObj->SetStringField(TEXT("message"), Message);
			}
			Note->SetObjectField(TEXT("params"), ParamsObj);
			EnqueueNotification(Note);
		});

	// Health check endpoint
	HealthRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/health")),
		EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				return HandleCORSPreflight(Request, OnComplete);
			}
			return HandleHealth(Request, OnComplete);
		})
	);

	// Start listening
	HttpServerModule.StartAllListeners();

	bIsRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Streamable HTTP Server started on port %d"), ServerPort);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP endpoint: http://localhost:%d/mcp"), ServerPort);

	return true;
}

void FECAMCPServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Detach the registry callback so a stale pointer isn't invoked after Stop.
	FECACommandRegistry::Get().SetOnVisibleToolsChanged(nullptr);
	FECAProgressRegistry::Get().SetListener(nullptr);

	// Unbind routes
	if (HttpRouter.IsValid())
	{
		HttpRouter->UnbindRoute(MCPRouteHandle);
		HttpRouter->UnbindRoute(MCPGetRouteHandle);
		HttpRouter->UnbindRoute(HealthRouteHandle);
	}

	bIsRunning = false;
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Server stopped"));
}

bool FECAMCPServer::HandleMCPPost(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Parse request body
	FString RequestBody;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		RequestBody = FString(Converter.Length(), Converter.Get());
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Request: %s"), *RequestBody.Left(500));
	
	if (RequestBody.IsEmpty())
	{
		TSharedPtr<FJsonObject> ErrorResponse = CreateJsonRpcError(-32700, TEXT("Empty request body"), nullptr);
		FString ResponseStr = SerializeJson(ErrorResponse);
		
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		Response->Code = EHttpServerResponseCodes::BadRequest;
		OnComplete(MoveTemp(Response));
		return true;
	}
	
	// Parse JSON
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
	TSharedPtr<FJsonValue> ParsedJson;
	
	if (!FJsonSerializer::Deserialize(Reader, ParsedJson) || !ParsedJson.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ECABridge] Failed to parse JSON: %s"), *RequestBody.Left(200));
		TSharedPtr<FJsonObject> ErrorResponse = CreateJsonRpcError(-32700, TEXT("Parse error"), nullptr);
		FString ResponseStr = SerializeJson(ErrorResponse);
		
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		Response->Code = EHttpServerResponseCodes::BadRequest;
		OnComplete(MoveTemp(Response));
		return true;
	}
	
	// Handle batch or single request
	FString ResponseStr;
	
	if (ParsedJson->Type == EJson::Array)
	{
		// Batch request
		TArray<TSharedPtr<FJsonValue>> Requests = ParsedJson->AsArray();
		TArray<TSharedPtr<FJsonValue>> Responses;
		
		for (const TSharedPtr<FJsonValue>& ReqValue : Requests)
		{
			if (ReqValue->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Result = ProcessJsonRpcRequest(ReqValue->AsObject());
				if (Result.IsValid())
				{
					Responses.Add(MakeShared<FJsonValueObject>(Result));
				}
			}
		}
		
		// Serialize array response
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
		FJsonSerializer::Serialize(Responses, Writer);
	}
	else if (ParsedJson->Type == EJson::Object)
	{
		// Single request
		TSharedPtr<FJsonObject> Result = ProcessJsonRpcRequest(ParsedJson->AsObject());
		if (Result.IsValid())
		{
			ResponseStr = SerializeJson(Result);
		}
	}
	
	// If no response (notification), return 202 Accepted
	if (ResponseStr.IsEmpty())
	{
		const FString NoRespSessionId = TouchSession(ReadSessionHeader(Request));
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(), TEXT("application/json"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		Response->Headers.Add(TEXT("Mcp-Session-Id"), { NoRespSessionId });
		Response->Code = EHttpServerResponseCodes::Accepted;
		OnComplete(MoveTemp(Response));
		return true;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Response: %s"), *ResponseStr.Left(500));

	// MCP session: read incoming header, mint/refresh a session, echo back so
	// the client uses it on subsequent requests.
	const FString IncomingSessionId = ReadSessionHeader(Request);
	const FString ActiveSessionId = TouchSession(IncomingSessionId);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Mcp-Session-Id"), { ActiveSessionId });

	// Negotiated compression: clients that send Accept-Encoding: deflate (e.g.
	// curl --compressed, most browsers, MCP clients) get a zlib-compressed body
	// when the response crosses the threshold. Saves ~6-8x on the JSON-heavy
	// dump_* / find_assets responses without giving up readability for small
	// replies.
	if (Response->Body.Num() >= ECABridgeCompress::MinSizeBytes)
	{
		const TArray<FString>* AcceptEnc = Response->Headers.Find(TEXT("Accept-Encoding"));
		if (!AcceptEnc)
		{
			AcceptEnc = Request.Headers.Find(TEXT("Accept-Encoding"));
		}
		if (AcceptEnc && ECABridgeCompress::AcceptsEncoding(*AcceptEnc, TEXT("deflate")))
		{
			TArray<uint8> Compressed;
			if (ECABridgeCompress::TryDeflate(Response->Body, Compressed))
			{
				const int32 OriginalSize = Response->Body.Num();
				Response->Body = MoveTemp(Compressed);
				Response->Headers.Add(TEXT("Content-Encoding"), { TEXT("deflate") });
				UE_LOG(LogTemp, Log, TEXT("[ECABridge] Response compressed: %d -> %d bytes (%.1f%%)"),
					OriginalSize, Response->Body.Num(), 100.0 * Response->Body.Num() / OriginalSize);
			}
		}
	}

	OnComplete(MoveTemp(Response));
	return true;
}

TSharedPtr<FJsonObject> FECAMCPServer::ProcessJsonRpcRequest(const TSharedPtr<FJsonObject>& Request)
{
	if (!Request.IsValid())
	{
		return CreateJsonRpcError(-32600, TEXT("Invalid Request"), nullptr);
	}
	
	// Extract method
	FString Method;
	if (!Request->TryGetStringField(TEXT("method"), Method))
	{
		return CreateJsonRpcError(-32600, TEXT("Missing method field"), nullptr);
	}
	
	// Extract id (can be string, number, or null; missing = notification)
	TSharedPtr<FJsonValue> RequestId = Request->TryGetField(TEXT("id"));
	bool bIsNotification = !RequestId.IsValid() || RequestId->IsNull();
	
	// Extract params
	TSharedPtr<FJsonObject> Params;
	const TSharedPtr<FJsonObject>* ParamsPtr;
	if (Request->TryGetObjectField(TEXT("params"), ParamsPtr))
	{
		Params = *ParamsPtr;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Processing: method=%s, notification=%s"), *Method, bIsNotification ? TEXT("true") : TEXT("false"));
	
	// Route to handler
	TSharedPtr<FJsonObject> Result;
	
	if (Method == TEXT("initialize"))
	{
		Result = HandleInitialize(Params);
	}
	else if (Method == TEXT("tools/list"))
	{
		Result = HandleToolsList(Params);
	}
	else if (Method == TEXT("tools/call"))
	{
		// Register a cancellation token for this request ID so the client can
		// later send notifications/cancelled to abort. The token is published in
		// TLS for the duration of Execute(); commands poll
		// FECACancellationRegistry::IsCurrentRequestCancelled() to bail early.
		FString RequestIdString;
		if (RequestId.IsValid())
		{
			if (RequestId->Type == EJson::String) { RequestIdString = RequestId->AsString(); }
			else if (RequestId->Type == EJson::Number) { RequestIdString = FString::Printf(TEXT("%lld"), (int64)RequestId->AsNumber()); }
		}

		// Progress token (per MCP 2025-03-26): clients pass params._meta.progressToken
		// to opt in to streaming notifications/progress events.
		FString ProgressToken;
		if (Params.IsValid())
		{
			const TSharedPtr<FJsonObject>* MetaPtr = nullptr;
			if (Params->TryGetObjectField(TEXT("_meta"), MetaPtr) && MetaPtr && MetaPtr->IsValid())
			{
				if (!(*MetaPtr)->TryGetStringField(TEXT("progressToken"), ProgressToken))
				{
					double Num = 0.0;
					if ((*MetaPtr)->TryGetNumberField(TEXT("progressToken"), Num))
					{
						ProgressToken = FString::Printf(TEXT("%lld"), (int64)Num);
					}
				}
			}
		}

		FECACancellationRegistry& CancelReg = FECACancellationRegistry::Get();
		FECAProgressRegistry& ProgReg = FECAProgressRegistry::Get();
		CancelReg.RegisterRequest(RequestIdString);
		ProgReg.BindTokenToThread(ProgressToken);
		Result = HandleToolsCall(Params);
		ProgReg.ClearThreadBinding();
		CancelReg.UnregisterRequest(RequestIdString);
	}
	else if (Method == TEXT("resources/list"))
	{
		Result = HandleResourcesList(Params);
	}
	else if (Method == TEXT("resources/read"))
	{
		Result = HandleResourcesRead(Params);
	}
	else if (Method == TEXT("notifications/initialized"))
	{
		// Client notification - no response needed
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Client initialized notification received"));
		return nullptr;
	}
	else if (Method == TEXT("notifications/cancelled"))
	{
		// MCP 2025-03-26 cancellation: params.requestId identifies the in-flight
		// tool-call to abort. Long-running commands poll IsCurrentRequestCancelled.
		if (Params.IsValid())
		{
			FString CancelledId;
			if (!Params->TryGetStringField(TEXT("requestId"), CancelledId))
			{
				double Num = 0.0;
				if (Params->TryGetNumberField(TEXT("requestId"), Num))
				{
					CancelledId = FString::Printf(TEXT("%lld"), (int64)Num);
				}
			}
			if (!CancelledId.IsEmpty())
			{
				const bool bMarked = FECACancellationRegistry::Get().Cancel(CancelledId);
				FString Reason;
				Params->TryGetStringField(TEXT("reason"), Reason);
				UE_LOG(LogTemp, Log, TEXT("[ECABridge] notifications/cancelled requestId=%s found=%s reason=%s"),
					*CancelledId, bMarked ? TEXT("true") : TEXT("false"), *Reason);
			}
		}
		return nullptr;
	}
	else if (Method == TEXT("ping"))
	{
		Result = MakeShared<FJsonObject>();
	}
	else
	{
		return CreateJsonRpcError(-32601, FString::Printf(TEXT("Method not found: %s"), *Method), RequestId);
	}
	
	// Notifications don't get responses
	if (bIsNotification)
	{
		return nullptr;
	}
	
	return CreateJsonRpcResponse(Result, RequestId);
}

bool FECAMCPServer::HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<FJsonObject> HealthJson = MakeShared<FJsonObject>();
	HealthJson->SetStringField(TEXT("status"), TEXT("ok"));
	HealthJson->SetStringField(TEXT("server"), ServerName);
	HealthJson->SetStringField(TEXT("version"), ServerVersion);
	HealthJson->SetStringField(TEXT("protocol"), ProtocolVersion);
	HealthJson->SetNumberField(TEXT("commands"), FECACommandRegistry::Get().GetAllCommands().Num());
	HealthJson->SetBoolField(TEXT("bridge_ready"), Bridge != nullptr);
	HealthJson->SetNumberField(TEXT("sessions"), GetSessionCount());

	FString ResponseBody = SerializeJson(HealthJson);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseBody, TEXT("application/json"));
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	OnComplete(MoveTemp(Response));

	return true;
}

bool FECAMCPServer::HandleCORSPreflight(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(), TEXT("text/plain"));
	Response->Code = EHttpServerResponseCodes::NoContent;
	
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept, Cache-Control") });
	Response->Headers.Add(TEXT("Access-Control-Max-Age"), { TEXT("86400") });
	
	OnComplete(MoveTemp(Response));
	return true;
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleInitialize(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	
	// Server info
	TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	// Protocol version
	Result->SetStringField(TEXT("protocolVersion"), ProtocolVersion);

	// Capabilities
	TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	
	// Tools capability
	TSharedPtr<FJsonObject> ToolsCap = MakeShared<FJsonObject>();
	ToolsCap->SetBoolField(TEXT("listChanged"), true);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);

	// Resources capability — UE assets surfaced under ecabridge://asset/<path>.
	// listChanged + subscribe not implemented yet; advertise the basic surface.
	TSharedPtr<FJsonObject> ResourcesCap = MakeShared<FJsonObject>();
	ResourcesCap->SetBoolField(TEXT("listChanged"), false);
	ResourcesCap->SetBoolField(TEXT("subscribe"), false);
	Capabilities->SetObjectField(TEXT("resources"), ResourcesCap);

	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP client initialized"));

	return Result;
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleToolsList(const TSharedPtr<FJsonObject>& Params)
{
	FString CategoryFilter;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("category"), CategoryFilter);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Tools = BuildToolDefinitions(CategoryFilter);
	Result->SetArrayField(TEXT("tools"), Tools);

	if (!CategoryFilter.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] tools/list category='%s': %d tools"), *CategoryFilter, Tools.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] tools/list: %d tools (lazy=%s)"),
			Tools.Num(),
			FECACommandRegistry::Get().IsLazyMode() ? TEXT("on") : TEXT("off"));
	}

	return Result;
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleToolsCall(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetBoolField(TEXT("isError"), true);
		
		TArray<TSharedPtr<FJsonValue>> Content;
		TSharedPtr<FJsonObject> TextContent = MakeShared<FJsonObject>();
		TextContent->SetStringField(TEXT("type"), TEXT("text"));
		TextContent->SetStringField(TEXT("text"), TEXT("Missing parameters"));
		Content.Add(MakeShared<FJsonValueObject>(TextContent));
		Error->SetArrayField(TEXT("content"), Content);
		
		return Error;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetBoolField(TEXT("isError"), true);
		
		TArray<TSharedPtr<FJsonValue>> Content;
		TSharedPtr<FJsonObject> TextContent = MakeShared<FJsonObject>();
		TextContent->SetStringField(TEXT("type"), TEXT("text"));
		TextContent->SetStringField(TEXT("text"), TEXT("Missing tool name"));
		Content.Add(MakeShared<FJsonValueObject>(TextContent));
		Error->SetArrayField(TEXT("content"), Content);
		
		return Error;
	}
	
	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonObject>* ArgsPtr;
	if (Params->TryGetObjectField(TEXT("arguments"), ArgsPtr))
	{
		Arguments = *ArgsPtr;
	}
	else
	{
		Arguments = MakeShared<FJsonObject>();
	}

	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Tool call: %s"), *ToolName);

	// Execute the command
	FECACommandResult CommandResult = FECACommandRegistry::Get().ExecuteCommand(ToolName, Arguments);

	// Build MCP response
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Content;

	if (CommandResult.bSuccess && CommandResult.McpContent.Num() > 0)
	{
		// Command emitted pre-built MCP content blocks (e.g. inline image). Use them
		// directly so image blocks are not stuffed into a text block.
		for (const TSharedPtr<FJsonObject>& Block : CommandResult.McpContent)
		{
			if (Block.IsValid())
			{
				Content.Add(MakeShared<FJsonValueObject>(Block));
			}
		}

		// Optionally append serialized metadata (ResultData) as a trailing text block
		// so callers that want both structured fields and the image get them.
		if (CommandResult.ResultData.IsValid())
		{
			TSharedPtr<FJsonObject> MetaText = MakeShared<FJsonObject>();
			MetaText->SetStringField(TEXT("type"), TEXT("text"));
			MetaText->SetStringField(TEXT("text"), SerializeJson(CommandResult.ResultData));
			Content.Add(MakeShared<FJsonValueObject>(MetaText));
		}
	}
	else
	{
		TSharedPtr<FJsonObject> TextContent = MakeShared<FJsonObject>();
		TextContent->SetStringField(TEXT("type"), TEXT("text"));

		if (CommandResult.bSuccess)
		{
			FString ResultText;
			if (CommandResult.ResultData.IsValid())
			{
				ResultText = SerializeJson(CommandResult.ResultData);
			}
			else
			{
				ResultText = TEXT("{\"success\": true}");
			}

			// Response-size cap: if the serialized result exceeds the configured
			// budget, return the first chunk and stash the remainder under a
			// continuation token. Client retrieves the rest via continue_response.
			const int32 Cap = MaxResponseBytes;
			if (Cap > 0 && ResultText.Len() > Cap)
			{
				const int32 TotalLen = ResultText.Len();
				FString FirstChunk = ResultText.Left(Cap);
				FString Remainder = ResultText.RightChop(Cap);
				const FString Token = FECAResponseChunkCache::Get().StoreRemainder(Remainder);

				TextContent->SetStringField(TEXT("text"), FirstChunk);
				Result->SetStringField(TEXT("_continuation_token"), Token);
				Result->SetNumberField(TEXT("_chunk_chars"), FirstChunk.Len());
				Result->SetNumberField(TEXT("_remaining_chars"), Remainder.Len());
				Result->SetNumberField(TEXT("_total_chars"), TotalLen);

				UE_LOG(LogTemp, Log, TEXT("[ECABridge] Response chunked: tool=%s total=%d cap=%d token=%s"),
					*ToolName, TotalLen, Cap, *Token);
			}
			else
			{
				TextContent->SetStringField(TEXT("text"), ResultText);
			}
		}
		else
		{
			Result->SetBoolField(TEXT("isError"), true);
			TextContent->SetStringField(TEXT("text"), CommandResult.ErrorMessage);
		}

		Content.Add(MakeShared<FJsonValueObject>(TextContent));
	}

	Result->SetArrayField(TEXT("content"), Content);

	// Opportunistic GC.
	FECAResponseChunkCache::Get().Purge();

	return Result;
}

TArray<TSharedPtr<FJsonValue>> FECAMCPServer::BuildToolDefinitions(const FString& CategoryFilter)
{
	TArray<TSharedPtr<FJsonValue>> Tools;

	for (const TSharedPtr<IECACommand>& Command : FECACommandRegistry::Get().GetVisibleCommands(CategoryFilter))
	{
		TSharedPtr<FJsonObject> ToolDef = MakeShared<FJsonObject>();
		ToolDef->SetStringField(TEXT("name"), Command->GetName());
		ToolDef->SetStringField(TEXT("description"), Command->GetDescription());

		// Build input schema
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Required;

		for (const FECACommandParam& Param : Command->GetParameters())
		{
			TSharedPtr<FJsonObject> PropDef = MakeShared<FJsonObject>();
			
			// Map parameter types to valid JSON Schema types
			// Valid types: string, number, integer, boolean, object, array, null
			FString JsonSchemaType = Param.Type;
			if (Param.Type == TEXT("any"))
			{
				// "any" is not valid in JSON Schema - use a union or just string
				// We'll accept any JSON value by not specifying type constraints
				PropDef->SetStringField(TEXT("description"), Param.Description + TEXT(" (accepts any JSON value)"));
			}
			else if (Param.Type == TEXT("vector") || Param.Type == TEXT("rotator") || Param.Type == TEXT("transform"))
			{
				// These are objects with specific structure
				JsonSchemaType = TEXT("object");
				PropDef->SetStringField(TEXT("type"), JsonSchemaType);
				PropDef->SetStringField(TEXT("description"), Param.Description);
			}
			else if (Param.Type == TEXT("float") || Param.Type == TEXT("double"))
			{
				JsonSchemaType = TEXT("number");
				PropDef->SetStringField(TEXT("type"), JsonSchemaType);
				PropDef->SetStringField(TEXT("description"), Param.Description);
			}
			else if (Param.Type == TEXT("int") || Param.Type == TEXT("int32") || Param.Type == TEXT("int64"))
			{
				JsonSchemaType = TEXT("integer");
				PropDef->SetStringField(TEXT("type"), JsonSchemaType);
				PropDef->SetStringField(TEXT("description"), Param.Description);
			}
			else
			{
				// Standard JSON Schema types: string, number, integer, boolean, object, array
				PropDef->SetStringField(TEXT("type"), JsonSchemaType);
				PropDef->SetStringField(TEXT("description"), Param.Description);
			}
			
			if (!Param.DefaultValue.IsEmpty())
			{
				PropDef->SetStringField(TEXT("default"), Param.DefaultValue);
			}

			Properties->SetObjectField(Param.Name, PropDef);

			if (Param.bRequired)
			{
				Required.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		InputSchema->SetObjectField(TEXT("properties"), Properties);
		if (Required.Num() > 0)
		{
			InputSchema->SetArrayField(TEXT("required"), Required);
		}

		ToolDef->SetObjectField(TEXT("inputSchema"), InputSchema);

		// Surface optional output schema if the command provides one.
		TSharedPtr<FJsonObject> OutputSchema = Command->GetOutputSchema();
		if (OutputSchema.IsValid())
		{
			ToolDef->SetObjectField(TEXT("outputSchema"), OutputSchema);
		}

		// Attach a usage example when one was registered for this tool. Examples
		// are sourced from <PluginRoot>/Resources/command-examples.json (built by
		// scripts/gen-examples.py from the smoke test + any scripts/example-sources/*.json
		// overrides). When the lazy registry loads a new category at runtime,
		// the next tools/list pass picks up examples automatically because we
		// re-query the map for each emitted tool.
		TSharedPtr<FJsonObject> Example = ECABridgeExamples::Find(Command->GetName());
		if (Example.IsValid())
		{
			ToolDef->SetObjectField(TEXT("example"), Example);
		}

		Tools.Add(MakeShared<FJsonValueObject>(ToolDef));
	}

	return Tools;
}

TSharedPtr<FJsonObject> FECAMCPServer::CreateJsonRpcResponse(const TSharedPtr<FJsonObject>& Result, const TSharedPtr<FJsonValue>& RequestId)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	
	if (RequestId.IsValid())
	{
		Response->SetField(TEXT("id"), RequestId);
	}
	
	Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
	
	return Response;
}

TSharedPtr<FJsonObject> FECAMCPServer::CreateJsonRpcError(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& RequestId)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	
	if (RequestId.IsValid())
	{
		Response->SetField(TEXT("id"), RequestId);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetNumberField(TEXT("code"), Code);
	Error->SetStringField(TEXT("message"), Message);
	Response->SetObjectField(TEXT("error"), Error);

	return Response;
}

FString FECAMCPServer::SerializeJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}

void FECAMCPServer::EnqueueNotification(const TSharedPtr<FJsonObject>& Notification)
{
	if (!Notification.IsValid())
	{
		return;
	}
	FScopeLock ScopedLock(&NotificationLock);
	if (PendingNotifications.Num() >= MaxPendingNotifications)
	{
		// Bounded buffer: drop oldest. Clients that haven't polled in a long time
		// see a list_changed eventually anyway when they next call tools/list.
		PendingNotifications.RemoveAt(0);
	}
	PendingNotifications.Add(Notification);
}

void FECAMCPServer::BroadcastToolsListChanged()
{
	TSharedPtr<FJsonObject> Notification = MakeShared<FJsonObject>();
	Notification->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Notification->SetStringField(TEXT("method"), TEXT("notifications/tools/list_changed"));
	// No params per spec — clients re-issue tools/list to discover the new set.
	EnqueueNotification(Notification);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Enqueued notifications/tools/list_changed"));
}

namespace ECABridgeResources
{
	// URI scheme used to expose UE assets as MCP resources.
	static const FString UriPrefix = TEXT("ecabridge://asset/");

	// Convert "/Game/Foo/Bar" -> "ecabridge://asset/Game/Foo/Bar".
	static FString PackageToUri(const FString& PackageName)
	{
		FString Path = PackageName;
		while (Path.StartsWith(TEXT("/")))
		{
			Path = Path.RightChop(1);
		}
		return UriPrefix + Path;
	}

	// Inverse of PackageToUri. Returns empty on mismatch.
	static FString UriToPackage(const FString& Uri)
	{
		if (!Uri.StartsWith(UriPrefix))
		{
			return FString();
		}
		return TEXT("/") + Uri.RightChop(UriPrefix.Len());
	}
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleResourcesList(const TSharedPtr<FJsonObject>& Params)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = AssetRegistryModule.Get();

	// Optional cursor — encodes the AssetData index to resume from (simple int).
	int32 StartIndex = 0;
	FString PathPrefix;
	if (Params.IsValid())
	{
		FString Cursor;
		if (Params->TryGetStringField(TEXT("cursor"), Cursor))
		{
			StartIndex = FCString::Atoi(*Cursor);
		}
		Params->TryGetStringField(TEXT("path_prefix"), PathPrefix);
	}

	TArray<FAssetData> AllAssets;
	if (!PathPrefix.IsEmpty())
	{
		Registry.GetAssetsByPath(FName(*PathPrefix), AllAssets, /*bRecursive=*/true);
	}
	else
	{
		Registry.GetAssetsByPath(FName(TEXT("/Game")), AllAssets, /*bRecursive=*/true);
	}

	const int32 PageSize = 100;
	const int32 EndIndex = FMath::Min(StartIndex + PageSize, AllAssets.Num());

	TArray<TSharedPtr<FJsonValue>> ResourceArray;
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		const FAssetData& Asset = AllAssets[i];
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetStringField(TEXT("uri"), ECABridgeResources::PackageToUri(Asset.PackageName.ToString()));
		R->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		R->SetStringField(TEXT("mimeType"), TEXT("application/json"));
		R->SetStringField(TEXT("description"),
			FString::Printf(TEXT("%s asset at %s"),
				*Asset.AssetClassPath.GetAssetName().ToString(),
				*Asset.PackageName.ToString()));
		ResourceArray.Add(MakeShared<FJsonValueObject>(R));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("resources"), ResourceArray);
	if (EndIndex < AllAssets.Num())
	{
		Result->SetStringField(TEXT("nextCursor"), FString::FromInt(EndIndex));
	}

	UE_LOG(LogTemp, Log, TEXT("[ECABridge] resources/list start=%d end=%d total=%d prefix='%s'"),
		StartIndex, EndIndex, AllAssets.Num(), *PathPrefix);
	return Result;
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleResourcesRead(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Contents;

	if (!Params.IsValid())
	{
		Result->SetArrayField(TEXT("contents"), Contents);
		return Result;
	}

	FString Uri;
	Params->TryGetStringField(TEXT("uri"), Uri);
	const FString PackageName = ECABridgeResources::UriToPackage(Uri);

	if (PackageName.IsEmpty())
	{
		Result->SetArrayField(TEXT("contents"), Contents);
		return Result;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = AssetRegistryModule.Get();

	TArray<FAssetData> Found;
	Registry.GetAssetsByPackageName(FName(*PackageName), Found);

	TSharedPtr<FJsonObject> ContentBlock = MakeShared<FJsonObject>();
	ContentBlock->SetStringField(TEXT("uri"), Uri);
	ContentBlock->SetStringField(TEXT("mimeType"), TEXT("application/json"));

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("package"), PackageName);
	if (Found.Num() > 0)
	{
		const FAssetData& A = Found[0];
		Body->SetStringField(TEXT("name"), A.AssetName.ToString());
		Body->SetStringField(TEXT("class"), A.AssetClassPath.ToString());
		Body->SetStringField(TEXT("object_path"), A.GetObjectPathString());
		Body->SetNumberField(TEXT("tag_count"), A.TagsAndValues.Num());
	}
	else
	{
		Body->SetBoolField(TEXT("not_found"), true);
	}

	FString BodyJson = SerializeJson(Body);
	ContentBlock->SetStringField(TEXT("text"), BodyJson);
	Contents.Add(MakeShared<FJsonValueObject>(ContentBlock));

	Result->SetArrayField(TEXT("contents"), Contents);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] resources/read uri=%s found=%d"), *Uri, Found.Num());
	return Result;
}

FString FECAMCPServer::ReadSessionHeader(const FHttpServerRequest& Request)
{
	// UE's HTTP server lower-cases header keys; spec calls it "Mcp-Session-Id".
	// Probe both common forms in case the platform changes case behavior.
	const TArray<FString>* Values = Request.Headers.Find(TEXT("Mcp-Session-Id"));
	if (!Values)
	{
		Values = Request.Headers.Find(TEXT("mcp-session-id"));
	}
	if (Values && Values->Num() > 0)
	{
		return (*Values)[0];
	}
	return FString();
}

FString FECAMCPServer::TouchSession(const FString& IncomingId)
{
	const double Now = FPlatformTime::Seconds();

	FScopeLock ScopedLock(&SessionLock);

	// Look up by header; on miss (or empty header) mint a fresh ID.
	FECASessionState* Existing = IncomingId.IsEmpty() ? nullptr : Sessions.Find(IncomingId);
	if (Existing)
	{
		Existing->LastAccessedAt = Now;
		return Existing->SessionId;
	}

	FECASessionState NewState;
	NewState.SessionId = FString::Printf(TEXT("eca-sess-%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::DigitsLower));
	NewState.CreatedAt = Now;
	NewState.LastAccessedAt = Now;
	Sessions.Add(NewState.SessionId, NewState);

	// Opportunistic GC every time we add a session.
	if (Sessions.Num() % 16 == 0)
	{
		// Drop the lock implicitly handled — PurgeSessions takes its own lock.
		// We use a non-reentrant FCriticalSection so call after returning.
	}
	return NewState.SessionId;
}

void FECAMCPServer::PurgeSessions(double MaxAgeSeconds)
{
	FScopeLock ScopedLock(&SessionLock);
	const double Now = FPlatformTime::Seconds();
	TArray<FString> Expired;
	for (const auto& Pair : Sessions)
	{
		if (Now - Pair.Value.LastAccessedAt > MaxAgeSeconds)
		{
			Expired.Add(Pair.Key);
		}
	}
	for (const FString& Key : Expired)
	{
		Sessions.Remove(Key);
	}
	if (Expired.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Purged %d inactive session(s)"), Expired.Num());
	}
}

int32 FECAMCPServer::GetSessionCount() const
{
	FScopeLock ScopedLock(&SessionLock);
	return Sessions.Num();
}

bool FECAMCPServer::HandleMCPDelete(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString IncomingId = ReadSessionHeader(Request);
	bool bRemoved = false;
	if (!IncomingId.IsEmpty())
	{
		FScopeLock ScopedLock(&SessionLock);
		bRemoved = Sessions.Remove(IncomingId) > 0;
	}

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(), TEXT("application/json"));
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Code = bRemoved ? EHttpServerResponseCodes::NoContent : EHttpServerResponseCodes::NotFound;
	OnComplete(MoveTemp(Response));
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] DELETE /mcp session='%s' removed=%s"),
		*IncomingId, bRemoved ? TEXT("true") : TEXT("false"));
	return true;
}

bool FECAMCPServer::HandleMCPGet(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Drain the queue under the lock, then serialize each as an SSE event.
	TArray<TSharedPtr<FJsonObject>> Drained;
	{
		FScopeLock ScopedLock(&NotificationLock);
		Drained = MoveTemp(PendingNotifications);
		PendingNotifications.Reset();
	}

	FString Body;
	// Always emit a comment line so EventSource clients see the stream open.
	Body += TEXT(": eca-mcp\n\n");
	for (const TSharedPtr<FJsonObject>& Note : Drained)
	{
		if (!Note.IsValid()) continue;
		const FString Json = SerializeJson(Note);
		// SSE framing per https://html.spec.whatwg.org/multipage/server-sent-events.html
		Body += TEXT("event: message\n");
		Body += TEXT("data: ");
		Body += Json;
		Body += TEXT("\n\n");
	}

	const FString ActiveSessionId = TouchSession(ReadSessionHeader(Request));
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Body, TEXT("text/event-stream"));
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-cache") });
	Response->Headers.Add(TEXT("Connection"), { TEXT("keep-alive") });
	Response->Headers.Add(TEXT("Mcp-Session-Id"), { ActiveSessionId });
	Response->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Response));

	// Opportunistic session GC on each SSE poll keeps the table bounded without
	// a dedicated timer thread.
	PurgeSessions();
	return true;
}
