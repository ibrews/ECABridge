// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAStateTreeCommands.h"

#if WITH_ECA_STATETREE

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeEditorNode.h"
#include "StateTreeTypes.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace ECAStateTreeInternal
{
	UStateTree* LoadStateTreeByPath(const FString& Path)
	{
		FSoftObjectPath SoftPath(Path);
		if (!SoftPath.IsValid())
		{
			return nullptr;
		}
		return Cast<UStateTree>(SoftPath.TryLoad());
	}

	// Resolve a StateTree + its editor data. Sets OutError on failure and returns nullptr.
	UStateTreeEditorData* ResolveEditorData(const FString& Path, FString& OutError)
	{
		UStateTree* Tree = LoadStateTreeByPath(Path);
		if (!Tree)
		{
			OutError = FString::Printf(TEXT("Failed to load UStateTree at '%s'."), *Path);
			return nullptr;
		}
		// 5.8 added the static GetEditorData helper; 5.7 stores it on UStateTree::EditorData
		// (a TObjectPtr<UObject> under WITH_EDITORONLY_DATA, because the runtime module
		// can't reference the editor-data UClass directly).
#if WITH_EDITORONLY_DATA && UE_VERSION_OLDER_THAN(5, 8, 0)
		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData);
#else
		UStateTreeEditorData* EditorData = UStateTreeEditorData::GetEditorData(Tree);
#endif
		if (!EditorData)
		{
			OutError = TEXT("StateTree has no editor data (asset is cooked or stripped).");
			return nullptr;
		}
		return EditorData;
	}

	FString StateTypeToString(EStateTreeStateType Type)
	{
		if (UEnum* Enum = StaticEnum<EStateTreeStateType>())
		{
			return Enum->GetNameStringByValue((int64)Type);
		}
		return FString::Printf(TEXT("%d"), (int32)Type);
	}

	// Best-effort class / struct name from an FStateTreeEditorNode (either struct-based or object-based).
	FString GetEditorNodeClassName(const FStateTreeEditorNode& Node)
	{
		if (Node.InstanceObject)
		{
			return Node.InstanceObject->GetClass()->GetName();
		}
		if (const UScriptStruct* Struct = Node.Node.GetScriptStruct())
		{
			return Struct->GetName();
		}
		return FString();
	}

	// Render an FStateTreeEditorNode description via the editor's own describer.
	// Falls back to GetName() if the editor returns empty.
	FString GetEditorNodeDescription(const UStateTreeEditorData& EditorData, const FStateTreeEditorNode& Node)
	{
		const FText Desc = EditorData.GetNodeDescription(Node);
		FString Out = Desc.ToString();
		if (Out.IsEmpty())
		{
			const FName Name = Node.GetName();
			Out = Name.IsNone() ? GetEditorNodeClassName(Node) : Name.ToString();
		}
		return Out;
	}

	TSharedPtr<FJsonObject> StateSummary(const UStateTreeState* State)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!State)
		{
			return Obj;
		}
		Obj->SetStringField(TEXT("name"), State->Name.ToString());
		Obj->SetStringField(TEXT("type"), StateTypeToString(State->Type));
		Obj->SetNumberField(TEXT("child_count"), State->Children.Num());
		Obj->SetNumberField(TEXT("task_count"), State->Tasks.Num());
		Obj->SetNumberField(TEXT("transition_count"), State->Transitions.Num());
		Obj->SetNumberField(TEXT("enter_condition_count"), State->EnterConditions.Num());
		return Obj;
	}
}

FECACommandResult FECACommand_GetStateTreeEditorData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TreePath;
	if (!GetStringParam(Params, TEXT("state_tree_path"), TreePath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("state_tree_path is required"));
	}

	FString Error;
	UStateTreeEditorData* EditorData = ECAStateTreeInternal::ResolveEditorData(TreePath, Error);
	if (!EditorData)
	{
		return FECACommandResult::Error(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("state_tree_path"), TreePath);
	Result->SetStringField(TEXT("editor_data_path"), EditorData->GetPathName());
	Result->SetNumberField(TEXT("root_state_count"), EditorData->SubTrees.Num());
	Result->SetNumberField(TEXT("global_task_count"), EditorData->GlobalTasks.Num());
	Result->SetNumberField(TEXT("evaluator_count"), EditorData->Evaluators.Num());
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_GetStateTreeEditorData);

FECACommandResult FECACommand_ListStateTreeRootStates::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TreePath;
	if (!GetStringParam(Params, TEXT("state_tree_path"), TreePath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("state_tree_path is required"));
	}

	FString Error;
	UStateTreeEditorData* EditorData = ECAStateTreeInternal::ResolveEditorData(TreePath, Error);
	if (!EditorData)
	{
		return FECACommandResult::Error(Error);
	}

	TArray<TSharedPtr<FJsonValue>> StateValues;
	StateValues.Reserve(EditorData->SubTrees.Num());
	for (const TObjectPtr<UStateTreeState>& State : EditorData->SubTrees)
	{
		StateValues.Add(MakeShared<FJsonValueObject>(ECAStateTreeInternal::StateSummary(State)));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("state_tree_path"), TreePath);
	Result->SetNumberField(TEXT("count"), StateValues.Num());
	Result->SetArrayField(TEXT("states"), StateValues);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListStateTreeRootStates);

FECACommandResult FECACommand_ListStateTreeGlobalTasks::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TreePath;
	if (!GetStringParam(Params, TEXT("state_tree_path"), TreePath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("state_tree_path is required"));
	}

	FString Error;
	UStateTreeEditorData* EditorData = ECAStateTreeInternal::ResolveEditorData(TreePath, Error);
	if (!EditorData)
	{
		return FECACommandResult::Error(Error);
	}

	TArray<TSharedPtr<FJsonValue>> Tasks;
	Tasks.Reserve(EditorData->GlobalTasks.Num());
	for (int32 Idx = 0; Idx < EditorData->GlobalTasks.Num(); ++Idx)
	{
		const FStateTreeEditorNode& Node = EditorData->GlobalTasks[Idx];
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("index"), Idx);
		Entry->SetStringField(TEXT("class_name"), ECAStateTreeInternal::GetEditorNodeClassName(Node));
		Entry->SetStringField(TEXT("description"), ECAStateTreeInternal::GetEditorNodeDescription(*EditorData, Node));
		Tasks.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("state_tree_path"), TreePath);
	Result->SetNumberField(TEXT("count"), Tasks.Num());
	Result->SetArrayField(TEXT("tasks"), Tasks);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListStateTreeGlobalTasks);

#endif // WITH_ECA_STATETREE
