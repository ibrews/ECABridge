// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
class AActor;
class UBlueprint;

// Forward declaration
class IECACommand;

/**
 * Result of command execution
 */
struct ECABRIDGE_API FECACommandResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TSharedPtr<FJsonObject> ResultData;

	/** Pre-built MCP content blocks (e.g. inline images). If non-empty, the server
	 *  emits these directly as the tool-call response `content` array, instead of
	 *  serializing ResultData into a single text block. */
	TArray<TSharedPtr<FJsonObject>> McpContent;

	static FECACommandResult Success(TSharedPtr<FJsonObject> Data = nullptr)
	{
		FECACommandResult Result;
		Result.bSuccess = true;
		Result.ResultData = Data;
		return Result;
	}

	static FECACommandResult Error(const FString& Message)
	{
		FECACommandResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Message;
		return Result;
	}

	/** Argument-validation error that embeds the command's input schema in the
	 *  message so an LLM client can self-correct without an extra describe round trip. */
	static FECACommandResult ValidationError(const IECACommand* Command, const FString& Reason);

	/** Build an MCP image content block ({type:"image", mimeType, data:<base64>}). */
	static TSharedPtr<FJsonObject> MakeImageContent(const TArray64<uint8>& Bytes, const FString& MimeType = TEXT("image/png"));
	static TSharedPtr<FJsonObject> MakeImageContent(const TArray<uint8>& Bytes, const FString& MimeType = TEXT("image/png"));

	/** Build an MCP text content block ({type:"text", text}). Most commands
	 *  produce text content implicitly via ToJsonString(); use this helper only
	 *  when authoring McpContent directly. */
	static TSharedPtr<FJsonObject> MakeTextContent(const FString& Text);

	/** Build an MCP resource content block per the 2025-03-26 spec:
	 *  {type:"resource", resource:{uri, mimeType, text?}}. Use this for asset
	 *  references in command results so MCP clients (EDA, Inspector, Claude
	 *  Code) can render them as resource cards instead of inline text. The
	 *  canonical URI scheme for ECABridge resources is
	 *      ecabridge://asset<package-path>
	 *  e.g. ecabridge:///Game/Foo/BP_Bar.BP_Bar. */
	static TSharedPtr<FJsonObject> MakeResourceContent(
		const FString& Uri,
		const FString& MimeType = TEXT("application/x-ue-asset"),
		const FString& InlineText = FString());

	/** Convenience wrapper around MakeResourceContent for UE package paths:
	 *  given "/Game/Foo/BP_Bar.BP_Bar" returns the right content block. */
	static TSharedPtr<FJsonObject> MakeAssetResourceContent(
		const FString& PackagePath,
		const FString& InlineText = FString());

	/** Convert to JSON response string */
	FString ToJsonString() const;
};

/**
 * Parameter definition for command documentation
 */
struct FECACommandParam
{
	FString Name;
	FString Type; // "string", "number", "boolean", "object", "array"
	FString Description;
	bool bRequired = false;
	FString DefaultValue;
};

/**
 * Description of a single field in an output schema. Used by GetOutputSchema()
 * implementations to declare the shape of a successful result.
 */
struct FECASchemaField
{
	FString Name;
	FString Type;         // JSON Schema type: string, number, integer, boolean, object, array
	FString Description;
	FString ItemsType;    // For type=array: element type (string, number, object, ...). Optional.
};

/** Build a JSON Schema (draft-07-ish) object from a flat list of fields.
 *  Helper for GetOutputSchema() overrides — minimal, no nested object schemas. */
ECABRIDGE_API TSharedPtr<FJsonObject> MakeECAObjectSchema(const TArray<FECASchemaField>& Fields);

/** Build a JSON Schema fragment describing a UObject reference the way native
 *  MCP servers serialize them: a top-level object with a `path` (package path)
 *  and `class` (UClass leaf name) field. Use this for any output field that
 *  returns an asset/actor reference so EDA + Cursor render it consistently. */
