// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Editor-only: load all World Partition cells whose bounds intersect a given
 * axis-aligned box, by registering it as a user-loaded region via
 * UWorldPartition::LoadLastLoadedRegions(TArray<FBox>). Useful before bulk
 * edits in a region that would otherwise stay unloaded.
 */
class FECACommand_ForceLoadWPRegion : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("force_load_wp_region"); }
	virtual FString GetDescription() const override { return TEXT("Editor: force-load WP cells intersecting a bounding box by registering a user-loaded region. Calls UWorldPartition::LoadLastLoadedRegions({Box})."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("min"), TEXT("object"),  TEXT("Box minimum {x,y,z} (world space). Required."), true },
			{ TEXT("max"), TEXT("object"),  TEXT("Box maximum {x,y,z} (world space). Required."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("world"),                TEXT("string"),  TEXT("Editor world path") },
			{ TEXT("region_registered"),    TEXT("boolean"), TEXT("True when the region was passed to LoadLastLoadedRegions") },
			{ TEXT("box"),                  TEXT("object"),  TEXT("Echoed bounding box {min:{x,y,z}, max:{x,y,z}}") },
			{ TEXT("cells_intersecting"),   TEXT("integer"), TEXT("Streaming cells whose bounds intersect the box after the call") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Editor-only: pin one or more World Partition actors so they remain loaded
 * regardless of streaming. Wraps UWorldPartition::PinActors(TArray<FGuid>).
 */
class FECACommand_PinWPActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("pin_wp_actors"); }
	virtual FString GetDescription() const override { return TEXT("Editor: pin World Partition actors so they stay loaded. Wraps UWorldPartition::PinActors."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_guids"), TEXT("array"), TEXT("Actor GUIDs (string form) to pin. Required."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("requested"),  TEXT("integer"), TEXT("Number of GUIDs requested") },
			{ TEXT("parsed"),     TEXT("integer"), TEXT("Number of GUIDs that parsed cleanly") },
			{ TEXT("pinned_now"), TEXT("integer"), TEXT("Number of parsed GUIDs that IsActorPinned() reports as pinned after the call") },
			{ TEXT("invalid"),    TEXT("array"),   TEXT("GUID strings that failed to parse"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Editor-only: unpin previously-pinned World Partition actors. Wraps
 * UWorldPartition::UnpinActors(TArray<FGuid>).
 */
class FECACommand_UnpinWPActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("unpin_wp_actors"); }
	virtual FString GetDescription() const override { return TEXT("Editor: unpin World Partition actors. Wraps UWorldPartition::UnpinActors."); }
	virtual FString GetCategory() const override { return TEXT("WorldPartition"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_guids"), TEXT("array"), TEXT("Actor GUIDs (string form) to unpin. Required."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("requested"),    TEXT("integer"), TEXT("Number of GUIDs requested") },
			{ TEXT("parsed"),       TEXT("integer"), TEXT("Number of GUIDs that parsed cleanly") },
			{ TEXT("unpinned_now"), TEXT("integer"), TEXT("Number of parsed GUIDs that IsActorPinned() reports as unpinned after the call") },
			{ TEXT("invalid"),      TEXT("array"),   TEXT("GUID strings that failed to parse"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
