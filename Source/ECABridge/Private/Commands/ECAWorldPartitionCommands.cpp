// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWorldPartitionCommands.h"
#include "Commands/ECACommand.h"

#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"

#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/Class.h"

REGISTER_ECA_COMMAND(FECACommand_GetWorldPartitionInfo)
REGISTER_ECA_COMMAND(FECACommand_ListWorldPartitionCells)
REGISTER_ECA_COMMAND(FECACommand_DumpWorldPartitionGrid)

namespace ECAWorldPartitionHelpers
{
	static TSharedPtr<FJsonObject> BoundsToJson(const FBox& Box)
	{
		TSharedPtr<FJsonObject> Min = MakeShared<FJsonObject>();
		Min->SetNumberField(TEXT("x"), Box.Min.X);
		Min->SetNumberField(TEXT("y"), Box.Min.Y);
		Min->SetNumberField(TEXT("z"), Box.Min.Z);

		TSharedPtr<FJsonObject> Max = MakeShared<FJsonObject>();
		Max->SetNumberField(TEXT("x"), Box.Max.X);
		Max->SetNumberField(TEXT("y"), Box.Max.Y);
		Max->SetNumberField(TEXT("z"), Box.Max.Z);

		TSharedPtr<FJsonObject> Bounds = MakeShared<FJsonObject>();
		Bounds->SetObjectField(TEXT("min"), Min);
		Bounds->SetObjectField(TEXT("max"), Max);
		Bounds->SetBoolField(TEXT("valid"), Box.IsValid != 0);
		return Bounds;
	}

	static FString CellStateString(EWorldPartitionRuntimeCellState State)
	{
		if (const UEnum* Enum = StaticEnum<EWorldPartitionRuntimeCellState>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(State));
		}
		return FString::Printf(TEXT("State(%d)"), static_cast<int32>(State));
	}

	// Walk all streaming cells via the public const-iterator overload on the base hash.
	// Both spatial and any custom hash subclass implement this; the non-const
	// overload exists on the base UWorldPartitionRuntimeHash but isn't part of
	// the public API in 5.8.
	static void ForEachCell(const UWorldPartitionRuntimeHash& Hash, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Fn)
	{
		Hash.ForEachStreamingCells(Fn);
	}
}

//==============================================================================
// get_world_partition_info
//==============================================================================
FECACommandResult FECACommand_GetWorldPartitionInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("world"), World->GetPathName());

	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP)
	{
		Result->SetBoolField(TEXT("is_partitioned"), false);
		return FECACommandResult::Success(Result);
	}

	Result->SetBoolField(TEXT("is_partitioned"), true);
	Result->SetBoolField(TEXT("streaming_enabled"),    WP->IsStreamingEnabled());
	Result->SetBoolField(TEXT("server_streaming"),     WP->IsServerStreamingEnabled());
	Result->SetBoolField(TEXT("server_streaming_out"), WP->IsServerStreamingOutEnabled());
	Result->SetBoolField(TEXT("initialized"),          WP->IsInitialized());

	UWorldPartitionRuntimeHash* Hash = WP->RuntimeHash;
	if (Hash)
	{
		Result->SetStringField(TEXT("runtime_hash_class"), Hash->GetClass()->GetName());
	}
	else
	{
		Result->SetStringField(TEXT("runtime_hash_class"), TEXT(""));
	}

	// Cell count is a quick traversal — useful sanity signal for whether streaming
	// has been baked. Skipped if no hash (editor-only worlds).
	int32 CellCount = 0;
	if (Hash)
	{
		ECAWorldPartitionHelpers::ForEachCell(*Hash, [&CellCount](const UWorldPartitionRuntimeCell*) -> bool
		{
			++CellCount;
			return true;
		});
	}
	Result->SetNumberField(TEXT("cell_count"), CellCount);

	// Spatial-hash grid summary
	if (const UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(Hash))
	{
		TArray<TSharedPtr<FJsonValue>> Grids;
		SpatialHash->ForEachStreamingGrid([&Grids](const FSpatialHashStreamingGrid& Grid)
		{
			TSharedPtr<FJsonObject> GridObj = MakeShared<FJsonObject>();
			GridObj->SetStringField(TEXT("name"),          Grid.GridName.ToString());
			GridObj->SetNumberField(TEXT("index"),         Grid.GridIndex);
			GridObj->SetNumberField(TEXT("cell_size"),     Grid.CellSize);
			GridObj->SetNumberField(TEXT("loading_range"), Grid.LoadingRange);
			GridObj->SetNumberField(TEXT("level_count"),   Grid.GridLevels.Num());
			Grids.Add(MakeShared<FJsonValueObject>(GridObj));
		});
		Result->SetArrayField(TEXT("grid_levels"), Grids);
		Result->SetNumberField(TEXT("grid_count"), SpatialHash->GetNumGrids());
	}
	else
	{
		Result->SetArrayField(TEXT("grid_levels"), TArray<TSharedPtr<FJsonValue>>());
	}

	return FECACommandResult::Success(Result);
}

