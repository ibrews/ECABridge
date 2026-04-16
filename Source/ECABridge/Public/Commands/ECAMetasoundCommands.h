// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Get available MetaSound sources in the project
 */
class FECACommand_GetMetasoundSources : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_metasound_sources"); }
	virtual FString GetDescription() const override { return TEXT("Get all available MetaSound source assets in the project"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Filter sources by path (partial match)"), false },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Filter sources by name (partial match)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new MetaSound source asset
 */
class FECACommand_CreateMetasoundSource : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_metasound_source"); }
	virtual FString GetDescription() const override { return TEXT("Create a new MetaSound source asset"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path for the new asset (e.g., /Game/Audio/MS_NewSound)"), true },
			{ TEXT("output_format"), TEXT("string"), TEXT("Output format: Mono or Stereo"), false, TEXT("Stereo") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get all nodes in a MetaSound source
 */
class FECACommand_GetMetasoundNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_metasound_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Get all nodes in a MetaSound source graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("include_details"), TEXT("boolean"), TEXT("Include detailed node information (pins, values)"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a node to a MetaSound source
 */
class FECACommand_AddMetasoundNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_metasound_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a node to a MetaSound source graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("node_class"), TEXT("string"), TEXT("Node class name (e.g., 'Oscillator', 'AD Envelope', 'Trigger Repeat')"), true },
			{ TEXT("node_name"), TEXT("string"), TEXT("Optional display name for the node"), false },
			{ TEXT("position_x"), TEXT("number"), TEXT("X position in the graph"), false, TEXT("0") },
			{ TEXT("position_y"), TEXT("number"), TEXT("Y position in the graph"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a node from a MetaSound source
 */
class FECACommand_RemoveMetasoundNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_metasound_node"); }
	virtual FString GetDescription() const override { return TEXT("Remove a node from a MetaSound source graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("ID of the node to remove"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two nodes in a MetaSound source
 */
class FECACommand_ConnectMetasoundNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_metasound_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Connect two nodes in a MetaSound source graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("source_node_id"), TEXT("string"), TEXT("ID of the source node"), true },
			{ TEXT("source_pin"), TEXT("string"), TEXT("Name of the output pin on the source node"), true },
			{ TEXT("target_node_id"), TEXT("string"), TEXT("ID of the target node"), true },
			{ TEXT("target_pin"), TEXT("string"), TEXT("Name of the input pin on the target node"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Disconnect nodes in a MetaSound source
 */
class FECACommand_DisconnectMetasoundNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_metasound_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Disconnect a connection between nodes in a MetaSound source graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("source_node_id"), TEXT("string"), TEXT("ID of the source node"), true },
			{ TEXT("source_pin"), TEXT("string"), TEXT("Name of the output pin on the source node"), true },
			{ TEXT("target_node_id"), TEXT("string"), TEXT("ID of the target node"), true },
			{ TEXT("target_pin"), TEXT("string"), TEXT("Name of the input pin on the target node"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set an input value on a MetaSound node
 */
class FECACommand_SetMetasoundNodeInput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metasound_node_input"); }
	virtual FString GetDescription() const override { return TEXT("Set an input value on a MetaSound node"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("ID of the node"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name of the input pin"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set (type depends on input)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add an input to a MetaSound source
 */
class FECACommand_AddMetasoundInput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_metasound_input"); }
	virtual FString GetDescription() const override { return TEXT("Add an input to a MetaSound source"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name for the input"), true },
			{ TEXT("data_type"), TEXT("string"), TEXT("Data type (e.g., Float, Int32, Bool, Trigger, Audio)"), true },
			{ TEXT("default_value"), TEXT("any"), TEXT("Default value for the input"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove an input from a MetaSound source
 */
class FECACommand_RemoveMetasoundInput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_metasound_input"); }
	virtual FString GetDescription() const override { return TEXT("Remove an input from a MetaSound source"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name of the input to remove"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get available MetaSound node types
 */
class FECACommand_GetMetasoundNodeTypes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_metasound_node_types"); }
	virtual FString GetDescription() const override { return TEXT("Get all available MetaSound node types that can be added to a graph"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category_filter"), TEXT("string"), TEXT("Filter by category (e.g., 'Generators', 'Envelopes', 'Filters')"), false },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Filter by name (partial match)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Play/preview a MetaSound in the editor
 */
class FECACommand_PreviewMetasound : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("preview_metasound"); }
	virtual FString GetDescription() const override { return TEXT("Play/preview a MetaSound asset in the editor"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("action"), TEXT("string"), TEXT("Action: play, stop"), false, TEXT("play") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get MetaSound source inputs and outputs
 */
class FECACommand_GetMetasoundInterface : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_metasound_interface"); }
	virtual FString GetDescription() const override { return TEXT("Get the inputs and outputs of a MetaSound source"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Spawn a MetaSound player in the level
 */
class FECACommand_SpawnMetasoundPlayer : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_metasound_player"); }
	virtual FString GetDescription() const override { return TEXT("Spawn an audio component playing a MetaSound in the level"); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false },
			{ TEXT("name"), TEXT("string"), TEXT("Display name for the actor"), false },
			{ TEXT("auto_play"), TEXT("boolean"), TEXT("Whether to auto-play the sound"), false, TEXT("true") },
			{ TEXT("folder"), TEXT("string"), TEXT("Outliner folder path"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


/**
 * Auto-layout MetaSound nodes for visual clarity
 * 
 * Layout strategies:
 * - horizontal: Arrange nodes left-to-right from inputs to outputs (default)
 * - vertical: Arrange nodes top-to-bottom from inputs to outputs
 * - tree: Arrange as a tree structure
 * - compact: Minimize total graph area while maintaining readability
 * 
 * The algorithm:
 * 1. Identifies input nodes (audio inputs, parameters)
 * 2. Traverses connections forward to determine node order/depth
 * 3. Positions nodes with consistent spacing to avoid overlaps
 */
/**
 * Dump full MetaSound graph: all nodes, connections, inputs, outputs in one call.
 */
class FECACommand_DumpMetasoundGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_metasound_graph"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a complete MetaSound source to JSON: all nodes with pins and connections, source inputs/outputs, and graph structure. Combines get_metasound_nodes + get_metasound_interface in one call."); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AutoLayoutMetasoundGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("auto_layout_metasound_graph"); }
	virtual FString GetDescription() const override { return TEXT("Automatically arrange MetaSound nodes for visual clarity. Organizes by connection flow with consistent spacing."); }
	virtual FString GetCategory() const override { return TEXT("Metasound"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path to the MetaSound source asset"), true },
			{ TEXT("strategy"), TEXT("string"), TEXT("Layout strategy: horizontal (default), vertical, tree, compact"), false, TEXT("horizontal") },
			{ TEXT("spacing_x"), TEXT("number"), TEXT("Horizontal spacing between nodes (default: 300)"), false, TEXT("300") },
			{ TEXT("spacing_y"), TEXT("number"), TEXT("Vertical spacing between nodes (default: 100)"), false, TEXT("100") },
			{ TEXT("node_ids"), TEXT("array"), TEXT("Specific node IDs to layout (optional, layouts all if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
