// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/** Save the active editor viewport's location/rotation/FOV into a named slot. */
class FECACommand_CreateViewportBookmark : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_viewport_bookmark"); }
	virtual FString GetDescription() const override { return TEXT("Save the active editor viewport's pose (location, rotation, FOV) under a named slot for later jump_to_bookmark."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("slot"),  TEXT("string"), TEXT("Bookmark slot identifier (any string, e.g., 'overview', 'A', '0')"), true },
			{ TEXT("label"), TEXT("string"), TEXT("Optional human-readable label for the bookmark"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Restore the active viewport to a saved bookmark slot. */
class FECACommand_JumpToBookmark : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("jump_to_bookmark"); }
	virtual FString GetDescription() const override { return TEXT("Move the active editor viewport to the location/rotation/FOV stored in a named bookmark slot."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("slot"), TEXT("string"), TEXT("Bookmark slot identifier to jump to"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Enumerate saved viewport bookmarks (per-project, stored under Saved/ViewportBookmarks.json). */
class FECACommand_ListBookmarks : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_bookmarks"); }
	virtual FString GetDescription() const override { return TEXT("List saved viewport bookmark slots and their stored pose data."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
