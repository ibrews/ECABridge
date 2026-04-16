// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Add an event node to a Blueprint's event graph
 */
class FECACommand_AddBlueprintEventNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_event_node"); }
	virtual FString GetDescription() const override { return TEXT("Add an event node (BeginPlay, Tick, etc.) to a Blueprint's event graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("event_name"), TEXT("string"), TEXT("Event name: ReceiveBeginPlay, ReceiveTick, ReceiveActorBeginOverlap, etc."), true },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add an input action event node to a Blueprint
 */
class FECACommand_AddBlueprintInputActionNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_input_action_node"); }
	virtual FString GetDescription() const override { return TEXT("Add an input action event node to a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("action_name"), TEXT("string"), TEXT("Name of the input action"), true },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a function call node to a Blueprint
 */
class FECACommand_AddBlueprintFunctionNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_function_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a function call node to a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("function_name"), TEXT("string"), TEXT("Name of the function to call (e.g., PrintString, SetActorLocation)"), true },
			{ TEXT("target"), TEXT("string"), TEXT("Target for the function call (self, component name, or class)"), false, TEXT("self") },
			{ TEXT("target_class"), TEXT("string"), TEXT("Class containing the function (e.g., KismetSystemLibrary, Actor)"), false },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to add to (EventGraph or function name)"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a macro node (ForLoop, WhileLoop, ForEachLoop, etc.) to a Blueprint
 * 
 * Common macros: ForLoop, ForLoopWithBreak, WhileLoop, ForEachLoop, ForEachLoopWithBreak,
 * Gate, MultiGate, DoOnce, DoN, FlipFlop, Sequence, IsValid
 * 
 * Example: {"blueprint_path":"/Game/BP_Test", "macro_name":"ForLoop", "node_position":{"x":200,"y":100}}
 */
class FECACommand_AddBlueprintMacroNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_macro_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a macro node to a Blueprint. Common macros: ForLoop, ForLoopWithBreak, WhileLoop, ForEachLoop, Gate, MultiGate, DoOnce, DoN, FlipFlop, Sequence, IsValid. Example: {\"macro_name\":\"ForLoop\"}"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("macro_name"), TEXT("string"), TEXT("Name of the macro: ForLoop, WhileLoop, ForEachLoop, Gate, etc."), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to add to"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a cast node (Cast To X) to a Blueprint
 * 
 * Example: {"blueprint_path":"/Game/BP_Test", "target_class":"Character", "node_position":{"x":200,"y":100}}
 * For Blueprint classes: {"target_class":"/Game/Blueprints/BP_Enemy"}
 */
class FECACommand_AddBlueprintCastNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_cast_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a Cast To node. For native classes use name like Character, Actor, Pawn. For Blueprints use full path like /Game/BP_Enemy. Example: {\"target_class\":\"Character\"}"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("target_class"), TEXT("string"), TEXT("Class to cast to (e.g., Character, Pawn, or /Game/BP_Enemy for Blueprints)"), true },
			{ TEXT("pure"), TEXT("boolean"), TEXT("Create a pure cast (no exec pins)"), false, TEXT("false") },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to add to"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Connect two nodes in a Blueprint graph
 */
class FECACommand_ConnectBlueprintNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_blueprint_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Connect two nodes in a Blueprint graph by their pins"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("source_node_id"), TEXT("string"), TEXT("GUID of the source node"), true },
			{ TEXT("source_pin"), TEXT("string"), TEXT("Name of the output pin on the source node"), true },
			{ TEXT("target_node_id"), TEXT("string"), TEXT("GUID of the target node"), true },
			{ TEXT("target_pin"), TEXT("string"), TEXT("Name of the input pin on the target node"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a self reference node to a Blueprint
 */
class FECACommand_AddBlueprintSelfReference : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_self_reference"); }
	virtual FString GetDescription() const override { return TEXT("Add a 'Get Self' reference node to a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a component reference (getter) node to a Blueprint
 */
class FECACommand_AddBlueprintComponentReference : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_component_reference"); }
	virtual FString GetDescription() const override { return TEXT("Add a component reference (getter) node to a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component to reference"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Find nodes in a Blueprint graph
 */
class FECACommand_FindBlueprintNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_blueprint_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Find nodes in a Blueprint graph by type or name"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to search"), false, TEXT("EventGraph") },
			{ TEXT("node_class"), TEXT("string"), TEXT("Filter by node class name"), false },
			{ TEXT("node_title"), TEXT("string"), TEXT("Filter by node title (partial match)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a variable get node to a Blueprint
 */
class FECACommand_AddBlueprintVariableGetNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_variable_get"); }
	virtual FString GetDescription() const override { return TEXT("Add a variable getter node to a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("variable_name"), TEXT("string"), TEXT("Name of the variable to get"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a variable set node to a Blueprint
 */
class FECACommand_AddBlueprintVariableSetNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_variable_set"); }
	virtual FString GetDescription() const override { return TEXT("Add a variable setter node to a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("variable_name"), TEXT("string"), TEXT("Name of the variable to set"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Batch create nodes and connections in a single operation.
 * 
 * This command allows you to create multiple nodes and wire them together atomically.
 * Nodes are assigned temporary IDs (strings you define) which can be used to reference
 * them in the connections array. The response includes a mapping of your temporary IDs
 * to the actual node GUIDs.
 * 
 * Example usage:
 * {
 *   "blueprint_path": "/Game/MyBlueprint",
 *   "graph_name": "EventGraph",
 *   "nodes": [
 *     { "temp_id": "event", "type": "event", "event_name": "ReceiveBeginPlay" },
 *     { "temp_id": "print", "type": "function", "function_name": "PrintString", "node_position": {"x": 300, "y": 0} }
 *   ],
 *   "connections": [
 *     { "source_node": "event", "source_pin": "then", "target_node": "print", "target_pin": "execute" }
 *   ]
 * }
 */
class FECACommand_BatchEditBlueprintNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("batch_edit_blueprint_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Create multiple nodes and wire them together atomically. Node types: event, custom_event, function, variable_get, variable_set, self, component, input_action, cast, macro. Cast example: {\"type\":\"cast\", \"target_class\":\"Character\"}. Macro example: {\"type\":\"macro\", \"macro_name\":\"ForLoop\"}"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph (EventGraph or function name)"), false, TEXT("EventGraph") },
			{ TEXT("nodes"), TEXT("array"), TEXT("Array of node definitions with temp_id and type. Types: event, custom_event, function, variable_get, variable_set, self, component, input_action, cast (target_class), macro (macro_name)"), true },
			{ TEXT("connections"), TEXT("array"), TEXT("Array of connection definitions: {source_node, source_pin, target_node, target_pin}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get detailed information about a node's pins.
 * Useful for discovering pin names before making connections.
 */
class FECACommand_GetBlueprintNodePins : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_blueprint_node_pins"); }
	virtual FString GetDescription() const override { return TEXT("Get detailed pin information for a node by its GUID. Use this to discover pin names for connections."); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


/**
 * Delete a node from a Blueprint graph by its ID
 */
class FECACommand_DeleteBlueprintNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_blueprint_node"); }
	virtual FString GetDescription() const override { return TEXT("Delete a node from a Blueprint graph by its GUID"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node to delete"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Find and delete orphan nodes (nodes with no exec pin connections) from a Blueprint graph
 */
class FECACommand_CleanupOrphanNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("cleanup_orphan_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Find and optionally delete orphan nodes (nodes with no execution flow connections) from a Blueprint graph"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") },
			{ TEXT("delete"), TEXT("boolean"), TEXT("If true, delete the orphan nodes. If false, just report them."), false, TEXT("false") },
			{ TEXT("node_class_filter"), TEXT("string"), TEXT("Only consider nodes of this class (e.g., K2Node_CallFunction)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Disconnect all pins on a node
 */
class FECACommand_DisconnectBlueprintNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disconnect_blueprint_node"); }
	virtual FString GetDescription() const override { return TEXT("Disconnect all pins or specific pins on a Blueprint node"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true },
			{ TEXT("pin_name"), TEXT("string"), TEXT("Specific pin to disconnect (optional, disconnects all if not specified)"), false },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


/**
 * Set the default value of a pin on a Blueprint node
 */
class FECACommand_SetBlueprintPinValue : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_blueprint_pin_value"); }
	virtual FString GetDescription() const override { return TEXT("Set the default value of an input pin on a Blueprint node"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("node_id"), TEXT("string"), TEXT("GUID of the node"), true },
			{ TEXT("pin_name"), TEXT("string"), TEXT("Name of the pin to set"), true },
			{ TEXT("value"), TEXT("string"), TEXT("Value to set (as string - will be parsed based on pin type)"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set the default value of a Blueprint variable
 */
class FECACommand_SetBlueprintVariableDefault : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_blueprint_variable_default"); }
	virtual FString GetDescription() const override { return TEXT("Set the default value of a Blueprint variable"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("variable_name"), TEXT("string"), TEXT("Name of the variable"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Default value to set (type must match variable type)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get the default value of a Blueprint variable
 */
class FECACommand_GetBlueprintVariableDefault : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_blueprint_variable_default"); }
	virtual FString GetDescription() const override { return TEXT("Get the default value of a Blueprint variable"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("variable_name"), TEXT("string"), TEXT("Name of the variable"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Break a specific pin connection
 */
class FECACommand_BreakPinConnection : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("break_pin_connection"); }
	virtual FString GetDescription() const override { return TEXT("Break a connection between two specific pins"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("source_node_id"), TEXT("string"), TEXT("GUID of the source node"), true },
			{ TEXT("source_pin"), TEXT("string"), TEXT("Name of the source pin"), true },
			{ TEXT("target_node_id"), TEXT("string"), TEXT("GUID of the target node"), true },
			{ TEXT("target_pin"), TEXT("string"), TEXT("Name of the target pin"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph"), false, TEXT("EventGraph") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a component-bound event node (e.g., OnComponentBeginOverlap for a specific component)
 */
class FECACommand_AddComponentEventNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_component_event_node"); }
	virtual FString GetDescription() const override { return TEXT("Add an event node bound to a specific component (e.g., OnComponentBeginOverlap, OnComponentEndOverlap, OnComponentHit)"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component to bind the event to"), true },
			{ TEXT("event_name"), TEXT("string"), TEXT("Event name: OnComponentBeginOverlap, OnComponentEndOverlap, OnComponentHit, OnComponentWake, OnComponentSleep, etc."), true },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete a component from a Blueprint
 */
class FECACommand_DeleteBlueprintComponent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_blueprint_component"); }
	virtual FString GetDescription() const override { return TEXT("Delete a component from a Blueprint by name"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component to delete"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a flow control node (Branch, Select, Switch) to a Blueprint
 * 
 * Flow control node types:
 * - branch: If/Then/Else (UK2Node_IfThenElse) - has Condition input, True/False exec outputs
 * - select: Select based on index (UK2Node_Select) - selects from array of options
 * - switch_int: Switch on integer value
 * - switch_string: Switch on string value  
 * - switch_name: Switch on FName value
 * - switch_enum: Switch on enum value (requires enum_type parameter)
 * 
 * Example: {"blueprint_path":"/Game/BP_Test", "node_type":"branch", "node_position":{"x":200,"y":100}}
 */
class FECACommand_AddBlueprintFlowControlNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_flow_control_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a flow control node: branch (if/else), select, switch_int, switch_string, switch_name, switch_enum. Example: {\"node_type\":\"branch\"}"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("node_type"), TEXT("string"), TEXT("Type: branch, select, switch_int, switch_string, switch_name, switch_enum"), true },
			{ TEXT("enum_type"), TEXT("string"), TEXT("For switch_enum: the enum type path (e.g., /Script/Engine.ECollisionChannel)"), false },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to add to"), false, TEXT("EventGraph") },
			{ TEXT("node_position"), TEXT("object"), TEXT("Node position in graph {x, y}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Auto-layout nodes in a Blueprint graph for visual clarity
 * 
 * Layout strategies:
 * - horizontal: Arrange nodes left-to-right following execution flow (default)
 * - vertical: Arrange nodes top-to-bottom following execution flow
 * - tree: Arrange as a tree structure from root events
 * - compact: Minimize total graph area while maintaining readability
 * 
 * The algorithm:
 * 1. Identifies root nodes (events, entry points with no input exec connections)
 * 2. Traverses execution flow to determine node order/depth
 * 3. Positions nodes with consistent spacing to avoid overlaps
 * 4. Aligns data-flow nodes (getters, pure functions) near their consumers
 * 
 * Example: {"blueprint_path":"/Game/BP_Test", "graph_name":"EventGraph", "strategy":"horizontal"}
 */
class FECACommand_AutoLayoutBlueprintGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("auto_layout_blueprint_graph"); }
	virtual FString GetDescription() const override { return TEXT("Automatically arrange nodes in a Blueprint graph for visual clarity. Organizes by execution flow with consistent spacing."); }
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Name of the graph to layout"), false, TEXT("EventGraph") },
			{ TEXT("strategy"), TEXT("string"), TEXT("Layout strategy: horizontal (default), vertical, tree, compact"), false, TEXT("horizontal") },
			{ TEXT("spacing_x"), TEXT("number"), TEXT("Horizontal spacing between nodes (default: 400)"), false, TEXT("400") },
			{ TEXT("spacing_y"), TEXT("number"), TEXT("Vertical spacing between nodes (default: 150)"), false, TEXT("150") },
			{ TEXT("align_comments"), TEXT("boolean"), TEXT("Also reposition comment nodes to wrap their contents"), false, TEXT("true") },
			{ TEXT("selected_only"), TEXT("boolean"), TEXT("Only layout selected nodes (false = entire graph)"), false, TEXT("false") },
			{ TEXT("node_ids"), TEXT("array"), TEXT("Specific node IDs to layout (optional, overrides selected_only)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
