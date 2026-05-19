// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASourceControlReconcileCommands.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"

FECACommandResult FECACommand_ReconcileOfflineChanges::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	bool bUpdateHistory = false;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("path_filter"), PathFilter);
		Params->TryGetBoolField(TEXT("update_history"), bUpdateHistory);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path_filter"), PathFilter);
	Result->SetBoolField(TEXT("update_history"), bUpdateHistory);

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	if (!SCCModule.IsEnabled())
	{
		Result->SetStringField(TEXT("provider"), TEXT("none"));
		Result->SetBoolField(TEXT("enabled"), false);
		Result->SetBoolField(TEXT("operation_executed"), false);
		Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled."));
		Result->SetArrayField(TEXT("reconciled_files"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	ISourceControlProvider& Provider = SCCModule.GetProvider();
	Result->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Result->SetBoolField(TEXT("enabled"), true);

	if (!Provider.IsAvailable())
	{
		Result->SetBoolField(TEXT("operation_executed"), false);
		Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Source control provider '%s' is not available."), *Provider.GetName().ToString()));
		Result->SetArrayField(TEXT("reconciled_files"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	// Build the filename set from the asset registry, mirroring get_source_control_status.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*PathFilter), AssetDataList, /*bRecursive=*/ true);

	TSet<FString> UniquePackageNames;
	TArray<FString> Filenames;
	Filenames.Reserve(AssetDataList.Num());
	for (const FAssetData& AssetData : AssetDataList)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		if (UniquePackageNames.Contains(PackageName)) { continue; }
		UniquePackageNames.Add(PackageName);

		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
		{
			Filenames.Add(FPaths::ConvertRelativePathToFull(Filename));
		}
	}
	Result->SetNumberField(TEXT("scanned_file_count"), Filenames.Num());

	// Run a forced UpdateStatus with modified-state detection. This is the SCC-flavoured
	// equivalent of "Reconcile offline work" in the editor's Source Control menu.
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateOp->SetUpdateModifiedState(true);
	UpdateOp->SetUpdateModifiedStateToLocalRevision(true);
	UpdateOp->SetForceUpdate(true);
	UpdateOp->SetCheckingAllFiles(false);
	UpdateOp->SetUpdateHistory(bUpdateHistory);

	const ECommandResult::Type Outcome = (Filenames.Num() > 0)
		? Provider.Execute(UpdateOp, Filenames, EConcurrency::Synchronous)
		: ECommandResult::Succeeded;

	Result->SetBoolField(TEXT("operation_executed"), true);
	Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
	Result->SetStringField(TEXT("outcome"),
		Outcome == ECommandResult::Succeeded ? TEXT("succeeded") :
		Outcome == ECommandResult::Cancelled ? TEXT("cancelled") : TEXT("failed"));

	// Re-query state from the cache and report anything that's now reporting changes.
	TArray<TSharedPtr<FJsonValue>> ReconciledArr;
	if (Filenames.Num() > 0)
	{
		TArray<FSourceControlStateRef> States;
		Provider.GetState(Filenames, States, EStateCacheUsage::Use);
		for (const FSourceControlStateRef& State : States)
		{
			const bool bChanged =
				State->IsCheckedOut() ||
				State->IsModified() ||
				State->IsAdded() ||
				State->IsConflicted() ||
				State->IsDeleted();
			if (!bChanged) { continue; }

			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("filename"), State->GetFilename());
			Entry->SetBoolField(TEXT("is_checked_out"), State->IsCheckedOut());
			Entry->SetBoolField(TEXT("is_modified"), State->IsModified());
			Entry->SetBoolField(TEXT("is_added"), State->IsAdded());
			Entry->SetBoolField(TEXT("is_deleted"), State->IsDeleted());
			Entry->SetBoolField(TEXT("is_conflicted"), State->IsConflicted());
			Entry->SetStringField(TEXT("status_text"), State->GetDisplayName().ToString());
			ReconciledArr.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}
	Result->SetNumberField(TEXT("reconciled_file_count"), ReconciledArr.Num());
	Result->SetArrayField(TEXT("reconciled_files"), ReconciledArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ReconcileOfflineChanges);
