// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Replace all references to one asset with another across the project.
 * Essential for safe refactoring — swap textures, materials, meshes without breaking anything.
 */
class FECACommand_ReplaceAssetReferences : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("replace_asset_references"); }
	virtual FString GetDescription() const override { return TEXT("Replace all references to one asset with another across the entire project. Finds every asset that references the old asset and rewires it to the new one. Essential for safe refactoring."); }
	virtual FString GetCategory() const override { return TEXT("Refactor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("old_asset_path"), TEXT("string"), TEXT("Content path to the asset being replaced"), true },
			{ TEXT("new_asset_path"), TEXT("string"), TEXT("Content path to the replacement asset"), true },
			{ TEXT("dry_run"), TEXT("boolean"), TEXT("If true, only report what would change without making changes (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename multiple assets using pattern matching.
 */
class FECACommand_BulkRenameAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("bulk_rename_assets"); }
	virtual FString GetDescription() const override { return TEXT("Rename multiple assets using find/replace pattern. Supports prefix/suffix addition and substring replacement."); }
	virtual FString GetCategory() const override { return TEXT("Refactor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Content path to search in"), true },
			{ TEXT("find"), TEXT("string"), TEXT("Substring to find in asset names"), true },
			{ TEXT("replace"), TEXT("string"), TEXT("Replacement substring"), true },
			{ TEXT("class_filter"), TEXT("string"), TEXT("Only rename assets of this class (optional)"), false },
			{ TEXT("dry_run"), TEXT("boolean"), TEXT("Preview changes without renaming (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Find all actors with a specific property value and change it.
 * Bulk property editing across the level.
 */
class FECACommand_SearchAndReplaceProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("search_and_replace_property"); }
	virtual FString GetDescription() const override { return TEXT("Find all actors in the level with a specific property value and change it. Bulk property editing — e.g., change all PointLights with intensity 5000 to 3000, or set all StaticMeshActors with tag 'old' to tag 'new'."); }
	virtual FString GetCategory() const override { return TEXT("Refactor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_filter"), TEXT("string"), TEXT("Actor class to search (e.g., PointLight, StaticMeshActor). Required."), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("Property name to search/modify"), true },
			{ TEXT("find_value"), TEXT("string"), TEXT("Current value to match (as string). Leave empty to match all actors of the class."), false },
			{ TEXT("replace_value"), TEXT("string"), TEXT("New value to set"), true },
			{ TEXT("dry_run"), TEXT("boolean"), TEXT("Preview matches without making changes (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
