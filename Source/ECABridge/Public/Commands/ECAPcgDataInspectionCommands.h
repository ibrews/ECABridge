// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Dump the data the actor's UPCGComponent is currently carrying as its last
 * generation output (point clouds, splines, surfaces — whatever the graph emitted).
 */
class FECACommand_DumpPCGData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_pcg_data"); }
	virtual FString GetDescription() const override { return TEXT("Return a summary of the per-pin output data on an actor's UPCGComponent (data class, tags, point count for point data). The debug surface for 'why did the PCG graph produce this?'."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Label of the actor whose PCGComponent should be inspected"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("actor_name"),    TEXT("string"),  TEXT("Resolved actor label") },
			{ TEXT("graph_path"),    TEXT("string"),  TEXT("Path of the graph assigned to the component, if any") },
			{ TEXT("data"),          TEXT("array"),   TEXT("Tagged data: {pin, class, tags, point_count?}"), TEXT("object") },
			{ TEXT("data_count"),    TEXT("integer"), TEXT("Number of tagged data entries") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
