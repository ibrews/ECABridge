// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * get_ddc_stats — snapshot the Derived Data Cache: graph name, shared-vs-local backend,
 * and per-asset-type resource stats (load count / build count / time / size). The same
 * data the editor's DDC HUD displays, returned as JSON.
 */
class FECACommand_GetDDCStats : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_ddc_stats"); }
	virtual FString GetDescription() const override { return TEXT("Snapshot DerivedDataCache statistics: graph name, shared/local mode, and per-asset-type load/build counts, times and sizes."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_name"),        TEXT("string"),  TEXT("DDC graph name (e.g. Default, NoShared, Test)") },
			{ TEXT("default_graph_name"),TEXT("string"),  TEXT("DDC graph the engine falls back to") },
			{ TEXT("using_shared_ddc"),  TEXT("boolean"), TEXT("Whether a shared/network DDC backend is in use") },
			{ TEXT("async_pending"),     TEXT("boolean"), TEXT("Whether the DDC has outstanding async requests") },
			{ TEXT("total_load_count"),  TEXT("integer"), TEXT("Sum of LoadCount across all per-asset-type resource stat rows") },
			{ TEXT("total_build_count"), TEXT("integer"), TEXT("Sum of BuildCount across all per-asset-type resource stat rows") },
			{ TEXT("resource_stats"),    TEXT("array"),   TEXT("Per-asset-type rows (asset_type, load_count, build_count, ...)"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * purge_ddc — invoke an editor console command that wipes a subset of the DDC. By default
 * runs `DerivedDataCache.Cleanup` (the engine's standard cleanup command). Callers can pass
 * a custom_command string for non-default cleanup invocations. NOT a per-key delete — the
 * public DDC interface doesn't expose targeted invalidation.
 */
class FECACommand_PurgeDDC : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("purge_ddc"); }
	virtual FString GetDescription() const override { return TEXT("Run the DerivedDataCache.Cleanup console command (or a caller-specified DDC console command) to age out stale entries. NOTE: the public DDC API does not support targeted per-key purges — this is the available housekeeping operation."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("custom_command"), TEXT("string"), TEXT("Optional console command to run instead of DerivedDataCache.Cleanup (e.g. DDC.Verify, DDC.MountPak <path>)"), false, TEXT("DerivedDataCache.Cleanup") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * warm_ddc — load each asset in asset_list synchronously to trigger DDC fetch + cook
 * as a side effect of any first-use derived-data builds. Returns per-asset success/failure
 * + the DDC graph used.
 */
class FECACommand_WarmDDC : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("warm_ddc"); }
	virtual FString GetDescription() const override { return TEXT("Load each asset in asset_list synchronously so any required DDC entries are fetched or built. Returns per-asset success status."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_list"), TEXT("array"), TEXT("List of asset object paths to load (e.g. /Game/Foo/Bar)"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
