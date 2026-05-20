// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Read-side Blueprint analysis tools (roadmap item #6).
 *
 * These complement dump_blueprint_graph, which produces an exhaustive node-by-node
 * dump that is too large for an LLM context window on non-trivial Blueprints. The
 * analysis commands return summary-shaped output: control flow, callers, topology
 * diffs, and unreferenced assets — so agents can *understand* a Blueprint before
 * editing it without having to round-trip every pin.
 */

/**
 * Produce a compact, pseudo-code-style summary of a Blueprint's control flow.
 * Walks each EventGraph + Function graph from its entry points, following exec
 * pins, and emits one short line per branch. Pure (data-only) nodes are not
 * walked; they're noted as "data sources" feeding the executed node.
 */
class FECACommand_SummarizeBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("summarize_blueprint"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Compact pseudo-code summary of a Blueprint's event handlers and functions. "
		            "Use this BEFORE dump_blueprint_graph — orders of magnitude smaller, captures "
		            "the control flow (branches/casts/loops/calls) for each entry point.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Analysis"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"),    TEXT("string"),  TEXT("Path to the Blueprint asset (e.g. /Game/Foo/BP_Bar.BP_Bar)"), true },
			{ TEXT("include_functions"), TEXT("boolean"), TEXT("Include user-defined function graphs (not just event graphs)"), false, TEXT("true") },
			{ TEXT("max_depth"),         TEXT("integer"), TEXT("Maximum walk depth before collapsing as '...' (default 6)"),  false, TEXT("6") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path of the Blueprint that was summarized") },
			{ TEXT("parent_class"),   TEXT("string"), TEXT("Parent UClass name") },
			{ TEXT("graphs"),         TEXT("array"),  TEXT("Per-graph summaries: {name, kind, events|summary}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Reverse lookup: find every Blueprint that contains a node referencing a
 * specific function / variable / event. Loads each Blueprint under search_path
 * and walks every K2Node_CallFunction / K2Node_Variable* / K2Node_Event.
 */
class FECACommand_FindBlueprintCallers : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_blueprint_callers"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Reverse-lookup: find every Blueprint that contains a node referencing the given "
		            "function, variable, or event. Loads every Blueprint under search_path and walks its graphs.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Analysis"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("function_name"), TEXT("string"),  TEXT("Function to find callers of (required if variable_name/event_name not set)"), false },
			{ TEXT("variable_name"), TEXT("string"),  TEXT("Variable to find get/set references for"),                                    false },
			{ TEXT("event_name"),    TEXT("string"),  TEXT("Custom event to find dispatchers/calls of"),                                  false },
			{ TEXT("target_class"),  TEXT("string"),  TEXT("Disambiguate by member class (e.g. Character, KismetSystemLibrary)"),         false },
			{ TEXT("search_path"),   TEXT("string"),  TEXT("Restrict search to a content folder (default /Game)"),                        false, TEXT("/Game") },
			{ TEXT("max_results"),   TEXT("integer"), TEXT("Stop after this many hits (default 100)"),                                    false, TEXT("100") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("search_term"), TEXT("string"),  TEXT("Search term that was matched") },
			{ TEXT("search_kind"), TEXT("string"),  TEXT("Kind: function | variable | event") },
			{ TEXT("callers"),     TEXT("array"),   TEXT("Hits: {blueprint_path, graph_name, node_id, node_title, context}"), TEXT("object") },
			{ TEXT("count"),       TEXT("integer"), TEXT("Number of hits returned") },
			{ TEXT("truncated"),   TEXT("boolean"), TEXT("True when more hits were available than max_results") },
			{ TEXT("scanned"),     TEXT("integer"), TEXT("Number of Blueprints inspected") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Topology-level diff between two Blueprints. For each graph that exists in
 * both, emits added/removed nodes (by class+title) and added/removed
 * connections. Optionally compares default pin literal values.
 */
class FECACommand_DiffBlueprints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("diff_blueprints"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Topology diff between two Blueprints — per graph, the added/removed nodes "
		            "(matched by node class + title) and added/removed connections. Not a textual diff.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Analysis"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_a"),        TEXT("string"),  TEXT("Path to first Blueprint"),  true },
			{ TEXT("blueprint_b"),        TEXT("string"),  TEXT("Path to second Blueprint"), true },
			{ TEXT("include_pin_values"), TEXT("boolean"), TEXT("Also report changed default pin literal values"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("blueprint_a"),       TEXT("string"), TEXT("Path of the first Blueprint") },
			{ TEXT("blueprint_b"),       TEXT("string"), TEXT("Path of the second Blueprint") },
			{ TEXT("graphs"),            TEXT("array"),  TEXT("Per-graph diff: {name, added_nodes, removed_nodes, added_connections, removed_connections, changed_pin_values?}"), TEXT("object") },
			{ TEXT("graphs_only_in_a"),  TEXT("array"),  TEXT("Graph names present only in blueprint_a"), TEXT("string") },
			{ TEXT("graphs_only_in_b"),  TEXT("array"),  TEXT("Graph names present only in blueprint_b"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Walk a content path and return assets that no other asset references.
 * Skips Worlds / Levels / Class-default placeholders (they are roots by
 * nature, not orphans).
 */
class FECACommand_FindUnusedAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_unused_assets"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Scan a content path for assets with no referencers. Skips Worlds/Levels "
		            "(they are roots) and class-defaults. Useful for cleanup audits.");
	}
	virtual FString GetCategory() const override { return TEXT("Blueprint Analysis"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("search_path"), TEXT("string"),  TEXT("Content path to scan (default /Game)"),                          false, TEXT("/Game") },
			{ TEXT("asset_class"), TEXT("string"),  TEXT("Restrict to a UClass leaf name (Texture2D, Blueprint, Material)"), false },
			{ TEXT("max_results"), TEXT("integer"), TEXT("Stop after this many unused assets (default 200)"),               false, TEXT("200") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("search_path"), TEXT("string"),  TEXT("Path that was scanned") },
			{ TEXT("unused"),      TEXT("array"),   TEXT("Unused assets: {path, class, size_kb?}"), TEXT("object") },
			{ TEXT("count"),       TEXT("integer"), TEXT("Number of unused assets returned") },
			{ TEXT("truncated"),   TEXT("boolean"), TEXT("True when more unused assets existed than max_results") },
			{ TEXT("scanned"),     TEXT("integer"), TEXT("Number of assets inspected (after filtering by asset_class)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
