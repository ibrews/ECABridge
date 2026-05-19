// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Enumerate every concrete UPCGSettings subclass loaded in the current process.
 * This is the catalogue add_pcg_node draws settings_class names from.
 */
class FECACommand_ListPCGNodeTypes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_pcg_node_types"); }
	virtual FString GetDescription() const override { return TEXT("Return every concrete UPCGSettings subclass (settings_class names) usable by add_pcg_node. Optional name_filter substring-matches on the class name."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_filter"), TEXT("string"), TEXT("Substring filter on class name (case-insensitive)"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("node_types"), TEXT("array"),   TEXT("{class_name, default_title, parent_class}"), TEXT("object") },
			{ TEXT("count"),      TEXT("integer"), TEXT("Number of matching settings classes") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect the default input/output pins of a UPCGSettings subclass.
 */
class FECACommand_GetPCGNodePins : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pcg_node_pins"); }
	virtual FString GetDescription() const override { return TEXT("Return the default input and output pins of a UPCGSettings subclass (the pin shape a freshly-added node will have). Use the pin labels with connect_pcg_nodes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("settings_class"), TEXT("string"), TEXT("UPCGSettings subclass name (e.g. 'PCGCreatePointsSettings')"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("settings_class"), TEXT("string"), TEXT("Resolved settings class name") },
			{ TEXT("input_pins"),     TEXT("array"),  TEXT("Default input pins: {label, allowed_types, allow_multiple_data, allow_multiple_connections}"), TEXT("object") },
			{ TEXT("output_pins"),    TEXT("array"),  TEXT("Default output pins"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
