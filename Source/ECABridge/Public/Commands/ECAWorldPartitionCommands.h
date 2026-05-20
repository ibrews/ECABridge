// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Top-level info on the editor world's World Partition: enabled, hash class,
 * streaming flags, total cell count, and (for spatial hash) per-grid-level
 * cell sizes.
 */
class FECACommand_GetWorldPartitionInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_world_partition_info"); }
	virtual FString GetDescription() const override { return TEXT("Return World Partition info for the current editor world: enabled flag, runtime hash class, streaming flags, cell totals, and (for spatial hash) per-grid-level cell sizes."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("is_partitioned"),       TEXT("boolean"), TEXT("True when the editor world has a UWorldPartition") },
			{ TEXT("world"),                TEXT("string"),  TEXT("Editor world path") },
			{ TEXT("streaming_enabled"),    TEXT("boolean"), TEXT("UWorldPartition::IsStreamingEnabled") },
			{ TEXT("server_streaming"),     TEXT("boolean"), TEXT("UWorldPartition::IsServerStreamingEnabled") },
			{ TEXT("server_streaming_out"), TEXT("boolean"), TEXT("UWorldPartition::IsServerStreamingOutEnabled") },
			{ TEXT("runtime_hash_class"),   TEXT("string"),  TEXT("RuntimeHash class name (e.g. WorldPartitionRuntimeSpatialHash)") },
			{ TEXT("cell_count"),           TEXT("integer"), TEXT("Total streaming-cell count enumerated") },
			{ TEXT("grid_levels"),          TEXT("array"),   TEXT("[{level, cell_size}] from UWorldPartitionRuntimeSpatialHash::GetCellSize"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Enumerate runtime cells of the editor world's WP.
 * Returns up to max_results entries (default 200). Cells are reported with
 * debug name, current state, level package name, actor count, and bounds.
 */
class FECACommand_ListWorldPartitionCells : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_world_partition_cells"); }
	virtual FString GetDescription() const override { return TEXT("Enumerate runtime cells of the current editor world's World Partition. Each entry: debug_name, state, level_package, actor_count, bounds."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_filter"), TEXT("string"),  TEXT("Optional cell-state filter: Unloaded / Loaded / Activated"), false },
			{ TEXT("name_filter"),  TEXT("string"),  TEXT("Substring filter on cell debug name (case-insensitive)"), false },
			{ TEXT("max_results"),  TEXT("integer"), TEXT("Cap results (default 200, max 5000)"), false, TEXT("200") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("total_found"), TEXT("integer"), TEXT("Number of cells after filtering") },
			{ TEXT("returned"),    TEXT("integer"), TEXT("Number of cells in 'cells'") },
			{ TEXT("cells"),       TEXT("array"),   TEXT("Per-cell {debug_name, state, level_package, actor_count, bounds:{min:{x,y,z}, max:{x,y,z}}}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * For spatial-hash WP, dump grid-level cell sizes + cell-count per level by
 * walking the cells and bucketing them by GetLevel() (where the cell's debug
 * name encodes the grid level).
 *
 * Provides a quick summary useful for understanding the streaming topology
 * without rendering DrawRuntimeHash2D.
 */
class FECACommand_DumpWorldPartitionGrid : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_world_partition_grid"); }
	virtual FString GetDescription() const override { return TEXT("Dump World Partition spatial-hash grid spec: per-level cell size and cell-count summary. No-op when WP is non-spatial."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("max_levels"), TEXT("integer"), TEXT("Cap grid levels to inspect (default 8, max 16)"), false, TEXT("8") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("hash_class"),  TEXT("string"),  TEXT("Runtime hash class name") },
			{ TEXT("is_spatial"),  TEXT("boolean"), TEXT("True when the hash is UWorldPartitionRuntimeSpatialHash") },
			{ TEXT("grid_levels"), TEXT("array"),   TEXT("[{level, cell_size}] for each grid level"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
