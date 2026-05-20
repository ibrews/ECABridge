// Copyright Epic Games, Inc. All Rights Reserved.
//
// MCP commands that expose the session-scoped node-name registry to clients.
// See Commands/ECANodeNameRegistry.h for the rationale. These all advertise
// under category "Blueprint Node" so they get loaded by the same load_category
// call agents already use when authoring Blueprint graphs.

#include "Commands/ECACommand.h"
#include "Commands/ECANodeNameRegistry.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

namespace
{
	/**
	 * Find a graph on a Blueprint by name. Local copy of the helper used by
	 * ECABlueprintNodeCommands.cpp — duplicated here so we don't have to
	 * widen its linkage just for these meta-tools.
	 */
	UEdGraph* FindGraphOnBlueprint(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph && (Graph->GetName() == GraphName || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase)))
			{
				return Graph;
			}
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}

		return nullptr;
	}

	UEdGraphNode* FindNodeInGraph(UEdGraph* Graph, const FGuid& NodeGuid)
	{
		if (!Graph)
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				return Node;
			}
		}
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// name_blueprint_node — register a name for an existing node
//------------------------------------------------------------------------------

class FECACommand_NameBlueprintNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("name_blueprint_node"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Bind a friendly name to an existing Blueprint node so later commands can pass `node_name` instead of `node_id`. Names are session-scoped (lost on editor restart) and case-sensitive. Re-naming an existing name overwrites it.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"),           TEXT("string"), TEXT("Friendly name to bind (case-sensitive, non-empty)"), true },
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset that owns the node"), true },
			{ TEXT("node_id"),        TEXT("string"), TEXT("GUID of the node to name"), true },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Name of the graph containing the node"), false, TEXT("EventGraph") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),        TEXT("boolean"), TEXT("Always true on success") },
			{ TEXT("name"),           TEXT("string"),  TEXT("The bound friendly name") },
			{ TEXT("node_id"),        TEXT("string"),  TEXT("The GUID the name now resolves to") },
			{ TEXT("blueprint_path"), TEXT("string"),  TEXT("The Blueprint that owns the node") },
			{ TEXT("graph_name"),     TEXT("string"),  TEXT("The graph that owns the node") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Name;
		if (!GetStringParam(Params, TEXT("name"), Name) || Name.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing or empty required parameter: name"));
		}

		FString BlueprintPath;
		if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path"));
		}

		FString NodeId;
		if (!GetStringParam(Params, TEXT("node_id"), NodeId))
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: node_id"));
		}

		FString GraphName = TEXT("EventGraph");
		GetStringParam(Params, TEXT("graph_name"), GraphName, false);

		UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
		if (!Blueprint)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
		}

		UEdGraph* Graph = FindGraphOnBlueprint(Blueprint, GraphName);
		if (!Graph)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		}

		FGuid NodeGuid;
		if (!FGuid::Parse(NodeId, NodeGuid))
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Invalid GUID format: %s"), *NodeId));
		}

		UEdGraphNode* Node = FindNodeInGraph(Graph, NodeGuid);
		if (!Node)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Node not found with GUID %s in graph %s"), *NodeId, *GraphName));
		}

		FECANodeNameRegistry::Get().Register(Name, BlueprintPath, GraphName, NodeGuid);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("name"), Name);
		Result->SetStringField(TEXT("node_id"), NodeGuid.ToString());
		Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
		Result->SetStringField(TEXT("graph_name"), GraphName);
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// unname_blueprint_node — drop a name binding
//------------------------------------------------------------------------------

class FECACommand_UnnameBlueprintNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("unname_blueprint_node"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Remove a friendly-name binding from the session registry. Does NOT delete the underlying node — only the name -> GUID mapping. Reports whether the name was previously bound.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Friendly name to unbind (case-sensitive)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),   TEXT("boolean"), TEXT("Always true (the command itself succeeded)") },
			{ TEXT("name"),      TEXT("string"),  TEXT("The name passed in") },
			{ TEXT("was_bound"), TEXT("boolean"), TEXT("True if the name was previously registered") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Name;
		if (!GetStringParam(Params, TEXT("name"), Name) || Name.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing or empty required parameter: name"));
		}

		const bool bWasBound = FECANodeNameRegistry::Get().Unregister(Name);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("name"), Name);
		Result->SetBoolField(TEXT("was_bound"), bWasBound);
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// list_node_names — snapshot all current bindings
//------------------------------------------------------------------------------

class FECACommand_ListNodeNames : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_node_names"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Return every friendly-name -> node binding currently held by the session registry. Read-only.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),    TEXT("integer"), TEXT("Number of bindings returned") },
			{ TEXT("bindings"), TEXT("array"),   TEXT("Each entry: { name, node_id, blueprint_path, graph_name }"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& /*Params*/) override
	{
		const TArray<FECANodeNameRegistry::FEntry> Entries = FECANodeNameRegistry::Get().Snapshot();

		TArray<TSharedPtr<FJsonValue>> Items;
		Items.Reserve(Entries.Num());
		for (const FECANodeNameRegistry::FEntry& Entry : Entries)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Entry.Name);
			Obj->SetStringField(TEXT("node_id"), Entry.NodeGuid.ToString());
			Obj->SetStringField(TEXT("blueprint_path"), Entry.BlueprintPath);
			Obj->SetStringField(TEXT("graph_name"), Entry.GraphName);
			Items.Add(MakeShared<FJsonValueObject>(Obj));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("count"), Items.Num());
		Result->SetArrayField(TEXT("bindings"), Items);
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// resolve_node_name — look up a name without mutating
//------------------------------------------------------------------------------

class FECACommand_ResolveNodeName : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("resolve_node_name"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Look up a friendly node name in the session registry without mutating anything. Returns { found:false } when the name is not bound.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Node"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Friendly name to look up (case-sensitive)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("name"),           TEXT("string"),  TEXT("The name that was looked up") },
			{ TEXT("found"),          TEXT("boolean"), TEXT("True if the name resolves") },
			{ TEXT("node_id"),        TEXT("string"),  TEXT("Resolved GUID (only when found)") },
			{ TEXT("blueprint_path"), TEXT("string"),  TEXT("Resolved Blueprint path (only when found)") },
			{ TEXT("graph_name"),     TEXT("string"),  TEXT("Resolved graph name (only when found)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Name;
		if (!GetStringParam(Params, TEXT("name"), Name) || Name.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing or empty required parameter: name"));
		}

		FString BlueprintPath, GraphName;
		FGuid NodeGuid;
		const bool bFound = FECANodeNameRegistry::Get().Resolve(Name, BlueprintPath, GraphName, NodeGuid);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("name"), Name);
		Result->SetBoolField(TEXT("found"), bFound);
		if (bFound)
		{
			Result->SetStringField(TEXT("node_id"), NodeGuid.ToString());
			Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
			Result->SetStringField(TEXT("graph_name"), GraphName);
		}
		return FECACommandResult::Success(Result);
	}
};

REGISTER_ECA_COMMAND(FECACommand_NameBlueprintNode)
REGISTER_ECA_COMMAND(FECACommand_UnnameBlueprintNode)
REGISTER_ECA_COMMAND(FECACommand_ListNodeNames)
REGISTER_ECA_COMMAND(FECACommand_ResolveNodeName)
