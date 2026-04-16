// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECALandscapeSourceControlCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeComponent.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "SourceControlOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

//////////////////////////////////////////////////////////////////////////
// FECACommand_DumpLandscape

FECACommandResult FECACommand_DumpLandscape::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available."));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available."));
	}

	TArray<TSharedPtr<FJsonValue>> LandscapesArray;
	int32 LandscapeCount = 0;

	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		if (!Landscape)
		{
			continue;
		}

		TSharedPtr<FJsonObject> LandscapeObj = MakeShared<FJsonObject>();

		LandscapeObj->SetStringField(TEXT("actor_label"), Landscape->GetActorLabel());
		LandscapeObj->SetStringField(TEXT("actor_name"), Landscape->GetName());
		LandscapeObj->SetStringField(TEXT("actor_path"), Landscape->GetPathName());

		// Material
		UMaterialInterface* LandscapeMaterial = Landscape->GetLandscapeMaterial();
		if (LandscapeMaterial)
		{
			LandscapeObj->SetStringField(TEXT("landscape_material"), LandscapeMaterial->GetPathName());
		}
		else
		{
			LandscapeObj->SetStringField(TEXT("landscape_material"), TEXT(""));
		}

		// Component info
		LandscapeObj->SetNumberField(TEXT("component_count"), Landscape->LandscapeComponents.Num());
		LandscapeObj->SetNumberField(TEXT("component_size_quads"), Landscape->ComponentSizeQuads);
		LandscapeObj->SetNumberField(TEXT("num_subsections"), Landscape->NumSubsections);
		LandscapeObj->SetNumberField(TEXT("subsection_size_quads"), Landscape->SubsectionSizeQuads);

		// Actor location
		const FVector ActorLocation = Landscape->GetActorLocation();
		TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
		LocationObj->SetNumberField(TEXT("x"), ActorLocation.X);
		LocationObj->SetNumberField(TEXT("y"), ActorLocation.Y);
		LocationObj->SetNumberField(TEXT("z"), ActorLocation.Z);
		LandscapeObj->SetObjectField(TEXT("location"), LocationObj);

		// Layers
		TArray<TSharedPtr<FJsonValue>> LayersArray;
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (LandscapeInfo)
		{
			for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
			{
				TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
				LayerObj->SetStringField(TEXT("layer_name"), LayerSettings.GetLayerName().ToString());

				if (LayerSettings.LayerInfoObj)
				{
					LayerObj->SetStringField(TEXT("layer_info_path"), LayerSettings.LayerInfoObj->GetPathName());
				}
				else
				{
					LayerObj->SetStringField(TEXT("layer_info_path"), TEXT(""));
				}

				LayersArray.Add(MakeShared<FJsonValueObject>(LayerObj));
			}
		}
		LandscapeObj->SetArrayField(TEXT("layers"), LayersArray);

		LandscapesArray.Add(MakeShared<FJsonValueObject>(LandscapeObj));
		++LandscapeCount;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("landscape_count"), LandscapeCount);
	ResultObj->SetArrayField(TEXT("landscapes"), LandscapesArray);

	return FECACommandResult::Success(ResultObj);
}

REGISTER_ECA_COMMAND(FECACommand_DumpLandscape);

//////////////////////////////////////////////////////////////////////////
// FECACommand_GetSourceControlStatus

