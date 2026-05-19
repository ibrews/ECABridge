// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASourceControlMutateCommands.h"
#include "Commands/ECASourceControlSupport.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

namespace
{
	/** Common preamble: pull asset paths from params, resolve them to absolute filenames,
	 *  and verify the SCC provider is available. Builds the leading section of the response
	 *  JSON in OutResult. Returns nullptr on success; otherwise an Error FECACommandResult. */
	TOptional<FECACommandResult> PreflightSourceControl(
		const TSharedPtr<FJsonObject>& Params,
		const IECACommand* Command,
		TArray<FString>& OutFilenames,
		TSharedPtr<FJsonObject>& OutResult,
		ISourceControlProvider*& OutProvider)
	{
		TArray<FString> AssetPaths;
		FString Err;
		if (!ECASourceControlSupport::GetAssetPathsParam(Params, AssetPaths, Err))
		{
			return FECACommandResult::ValidationError(Command, Err);
		}

		ISourceControlModule& SCCModule = ISourceControlModule::Get();
		if (!SCCModule.IsEnabled())
		{
			return FECACommandResult::Error(TEXT("Source control is not enabled in this project."));
		}

		ISourceControlProvider& Provider = SCCModule.GetProvider();
		if (!Provider.IsAvailable())
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Source control provider '%s' is not available (connection down or not configured)."),
				*Provider.GetName().ToString()));
		}

		TArray<TSharedPtr<FJsonValue>> Unresolved;
		ECASourceControlSupport::ResolveAssetPathBatch(AssetPaths, OutFilenames, Unresolved);

		OutResult = MakeShared<FJsonObject>();
		OutResult->SetStringField(TEXT("provider"), Provider.GetName().ToString());
		OutResult->SetNumberField(TEXT("requested_count"), AssetPaths.Num());
		OutResult->SetNumberField(TEXT("resolved_count"), OutFilenames.Num());
		OutResult->SetArrayField(TEXT("unresolved"), Unresolved);

		if (OutFilenames.Num() == 0)
		{
			OutResult->SetBoolField(TEXT("operation_executed"), false);
			OutResult->SetStringField(TEXT("message"), TEXT("No asset paths could be resolved to filenames."));
			return FECACommandResult::Success(OutResult);
		}

		OutProvider = &Provider;
		return TOptional<FECACommandResult>();
	}

	/** Run a no-args SCC operation (CheckOut/MarkForAdd/Delete) on a file list and build the result. */
	template<typename TOp>
	FECACommandResult RunSimpleOperation(ISourceControlProvider& Provider, const TArray<FString>& Filenames, TSharedPtr<FJsonObject>& Result, const TCHAR* OperationLabel)
	{
		TSharedRef<TOp, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<TOp>();
		const ECommandResult::Type Outcome = Provider.Execute(Op, Filenames, EConcurrency::Synchronous);

		Result->SetStringField(TEXT("operation"), OperationLabel);
		Result->SetBoolField(TEXT("operation_executed"), true);
		Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
		Result->SetStringField(TEXT("outcome"),
			Outcome == ECommandResult::Succeeded ? TEXT("succeeded") :
			Outcome == ECommandResult::Cancelled ? TEXT("cancelled") : TEXT("failed"));

		TArray<TSharedPtr<FJsonValue>> FileArr;
		for (const FString& F : Filenames) { FileArr.Add(MakeShared<FJsonValueString>(F)); }
		Result->SetArrayField(TEXT("files"), FileArr);

		return FECACommandResult::Success(Result);
	}
}

//////////////////////////////////////////////////////////////////////////
// FECACommand_CheckOutAsset

FECACommandResult FECACommand_CheckOutAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<FString> Filenames;
	TSharedPtr<FJsonObject> Result;
	ISourceControlProvider* Provider = nullptr;
	if (TOptional<FECACommandResult> Early = PreflightSourceControl(Params, this, Filenames, Result, Provider))
	{
		return Early.GetValue();
	}
	return RunSimpleOperation<FCheckOut>(*Provider, Filenames, Result, TEXT("check_out"));
}

REGISTER_ECA_COMMAND(FECACommand_CheckOutAsset);

//////////////////////////////////////////////////////////////////////////
// FECACommand_RevertAsset

FECACommandResult FECACommand_RevertAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<FString> Filenames;
	TSharedPtr<FJsonObject> Result;
	ISourceControlProvider* Provider = nullptr;
	if (TOptional<FECACommandResult> Early = PreflightSourceControl(Params, this, Filenames, Result, Provider))
	{
		return Early.GetValue();
	}

	bool bSoftRevert = false;
	if (Params.IsValid())
	{
		Params->TryGetBoolField(TEXT("soft_revert"), bSoftRevert);
	}

	TSharedRef<FRevert, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FRevert>();
	Op->SetSoftRevert(bSoftRevert);
	const ECommandResult::Type Outcome = Provider->Execute(Op, Filenames, EConcurrency::Synchronous);

	Result->SetStringField(TEXT("operation"), TEXT("revert"));
	Result->SetBoolField(TEXT("operation_executed"), true);
	Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
	Result->SetStringField(TEXT("outcome"),
		Outcome == ECommandResult::Succeeded ? TEXT("succeeded") :
		Outcome == ECommandResult::Cancelled ? TEXT("cancelled") : TEXT("failed"));
	Result->SetBoolField(TEXT("soft_revert"), bSoftRevert);

	TArray<TSharedPtr<FJsonValue>> FileArr;
	for (const FString& F : Filenames) { FileArr.Add(MakeShared<FJsonValueString>(F)); }
	Result->SetArrayField(TEXT("files"), FileArr);

	TArray<TSharedPtr<FJsonValue>> DeletedArr;
	for (const FString& F : Op->GetDeletedFiles()) { DeletedArr.Add(MakeShared<FJsonValueString>(F)); }
	Result->SetArrayField(TEXT("deleted_files"), DeletedArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_RevertAsset);

//////////////////////////////////////////////////////////////////////////
// FECACommand_MarkForAdd

FECACommandResult FECACommand_MarkForAdd::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<FString> Filenames;
	TSharedPtr<FJsonObject> Result;
	ISourceControlProvider* Provider = nullptr;
	if (TOptional<FECACommandResult> Early = PreflightSourceControl(Params, this, Filenames, Result, Provider))
	{
		return Early.GetValue();
	}
	return RunSimpleOperation<FMarkForAdd>(*Provider, Filenames, Result, TEXT("mark_for_add"));
}

REGISTER_ECA_COMMAND(FECACommand_MarkForAdd);

//////////////////////////////////////////////////////////////////////////
// FECACommand_MarkForDelete

FECACommandResult FECACommand_MarkForDelete::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<FString> Filenames;
	TSharedPtr<FJsonObject> Result;
	ISourceControlProvider* Provider = nullptr;
	if (TOptional<FECACommandResult> Early = PreflightSourceControl(Params, this, Filenames, Result, Provider))
	{
		return Early.GetValue();
	}
	return RunSimpleOperation<FDelete>(*Provider, Filenames, Result, TEXT("mark_for_delete"));
}

REGISTER_ECA_COMMAND(FECACommand_MarkForDelete);