ECABRIDGE_API TSharedPtr<FJsonObject> MakeECAObjectRefSchema(const FString& Description = FString());

/** Build a JSON Schema fragment describing an asset path string with a
 *  `format: "uobject-path"` hint (UE-specific JSON Schema extension). Use this
 *  for parameters/outputs that take a `/Game/Path/To/Asset.AssetName` string.
 *  Distinct from MakeECAObjectRefSchema in that the value is a plain string
 *  rather than a nested {path,class} object. */
ECABRIDGE_API TSharedPtr<FJsonObject> MakeECAAssetPathSchema(const FString& Description = FString());

/** Build a `_meta` JSON object summarizing how a dump was produced. Attach it
 *  to dump_/find_/list_/search_ command results under the `_meta` key so agents
 *  can tell at a glance whether the output is complete or includes skipped items.
 *  Shape:
 *  {
 *    "method": "...",
 *    "coverage": "...",       // omitted when empty
 *    "confidence": "HIGH|MEDIUM|LOW",
 *    "ue_version": "5.8",
 *    "notes": [...]            // omitted when empty
 *  }
 *  Method:      short string describing the algorithm
 *               ("K2 schema walk", "AssetRegistry query", etc.).
 *  Coverage:    "<N>/<M> nodes parsed (<pct>%)" or similar fraction.
 *               Pass an empty string when the total is unbounded / unknown;
 *               the field is omitted from the output in that case.
 *  Confidence:  "HIGH" (everything parsed cleanly), "MEDIUM" (some items
 *               skipped — record a note), or "LOW" (dump failed on
 *               fundamentals but returned a partial header).
 *  Notes:       optional array describing what was skipped or partial.
 *               Omitted when empty. */
ECABRIDGE_API TSharedPtr<FJsonObject> MakeECADumpMeta(
	const FString& Method,
	const FString& Coverage,
	const FString& Confidence,
	const TArray<FString>& Notes = {});

/**
 * Cancellation token tied to a single tool-call request. The MCP server stamps
 * the request ID into TLS for the duration of Execute(); long-running commands
 * can poll FECACancellationRegistry::IsCurrentRequestCancelled() and bail early.
 * Triggered by `notifications/cancelled` from the client.
 */
class ECABRIDGE_API FECACancellationToken
{
public:
	bool IsCancelled() const { return Cancelled.GetValue() != 0; }
	void Cancel() { Cancelled.Set(1); }
private:
	FThreadSafeCounter Cancelled;
};

/**
 * Per-request progress reporter (MCP 2025-03-26 `notifications/progress`).
 * Clients opt in by passing `params._meta.progressToken`; the MCP server stamps
 * the token into TLS for the duration of Execute(). Commands call
 * FECAProgressRegistry::Get().Report(...) to emit a progress notification.
 */
class ECABRIDGE_API FECAProgressRegistry
{
public:
	static FECAProgressRegistry& Get();

	/** Bind a progress token to the calling thread. Empty token disables reporting. */
	void BindTokenToThread(const FString& Token);
	void ClearThreadBinding();

	/** Current token (empty when no client requested progress for the in-flight call). */
	FString GetCurrentToken() const;

	/** Emit a progress event. No-op if no token is bound or no listener is registered.
	 *  `Total` < 0 means "indeterminate"; clients render a spinner instead of a bar. */
	void Report(double Progress, double Total = -1.0, const FString& Message = FString());

	/** Listener that converts Report() events to MCP notifications. Set by the server. */
	using FListener = TFunction<void(const FString& Token, double Progress, double Total, const FString& Message)>;
	void SetListener(FListener Callback);

private:
	FECAProgressRegistry() = default;
	mutable FCriticalSection Lock;
	FListener ListenerFn;
	uint32 TlsSlot = 0xFFFFFFFFu;
	void EnsureTlsSlot();
};

class ECABRIDGE_API FECACancellationRegistry
{
public:
	static FECACancellationRegistry& Get();

