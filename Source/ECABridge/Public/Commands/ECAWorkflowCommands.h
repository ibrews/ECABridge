// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Snapshot an asset's current property state for later comparison with diff_asset.
 */
class FECACommand_SnapshotAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("snapshot_asset"); }
	virtual FString GetDescription() const override { return TEXT("Take a snapshot of an asset's current property values. Returns a snapshot_id that can be passed to diff_asset to see what changed."); }
	virtual FString GetCategory() const override { return TEXT("Workflow"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to snapshot"), true },
			{ TEXT("snapshot_id"), TEXT("string"), TEXT("Optional ID for the snapshot (auto-generated if not provided)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Compare an asset's current state against a previous snapshot, or compare two assets.
 */
class FECACommand_DiffAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("diff_asset"); }
	virtual FString GetDescription() const override { return TEXT("Compare an asset against a snapshot (from snapshot_asset) or compare two different assets of the same class. Returns only the properties that differ."); }
	virtual FString GetCategory() const override { return TEXT("Workflow"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (or first asset if comparing two)"), true },
			{ TEXT("snapshot_id"), TEXT("string"), TEXT("Snapshot ID to compare against (from snapshot_asset)"), false },
			{ TEXT("compare_to"), TEXT("string"), TEXT("Content path to second asset to compare against (alternative to snapshot_id)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Execute multiple commands in a single undo transaction.
 */
class FECACommand_BatchOperation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("batch_operation"); }
	virtual FString GetDescription() const override { return TEXT("Execute multiple ECABridge commands wrapped in a single undo transaction. All commands succeed or the entire batch is rolled back. One Ctrl+Z undoes everything."); }
	virtual FString GetCategory() const override { return TEXT("Workflow"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("commands"), TEXT("array"), TEXT("Array of command objects: [{name: 'create_actor', arguments: {...}}, ...]"), true },
			{ TEXT("description"), TEXT("string"), TEXT("Description for the undo transaction (shown in Edit > Undo History)"), false, TEXT("Batch Operation") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
