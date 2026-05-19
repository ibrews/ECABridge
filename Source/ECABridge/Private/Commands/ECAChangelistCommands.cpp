// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAChangelistCommands.h"
#include "Commands/ECASourceControlSupport.h"
#include "ISourceControlChangelist.h"
#include "ISourceControlChangelistState.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "SourceControlOperations.h"

namespace
{
	ISourceControlProvider* RequireChangelistProvider(FString& OutError)
	{
		ISourceControlModule& SCCModule = ISourceControlModule::Get();
		if (!SCCModule.IsEnabled())
		{
			OutError = TEXT("Source control is not enabled in this project.");
			return nullptr;
		}
		ISourceControlProvider& Provider = SCCModule.GetProvider();
		if (!Provider.IsAvailable())
		{
			OutError = FString::Printf(TEXT("Source control provider '%s' is not available."), *Provider.GetName().ToString());
			return nullptr;
		}
		if (!Provider.UsesChangelists())
		{
			OutError = FString::Printf(TEXT("Provider '%s' does not use changelists (UsesChangelists()==false). Changelist operations are Perforce-specific."), *Provider.GetName().ToString());
			return nullptr;
		}
		return &Provider;
	}

	FSourceControlChangelistPtr FindChangelistById(ISourceControlProvider& Provider, const FString& ChangelistId)
	{
		const TArray<FSourceControlChangelistRef> Changelists = Provider.GetChangelists(EStateCacheUsage::Use);
		for (const FSourceControlChangelistRef& CL : Changelists)
		{
			if (CL->GetIdentifier().Equals(ChangelistId, ESearchCase::IgnoreCase))
			{
				return CL;
			}
		}
		return nullptr;
	}

	FString CommandResultLabel(ECommandResult::Type Outcome)
	{
		switch (Outcome)
		{
		case ECommandResult::Succeeded: return TEXT("succeeded");
		case ECommandResult::Cancelled: return TEXT("cancelled");
		default:                        return TEXT("failed");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FECACommand_CreateChangelist

FECACommandResult FECACommand_CreateChangelist::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Description;
	if (!GetStringParam(Params, TEXT("description"), Description, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'description' (string) is required."));
	}

	FString Err;
	ISourceControlProvider* Provider = RequireChangelistProvider(Err);
	if (!Provider)
	{
		return FECACommandResult::Error(Err);
	}

	TSharedRef<FNewChangelist, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FNewChangelist>();
	Op->SetDescription(FText::FromString(Description));

	const ECommandResult::Type Outcome = Provider->Execute(Op, EConcurrency::Synchronous);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("provider"), Provider->GetName().ToString());
	Result->SetStringField(TEXT("operation"), TEXT("create_changelist"));
	Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
	Result->SetStringField(TEXT("outcome"), CommandResultLabel(Outcome));
	Result->SetStringField(TEXT("description"), Description);

	FSourceControlChangelistPtr NewCL = Op->GetNewChangelist();
	if (NewCL.IsValid())
	{
		Result->SetStringField(TEXT("changelist_id"), NewCL->GetIdentifier());
	}

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_CreateChangelist);

//////////////////////////////////////////////////////////////////////////
// FECACommand_SubmitChangelist

FECACommandResult FECACommand_SubmitChangelist::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ChangelistId;
	if (!GetStringParam(Params, TEXT("changelist_id"), ChangelistId, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'changelist_id' (string) is required."));
	}

	FString Description;
	if (Params.IsValid()) { Params->TryGetStringField(TEXT("description"), Description); }
	bool bKeepCheckedOut = false;
	if (Params.IsValid()) { Params->TryGetBoolField(TEXT("keep_checked_out"), bKeepCheckedOut); }

	FString Err;
	ISourceControlProvider* Provider = RequireChangelistProvider(Err);
	if (!Provider)
	{
		return FECACommandResult::Error(Err);
	}

	FSourceControlChangelistPtr CL = FindChangelistById(*Provider, ChangelistId);
	if (!CL.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Changelist '%s' not found among pending changelists for provider '%s'."),
			*ChangelistId, *Provider->GetName().ToString()));
	}

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCheckIn>();
	if (!Description.IsEmpty())
	{
		Op->SetDescription(FText::FromString(Description));
	}
	Op->SetKeepCheckedOut(bKeepCheckedOut);

	const ECommandResult::Type Outcome = Provider->Execute(Op, CL, TArray<FString>(), EConcurrency::Synchronous);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("provider"), Provider->GetName().ToString());
	Result->SetStringField(TEXT("operation"), TEXT("submit_changelist"));
	Result->SetStringField(TEXT("changelist_id"), ChangelistId);
	Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
	Result->SetStringField(TEXT("outcome"), CommandResultLabel(Outcome));
	Result->SetBoolField(TEXT("keep_checked_out"), bKeepCheckedOut);
	Result->SetStringField(TEXT("success_message"), Op->GetSuccessMessage().ToString());

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_SubmitChangelist);

//////////////////////////////////////////////////////////////////////////
// FECACommand_ListChangelists

