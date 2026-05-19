// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_SnapshotMemory : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("snapshot_memory"); }
	virtual FString GetDescription() const override { return TEXT("Capture a labeled memory snapshot (FPlatformMemory stats + UObject counts) for later diffing."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return { { TEXT("label"), TEXT("string"), TEXT("Snapshot label (used as the key for diff_memory_snapshots)"), true } };
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DiffMemorySnapshots : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("diff_memory_snapshots"); }
	virtual FString GetDescription() const override { return TEXT("Diff two previously captured memory snapshots. Returns per-metric delta plus a small leak-suspect summary."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("label_a"), TEXT("string"), TEXT("Earlier snapshot label"), true },
			{ TEXT("label_b"), TEXT("string"), TEXT("Later snapshot label"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
