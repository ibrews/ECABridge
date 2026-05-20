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