FECACommandResult FECACommand_ListChangelists::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bForceRefresh = false;
	if (Params.IsValid()) { Params->TryGetBoolField(TEXT("force_refresh"), bForceRefresh); }

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	if (!SCCModule.IsEnabled())
	{
		Result->SetStringField(TEXT("provider"), TEXT("none"));
		Result->SetBoolField(TEXT("enabled"), false);
		Result->SetArrayField(TEXT("changelists"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	ISourceControlProvider& Provider = SCCModule.GetProvider();
	Result->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Result->SetBoolField(TEXT("enabled"), true);
	Result->SetBoolField(TEXT("available"), Provider.IsAvailable());
	Result->SetBoolField(TEXT("uses_changelists"), Provider.UsesChangelists());

	if (!Provider.IsAvailable() || !Provider.UsesChangelists())
	{
		Result->SetArrayField(TEXT("changelists"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	if (bForceRefresh)
	{
		TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> Update = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
		Update->SetUpdateAllChangelists(true);
		Update->SetUpdateFilesStates(true);
		Provider.Execute(Update, EConcurrency::Synchronous);
	}

	const EStateCacheUsage::Type CacheUsage = bForceRefresh ? EStateCacheUsage::ForceUpdate : EStateCacheUsage::Use;
	const TArray<FSourceControlChangelistRef> Changelists = Provider.GetChangelists(CacheUsage);

	TArray<FSourceControlChangelistStateRef> States;
	Provider.GetState(Changelists, States, CacheUsage);

	TMap<FString, FSourceControlChangelistStateRef> StateById;
	for (const FSourceControlChangelistStateRef& State : States)
	{
		StateById.Add(State->GetChangelist()->GetIdentifier(), State);
	}

	TArray<TSharedPtr<FJsonValue>> ChangelistsArr;
	for (const FSourceControlChangelistRef& CL : Changelists)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("changelist_id"), CL->GetIdentifier());
		Obj->SetBoolField(TEXT("is_default"), CL->IsDefault());
		Obj->SetBoolField(TEXT("can_delete"), CL->CanDelete());

		if (const FSourceControlChangelistStateRef* StatePtr = StateById.Find(CL->GetIdentifier()))
		{
			const FSourceControlChangelistStateRef& State = *StatePtr;
			Obj->SetStringField(TEXT("description"), State->GetDescriptionText().ToString());
			Obj->SetNumberField(TEXT("file_count"), State->GetFilesStatesNum());
			Obj->SetNumberField(TEXT("shelved_file_count"), State->GetShelvedFilesStatesNum());
			Obj->SetBoolField(TEXT("supports_persistent_description"), State->SupportsPersistentDescription());
		}
		else
		{
			Obj->SetStringField(TEXT("description"), TEXT(""));
			Obj->SetNumberField(TEXT("file_count"), 0);
			Obj->SetNumberField(TEXT("shelved_file_count"), 0);
		}

		ChangelistsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Result->SetNumberField(TEXT("changelist_count"), ChangelistsArr.Num());
	Result->SetArrayField(TEXT("changelists"), ChangelistsArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListChangelists);

//////////////////////////////////////////////////////////////////////////
// FECACommand_MoveToChangelist

FECACommandResult FECACommand_MoveToChangelist::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ChangelistId;
	if (!GetStringParam(Params, TEXT("changelist_id"), ChangelistId, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'changelist_id' (string) is required."));
	}

	TArray<FString> AssetPaths;
	FString PathErr;
	if (!ECASourceControlSupport::GetAssetPathsParam(Params, AssetPaths, PathErr))
	{
		return FECACommandResult::ValidationError(this, PathErr);
	}

	FString Err;
	ISourceControlProvider* Provider = RequireChangelistProvider(Err);
	if (!Provider)
	{
		return FECACommandResult::Error(Err);
	}

	FSourceControlChangelistPtr CL = FindChangelistById(*Provider, ChangelistId);
	if (!CL.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Changelist '%s' not found among pending changelists for provider '%s'."),
			*ChangelistId, *Provider->GetName().ToString()));
	}

	TArray<FString> Filenames;
	TArray<TSharedPtr<FJsonValue>> Unresolved;
	ECASourceControlSupport::ResolveAssetPathBatch(AssetPaths, Filenames, Unresolved);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("provider"), Provider->GetName().ToString());
	Result->SetStringField(TEXT("operation"), TEXT("move_to_changelist"));
	Result->SetStringField(TEXT("changelist_id"), ChangelistId);
	Result->SetNumberField(TEXT("requested_count"), AssetPaths.Num());
	Result->SetNumberField(TEXT("resolved_count"), Filenames.Num());
	Result->SetArrayField(TEXT("unresolved"), Unresolved);

	if (Filenames.Num() == 0)
	{
		Result->SetBoolField(TEXT("operation_executed"), false);
		Result->SetStringField(TEXT("message"), TEXT("No asset paths resolved to filenames."));
		return FECACommandResult::Success(Result);
	}

	TSharedRef<FMoveToChangelist, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FMoveToChangelist>();
	const ECommandResult::Type Outcome = Provider->Execute(Op, CL, Filenames, EConcurrency::Synchronous);

	Result->SetBoolField(TEXT("operation_executed"), true);
	Result->SetBoolField(TEXT("succeeded"), Outcome == ECommandResult::Succeeded);
	Result->SetStringField(TEXT("outcome"), CommandResultLabel(Outcome));

	TArray<TSharedPtr<FJsonValue>> FileArr;
	for (const FString& F : Filenames) { FileArr.Add(MakeShared<FJsonValueString>(F)); }
	Result->SetArrayField(TEXT("files"), FileArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_MoveToChangelist);
