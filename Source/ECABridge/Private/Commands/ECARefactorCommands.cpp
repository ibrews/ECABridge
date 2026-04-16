// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECARefactorCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/UObjectIterator.h"

// Register all refactor commands
REGISTER_ECA_COMMAND(FECACommand_ReplaceAssetReferences)
REGISTER_ECA_COMMAND(FECACommand_BulkRenameAssets)
REGISTER_ECA_COMMAND(FECACommand_SearchAndReplaceProperty)

//------------------------------------------------------------------------------
// Replace Asset References
//------------------------------------------------------------------------------

FECACommandResult FECACommand_ReplaceAssetReferences::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString OldAssetPath;
	if (!GetStringParam(Params, TEXT("old_asset_path"), OldAssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: old_asset_path"));
	}

	FString NewAssetPath;
	if (!GetStringParam(Params, TEXT("new_asset_path"), NewAssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: new_asset_path"));
	}

	bool bDryRun = false;
	GetBoolParam(Params, TEXT("dry_run"), bDryRun, false);

	// Load the old asset
	UObject* OldAsset = LoadObject<UObject>(nullptr, *OldAssetPath);
	if (!OldAsset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Old asset not found: %s"), *OldAssetPath));
	}

	// Load the new asset
	UObject* NewAsset = LoadObject<UObject>(nullptr, *NewAssetPath);
	if (!NewAsset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("New asset not found: %s"), *NewAssetPath));
	}

	// Get the package name for the old asset to find referencers
	FName OldPackageName = FName(*OldAsset->GetOutermost()->GetName());

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Find all referencers of the old asset
	TArray<FName> Referencers;
	AssetRegistry.GetReferencers(OldPackageName, Referencers);

	// Filter to only /Game/ paths (skip engine/plugin content)
	TArray<FName> GameReferencers;
	for (const FName& RefName : Referencers)
	{
		if (RefName.ToString().StartsWith(TEXT("/Game/")))
		{
			GameReferencers.Add(RefName);
		}
	}

	// Build list of affected assets
	TArray<TSharedPtr<FJsonValue>> AffectedAssetsArray;
	for (const FName& RefPackage : GameReferencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(RefPackage, Assets);

		for (const FAssetData& AssetData : Assets)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			AssetObj->SetStringField(TEXT("package"), RefPackage.ToString());
			AssetObj->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
			AssetObj->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
			AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
			AffectedAssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
	}

	int32 ReplacementsMade = 0;

	if (!bDryRun && GameReferencers.Num() > 0)
	{
		// Load all referencing objects so ForceReplaceReferences can find them
		for (const FName& RefPackage : GameReferencers)
		{
			LoadObject<UObject>(nullptr, *RefPackage.ToString());
		}

		// Use ObjectTools to consolidate references (replaces old with new)
		TArray<UObject*> ObjectsToConsolidate;
		ObjectsToConsolidate.Add(OldAsset);
		ObjectTools::ConsolidateObjects(NewAsset, ObjectsToConsolidate, false);
		ReplacementsMade = GameReferencers.Num();

		// Mark affected packages as dirty so they save with the new references
		for (const FName& RefPackage : GameReferencers)
		{
			UPackage* Package = FindPackage(nullptr, *RefPackage.ToString());
			if (Package)
			{
				Package->MarkPackageDirty();
			}
		}
	}
	else if (bDryRun)
	{
		ReplacementsMade = GameReferencers.Num();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("dry_run"), bDryRun);
	Result->SetStringField(TEXT("old_asset"), OldAssetPath);
	Result->SetStringField(TEXT("new_asset"), NewAssetPath);
	Result->SetNumberField(TEXT("replacements_made"), ReplacementsMade);
	Result->SetArrayField(TEXT("affected_assets"), AffectedAssetsArray);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// Bulk Rename Assets
//------------------------------------------------------------------------------

