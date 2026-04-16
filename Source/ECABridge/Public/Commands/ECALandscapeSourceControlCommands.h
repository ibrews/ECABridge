// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Dump landscape actors in the current level with their layers, components, and materials.
 */
class FECACommand_DumpLandscape : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_landscape"); }
	virtual FString GetDescription() const override { return TEXT("Serialize landscape actors in the current level: layer names, landscape material, component counts, landscape size, heightmap resolution. One call to understand all terrain in a level."); }
	virtual FString GetCategory() const override { return TEXT("Landscape"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get source control status: which assets are checked out, modified, added, or conflicted.
 */
class FECACommand_GetSourceControlStatus : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_source_control_status"); }
	virtual FString GetDescription() const override { return TEXT("Get source control status: provider (Perforce/Git/etc), which files are checked out, modified, added, or conflicted. Essential for team workflows."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Content path to scan (default /Game/)"), false, TEXT("/Game/") },
			{ TEXT("include_unchanged"), TEXT("boolean"), TEXT("Include files with no pending changes (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
