// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new UPCGGraph asset at a content path.
 */
class FECACommand_CreatePCGGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_pcg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Create a new PCG graph asset at the given /Game path. Returns the saved asset path."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Full content path, e.g. /Game/MyFolder/MyPCGGraph"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("asset_path"), TEXT("string"), TEXT("Saved PCGGraph asset path") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Asset name (last path component)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a PCG node (settings class instance) to a graph.
 */
class FECACommand_AddPCGNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_pcg_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a new PCG node of the given settings class to a graph. Returns the new node_id (stable name within the graph). Use list_pcg_node_types to discover settings classes."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Class name of the UPCGSettings subclass (e.g. 'PCGCreatePointsSettings')"), true },
			{ TEXT("position_x"),    TEXT("number"), TEXT("Editor X position; default 0"), false, TEXT("0") },
			{ TEXT("position_y"),    TEXT("number"), TEXT("Editor Y position; default 0"), false, TEXT("0") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),     TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("node_id"),        TEXT("string"), TEXT("Stable name of the new node within the graph") },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Resolved settings class name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two pins between two nodes inside a PCG graph.
 */
class FECACommand_ConnectPCGNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_pcg_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Add an edge from one PCG node's output pin to another node's input pin. Default pins are inferred when from_pin/to_pin are omitted."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),   TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("from_node_id"), TEXT("string"), TEXT("Stable name of the upstream node"), true },
			{ TEXT("to_node_id"),   TEXT("string"), TEXT("Stable name of the downstream node"), true },
			{ TEXT("from_pin"),     TEXT("string"), TEXT("Output pin label on the upstream node; if omitted uses the first output pin"), false },
			{ TEXT("to_pin"),       TEXT("string"), TEXT("Input pin label on the downstream node; if omitted uses the first input pin"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),   TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("from_node_id"), TEXT("string"), TEXT("Upstream node id") },
			{ TEXT("from_pin"),     TEXT("string"), TEXT("Resolved upstream pin label") },
			{ TEXT("to_node_id"),   TEXT("string"), TEXT("Downstream node id") },
			{ TEXT("to_pin"),       TEXT("string"), TEXT("Resolved downstream pin label") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a property by name on a PCG node's settings object using UE reflection.
 */
class FECACommand_SetPCGNodeProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_pcg_node_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a PCG node's settings object using UE reflection. Supports scalars (string/number/bool), FVector, FName. The value is passed as JSON of the appropriate shape."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset"), true },
			{ TEXT("node_id"),       TEXT("string"), TEXT("Stable name of the target node"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("FProperty name on the settings UObject"), true },
			{ TEXT("value"),         TEXT("object"), TEXT("Value to assign. JSON shape depends on property type."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),    TEXT("string"), TEXT("Path to the PCGGraph asset") },
			{ TEXT("node_id"),       TEXT("string"), TEXT("Target node id") },
			{ TEXT("property_name"), TEXT("string"), TEXT("Property modified") },
			{ TEXT("property_type"), TEXT("string"), TEXT("Resolved C++ type") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
