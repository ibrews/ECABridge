// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commands/ECACommand.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ECAProgrammaticCommands.generated.h"

/**
 * Python-facing static helper exposed to the UE Python interpreter as
 * `unreal.ECABridgeProgrammatic`. The single UFUNCTION lets a Python script
 * invoke any registered ECABridge command and receive the JSON-serialized result.
 *
 * Translates to Python as `unreal.ECABridgeProgrammatic.execute_tool_json(name, args_json)`.
 */
UCLASS()
class ECABRIDGE_API UECABridgeProgrammatic : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Execute a registered ECABridge command by name. ArgsJson is a JSON-string
	 *  representation of the argument object. Returns a JSON-string
	 *  {success: bool, result?: object, error?: string}. */
	UFUNCTION(BlueprintCallable, Category = "ECABridge")
	static FString ExecuteToolJson(const FString& ToolName, const FString& ArgsJson);

	/** Sink for the return value of an execute_script user's `run() -> dict`. The
	 *  preamble footer calls this with a JSON-serialized version of run()'s return.
	 *  FECACommand_ExecuteScript clears it before each invocation and reads it
	 *  back into command_result after Python returns. */
	UFUNCTION(BlueprintCallable, Category = "ECABridge")
	static void SetCommandResult(const FString& JsonValue);

	/** Read and clear the slot set by SetCommandResult. */
	static FString ConsumeCommandResult();
};

/**
 * Run a multi-step Python script in the editor's Python environment, with a
 * preamble that exposes execute_tool(toolset, tool, json_input) -> dict so the
 * script can chain any number of ECABridge tool calls in a single round trip.
 *
 * SECURITY: This is NOT a sandbox. UE's Python runtime has full editor access -
 * filesystem, asset registry, console commands, the works. Use only with trusted
 * AI agents. There is no allow-list of imports or restricted namespace.
 */
class FECACommand_ExecuteScript : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("execute_script"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Run a Python script in the editor's interpreter with an injected `execute_tool(toolset, tool, json_input)` helper that calls any registered ECABridge command server-side. Lets an agent chain dozens of introspection calls in one round trip. NOT a security sandbox - the script has full UE Python access; use with trusted agents only.");
	}
	virtual FString GetCategory() const override { return TEXT("Programmatic"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("script"), TEXT("string"), TEXT("Python source. The preamble exposes `execute_tool(toolset, tool, json_input)` returning a parsed dict; use it to chain ECABridge calls. Stdout/stderr captured in `log_output`."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),     TEXT("boolean"), TEXT("True if Python execution completed without an unhandled exception") },
			{ TEXT("command_result"), TEXT("string"), TEXT("Final expression result from ExecPythonCommandEx (if any)") },
			{ TEXT("log_output"),  TEXT("array"),   TEXT("Stdout/stderr lines captured during execution"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Document the Python execution environment: what helpers are exposed, what
 * stdlib modules are available (everything - no allow-list), and the list of
 * registered ECABridge tool names that the script's execute_tool() helper can
 * invoke. Mirrors the native UE 5.8 MCP plugin's `get_execution_environment`.
 */
class FECACommand_GetExecutionEnvironment : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_execution_environment"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Describe the Python execution environment for execute_script: injected helpers, available modules, and the full list of ECABridge tool names invocable via execute_tool(). Call this first before writing a script.");
	}
	virtual FString GetCategory() const override { return TEXT("Programmatic"); }

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("instructions"), TEXT("string"),  TEXT("Human-readable instructions for writing a script") },
			{ TEXT("preamble"),     TEXT("string"),  TEXT("The Python preamble injected before the user script (for reference)") },
			{ TEXT("helpers"),      TEXT("array"),   TEXT("Names of injected helper callables (execute_tool, etc.)"), TEXT("string") },
			{ TEXT("allowed_modules"), TEXT("string"), TEXT("Module policy. ECABridge does NOT sandbox imports; any stdlib or installed package is reachable.") },
			{ TEXT("security_notice"), TEXT("string"), TEXT("Warning that execute_script is not a security boundary.") },
			{ TEXT("commands"),     TEXT("array"),   TEXT("All registered ECABridge command names callable via execute_tool"), TEXT("string") },
			{ TEXT("command_count"),TEXT("integer"), TEXT("Number of registered commands") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a property on a class's CDO (ClassDefaultObject) with read-back verification.
 * Sets the property, reads it back, and compares. On mismatch, returns an error
 * with a hint about snake_case vs PascalCase property naming issues.
 */
class FECACommand_SetUPropertyChecked : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_uproperty_checked"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Set a UPROPERTY on a class's CDO and verify the write succeeded by reading back. Returns MCP error with hint if the value doesn't stick (common when Python attribute names use snake_case instead of PascalCase).");
	}
	virtual FString GetCategory() const override { return TEXT("Programmatic"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_path"), TEXT("string"), TEXT("Full class path (e.g., '/Script/Engine.Character' or '/Game/MyBP.MyBP_C')"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("UPROPERTY name in PascalCase (e.g., 'AutoPossessAI' not 'auto_possess_ai')"), true },
			{ TEXT("property_value"), TEXT("string"), TEXT("Value to set (will be parsed according to the property's type)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("class_path"), TEXT("string"), TEXT("The class that was modified") },
			{ TEXT("property_name"), TEXT("string"), TEXT("The property that was set") },
			{ TEXT("expected"), TEXT("string"), TEXT("The value we attempted to set") },
			{ TEXT("actual"), TEXT("string"), TEXT("The value we read back (should match expected)") },
			{ TEXT("verified"), TEXT("boolean"), TEXT("True if read-back matched the set value") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
