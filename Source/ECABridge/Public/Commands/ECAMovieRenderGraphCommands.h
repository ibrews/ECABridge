// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"
#include "Misc/EngineVersionComparison.h"

// Movie Render Graph (MRG) was introduced in UE 5.8 as the strategic replacement
// for Movie Render Queue. The entire command set is 5.8-only and additionally
// requires the MovieRenderPipeline plugin (already gated by WITH_ECA_MOVIE_RENDER_PIPELINE).
#if WITH_ECA_MOVIE_RENDER_PIPELINE && !UE_VERSION_OLDER_THAN(5, 8, 0)

class FECACommand_CreateMrgGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_mrg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Movie Render Graph (UMovieGraphConfig) asset"); }
	virtual FString GetCategory() const override { return TEXT("MovieRenderGraph"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Destination asset path (e.g. /Game/Cinematics/MyGraph)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddMrgNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_mrg_node"); }
	virtual FString GetDescription() const override { return TEXT("Add a node of a given UMovieGraphNode subclass to a Movie Render Graph"); }
	virtual FString GetCategory() const override { return TEXT("MovieRenderGraph"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Asset path of the UMovieGraphConfig"), true },
			{ TEXT("node_class"), TEXT("string"), TEXT("UMovieGraphNode subclass name (e.g. MovieGraphImageSequenceOutputNode_PNG, MovieGraphSamplingMethodNode)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ConnectMrgNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("connect_mrg_nodes"); }
	virtual FString GetDescription() const override { return TEXT("Add a labeled edge between two nodes on a Movie Render Graph (UMovieGraphConfig::AddLabeledEdge)"); }
	virtual FString GetCategory() const override { return TEXT("MovieRenderGraph"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Asset path of the UMovieGraphConfig"), true },
			{ TEXT("from_node"), TEXT("string"), TEXT("Source node name (e.g. 'Inputs', 'Outputs', or a class-derived name)"), true },
			{ TEXT("from_pin"), TEXT("string"), TEXT("Source pin label (empty for default/unnamed pin)"), false },
			{ TEXT("to_node"), TEXT("string"), TEXT("Destination node name"), true },
			{ TEXT("to_pin"), TEXT("string"), TEXT("Destination pin label (empty for default/unnamed pin)"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ExecuteMrgGraph : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("execute_mrg_graph"); }
	virtual FString GetDescription() const override { return TEXT("Enqueue a Movie Render Graph + Level Sequence onto the Movie Pipeline Queue and start rendering"); }
	virtual FString GetCategory() const override { return TEXT("MovieRenderGraph"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Asset path of the UMovieGraphConfig"), true },
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence to render"), true },
			{ TEXT("output_dir"), TEXT("string"), TEXT("Output directory for rendered frames (default Saved/MovieRenders)"), false, TEXT("Saved/MovieRenders") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif // WITH_ECA_MOVIE_RENDER_PIPELINE && !UE_VERSION_OLDER_THAN(5, 8, 0)
