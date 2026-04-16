// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create an input mapping (action or axis)
 */
class FECACommand_CreateInputMapping : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_input_mapping"); }
	virtual FString GetDescription() const override { return TEXT("Create an input action or axis mapping in project settings"); }
	virtual FString GetCategory() const override { return TEXT("Project"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("action_name"), TEXT("string"), TEXT("Name of the input action or axis"), true },
			{ TEXT("key"), TEXT("string"), TEXT("Key to bind (SpaceBar, LeftMouseButton, W, Gamepad_LeftX, etc.)"), true },
			{ TEXT("input_type"), TEXT("string"), TEXT("Type of input mapping (Action or Axis)"), false, TEXT("Action") },
			{ TEXT("scale"), TEXT("number"), TEXT("Scale for axis mappings (-1.0 to 1.0)"), false, TEXT("1.0") },
			{ TEXT("shift"), TEXT("boolean"), TEXT("Require Shift modifier"), false, TEXT("false") },
			{ TEXT("ctrl"), TEXT("boolean"), TEXT("Require Ctrl modifier"), false, TEXT("false") },
			{ TEXT("alt"), TEXT("boolean"), TEXT("Require Alt modifier"), false, TEXT("false") },
			{ TEXT("cmd"), TEXT("boolean"), TEXT("Require Cmd modifier"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get all input mappings
 */
class FECACommand_GetInputMappings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_input_mappings"); }
	virtual FString GetDescription() const override { return TEXT("Get all input action and axis mappings"); }
	virtual FString GetCategory() const override { return TEXT("Project"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("input_type"), TEXT("string"), TEXT("Filter by type (Action, Axis, or All)"), false, TEXT("All") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove an input mapping
 */
class FECACommand_RemoveInputMapping : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_input_mapping"); }
	virtual FString GetDescription() const override { return TEXT("Remove an input action or axis mapping"); }
	virtual FString GetCategory() const override { return TEXT("Project"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("action_name"), TEXT("string"), TEXT("Name of the input action or axis to remove"), true },
			{ TEXT("input_type"), TEXT("string"), TEXT("Type of mapping (Action or Axis)"), false, TEXT("Action") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get project settings
 */
class FECACommand_GetProjectSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_project_settings"); }
	virtual FString GetDescription() const override { return TEXT("Get common project settings"); }
	virtual FString GetCategory() const override { return TEXT("Project"); }
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a project setting
 */
class FECACommand_SetProjectSetting : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_project_setting"); }
	virtual FString GetDescription() const override { return TEXT("Set a project setting value"); }
	virtual FString GetCategory() const override { return TEXT("Project"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("setting_name"), TEXT("string"), TEXT("Name of the setting (e.g., GameDefaultMap, GameInstanceClass)"), true },
			{ TEXT("setting_value"), TEXT("string"), TEXT("Value to set"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
