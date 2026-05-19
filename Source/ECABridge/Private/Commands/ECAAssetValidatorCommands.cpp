// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAssetValidatorCommands.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Misc/PackageName.h"

#if WITH_ECA_DATAVALIDATION
#include "EditorValidatorBase.h"
#include "EditorValidatorSubsystem.h"
#endif

namespace
{
#if WITH_ECA_DATAVALIDATION
	EDataValidationUsecase ParseUsecase(const FString& UsecaseStr)
	{
		if (UsecaseStr.Equals(TEXT("Save"), ESearchCase::IgnoreCase))       return EDataValidationUsecase::Save;
		if (UsecaseStr.Equals(TEXT("PreSubmit"), ESearchCase::IgnoreCase))  return EDataValidationUsecase::PreSubmit;
		if (UsecaseStr.Equals(TEXT("Commandlet"), ESearchCase::IgnoreCase)) return EDataValidationUsecase::Commandlet;
		if (UsecaseStr.Equals(TEXT("Script"), ESearchCase::IgnoreCase))     return EDataValidationUsecase::Script;
		return EDataValidationUsecase::Manual;
	}

	FString ResolvePackageName(const FString& InPath)
	{
		// Accept either object paths (/Game/Foo/Bar or /Game/Foo/Bar.Bar) or raw
		// package paths. Strip an asset-name suffix if present.
		FString Package = InPath;
		int32 DotIdx = INDEX_NONE;
		if (Package.FindChar('.', DotIdx))
		{
			Package = Package.Left(DotIdx);
		}
		return Package;
	}

	UEditorValidatorSubsystem* GetValidator()
	{
		if (!GEditor) return nullptr;
		return GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	}
#endif
}

FECACommandResult FECACommand_ListValidationRules::Execute(const TSharedPtr<FJsonObject>& /*Params*/)
{
#if WITH_ECA_DATAVALIDATION
	UEditorValidatorSubsystem* Validator = GetValidator();
	if (!Validator)
	{
		return FECACommandResult::Error(TEXT("UEditorValidatorSubsystem is not available."));
	}

	TArray<TSharedPtr<FJsonValue>> Arr;
	Validator->ForEachEnabledValidator([&Arr](UEditorValidatorBase* V) -> bool
	{
		if (!V) return true;
		UClass* C = V->GetClass();
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("class_name"), C ? C->GetName() : TEXT(""));
		Entry->SetStringField(TEXT("class_path"), C ? C->GetPathName() : TEXT(""));
		Entry->SetBoolField(TEXT("enabled"), V->IsEnabled());
		Entry->SetStringField(TEXT("source"), (C && C->ClassGeneratedBy) ? TEXT("Blueprint") : TEXT("Native"));
		Arr.Add(MakeShared<FJsonValueObject>(Entry));
		return true;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("data_validation_available"), true);
	Result->SetNumberField(TEXT("validator_count"), Arr.Num());
	Result->SetArrayField(TEXT("validators"), Arr);
	return FECACommandResult::Success(Result);
#else
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("data_validation_available"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without DataValidation (WITH_ECA_DATAVALIDATION=0)."));
	return FECACommandResult::Success(Result);
#endif
}

FECACommandResult FECACommand_RunAssetValidator::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr;
	if (!GetArrayParam(Params, TEXT("asset_paths"), PathsArr, /*bRequired=*/ true) || !PathsArr)
	{
		return FECACommandResult::ValidationError(this, TEXT("'asset_paths' (array of strings) is required."));
	}
	FString UsecaseStr;
	GetStringParam(Params, TEXT("usecase"), UsecaseStr, /*bRequired=*/ false);

#if WITH_ECA_DATAVALIDATION
	UEditorValidatorSubsystem* Validator = GetValidator();
	if (!Validator)
	{
		return FECACommandResult::Error(TEXT("UEditorValidatorSubsystem is not available."));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	TArray<TSharedPtr<FJsonValue>> Skipped;
	for (const TSharedPtr<FJsonValue>& V : *PathsArr)
	{
		if (!V.IsValid() || V->Type != EJson::String) continue;
		const FString Raw = V->AsString();
		const FString Package = ResolvePackageName(Raw);

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(FName(*Package), Assets);
		if (Assets.Num() == 0)
		{
			TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
			S->SetStringField(TEXT("input"), Raw);
			S->SetStringField(TEXT("reason"), TEXT("no_asset_in_registry"));
			Skipped.Add(MakeShared<FJsonValueObject>(S));
			continue;
		}
		AssetDataList.Append(Assets);
	}

	FValidateAssetsSettings Settings;
	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.bCollectPerAssetDetails = true;
	Settings.ValidationUsecase = ParseUsecase(UsecaseStr);

	FValidateAssetsResults Results;
	const int32 FailureCount = Validator->ValidateAssetsWithSettings(AssetDataList, Settings, Results);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("data_validation_available"), true);
	Result->SetBoolField(TEXT("passed"), FailureCount == 0);
	Result->SetNumberField(TEXT("num_requested"), Results.NumRequested);
	Result->SetNumberField(TEXT("num_checked"), Results.NumChecked);
	Result->SetNumberField(TEXT("num_valid"), Results.NumValid);
	Result->SetNumberField(TEXT("num_invalid"), Results.NumInvalid);
	Result->SetNumberField(TEXT("num_skipped"), Results.NumSkipped);
	Result->SetNumberField(TEXT("num_warnings"), Results.NumWarnings);
	Result->SetNumberField(TEXT("num_unable_to_validate"), Results.NumUnableToValidate);
	Result->SetArrayField(TEXT("skipped_inputs"), Skipped);

	TArray<TSharedPtr<FJsonValue>> Details;
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
		for (const FText& Err : Detail.ValidationErrors) Errs.Add(MakeShared<FJsonValueString>(Err.ToString()));
		Entry->SetArrayField(TEXT("errors"), Errs);
		TArray<TSharedPtr<FJsonValue>> Warns;
		for (const FText& W : Detail.ValidationWarnings) Warns.Add(MakeShared<FJsonValueString>(W.ToString()));
		Entry->SetArrayField(TEXT("warnings"), Warns);

		Details.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("details"), Details);
	return FECACommandResult::Success(Result);
