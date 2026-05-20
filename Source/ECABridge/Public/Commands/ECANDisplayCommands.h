// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * get_ndisplay_status — report the active nDisplay operation mode and, when the
 * editor is acting as a cluster node, the node ID + cluster role + node count.
 *
 * In an editor session the operation mode is typically "Editor" or "Disabled"
 * (no live cluster bound), and there are no peer nodes. When the editor is
 * launched as a cluster slave/master via `-game -dc_cluster` etc., this command
 * reports the real role.
 *
 * Compiled out (WITH_ECA_NDISPLAY=0) when the nDisplay engine plugin isn't
 * installed; at runtime the command also unregisters if the DisplayCluster
 * module fails to load (see ECABridgeModule).
 */
class FECACommand_GetNDisplayStatus : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_ndisplay_status"); }
	virtual FString GetDescription() const override { return TEXT("Get the current nDisplay cluster status: operation mode, node id (when running as a cluster node), cluster role, primary node id, and peer node count."); }
	virtual FString GetCategory() const override { return TEXT("NDisplay"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * list_ndisplay_root_actors — find every `ADisplayClusterRootActor` in the
 * current editor world and return each one's actor name + label + config asset
 * path. Useful for confirming which nDisplay configuration is loaded into the
 * current map.
 */
class FECACommand_ListNDisplayRootActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_ndisplay_root_actors"); }
	virtual FString GetDescription() const override { return TEXT("List every ADisplayClusterRootActor in the current editor world with its actor name, label, and bound config asset path."); }
	virtual FString GetCategory() const override { return TEXT("NDisplay"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
