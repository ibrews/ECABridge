// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * launch_ndisplay_cluster — spawn UE child processes for one or more nodes of a
 * UDisplayClusterConfigurationData asset.
 *
 * Reads the config asset, then for each node in the (optionally filtered) node
 * set spawns the UE editor or game executable with `-dc_cluster -dc_cfg=<path>
 * -dc_node=<id>` plus any user-supplied extra args. Returns the spawned
 * { node_id, pid } pairs. The spawned process handles are tracked in module-
 * scoped static state so stop_ndisplay_cluster can terminate them.
 *
 * Gated by WITH_ECA_NDISPLAY so the command is compiled out on engines without
 * the nDisplay plugin.
 */
class FECACommand_LaunchNDisplayCluster : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("launch_ndisplay_cluster"); }
	virtual FString GetDescription() const override { return TEXT("Spawn UE child processes for the nodes of a UDisplayClusterConfigurationData asset. Each node gets its own process invoked with -dc_cluster -dc_cfg=<path> -dc_node=<id>. Returns { node_id, pid } pairs. Tracked so stop_ndisplay_cluster can terminate them."); }
	virtual FString GetCategory() const override { return TEXT("NDisplay"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("config_path"), TEXT("string"), TEXT("Asset path to a UDisplayClusterConfigurationData (e.g. /Game/nDisplay/MyConfig)."), true, TEXT("") },
			{ TEXT("nodes"), TEXT("array"), TEXT("Optional list of node IDs to launch. Default: every node in the config."), false, TEXT("") },
			{ TEXT("executable"), TEXT("string"), TEXT("Optional path to the UE executable. Defaults to the running editor/game executable (FPlatformProcess::ExecutablePath)."), false, TEXT("") },
			{ TEXT("project"), TEXT("string"), TEXT("Optional path to the .uproject. Defaults to the current project (FPaths::GetProjectFilePath())."), false, TEXT("") },
			{ TEXT("map"), TEXT("string"), TEXT("Optional map asset path to open. If empty, the process opens the project default map."), false, TEXT("") },
			{ TEXT("extra_args"), TEXT("string"), TEXT("Optional additional command-line flags appended verbatim to each spawned process."), false, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * stop_ndisplay_cluster — terminate every nDisplay node process spawned by an
 * earlier launch_ndisplay_cluster call.
 *
 * Walks the module-scoped tracked-process list and calls
 * FPlatformProcess::TerminateProc(.., bKillTree=true) on each, then clears the
 * list. Returns the number of processes terminated. Idempotent.
 */
class FECACommand_StopNDisplayCluster : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_ndisplay_cluster"); }
	virtual FString GetDescription() const override { return TEXT("Terminate every nDisplay node process spawned by an earlier launch_ndisplay_cluster call. Calls TerminateProc with bKillTree=true. Idempotent. Returns the number of processes terminated."); }
	virtual FString GetCategory() const override { return TEXT("NDisplay"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