FECACommandResult FECACommand_BulkRenameAssets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter;
	if (!GetStringParam(Params, TEXT("path_filter"), PathFilter))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: path_filter"));
	}

	FString Find;
	if (!GetStringParam(Params, TEXT("find"), Find))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: find"));
	}

	FString Replace;
	if (!GetStringParam(Params, TEXT("replace"), Replace))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: replace"));
	}

	FString ClassFilter;
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, false);

	bool bDryRun = false;
	GetBoolParam(Params, TEXT("dry_run"), bDryRun, false);

	// Query the asset registry
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*PathFilter));
	Filter.bRecursivePaths = true;

	// Apply class filter if provided
	if (!ClassFilter.IsEmpty())
	{
		UClass* FilterClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassFilter));
		if (!FilterClass)
		{
			FilterClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassFilter));
		}
		if (!FilterClass)
		{
			// Try searching all loaded classes
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == ClassFilter)
				{
					FilterClass = *It;
					break;
				}
			}
		}
		if (FilterClass)
		{
			Filter.ClassPaths.Add(FilterClass->GetClassPathName());
			Filter.bRecursiveClasses = true;
		}
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Find assets whose names contain the search string and build rename data
	TArray<FAssetRenameData> RenameData;
	TArray<TSharedPtr<FJsonValue>> RenamesArray;

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();
		if (!AssetName.Contains(Find))
		{
			continue;
		}

		FString NewName = AssetName.Replace(*Find, *Replace);
		FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
		FString OldObjectPath = AssetData.GetObjectPathString();
		FString NewObjectPath = PackagePath / NewName + TEXT(".") + NewName;

		TSharedPtr<FJsonObject> RenameObj = MakeShared<FJsonObject>();
		RenameObj->SetStringField(TEXT("old_name"), AssetName);
		RenameObj->SetStringField(TEXT("new_name"), NewName);
		RenameObj->SetStringField(TEXT("old_path"), OldObjectPath);
		RenameObj->SetStringField(TEXT("new_path"), NewObjectPath);
		RenamesArray.Add(MakeShared<FJsonValueObject>(RenameObj));

		if (!bDryRun)
		{
			UObject* Asset = AssetData.GetAsset();
			if (Asset)
			{
				RenameData.Add(FAssetRenameData(Asset, PackagePath, NewName));
			}
		}
	}

	// Perform the rename
	int32 RenamedCount = 0;
	if (!bDryRun && RenameData.Num() > 0)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		bool bSuccess = AssetToolsModule.Get().RenameAssets(RenameData);
		RenamedCount = bSuccess ? RenameData.Num() : 0;
	}
	else
	{
		RenamedCount = RenamesArray.Num();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("dry_run"), bDryRun);
	Result->SetStringField(TEXT("path_filter"), PathFilter);
	Result->SetStringField(TEXT("find"), Find);
	Result->SetStringField(TEXT("replace"), Replace);
	Result->SetNumberField(TEXT("renamed_count"), RenamedCount);
	Result->SetArrayField(TEXT("renames"), RenamesArray);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// Search and Replace Property
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SearchAndReplaceProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassFilter;
	if (!GetStringParam(Params, TEXT("class_filter"), ClassFilter))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: class_filter"));
	}

	FString PropertyName;
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_name"));
	}

	FString FindValue;
	GetStringParam(Params, TEXT("find_value"), FindValue, false);

	FString ReplaceValue;
	if (!GetStringParam(Params, TEXT("replace_value"), ReplaceValue))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: replace_value"));
	}

	bool bDryRun = false;
	GetBoolParam(Params, TEXT("dry_run"), bDryRun, false);

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	int32 MatchesFound = 0;
	int32 ChangesMade = 0;
	TArray<TSharedPtr<FJsonValue>> AffectedActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Apply class filter using class name matching
		FString ActorClassName = Actor->GetClass()->GetName();
		if (!ActorClassName.Contains(ClassFilter))
		{
			continue;
		}

		// Find the property by name using reflection
		FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			continue;
		}

		// Export the current value to a string
		FString CurrentValue;
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
		Property->ExportTextItem_Direct(CurrentValue, ValuePtr, nullptr, Actor, PPF_None);

		// Check if it matches the find_value (if provided)
		bool bMatches = FindValue.IsEmpty() || CurrentValue == FindValue;
		if (!bMatches)
		{
			continue;
		}

		MatchesFound++;

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
		ActorObj->SetStringField(TEXT("internal_name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("class"), ActorClassName);
		ActorObj->SetStringField(TEXT("property"), PropertyName);
		ActorObj->SetStringField(TEXT("old_value"), CurrentValue);
		ActorObj->SetStringField(TEXT("new_value"), ReplaceValue);

		if (!bDryRun)
		{
			// Use ImportText to set the new value
			const TCHAR* ImportResult = Property->ImportText_Direct(*ReplaceValue, ValuePtr, Actor, PPF_None);
			if (ImportResult)
			{
				Actor->MarkPackageDirty();
				Actor->PostEditChange();
				ChangesMade++;
				ActorObj->SetBoolField(TEXT("changed"), true);
			}
			else
			{
				ActorObj->SetBoolField(TEXT("changed"), false);
				ActorObj->SetStringField(TEXT("error"), TEXT("Failed to import new value"));
			}
		}
		else
		{
			ActorObj->SetBoolField(TEXT("would_change"), true);
		}

		AffectedActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("dry_run"), bDryRun);
	Result->SetStringField(TEXT("class_filter"), ClassFilter);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetNumberField(TEXT("matches_found"), MatchesFound);
	Result->SetNumberField(TEXT("changes_made"), bDryRun ? 0 : ChangesMade);
	Result->SetArrayField(TEXT("affected_actors"), AffectedActorsArray);

	return FECACommandResult::Success(Result);
}
