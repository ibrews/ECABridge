// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * check_out_asset — open one or more assets for edit via the active source control provider.
 *
 * For P4 this is `p4 edit`. For Git providers (which don't gate edits) this is a no-op that
 * succeeds. The command always returns the resolved filenames and the SCC provider name so
 * the caller can confirm what was touched.
 */
class FECACommand_CheckOutAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("check_out_asset"); }
	virtual FString GetDescription() const override { return TEXT("Check out one or more assets for edit through the active source control provider. Pass asset_path (string) or asset_paths (string array). For Perforce this issues 'p4 edit'; for Git/no-op providers it succeeds without changing state."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }
	// Note: mutates source-control state (issues 'p4 edit'), so left as IsMutating()=true.

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Single asset path (e.g. /Game/Foo/Bar)"), false, TEXT("") },
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of asset paths"), false, TEXT("[]") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * revert_asset — discard local changes for the given asset(s), restoring them to the
 * head revision in the depot. P4 'p4 revert', Git 'git checkout -- <file>'.
 */
class FECACommand_RevertAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("revert_asset"); }
	virtual FString GetDescription() const override { return TEXT("Revert (discard local changes for) one or more assets through the active source control provider. Pass asset_path or asset_paths. Optional soft_revert keeps the local file editable but unbinds from any pending CL."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Single asset path"), false, TEXT("") },
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of asset paths"), false, TEXT("[]") },
			{ TEXT("soft_revert"), TEXT("boolean"), TEXT("If true, perform a soft revert (keep editable but drop CL membership). P4-only."), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * mark_for_add — schedule one or more new assets for addition to source control on the
 * next submit. P4 'p4 add', Git stage-for-add.
 */
class FECACommand_MarkForAdd : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("mark_for_add"); }
	virtual FString GetDescription() const override { return TEXT("Mark one or more new assets for add through the active source control provider, so they'll be included in the next submit. Pass asset_path or asset_paths."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Single asset path"), false, TEXT("") },
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of asset paths"), false, TEXT("[]") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * mark_for_delete — schedule one or more assets for deletion from source control on
 * the next submit. P4 'p4 delete', Git stage-for-delete.
 */
class FECACommand_MarkForDelete : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("mark_for_delete"); }
	virtual FString GetDescription() const override { return TEXT("Mark one or more assets for delete through the active source control provider. Pass asset_path or asset_paths."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Single asset path"), false, TEXT("") },
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of asset paths"), false, TEXT("[]") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
