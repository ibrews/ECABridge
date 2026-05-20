// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// All commands gated by WITH_ECA_STATETREE. Class definitions live here, the
// REGISTER_ECA_COMMAND macros + Execute bodies live in the .cpp wrapped in
// #if WITH_ECA_STATETREE so neither is compiled when the StateTree plugin is
// missing from the engine.

/**
 * get_state_tree_editor_data â€” return summary counts for a StateTree asset's
 * editor data: root state count, global task count, evaluator count, plus the
 * resolved editor-data subobject path. Returns Error if the asset has no
 * editor data (cooked-only).
 */
class FECACommand_GetStateTreeEditorData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_state_tree_editor_data"); }
	virtual FString GetDescription() const override { return TEXT("Return summary counts for a StateTree asset's editor data (root_state_count, global_task_count, evaluator_count) plus the editor_data_path."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree (e.g. /Game/AI/ST_Guard)"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * list_state_tree_root_states â€” enumerate top-level states (SubTrees) of a
 * StateTree asset. Each entry has name, type, child_count, task_count,
 * transition_count, enter_condition_count.
 */
class FECACommand_ListStateTreeRootStates : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_root_states"); }
	virtual FString GetDescription() const override { return TEXT("List the top-level (root) states of a StateTree asset with their type and child/task/transition/enter-condition counts."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * list_state_tree_global_tasks â€” enumerate tasks that run across all states
 * of the StateTree (EditorData->GlobalTasks).
 */
class FECACommand_ListStateTreeGlobalTasks : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_global_tasks"); }
	virtual FString GetDescription() const override { return TEXT("List global tasks on a StateTree asset (tasks that run across all states)."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
