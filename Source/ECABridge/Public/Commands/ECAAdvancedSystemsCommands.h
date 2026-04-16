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

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Content path to the PCGGraph asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump a Control Rig: hierarchy elements (bones, controls, nulls), rig graph, and settings.
 */
class FECACommand_DumpControlRig : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_control_rig"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a Control Rig asset: rig hierarchy (bones, controls, nulls, curves), graph nodes, variables. Requires ControlRig plugin."); }
	virtual FString GetCategory() const override { return TEXT("ControlRig"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("rig_path"), TEXT("string"), TEXT("Content path to the Control Rig blueprint"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump Gameplay Ability System data: abilities, effects, attribute sets, tags.
 */
class FECACommand_DumpGameplayAbility : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_gameplay_ability"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a GameplayAbility, GameplayEffect, or AttributeSet blueprint: tags, cooldowns, costs, modifiers, granted abilities. Requires GameplayAbilities plugin."); }
	virtual FString GetCategory() const override { return TEXT("GAS"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to a GameplayAbility, GameplayEffect, or AttributeSet blueprint"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
