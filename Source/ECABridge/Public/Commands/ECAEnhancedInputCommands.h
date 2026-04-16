// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new Enhanced Input Action asset.
 */
class FECACommand_CreateInputAction : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_input_action"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UInputAction asset. Value types: Digital (bool), Analog1D (float), Analog2D (Vector2D), Analog3D (Vector)."); }
	virtual FString GetCategory() const override { return TEXT("EnhancedInput"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path for the new InputAction (e.g., /Game/Input/IA_Jump)"), true },
			{ TEXT("value_type"), TEXT("string"), TEXT("Value type: Digital, Analog1D, Analog2D, Analog3D"), false, TEXT("Digital") },
			{ TEXT("description"), TEXT("string"), TEXT("Description for the action"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new Input Mapping Context asset.
 */
class FECACommand_CreateInputMappingContext : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_input_mapping_context"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UInputMappingContext asset (groups key bindings to InputActions)."); }
	virtual FString GetCategory() const override { return TEXT("EnhancedInput"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path for the new Mapping Context (e.g., /Game/Input/IMC_Gameplay)"), true },
			{ TEXT("description"), TEXT("string"), TEXT("Description for the context"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a key → InputAction mapping to an Input Mapping Context.
 */
class FECACommand_AddInputMapping : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_input_mapping"); }
	virtual FString GetDescription() const override { return TEXT("Add a key → InputAction binding to an Input Mapping Context. Supports keyboard, mouse, and gamepad keys (e.g., SpaceBar, LeftMouseButton, Gamepad_FaceButton_Bottom)."); }
	virtual FString GetCategory() const override { return TEXT("EnhancedInput"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("context_path"), TEXT("string"), TEXT("Content path to the Input Mapping Context"), true },
			{ TEXT("action_path"), TEXT("string"), TEXT("Content path to the Input Action"), true },
			{ TEXT("key"), TEXT("string"), TEXT("Key name (e.g., SpaceBar, W, LeftMouseButton, Gamepad_FaceButton_Bottom)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump an Input Mapping Context: all bindings with keys, actions, modifiers, triggers.
 */
class FECACommand_DumpInputMappingContext : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_input_mapping_context"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a UInputMappingContext: all key → InputAction mappings with modifiers, triggers, and action details."); }
	virtual FString GetCategory() const override { return TEXT("EnhancedInput"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("context_path"), TEXT("string"), TEXT("Content path to the Input Mapping Context"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump an Input Action: value type, triggers, modifiers.
 */
class FECACommand_DumpInputAction : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_input_action"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a UInputAction: value type (Digital/Analog1D/2D/3D), triggers, modifiers, description."); }
	virtual FString GetCategory() const override { return TEXT("EnhancedInput"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("action_path"), TEXT("string"), TEXT("Content path to the Input Action"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
