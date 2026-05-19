// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/** Sync the content browser to a given folder path and optionally apply a search keyword
 *  (the content browser's search bar accepts asset-type tokens like 'Material',
 *  'StaticMesh', etc. — passing one as `asset_type` substitutes for the unavailable
 *  programmatic class filter on the default content browser). */
class FECACommand_SetContentBrowserFilter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_content_browser_filter"); }
	virtual FString GetDescription() const override { return TEXT("Navigate the Content Browser to the given folder path, optionally appending an asset_type keyword to the search bar."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"),       TEXT("string"), TEXT("Folder path to focus on (e.g., '/Game/FirstPerson'). Defaults to '/Game'."), false },
			{ TEXT("asset_type"), TEXT("string"), TEXT("Optional asset-type search keyword (e.g., 'Material', 'StaticMesh'). Applied via the search bar."), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Clear the content browser's search keyword and sync back to /Game. */
class FECACommand_ClearContentBrowserFilter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("clear_content_browser_filter"); }
	virtual FString GetDescription() const override { return TEXT("Clear the Content Browser search keyword and sync back to /Game."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
