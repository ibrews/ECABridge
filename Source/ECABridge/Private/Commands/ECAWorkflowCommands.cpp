// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWorkflowCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "UObject/UObjectIterator.h"

// Register all workflow commands
REGISTER_ECA_COMMAND(FECACommand_SnapshotAsset)
REGISTER_ECA_COMMAND(FECACommand_DiffAsset)
REGISTER_ECA_COMMAND(FECACommand_BatchOperation)

// Static storage for snapshots: snapshot_id -> (property_name -> string_value)
static TMap<FString, TMap<FString, FString>> SnapshotStore;

//------------------------------------------------------------------------------
// Helper: gather all editable, non-transient, non-deprecated property values
//------------------------------------------------------------------------------

static TMap<FString, FString> GatherPropertyValues(UObject* Object)
{
	TMap<FString, FString> Values;
	if (!Object)
	{
		return Values;
	}

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		// Skip transient and deprecated properties
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}

		// Only include editable properties
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FString ValueStr;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Object, PPF_None);

		Values.Add(Property->GetName(), ValueStr);
	}

	return Values;
}

//------------------------------------------------------------------------------
// snapshot_asset
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SnapshotAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("asset_path is required"));
	}

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}

	// Determine snapshot ID
	FString SnapshotId;
	if (!GetStringParam(Params, TEXT("snapshot_id"), SnapshotId, false) || SnapshotId.IsEmpty())
	{
		SnapshotId = FGuid::NewGuid().ToString();
	}

	// Gather property values and store them
	TMap<FString, FString> PropertyValues = GatherPropertyValues(Asset);
	SnapshotStore.Add(SnapshotId, PropertyValues);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("snapshot_id"), SnapshotId);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetNumberField(TEXT("property_count"), PropertyValues.Num());

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// diff_asset
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DiffAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("asset_path is required"));
	}

	FString SnapshotId;
	FString CompareTo;
	GetStringParam(Params, TEXT("snapshot_id"), SnapshotId, false);
	GetStringParam(Params, TEXT("compare_to"), CompareTo, false);

	if (SnapshotId.IsEmpty() && CompareTo.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Either snapshot_id or compare_to must be provided"));
	}

	// Load the primary asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}

	TMap<FString, FString> CurrentValues = GatherPropertyValues(Asset);

	// Get the baseline values to compare against
	TMap<FString, FString> BaselineValues;

	if (!SnapshotId.IsEmpty())
	{
		// Mode 1: Compare against a stored snapshot
		const TMap<FString, FString>* Found = SnapshotStore.Find(SnapshotId);
		if (!Found)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Snapshot not found: %s"), *SnapshotId));
		}
		BaselineValues = *Found;
	}
	else
	{
		// Mode 2: Compare against another asset
		UObject* OtherAsset = LoadObject<UObject>(nullptr, *CompareTo);
		if (!OtherAsset)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to load comparison asset: %s"), *CompareTo));
		}
		BaselineValues = GatherPropertyValues(OtherAsset);
	}

	// Compute diffs - only properties that differ
	TArray<TSharedPtr<FJsonValue>> DiffsArray;

	for (const auto& Pair : CurrentValues)
	{
		const FString* OldValue = BaselineValues.Find(Pair.Key);
		if (!OldValue || *OldValue != Pair.Value)
		{
			TSharedPtr<FJsonObject> DiffEntry = MakeShared<FJsonObject>();
			DiffEntry->SetStringField(TEXT("property"), Pair.Key);
			DiffEntry->SetStringField(TEXT("old_value"), OldValue ? *OldValue : TEXT("(not present)"));
			DiffEntry->SetStringField(TEXT("new_value"), Pair.Value);
			DiffsArray.Add(MakeShared<FJsonValueObject>(DiffEntry));
		}
	}

	// Also check for properties in the baseline that are missing from current
	for (const auto& Pair : BaselineValues)
	{
		if (!CurrentValues.Contains(Pair.Key))
		{
			TSharedPtr<FJsonObject> DiffEntry = MakeShared<FJsonObject>();
			DiffEntry->SetStringField(TEXT("property"), Pair.Key);
			DiffEntry->SetStringField(TEXT("old_value"), Pair.Value);
			DiffEntry->SetStringField(TEXT("new_value"), TEXT("(not present)"));
			DiffsArray.Add(MakeShared<FJsonValueObject>(DiffEntry));
		}
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("diffs"), DiffsArray);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// batch_operation
//------------------------------------------------------------------------------

FECACommandResult FECACommand_BatchOperation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
	if (!GetArrayParam(Params, TEXT("commands"), CommandsArray))
	{
		return FECACommandResult::Error(TEXT("commands array is required"));
	}

	FString Description;
	if (!GetStringParam(Params, TEXT("description"), Description, false) || Description.IsEmpty())
	{
		Description = TEXT("Batch Operation");
	}

	// Start undo transaction
	GEditor->BeginTransaction(FText::FromString(Description));

	TArray<TSharedPtr<FJsonValue>> ResultsArray;

	for (int32 i = 0; i < CommandsArray->Num(); ++i)
	{
		const TSharedPtr<FJsonValue>& CmdValue = (*CommandsArray)[i];
		const TSharedPtr<FJsonObject>* CmdObject = nullptr;
		if (!CmdValue.IsValid() || !CmdValue->TryGetObject(CmdObject) || !CmdObject || !(*CmdObject).IsValid())
		{
			GEditor->CancelTransaction(0);
			return FECACommandResult::Error(FString::Printf(TEXT("Command at index %d is not a valid object"), i));
		}

		FString Name;
		if (!(*CmdObject)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
		{
			GEditor->CancelTransaction(0);
			return FECACommandResult::Error(FString::Printf(TEXT("Command at index %d is missing 'name' field"), i));
		}

		// Get arguments (optional - default to empty object)
		TSharedPtr<FJsonObject> Arguments;
		const TSharedPtr<FJsonObject>* ArgumentsPtr = nullptr;
		if ((*CmdObject)->TryGetObjectField(TEXT("arguments"), ArgumentsPtr) && ArgumentsPtr)
		{
			Arguments = *ArgumentsPtr;
		}
		else
		{
			Arguments = MakeShared<FJsonObject>();
		}

		// Execute the command via the registry
		FECACommandResult CmdResult = FECACommandRegistry::Get().ExecuteCommand(Name, Arguments);

		if (!CmdResult.bSuccess)
		{
			GEditor->CancelTransaction(0);
			return FECACommandResult::Error(FString::Printf(
				TEXT("Command '%s' at index %d failed: %s"), *Name, i, *CmdResult.ErrorMessage));
		}

		// Collect the result
		TSharedPtr<FJsonObject> EntryResult = MakeShared<FJsonObject>();
		EntryResult->SetStringField(TEXT("command"), Name);
		if (CmdResult.ResultData.IsValid())
		{
			EntryResult->SetObjectField(TEXT("result"), CmdResult.ResultData);
		}
		ResultsArray.Add(MakeShared<FJsonValueObject>(EntryResult));
	}

	// All commands succeeded - commit the transaction
	GEditor->EndTransaction();

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("results"), ResultsArray);
	Result->SetStringField(TEXT("transaction_description"), Description);

	return FECACommandResult::Success(Result);
}
