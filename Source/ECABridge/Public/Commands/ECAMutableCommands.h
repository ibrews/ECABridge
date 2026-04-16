// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── get_co_info ────────────────────────────────────────────────
// Returns high-level info about a Customizable Object: its nodes, parameters,
// component count, and compilation state.
class FECACommand_GetCOInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_co_info"); }
	virtual FString GetDescription() const override { return TEXT("Get info about a Customizable Object: nodes, parameters, components, compile state"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object (e.g. /Game/Character/CO_Character)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_co_node_pins ───────────────────────────────────────────
// Shows all pins on a specific node in the CO graph.
class FECACommand_GetCONodePins : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_co_node_pins"); }
	virtual FString GetDescription() const override { return TEXT("List all pins on a Customizable Object graph node (names, types, directions, connections)"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("Name/title of the node (from get_co_info)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_co ──────────────────────────────────────────────────
// Creates a new Customizable Object asset.
class FECACommand_CreateCO : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_co"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Customizable Object asset at the given path"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Package directory (e.g. /Game/Character)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new CO asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── add_co_node ────────────────────────────────────────────────
// Adds a node to the CO graph. Discovers available types at runtime.
class FECACommand_AddCONode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_co_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a node to a Customizable Object graph. Pass node_class (e.g. CustomizableObjectNodeComponentMesh) or use list_co_node_types to discover available types"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("node_class"), TEXT("string"), TEXT("Class name of the node to create (e.g. CustomizableObjectNodeComponentMesh)"), true },
			{ TEXT("pos_x"), TEXT("number"), TEXT("X position in graph"), false, TEXT("0") },
			{ TEXT("pos_y"), TEXT("number"), TEXT("Y position in graph"), false, TEXT("0") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── list_co_node_types ─────────────────────────────────────────
// Lists all available CO node types that can be created.
class FECACommand_ListCONodeTypes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_co_node_types"); }
	virtual FString GetDescription() const override { return TEXT("List all available Customizable Object node types that can be added to a CO graph"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_co_node_property ───────────────────────────────────────
// Sets a UPROPERTY on a CO node via UE reflection.
class FECACommand_SetCONodeProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_co_node_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a Customizable Object graph node using UE reflection"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("Name/title of the target node"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("UPROPERTY name to set"), true },
			{ TEXT("property_value"), TEXT("any"), TEXT("Value to set (string, number, bool, or asset path for object refs)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── connect_co_nodes ───────────────────────────────────────────
// Wires two nodes together via their pins.
class FECACommand_ConnectCONodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_co_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Connect an output pin on one CO node to an input pin on another"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("source_node"), TEXT("string"), TEXT("Name of the source node"), true },
			{ TEXT("source_pin"), TEXT("string"), TEXT("Name of the output pin on the source node"), true },
			{ TEXT("target_node"), TEXT("string"), TEXT("Name of the target node"), true },
			{ TEXT("target_pin"), TEXT("string"), TEXT("Name of the input pin on the target node"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── disconnect_co_nodes ────────────────────────────────────────
// Breaks connections on CO node pins.
class FECACommand_DisconnectCONodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_co_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Break connections on a Customizable Object node pin"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("Name of the node"), true },
			{ TEXT("pin_name"), TEXT("string"), TEXT("Name of the pin to disconnect"), true },
			{ TEXT("target_node"), TEXT("string"), TEXT("If set, only break the link to this specific node"), false },
			{ TEXT("target_pin"), TEXT("string"), TEXT("If set, only break the link to this specific pin"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── remove_co_node ─────────────────────────────────────────────
// Removes a node from the CO graph.
class FECACommand_RemoveCONode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_co_node"); }
	virtual FString GetDescription() const override { return TEXT("Remove a node from a Customizable Object graph"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("Name/title of the node to remove"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── compile_co ─────────────────────────────────────────────────
// Compiles a Customizable Object.
class FECACommand_CompileCO : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("compile_co"); }
	virtual FString GetDescription() const override { return TEXT("Compile a Customizable Object"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true },
			{ TEXT("optimization_level"), TEXT("string"), TEXT("none, low, medium, high, max (default: none)"), false, TEXT("none") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_co_instance_params ─────────────────────────────────────
// Gets the runtime parameters of a CO instance for tweaking character appearance.
class FECACommand_GetCOInstanceParams : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_co_instance_params"); }
	virtual FString GetDescription() const override { return TEXT("Get the runtime parameter values of a Customizable Object Instance (for character customization)"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the CO or CO Instance"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── save_co ────────────────────────────────────────────────────
class FECACommand_SaveCO : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_co"); }
	virtual FString GetDescription() const override { return TEXT("Save a Customizable Object asset to disk"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the Customizable Object"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_co_actor ─────────────────────────────────────────────
class FECACommand_SpawnCOActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_co_actor"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a Customizable Skeletal Mesh Actor in the level using a compiled CO"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("object_path"), TEXT("string"), TEXT("Asset path of the compiled Customizable Object"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Display name for the actor"), false, TEXT("CustomCharacter") },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_co_instance_param ──────────────────────────────────────
class FECACommand_SetCOInstanceParam : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_co_instance_param"); }
	virtual FString GetDescription() const override { return TEXT("Set a runtime parameter on a CO instance actor in the level"); }
	virtual FString GetCategory() const override { return TEXT("Mutable"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor in the level"), true },
			{ TEXT("param_name"), TEXT("string"), TEXT("Name of the parameter to set"), true },
			{ TEXT("param_value"), TEXT("any"), TEXT("Value: color {r,g,b,a}, int index, float, bool, or enum string"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_metahuman ──────────────────────────────────────────
// Creates a new MetaHuman Character asset using runtime class discovery.
// Gracefully fails if the MetaHuman plugins are not enabled.
class FECACommand_CreateMetaHuman : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_metahuman"); }
	virtual FString GetDescription() const override { return TEXT("Create a new MetaHuman Character asset (requires MetaHuman plugin)"); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Package directory (e.g. /Game/Characters)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new MetaHuman Character asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