FECACommandResult FECACommand_GetSourceControlStatus::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	bool bIncludeUnchanged = false;

	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("path_filter"), PathFilter);
		Params->TryGetBoolField(TEXT("include_unchanged"), bIncludeUnchanged);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("path_filter"), PathFilter);
	ResultObj->SetBoolField(TEXT("include_unchanged"), bIncludeUnchanged);

	ISourceControlModule& SCCModule = ISourceControlModule::Get();

	if (!SCCModule.IsEnabled())
	{
		ResultObj->SetStringField(TEXT("provider"), TEXT("none"));
		ResultObj->SetBoolField(TEXT("enabled"), false);
		ResultObj->SetStringField(TEXT("message"), TEXT("Source control is not enabled."));
		ResultObj->SetArrayField(TEXT("file_states"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(ResultObj);
	}

	ISourceControlProvider& SCCProvider = SCCModule.GetProvider();
	const FString ProviderName = SCCProvider.GetName().ToString();

	ResultObj->SetStringField(TEXT("provider"), ProviderName);
	ResultObj->SetBoolField(TEXT("enabled"), true);
	ResultObj->SetBoolField(TEXT("available"), SCCProvider.IsAvailable());

	// Query asset registry for all assets under the path filter
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*PathFilter), AssetDataList, /*bRecursive=*/ true);

	// Collect filenames
	TArray<FString> Filenames;
	TMap<FString, FString> FilenameToPackageName;
	Filenames.Reserve(AssetDataList.Num());

	TSet<FString> UniquePackageNames;
	for (const FAssetData& AssetData : AssetDataList)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		if (UniquePackageNames.Contains(PackageName))
		{
			continue;
		}
		UniquePackageNames.Add(PackageName);

		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
		{
			const FString FullFilename = FPaths::ConvertRelativePathToFull(Filename);
			Filenames.Add(FullFilename);
			FilenameToPackageName.Add(FullFilename, PackageName);
		}
	}

	ResultObj->SetNumberField(TEXT("scanned_file_count"), Filenames.Num());

	TArray<TSharedPtr<FJsonValue>> FileStatesArray;

	if (Filenames.Num() > 0 && SCCProvider.IsAvailable())
	{
		TArray<FSourceControlStateRef> States;
		ECommandResult::Type QueryResult = SCCProvider.GetState(Filenames, States, EStateCacheUsage::Use);

		if (QueryResult == ECommandResult::Succeeded)
		{
			for (const FSourceControlStateRef& State : States)
			{
				const FString Filename = State->GetFilename();
				const bool bCheckedOut = State->IsCheckedOut();
				const bool bModified = State->IsModified();
				const bool bAdded = State->IsAdded();
				const bool bConflicted = State->IsConflicted();
				const bool bDeleted = State->IsDeleted();
				const bool bIgnored = State->IsIgnored();
				const bool bUnknown = State->IsUnknown();
				const bool bSourceControlled = State->IsSourceControlled();

				const bool bHasChanges = bCheckedOut || bModified || bAdded || bConflicted || bDeleted;

				if (!bIncludeUnchanged && !bHasChanges)
				{
					continue;
				}

				TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
				FileObj->SetStringField(TEXT("filename"), Filename);

				const FString* PackageNamePtr = FilenameToPackageName.Find(Filename);
				FileObj->SetStringField(TEXT("package_name"), PackageNamePtr ? *PackageNamePtr : TEXT(""));

				FileObj->SetBoolField(TEXT("is_checked_out"), bCheckedOut);
				FileObj->SetBoolField(TEXT("is_modified"), bModified);
				FileObj->SetBoolField(TEXT("is_added"), bAdded);
				FileObj->SetBoolField(TEXT("is_conflicted"), bConflicted);
				FileObj->SetBoolField(TEXT("is_deleted"), bDeleted);
				FileObj->SetBoolField(TEXT("is_ignored"), bIgnored);
				FileObj->SetBoolField(TEXT("is_unknown"), bUnknown);
				FileObj->SetBoolField(TEXT("is_source_controlled"), bSourceControlled);
				FileObj->SetStringField(TEXT("status_text"), State->GetDisplayName().ToString());
				FileObj->SetStringField(TEXT("status_tooltip"), State->GetDisplayTooltip().ToString());

				FileStatesArray.Add(MakeShared<FJsonValueObject>(FileObj));
			}
		}
		else
		{
			ResultObj->SetStringField(TEXT("query_error"), TEXT("Failed to query source control state."));
		}
	}

	ResultObj->SetNumberField(TEXT("reported_file_count"), FileStatesArray.Num());
	ResultObj->SetArrayField(TEXT("file_states"), FileStatesArray);

	return FECACommandResult::Success(ResultObj);
}

REGISTER_ECA_COMMAND(FECACommand_GetSourceControlStatus);
