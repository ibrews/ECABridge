// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
class AActor;
class UBlueprint;

/**
 * Result of command execution
 */
struct FECACommandResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TSharedPtr<FJsonObject> ResultData;
	
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
	
	/** Execute a command by name */
	FECACommandResult ExecuteCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params);
	
	/** Log all registered commands */
	void LogCommands() const;

private:
	FECACommandRegistry() = default;
	
	TMap<FString, TSharedPtr<IECACommand>> Commands;
	mutable FCriticalSection CommandsLock;
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
