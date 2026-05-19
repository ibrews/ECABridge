// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Run a PCG graph on an actor. Attaches a UPCGComponent if missing, assigns
 * the graph if provided, then calls Generate.
 */
class FECACommand_RunPCGOnActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("run_pcg_on_actor"); }
	virtual FString GetDescription() const override { return TEXT("Trigger PCG generation on a target actor. Adds a PCGComponent if absent. Optionally assigns a graph asset to the component before generating."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Label of the target actor in the editor world"), true },
			{ TEXT("graph_path"), TEXT("string"), TEXT("Optional PCGGraph asset path to assign before generating"), false },
			{ TEXT("force"),      TEXT("boolean"), TEXT("Force regeneration even if up-to-date; default false"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("actor_name"),   TEXT("string"),  TEXT("Resolved actor label") },
			{ TEXT("graph_path"),   TEXT("string"),  TEXT("PCGGraph asset path that was generated") },
			{ TEXT("component_added"), TEXT("boolean"), TEXT("Whether a new PCGComponent was attached") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Cleanup all PCG output owned by an actor's UPCGComponent.
 */
class FECACommand_ClearPCGOutput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("clear_pcg_output"); }
	virtual FString GetDescription() const override { return TEXT("Clean up all generated PCG output (spawned actors, ISMs, etc.) on the target actor's PCGComponent without removing the component itself."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Label of the actor whose PCGComponent should be cleaned"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("actor_name"),    TEXT("string"),  TEXT("Resolved actor label") },
			{ TEXT("had_component"), TEXT("boolean"), TEXT("Whether the actor had a PCGComponent") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