//==============================================================================
// list_world_partition_cells
//==============================================================================
FECACommandResult FECACommand_ListWorldPartitionCells::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP || !WP->RuntimeHash)
	{
		return FECACommandResult::Error(TEXT("Current world is not partitioned (no UWorldPartition or no RuntimeHash)"));
	}

	FString StateFilter;
	GetStringParam(Params, TEXT("state_filter"), StateFilter, false);

	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	int32 MaxResults = 200;
	GetIntParam(Params, TEXT("max_results"), MaxResults, false);
	MaxResults = FMath::Clamp(MaxResults, 1, 5000);

	// Resolve optional state filter
	bool bHasStateFilter = false;
	EWorldPartitionRuntimeCellState DesiredState = EWorldPartitionRuntimeCellState::Unloaded;
	if (!StateFilter.IsEmpty())
	{
		if (const UEnum* Enum = StaticEnum<EWorldPartitionRuntimeCellState>())
		{
			const int64 Value = Enum->GetValueByNameString(StateFilter);
			if (Value != INDEX_NONE)
			{
				DesiredState = static_cast<EWorldPartitionRuntimeCellState>(Value);
				bHasStateFilter = true;
			}
			else
			{
				return FECACommandResult::Error(FString::Printf(TEXT("state_filter '%s' is not a valid EWorldPartitionRuntimeCellState"), *StateFilter));
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> CellsJson;
	int32 TotalFound = 0;

	ECAWorldPartitionHelpers::ForEachCell(*WP->RuntimeHash, [&](const UWorldPartitionRuntimeCell* Cell) -> bool
	{
		if (!Cell) return true;

		const FString DebugName = Cell->GetDebugName();
		if (!NameFilter.IsEmpty() && !DebugName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const EWorldPartitionRuntimeCellState State = Cell->GetCurrentState();
		if (bHasStateFilter && State != DesiredState)
		{
			return true;
		}

		++TotalFound;
		if (CellsJson.Num() >= MaxResults)
		{
			return true;
		}

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("debug_name"),    DebugName);
		Obj->SetStringField(TEXT("state"),         ECAWorldPartitionHelpers::CellStateString(State));
		Obj->SetStringField(TEXT("level_package"), Cell->GetLevelPackageName().ToString());
		Obj->SetNumberField(TEXT("actor_count"),   Cell->GetActorCount());
		Obj->SetObjectField(TEXT("bounds"),        ECAWorldPartitionHelpers::BoundsToJson(Cell->GetCellBounds()));
		CellsJson.Add(MakeShared<FJsonValueObject>(Obj));
		return true;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("total_found"), TotalFound);
	Result->SetNumberField(TEXT("returned"),    CellsJson.Num());
	Result->SetArrayField(TEXT("cells"),        CellsJson);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_world_partition_grid
//==============================================================================
FECACommandResult FECACommand_DumpWorldPartitionGrid::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP || !WP->RuntimeHash)
	{
		return FECACommandResult::Error(TEXT("Current world is not partitioned (no UWorldPartition or no RuntimeHash)"));
	}

	int32 MaxLevels = 8;
	GetIntParam(Params, TEXT("max_levels"), MaxLevels, false);
	MaxLevels = FMath::Clamp(MaxLevels, 1, 16);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("hash_class"), WP->RuntimeHash->GetClass()->GetName());

	const UWorldPartitionRuntimeSpatialHash* Spatial = Cast<UWorldPartitionRuntimeSpatialHash>(WP->RuntimeHash);
	Result->SetBoolField(TEXT("is_spatial"), Spatial != nullptr);

	TArray<TSharedPtr<FJsonValue>> Grids;
	if (Spatial)
	{
		Spatial->ForEachStreamingGrid([&Grids, MaxLevels](const FSpatialHashStreamingGrid& Grid)
		{
			TSharedPtr<FJsonObject> GridObj = MakeShared<FJsonObject>();
			GridObj->SetStringField(TEXT("name"),          Grid.GridName.ToString());
			GridObj->SetNumberField(TEXT("index"),         Grid.GridIndex);
			GridObj->SetNumberField(TEXT("cell_size"),     Grid.CellSize);
			GridObj->SetNumberField(TEXT("loading_range"), Grid.LoadingRange);
			GridObj->SetNumberField(TEXT("level_count"),   Grid.GridLevels.Num());

			// Per-level cell size via FSpatialHashStreamingGrid::GetCellSize.
			TArray<TSharedPtr<FJsonValue>> Levels;
			const int32 LevelsToInspect = FMath::Min(MaxLevels, Grid.GridLevels.Num());
			for (int32 Level = 0; Level < LevelsToInspect; ++Level)
			{
				const int64 CellSize = Grid.GetCellSize(Level);
				if (CellSize <= 0) break;
				TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
				LevelObj->SetNumberField(TEXT("level"),     Level);
				LevelObj->SetNumberField(TEXT("cell_size"), static_cast<double>(CellSize));
				Levels.Add(MakeShared<FJsonValueObject>(LevelObj));
			}
			GridObj->SetArrayField(TEXT("levels"), Levels);

			Grids.Add(MakeShared<FJsonValueObject>(GridObj));
		});
	}
	Result->SetArrayField(TEXT("grids"), Grids);
	return FECACommandResult::Success(Result);
}
