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
	
	// Main MCP endpoint (POST for messages, OPTIONS for CORS)
	MCPRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				return HandleCORSPreflight(Request, OnComplete);
			}
			return HandleMCPPost(Request, OnComplete);
		})
	);

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

	// Unbind routes
	if (HttpRouter.IsValid())
	{
		HttpRouter->UnbindRoute(MCPRouteHandle);
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
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(), TEXT("application/json"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		Response->Code = EHttpServerResponseCodes::Accepted;
		OnComplete(MoveTemp(Response));
		return true;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP Response: %s"), *ResponseStr.Left(500));
	
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
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
		Result = HandleToolsList();
	}
	else if (Method == TEXT("tools/call"))
	{
		Result = HandleToolsCall(Params);
	}
	else if (Method == TEXT("notifications/initialized"))
	{
		// Client notification - no response needed
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Client initialized notification received"));
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
	ToolsCap->SetBoolField(TEXT("listChanged"), false);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);

	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	UE_LOG(LogTemp, Log, TEXT("[ECABridge] MCP client initialized"));

	return Result;
}

TSharedPtr<FJsonObject> FECAMCPServer::HandleToolsList()
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), BuildToolDefinitions());
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Returned %d tools"), FECACommandRegistry::Get().GetAllCommands().Num());
	
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
		TextContent->SetStringField(TEXT("text"), ResultText);
	}
	else
	{
		Result->SetBoolField(TEXT("isError"), true);
		TextContent->SetStringField(TEXT("text"), CommandResult.ErrorMessage);
	}
	
	Content.Add(MakeShared<FJsonValueObject>(TextContent));
	Result->SetArrayField(TEXT("content"), Content);

	return Result;
}

TArray<TSharedPtr<FJsonValue>> FECAMCPServer::BuildToolDefinitions()
{
	TArray<TSharedPtr<FJsonValue>> Tools;

	for (const TSharedPtr<IECACommand>& Command : FECACommandRegistry::Get().GetAllCommands())
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
