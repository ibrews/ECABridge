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

// â”€â”€ Task 2: state-level inspectors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

class FECACommand_ListStateTreeStateChildren : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_state_children"); }
	virtual FString GetDescription() const override { return TEXT("List immediate children of a state in a StateTree. state_path is a dot-joined display-name path (e.g. Combat.Aggressive)."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") },
			{ TEXT("state_path"),      TEXT("string"), TEXT("Dot-joined display-name path to a state (e.g. Combat.Aggressive)"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListStateTreeStateTasks : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_state_tasks"); }
	virtual FString GetDescription() const override { return TEXT("List tasks attached to a specific state in a StateTree."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") },
			{ TEXT("state_path"),      TEXT("string"), TEXT("Dot-joined display-name path to a state"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListStateTreeStateEnterConditions : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_state_enter_conditions"); }
	virtual FString GetDescription() const override { return TEXT("List enter conditions attached to a specific state in a StateTree."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") },
			{ TEXT("state_path"),      TEXT("string"), TEXT("Dot-joined display-name path to a state"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListStateTreeStateTransitions : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_state_transitions"); }
	virtual FString GetDescription() const override { return TEXT("List transitions attached to a specific state in a StateTree."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") },
			{ TEXT("state_path"),      TEXT("string"), TEXT("Dot-joined display-name path to a state"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ListStateTreeEvaluators : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_state_tree_evaluators"); }
	virtual FString GetDescription() const override { return TEXT("List evaluators on a StateTree asset (global, run for the whole tree)."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"), TEXT("Asset path to a UStateTree"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// â”€â”€ Task 3: node describer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

class FECACommand_DescribeStateTreeNode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_state_tree_node"); }
	virtual FString GetDescription() const override { return TEXT("Return the editor's human-readable description of a specific StateTree node (task/condition/transition/evaluator). state_path is required for state-scoped kinds; omit for global (global_task/evaluator)."); }
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("state_tree_path"), TEXT("string"),  TEXT("Asset path to a UStateTree"), true, TEXT("") },
			{ TEXT("node_kind"),       TEXT("string"),  TEXT("One of: task, condition, transition, evaluator, global_task"), true, TEXT("") },
			{ TEXT("state_path"),      TEXT("string"),  TEXT("Dot-joined state path; required for state-scoped kinds (task, condition, transition). Omit/empty to address global tasks or evaluators."), false, TEXT("") },
			{ TEXT("index"),           TEXT("integer"), TEXT("Zero-based index into the matching container."), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