#else
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("data_validation_available"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without DataValidation (WITH_ECA_DATAVALIDATION=0)."));
	return FECACommandResult::Success(Result);
#endif
}

FECACommandResult FECACommand_DumpValidationReport::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContentPath;
	if (!GetStringParam(Params, TEXT("content_path"), ContentPath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'content_path' (string) is required."));
	}
	int32 MaxAssets = 200;
	if (Params.IsValid())
	{
		double D = 0.0;
		if (Params->TryGetNumberField(TEXT("max_assets"), D) && D > 0.0) MaxAssets = static_cast<int32>(D);
	}

#if WITH_ECA_DATAVALIDATION
	UEditorValidatorSubsystem* Validator = GetValidator();
	if (!Validator)
	{
		return FECACommandResult::Error(TEXT("UEditorValidatorSubsystem is not available."));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*ContentPath), AssetDataList, /*bRecursive=*/ true);
	if (AssetDataList.Num() > MaxAssets)
	{
		AssetDataList.SetNum(MaxAssets);
	}

	FValidateAssetsSettings Settings;
	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.bCollectPerAssetDetails = true;
	Settings.ValidationUsecase = EDataValidationUsecase::Manual;

	FValidateAssetsResults Results;
	const int32 FailureCount = Validator->ValidateAssetsWithSettings(AssetDataList, Settings, Results);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("content_path"), ContentPath);
	Result->SetBoolField(TEXT("passed"), FailureCount == 0);
	Result->SetNumberField(TEXT("num_requested"), Results.NumRequested);
	Result->SetNumberField(TEXT("num_invalid"), Results.NumInvalid);
	Result->SetNumberField(TEXT("num_warnings"), Results.NumWarnings);
	Result->SetNumberField(TEXT("num_unable_to_validate"), Results.NumUnableToValidate);

	TArray<TSharedPtr<FJsonValue>> Summary;
	for (const TPair<FString, FValidateAssetsDetails>& Pair : Results.AssetsDetails)
	{
		const FValidateAssetsDetails& Detail = Pair.Value;
		// Compact: only assets that aren't cleanly valid get listed.
		if (Detail.Result == EDataValidationResult::Valid && Detail.ValidationWarnings.Num() == 0)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("asset"), Pair.Key);
		const TCHAR* ResultLabel =
			Detail.Result == EDataValidationResult::Valid ? TEXT("valid_with_warnings") :
			Detail.Result == EDataValidationResult::Invalid ? TEXT("invalid") :
			TEXT("not_validated");
		Entry->SetStringField(TEXT("result"), ResultLabel);
		Entry->SetNumberField(TEXT("error_count"), Detail.ValidationErrors.Num());
		Entry->SetNumberField(TEXT("warning_count"), Detail.ValidationWarnings.Num());
		Summary.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("issues"), Summary);
	return FECACommandResult::Success(Result);
#else
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("content_path"), ContentPath);
	Result->SetBoolField(TEXT("data_validation_available"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without DataValidation (WITH_ECA_DATAVALIDATION=0)."));
	return FECACommandResult::Success(Result);
#endif
}

REGISTER_ECA_COMMAND(FECACommand_ListValidationRules);
REGISTER_ECA_COMMAND(FECACommand_RunAssetValidator);
REGISTER_ECA_COMMAND(FECACommand_DumpValidationReport);
