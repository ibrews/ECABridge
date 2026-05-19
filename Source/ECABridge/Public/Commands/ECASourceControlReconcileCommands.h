// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * reconcile_offline_changes — sync the editor's source-control state cache against the
 * filesystem under a content path. Useful after editing files outside the editor (a sync
 * from a peer, a CLI 'p4 sync', or hand-edits while the editor was closed).
 *
 * Returns the set of paths whose modified/added/deleted status changed as a result, so
 * the caller can see what got reconciled without scanning the entire content tree.
 */
class FECACommand_ReconcileOfflineChanges : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("reconcile_offline_changes"); }
	virtual FString GetDescription() const override { return TEXT("Reconcile the source-control state cache against the filesystem for assets under path_filter. Runs a forced FUpdateStatus with modified-state detection and reports which files came back modified/added/deleted compared to head."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Content path to reconcile (default /Game/)"), false, TEXT("/Game/") },
			{ TEXT("update_history"), TEXT("boolean"), TEXT("Also pull history (slower; default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