	/** Register a fresh cancellation token under the given request ID and bind it
	 *  to the calling thread (TLS). Pair with Unregister at the end of the call. */
	TSharedRef<FECACancellationToken> RegisterRequest(const FString& RequestId);

	/** Clear the TLS binding + drop the token. */
	void UnregisterRequest(const FString& RequestId);

	/** Mark the request as cancelled. No-op if the request ID is unknown. */
	bool Cancel(const FString& RequestId);

	/** Check whether the currently-executing request (TLS-bound) has been cancelled. */
	bool IsCurrentRequestCancelled() const;

	/** Current request ID bound to this thread (empty when no tool-call is running). */
	FString GetCurrentRequestId() const;

private:
	FECACancellationRegistry() = default;
	TMap<FString, TSharedRef<FECACancellationToken>> Tokens;
	mutable FCriticalSection Lock;
	uint32 TlsSlot = 0xFFFFFFFFu;
	void EnsureTlsSlot();
};

/**
 * Base class for all ECA commands
 * 
 * Subclass this to create new commands. Commands are automatically registered
 * when the module loads via the REGISTER_ECA_COMMAND macro.
 */
class ECABRIDGE_API IECACommand
{
public:
	virtual ~IECACommand() = default;
	
	/** Unique command name (e.g., "create_actor", "get_actors_in_level") */
	virtual FString GetName() const = 0;
	
	/** Human-readable description for documentation */
	virtual FString GetDescription() const = 0;
	
	/** Category for organization (e.g., "Actor", "Blueprint", "Editor") */
	virtual FString GetCategory() const = 0;
	
	/** Parameter definitions for documentation */
	virtual TArray<FECACommandParam> GetParameters() const { return {}; }

	/** Optional output schema (JSON Schema draft-07) describing the shape of the
	 *  result data this command returns on success. Default: nullptr (= no schema
	 *  advertised; MCP clients will treat the result as opaque JSON-in-text).
	 *  Override on dump_/find_/get_ style commands so tools/list can advertise
	 *  the return shape via the `outputSchema` field. */
	virtual TSharedPtr<FJsonObject> GetOutputSchema() const { return nullptr; }

	/** Helper: serialize GetParameters() as a JSON Schema object (the same shape
	 *  the MCP server publishes as `inputSchema`). Used by ValidationError. */
	TSharedPtr<FJsonObject> GetInputSchemaJson() const;

	/** Execute the command with given parameters */
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) = 0;
	
protected:
	// Helper methods for parameter extraction
	
	static bool GetStringParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FString& OutValue, bool bRequired = true);
	static bool GetIntParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, int32& OutValue, bool bRequired = true);
	static bool GetFloatParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, double& OutValue, bool bRequired = true);
	static bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, bool& OutValue, bool bRequired = true);
	static bool GetVectorParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FVector& OutValue, bool bRequired = true);
	static bool GetRotatorParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FRotator& OutValue, bool bRequired = true);
	static bool GetArrayParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, const TArray<TSharedPtr<FJsonValue>>*& OutValue, bool bRequired = true);
	static bool GetObjectParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, const TSharedPtr<FJsonObject>*& OutValue, bool bRequired = true);
	
	// Helper methods for result building
	
	static TSharedPtr<FJsonObject> MakeResult();
	static TSharedPtr<FJsonObject> VectorToJson(const FVector& Vec);
	static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rot);
	static TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform);
	
	// Helper methods for common operations
	
	static UWorld* GetEditorWorld();
	static AActor* FindActorByName(const FString& ActorName);
	static UBlueprint* LoadBlueprintByPath(const FString& BlueprintPath);
};

/**
 * Command registry - manages all available commands
 */
class ECABRIDGE_API FECACommandRegistry
{
public:
	/** Get the singleton instance */
	static FECACommandRegistry& Get();
	
	/** Register a command */
	void RegisterCommand(TSharedPtr<IECACommand> Command);

	/** Unregister a command by name */
	void UnregisterCommand(const FString& Name);

