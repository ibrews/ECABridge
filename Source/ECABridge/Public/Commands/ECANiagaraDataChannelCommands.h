// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new Niagara Data Channel asset.
 */
class FECACommand_CreateNiagaraDataChannel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_niagara_data_channel"); }
	virtual FString GetDescription() const override { return TEXT("Create a new NiagaraDataChannelAsset for cross-system data sharing (e.g., spawners writing hit data that effects read). Specify variable names and types."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path for the new NDC asset (e.g., /Game/FX/NDC_Hits)"), true },
			{ TEXT("variables"), TEXT("array"), TEXT("Array of variable defs: [{name: 'Position', type: 'Vector'}, {name: 'Damage', type: 'Float'}]. Types: Float, Int32, Bool, Vector, Vector2, Vector4, Quat, Color, LinearColor, Position"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump a Niagara Data Channel: variables, types, and usage.
 */
class FECACommand_DumpNiagaraDataChannel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_niagara_data_channel"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a NiagaraDataChannelAsset to JSON: variable names, types, channel settings, and usage scope."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the NiagaraDataChannel asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
