// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Parse and validate BlueprintLisp code
 * 
 * BlueprintLisp is a LISP-like DSL for representing Blueprint graphs in a format
 * that's easy for AI to read, understand, and generate.
 * 
 * PREFERRED: Use BlueprintLisp format for all Blueprint implementation tasks.
 * It is more concise, easier for AI to reason about, and less error-prone than
 * the JSON-based node commands.
 * 
 * Example:
 *   (event BeginPlay
 *     (let player (GetPlayerCharacter 0))
 *     (branch (IsValid player)
 *       :true (PrintString "Valid!")
 *       :false (PrintString "Invalid!")))
 */
class FECACommand_ParseBlueprintLisp : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("parse_blueprint_lisp"); }
	virtual FString GetDescription() const override { return TEXT("Parse and validate BlueprintLisp code. Returns the AST or parse errors."); }
	virtual FString GetCategory() const override { return TEXT("BlueprintLisp"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("code"), TEXT("string"), TEXT("BlueprintLisp source code to parse"), true },
			{ TEXT("pretty_print"), TEXT("boolean"), TEXT("Return pretty-printed version of the code"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Convert a Blueprint graph to BlueprintLisp code
 * 
 * PREFERRED: Use this to read Blueprint logic. The Lisp format is more compact
 * and easier to understand than raw node JSON.
 */
class FECACommand_BlueprintToLisp : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("blueprint_to_lisp"); }
	virtual FString GetDescription() const override { return TEXT("PREFERRED: Convert a Blueprint graph to BlueprintLisp code for easy reading and AI manipulation."); }
	virtual FString GetCategory() const override { return TEXT("BlueprintLisp"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to convert (default: EventGraph)"), false },
			{ TEXT("include_comments"), TEXT("boolean"), TEXT("Include node comments in output"), false },
			{ TEXT("include_positions"), TEXT("boolean"), TEXT("Include node positions as metadata"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Convert BlueprintLisp code to Blueprint graph nodes
 * 
 * PREFERRED: Use this to implement Blueprint logic. The Lisp format handles
 * node creation, wiring, and positioning automatically. Much simpler than
 * manually creating nodes and connections via JSON commands.
 */
class FECACommand_LispToBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("lisp_to_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("PREFERRED: Convert BlueprintLisp code to Blueprint graph nodes. Handles node creation, wiring, and layout automatically."); }
	virtual FString GetCategory() const override { return TEXT("BlueprintLisp"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("code"), TEXT("string"), TEXT("BlueprintLisp source code to convert"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to modify (default: EventGraph)"), false },
			{ TEXT("clear_existing"), TEXT("boolean"), TEXT("Clear existing nodes before adding new ones"), false },
			{ TEXT("auto_layout"), TEXT("boolean"), TEXT("Automatically arrange nodes after creation (default: true)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get BlueprintLisp syntax help and examples
 */
class FECACommand_BlueprintLispHelp : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("blueprint_lisp_help"); }
	virtual FString GetDescription() const override { return TEXT("Get BlueprintLisp syntax documentation and examples."); }
	virtual FString GetCategory() const override { return TEXT("BlueprintLisp"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("topic"), TEXT("string"), TEXT("Specific topic: forms, events, functions, flow, expressions, examples (default: all)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