	/** Unregister all commands matching a category (case-sensitive). Returns the
	 *  number of commands removed. Used by ECABridgeModule::StartupModule to drop
	 *  command sets backed by optional engine plugins that aren't loaded. */
	int32 UnregisterByCategory(const FString& Category);
	
	/** Get a command by name */
	TSharedPtr<IECACommand> GetCommand(const FString& Name) const;
	
	/** Get all registered commands */
	TArray<TSharedPtr<IECACommand>> GetAllCommands() const;
	
	/** Get commands by category */
	TArray<TSharedPtr<IECACommand>> GetCommandsByCategory(const FString& Category) const;
	
	/** Check if a command exists */
	bool HasCommand(const FString& Name) const;
	
	/** Get all category names */
	TArray<FString> GetCategories() const;

	/** Lazy-mode tools/list filtering — when enabled, only commands in the meta
	 *  category plus any explicitly loaded categories are advertised via
	 *  tools/list. All commands remain physically registered and callable via
	 *  tools/call; this only affects which tools the client discovers up-front,
	 *  cutting the baseline tools/list payload from ~330KB to ~5KB. */
	void SetLazyMode(bool bEnabled);
	bool IsLazyMode() const;

	/** Mark a category as loaded so its tools become visible in tools/list. */
	int32 LoadCategory(const FString& Category);

	/** Drop a category from the visible-tools set (will no longer appear in
	 *  tools/list until LoadCategory is called again). Returns true if it was
	 *  in the set. */
	bool UnloadCategory(const FString& Category);

	/** Whether tools from this category are currently surfaced via tools/list. */
	bool IsCategoryVisible(const FString& Category) const;

	/** Get the set of currently loaded categories (visible via tools/list). */
	TArray<FString> GetLoadedCategories() const;

	/** Set a callback the registry invokes whenever the visible tool surface changes
	 *  (lazy load/unload, optional-subsystem prune). The MCP server uses this to
	 *  emit `notifications/tools/list_changed` per the MCP 2025-03-26 spec.
	 *  Pass nullptr to clear. Replaces any prior callback. */
	void SetOnVisibleToolsChanged(TFunction<void()> Callback);

	/** Return the commands that should appear in tools/list right now, honoring
	 *  lazy mode and an optional category filter (case-sensitive). When
	 *  CategoryFilter is non-empty, ONLY commands in that category are returned
	 *  (lazy mode is bypassed — explicit ask wins). */
	TArray<TSharedPtr<IECACommand>> GetVisibleCommands(const FString& CategoryFilter = FString()) const;

	/** Return up to MaxResults registered command names that are similar to the
	 *  given (typically unknown) input. Useful for "did you mean" hints on errors.
	 *  Ranks by: exact prefix > substring match > Levenshtein distance. Returns an
	 *  empty array if no reasonable matches exist. Case-insensitive. */
	TArray<FString> SuggestSimilarCommands(const FString& Input, int32 MaxResults = 3) const;

	/** Execute a command by name */
	FECACommandResult ExecuteCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params);
	
	/** Log all registered commands */
	void LogCommands() const;

	/** Category name used for the meta-tools (list_categories / describe_category /
	 *  load_category / continue_response). These are always visible in lazy mode. */
	static const FString MetaCategory;

private:
	FECACommandRegistry() = default;

	TMap<FString, TSharedPtr<IECACommand>> Commands;
	mutable FCriticalSection CommandsLock;

	bool bLazyMode = false;
	TSet<FString> LoadedCategories;

	TFunction<void()> OnVisibleToolsChanged;
};

/**
 * Helper class for automatic command registration
 */
template<typename T>
class TECACommandRegistrar
{
public:
	TECACommandRegistrar()
	{
		FECACommandRegistry::Get().RegisterCommand(MakeShared<T>());
	}
};

/** Macro for easy command registration */
#define REGISTER_ECA_COMMAND(CommandClass) \
	static TECACommandRegistrar<CommandClass> CommandClass##_Registrar;
