// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASourceControlValidateCommands.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "ISourceControlChangelist.h"
#include "ISourceControlChangelistState.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_ECA_DATAVALIDATION
#include "EditorValidatorSubsystem.h"
#endif

FECACommandResult FECACommand_ValidateBeforeSubmit::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ChangelistId;
	if (!GetStringParam(Params, TEXT("changelist_id"), ChangelistId, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'changelist_id' (string) is required."));
	}

	int32 MaxAssets = 100;
	if (Params.IsValid())
	{
		double D = 0.0;
		if (Params->TryGetNumberField(TEXT("max_assets"), D) && D > 0.0)
		{
			MaxAssets = static_cast<int32>(D);
		}
	}

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	if (!SCCModule.IsEnabled())
	{
		return FECACommandResult::Error(TEXT("Source control is not enabled in this project."));
	}
	ISourceControlProvider& Provider = SCCModule.GetProvider();
	if (!Provider.IsAvailable())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source control provider '%s' is not available."), *Provider.GetName().ToString()));
	}
	if (!Provider.UsesChangelists())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Provider '%s' does not use changelists. validate_before_submit is Perforce-specific."),
			*Provider.GetName().ToString()));
	}

	// Find the changelist.
	const TArray<FSourceControlChangelistRef> Changelists = Provider.GetChangelists(EStateCacheUsage::Use);
	FSourceControlChangelistRef* TargetCL = nullptr;
	FSourceControlChangelistPtr Found;
	for (const FSourceControlChangelistRef& CL : Changelists)
	{
		if (CL->GetIdentifier().Equals(ChangelistId, ESearchCase::IgnoreCase))
		{
			Found = CL;
			break;
		}
	}
	if (!Found.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Changelist '%s' not found among pending changelists for provider '%s'."),
			*ChangelistId, *Provider.GetName().ToString()));
	}

	// Get the changelist state -> list of file states.
	TArray<FSourceControlChangelistRef> ToQuery = { Found.ToSharedRef() };
	TArray<FSourceControlChangelistStateRef> CLStates;
	Provider.GetState(ToQuery, CLStates, EStateCacheUsage::Use);
	if (CLStates.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to retrieve changelist state."));
	}
	const FSourceControlChangelistStateRef& CLState = CLStates[0];
	const TArray<FSourceControlStateRef> FileStates = CLState->GetFilesStates();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Result->SetStringField(TEXT("changelist_id"), ChangelistId);
	Result->SetNumberField(TEXT("changelist_file_count"), FileStates.Num());

	// Convert file states -> asset data (the validator wants FAssetData).
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	TArray<TSharedPtr<FJsonValue>> SkippedArr;
	const int32 EffectiveCap = FMath::Min(MaxAssets, FileStates.Num());
	for (int32 i = 0; i < FileStates.Num() && AssetDataList.Num() < EffectiveCap; ++i)
	{
		const FString& Filename = FileStates[i]->GetFilename();
		FString PackageName;
		if (!FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
		{
			TSharedPtr<FJsonObject> Skip = MakeShared<FJsonObject>();
			Skip->SetStringField(TEXT("filename"), Filename);
			Skip->SetStringField(TEXT("reason"), TEXT("not_a_package"));
			SkippedArr.Add(MakeShared<FJsonValueObject>(Skip));
			continue;
		}
		TArray<FAssetData> AssetsForPackage;
		AssetRegistry.GetAssetsByPackageName(FName(*PackageName), AssetsForPackage);
		if (AssetsForPackage.Num() == 0)
		{
			TSharedPtr<FJsonObject> Skip = MakeShared<FJsonObject>();
			Skip->SetStringField(TEXT("filename"), Filename);
			Skip->SetStringField(TEXT("reason"), TEXT("no_asset_in_registry"));
			SkippedArr.Add(MakeShared<FJsonValueObject>(Skip));
			continue;
		}
		AssetDataList.Append(AssetsForPackage);
	}
	Result->SetNumberField(TEXT("validated_asset_count"), AssetDataList.Num());
	Result->SetArrayField(TEXT("skipped"), SkippedArr);

#if WITH_ECA_DATAVALIDATION
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available."));
	}
	UEditorValidatorSubsystem* Validator = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	if (!Validator)
	{
		return FECACommandResult::Error(TEXT("UEditorValidatorSubsystem is not available."));
	}

	FValidateAssetsSettings Settings;
	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.bCollectPerAssetDetails = true;
	Settings.ValidationUsecase = EDataValidationUsecase::PreSubmit;

	FValidateAssetsResults Results;
	const int32 FailureCount = Validator->ValidateAssetsWithSettings(AssetDataList, Settings, Results);

	Result->SetBoolField(TEXT("data_validation_available"), true);
	Result->SetBoolField(TEXT("passed"), FailureCount == 0);
	Result->SetNumberField(TEXT("num_requested"), Results.NumRequested);
	Result->SetNumberField(TEXT("num_checked"), Results.NumChecked);
	Result->SetNumberField(TEXT("num_valid"), Results.NumValid);
	Result->SetNumberField(TEXT("num_invalid"), Results.NumInvalid);
	Result->SetNumberField(TEXT("num_skipped"), Results.NumSkipped);
	Result->SetNumberField(TEXT("num_warnings"), Results.NumWarnings);
	Result->SetNumberField(TEXT("num_unable_to_validate"), Results.NumUnableToValidate);

	TArray<TSharedPtr<FJsonValue>> DetailsArr;
	for (const TPair<FString, FValidateAssetsDetails>& Pair : Results.AssetsDetails)
	{
		const FValidateAssetsDetails& Detail = Pair.Value;
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("asset"), Pair.Key);
		Entry->SetStringField(TEXT("package_name"), Detail.PackageName.ToString());
		Entry->SetStringField(TEXT("asset_name"), Detail.AssetName.ToString());
		const TCHAR* ResultLabel =
			Detail.Result == EDataValidationResult::Valid ? TEXT("valid") :
			Detail.Result == EDataValidationResult::Invalid ? TEXT("invalid") :
			TEXT("not_validated");
		Entry->SetStringField(TEXT("result"), ResultLabel);
		Entry->SetNumberField(TEXT("error_count"), Detail.ValidationErrors.Num());
		Entry->SetNumberField(TEXT("warning_count"), Detail.ValidationWarnings.Num());

		TArray<TSharedPtr<FJsonValue>> Errs;
		for (const FText& Err : Detail.ValidationErrors)
		{
			Errs.Add(MakeShared<FJsonValueString>(Err.ToString()));
		}
		Entry->SetArrayField(TEXT("errors"), Errs);

		TArray<TSharedPtr<FJsonValue>> Warns;
		for (const FText& W : Detail.ValidationWarnings)
		{
			Warns.Add(MakeShared<FJsonValueString>(W.ToString()));
		}
		Entry->SetArrayField(TEXT("warnings"), Warns);

		DetailsArr.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("details"), DetailsArr);
#else
	Result->SetBoolField(TEXT("data_validation_available"), false);
	Result->SetBoolField(TEXT("passed"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without DataValidation support (WITH_ECA_DATAVALIDATION=0). validate_before_submit cannot run."));
#endif

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ValidateBeforeSubmit);
