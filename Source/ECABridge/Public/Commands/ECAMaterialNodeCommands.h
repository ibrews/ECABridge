// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Add a material expression node to a material graph
 */
class FECACommand_AddMaterialNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_material_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a material expression node to a material graph"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_type"), TEXT("string"), TEXT("Type of node: TextureSample, TextureSampleParameter2D, Constant, Constant3Vector, Constant4Vector, VectorParameter, ScalarParameter, Multiply, Add, Lerp, ComponentMask, Append, etc."), true },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Parameter name (for parameter types)"), false },
			{ TEXT("default_value"), TEXT("any"), TEXT("Default value (scalar, vector {r,g,b,a}, or texture path)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two material expression nodes
 */
class FECACommand_ConnectMaterialNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_material_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Connect two material expression nodes by their outputs/inputs"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("source_node_id"), TEXT("string"), TEXT("GUID of the source expression node"), true },
			{ TEXT("source_output"), TEXT("string"), TEXT("Output name or index (RGB, R, G, B, A, or 0-4)"), false, TEXT("0") },
			{ TEXT("target_node_id"), TEXT("string"), TEXT("GUID of the target expression node, or 'material' for material inputs"), true },
			{ TEXT("target_input"), TEXT("string"), TEXT("Input name (BaseColor, Metallic, Roughness, Normal, EmissiveColor, etc. for material, or input index/name for nodes)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get information about nodes in a material
 */
class FECACommand_GetMaterialNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_material_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Get all expression nodes in a material with their connections"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_type_filter"), TEXT("string"), TEXT("Filter by node type (e.g., TextureSample)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get detailed pin information for a material node
 */
class FECACommand_GetMaterialNodeInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_material_node_info"); }
	virtual FString GetDescription() const override { return TEXT("Get detailed information about a material expression node including its inputs and outputs"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete a material expression node
 */
class FECACommand_DeleteMaterialNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_material_node"); }
	virtual FString GetDescription() const override { return TEXT("Delete a material expression node by its GUID"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node to delete"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Disconnect a material node's inputs or outputs
 */
class FECACommand_DisconnectMaterialNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_material_node"); }
	virtual FString GetDescription() const override { return TEXT("Disconnect all or specific connections on a material expression node"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Specific input to disconnect (optional, disconnects all if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a material node's property value
 */
class FECACommand_SetMaterialNodeProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_node_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property value on a material expression node"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("Name of the property to set"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Batch create nodes and connections in a material
 */
class FECACommand_BatchEditMaterialNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("batch_edit_material_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Create multiple material nodes and wire them together in a single atomic operation"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("nodes"), TEXT("array"), TEXT("Array of node definitions. Each node must have 'temp_id' and 'node_type'. Optional: parameter_name, default_value, node_position {x, y}"), true },
			{ TEXT("connections"), TEXT("array"), TEXT("Array of connection definitions: {source_node, source_output, target_node, target_input}. Use temp_id or existing node GUIDs. target_node can be 'material' to connect to material inputs."), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List available material expression types
 */
class FECACommand_ListMaterialExpressionTypes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_material_expression_types"); }
	virtual FString GetDescription() const override { return TEXT("List all available material expression types that can be added to materials"); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category_filter"), TEXT("string"), TEXT("Filter by category (e.g., Math, Texture, Parameters, Utility)"), false },
			{ TEXT("search"), TEXT("string"), TEXT("Search filter for expression names"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get material compilation errors
 */
class FECACommand_GetMaterialErrors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_material_errors"); }
	virtual FString GetDescription() const override { return TEXT("Get compilation errors for a material. Call this after making changes to check for issues."); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set an input name on a Custom material expression node
 */
class FECACommand_SetCustomNodeInputName : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_custom_node_input_name"); }
	virtual FString GetDescription() const override { return TEXT("Set the name of an input on a Custom material expression node. Creates the input if it doesn't exist at the specified index."); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the Custom node"), true },
			{ TEXT("input_index"), TEXT("number"), TEXT("Index of the input to set (0-based)"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name for the input"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set material properties using reflection (supports any UMaterial UPROPERTY)
 */
class FECACommand_SetMaterialProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_property"); }
	virtual FString GetDescription() const override { return TEXT("Set any UMaterial property using reflection. Supports snake_case or PascalCase names. Use get_material_property to discover available properties."); }
	virtual FString GetCategory() const override { return TEXT("Material"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Property name (e.g., BlendMode, bEnableTessellation, TwoSided, bUsedWithNanite, OpacityMaskClipValue). Supports snake_case aliases."), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set. For enums use the enum value name (e.g., 'BLEND_Masked'). For bools use true/false."), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get material properties using reflection (supports any UMaterial UPROPERTY)
 */
class FECACommand_GetMaterialProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_material_property"); }
	virtual FString GetDescription() const override { return TEXT("Get UMaterial properties using reflection. Returns all editable properties if no specific property is requested."); }
	virtual FString GetCategory() const override { return TEXT("Material"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Specific property name to get (optional - returns all editable properties if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


/**
 * Auto-layout material nodes for visual clarity
 * 
 * Layout strategies:
 * - horizontal: Arrange nodes left-to-right from material inputs (default)
 * - vertical: Arrange nodes top-to-bottom from material inputs
 * - tree: Arrange as a tree structure from material input connections
 * - compact: Minimize total graph area while maintaining readability
 * 
 * The algorithm:
 * 1. Identifies root nodes (material input connections: BaseColor, Normal, etc.)
 * 2. Traverses expression connections backward to determine node order/depth
 * 3. Positions nodes with consistent spacing to avoid overlaps
 * 4. Aligns data-flow nodes near their consumers
 */
class FECACommand_AutoLayoutMaterialGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("auto_layout_material_graph"); }
	virtual FString GetDescription() const override { return TEXT("Automatically arrange material expression nodes for visual clarity. Organizes by connection flow with consistent spacing."); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("strategy"), TEXT("string"), TEXT("Layout strategy: horizontal (default), vertical, tree, compact"), false, TEXT("horizontal") },
			{ TEXT("spacing_x"), TEXT("number"), TEXT("Horizontal spacing between nodes (default: 300)"), false, TEXT("300") },
			{ TEXT("spacing_y"), TEXT("number"), TEXT("Vertical spacing between nodes (default: 150)"), false, TEXT("150") },
			{ TEXT("node_ids"), TEXT("array"), TEXT("Specific node IDs to layout (optional, layouts all if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename parameter groups in a material
 * 
 * This command renames all material parameter expressions that belong to a specific group.
 * Useful for organizing parameters in material instances by renaming groups in bulk.
 * 
 * Examples:
 * - Rename "None" group to "Base Properties" 
 * - Rename "Textures" to "Surface Maps"
 * - Use empty string "" for the default/unnamed group (None)
 */
class FECACommand_RenameParameterGroup : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rename_parameter_group"); }
	virtual FString GetDescription() const override { return TEXT("Rename a parameter group in a material. All parameters belonging to the old group will be moved to the new group."); }
	virtual FString GetCategory() const override { return TEXT("Material"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true },
			{ TEXT("old_group_name"), TEXT("string"), TEXT("Current group name to rename (use empty string '' for the default/None group)"), true },
			{ TEXT("new_group_name"), TEXT("string"), TEXT("New group name (use empty string '' to move to default/None group)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List all parameter groups in a material
 * 
 * Returns all unique group names used by parameters in the material,
 * along with a count of parameters in each group.
 */
class FECACommand_ListParameterGroups : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_parameter_groups"); }
	virtual FString GetDescription() const override { return TEXT("List all parameter groups in a material with counts of parameters in each group."); }
	virtual FString GetCategory() const override { return TEXT("Material"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump complete material graph in one call
 *
 * Returns all expression nodes with GUIDs, types, positions, and connections,
 * material-level properties (BlendMode, ShadingModel, TwoSided, etc.),
 * material input connections, and compilation errors.
 */
class FECACommand_DumpMaterialGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_material_graph"); }
	virtual FString GetDescription() const override { return TEXT("Dump complete material graph: all nodes, connections, material properties, material inputs, and compilation errors in one call."); }
	virtual FString GetCategory() const override { return TEXT("Material Node"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the Material asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
