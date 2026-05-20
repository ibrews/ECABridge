// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Dump a PCG (Procedural Content Generation) graph: all nodes, edges, parameters.
 */
class FECACommand_DumpPCGGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_pcg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a PCG graph to JSON: all nodes with class, title, inputs/outputs, edges, and graph parameters. Requires PCG plugin."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Content path to the PCGGraph asset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path of the PCGGraph asset") },
			{ TEXT("nodes"),      TEXT("array"),  TEXT("Graph nodes: {id, class, title, inputs, outputs}"), TEXT("object") },
			{ TEXT("edges"),      TEXT("array"),  TEXT("Edges between node pins"), TEXT("object") },
			{ TEXT("parameters"), TEXT("array"),  TEXT("Graph-level parameters"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#if WITH_ECA_CONTROL_RIG
/**
 * Dump a Control Rig: hierarchy elements (bones, controls, nulls), rig graph, and settings.
 */
class FECACommand_DumpControlRig : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_control_rig"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a Control Rig asset: rig hierarchy (bones, controls, nulls, curves), graph nodes, variables. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("rig_path"),  TEXT("string"), TEXT("Path of the Control Rig asset") },
			{ TEXT("hierarchy"), TEXT("array"),  TEXT("Rig elements: bones, controls, nulls, curves"), TEXT("object") },
			{ TEXT("graph"),     TEXT("object"), TEXT("Rig graph: nodes, links, variables") },
			{ TEXT("variables"), TEXT("array"),  TEXT("Rig variables: {name, type, default}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
#endif // WITH_ECA_CONTROL_RIG

#if WITH_ECA_GAMEPLAY_ABILITIES
/**
 * Dump Gameplay Ability System data: abilities, effects, attribute sets, tags.
 */
class FECACommand_DumpGameplayAbility : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_gameplay_ability"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a GameplayAbility, GameplayEffect, or AttributeSet blueprint: tags, cooldowns, costs, modifiers, granted abilities. Requires GameplayAbilities plugin."); }
	virtual FString GetCategory() const override { return TEXT("GAS"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to a GameplayAbility, GameplayEffect, or AttributeSet blueprint"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("asset_path"),  TEXT("string"), TEXT("Path of the GAS asset") },
			{ TEXT("asset_class"), TEXT("string"), TEXT("Specific class (GameplayAbility, GameplayEffect, AttributeSet)") },
			{ TEXT("tags"),        TEXT("array"),  TEXT("Gameplay tags relevant to this asset"), TEXT("string") },
			{ TEXT("modifiers"),   TEXT("array"),  TEXT("Effect modifiers (for GameplayEffect)"), TEXT("object") },
			{ TEXT("attributes"),  TEXT("array"),  TEXT("Attribute definitions (for AttributeSet)"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
#endif // WITH_ECA_GAMEPLAY_ABILITIES
