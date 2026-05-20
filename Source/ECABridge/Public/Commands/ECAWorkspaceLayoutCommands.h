// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/** Snapshot the current editor window layout INI to Saved/WorkspaceLayouts/<name>.ini. */
class FECACommand_SaveWorkspaceLayout : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_workspace_layout"); }
	virtual FString GetDescription() const override { return TEXT("Force the editor to flush its tab/dock layout to disk, then copy that snapshot to Saved/WorkspaceLayouts/<name>.ini."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Layout snapshot name (becomes the .ini filename)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Restore a saved workspace layout snapshot. */
class FECACommand_LoadWorkspaceLayout : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("load_workspace_layout"); }
	virtual FString GetDescription() const override { return TEXT("Copy a saved layout snapshot back to the active editor layout INI. Requires editor restart to take effect; this command does not restart the editor."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Layout snapshot name to restore"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Enumerate saved workspace layouts. */
class FECACommand_ListWorkspaceLayouts : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_workspace_layouts"); }
	virtual FString GetDescription() const override { return TEXT("List the workspace layout snapshots available under Saved/WorkspaceLayouts/."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }
	virtual bool IsMutating() const override { return false; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
