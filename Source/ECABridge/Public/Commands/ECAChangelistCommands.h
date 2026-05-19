// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * create_changelist — create a new pending changelist with the given description.
 *
 * Only meaningful for providers that report UsesChangelists() == true (Perforce). Other
 * providers will refuse with a clear error.
 */
class FECACommand_CreateChangelist : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_changelist"); }
	virtual FString GetDescription() const override { return TEXT("Create a new pending changelist on the active source control provider. Requires a provider that uses changelists (Perforce). Returns the new changelist identifier."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("description"), TEXT("string"), TEXT("Description for the new changelist"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * submit_changelist — submit the named pending changelist via FCheckIn. P4 'p4 submit -c <CL>'.
 *
 * Requires a provider that uses changelists (Perforce).
 */
class FECACommand_SubmitChangelist : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("submit_changelist"); }
	virtual FString GetDescription() const override { return TEXT("Submit a pending changelist on the active source control provider (Perforce). Optionally override the description and choose whether to keep files checked out after submit."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("changelist_id"), TEXT("string"), TEXT("Identifier of the pending changelist (e.g. P4 CL number)"), true, TEXT("") },
			{ TEXT("description"), TEXT("string"), TEXT("Optional description override for the submit"), false, TEXT("") },
			{ TEXT("keep_checked_out"), TEXT("boolean"), TEXT("Keep files checked out after submit (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * list_changelists — list all pending changelists known to the active provider.
 */
class FECACommand_ListChangelists : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_changelists"); }
	virtual FString GetDescription() const override { return TEXT("List all pending changelists on the active source control provider. Includes id, description, file count, and shelved file count per CL. Empty list for providers that don't use changelists."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("force_refresh"), TEXT("boolean"), TEXT("Bypass the cache and re-query the provider (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * move_to_changelist — reassign one or more files between pending changelists. P4 'p4 reopen -c <CL>'.
 */
class FECACommand_MoveToChangelist : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("move_to_changelist"); }
	virtual FString GetDescription() const override { return TEXT("Move asset(s) to a pending changelist on the active source control provider (Perforce). Pass asset_path or asset_paths plus changelist_id."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Single asset path"), false, TEXT("") },
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of asset paths"), false, TEXT("[]") },
			{ TEXT("changelist_id"), TEXT("string"), TEXT("Identifier of the target changelist"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
