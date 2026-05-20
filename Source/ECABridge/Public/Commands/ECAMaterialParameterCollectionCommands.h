// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new UMaterialParameterCollection asset (the "MPC" used as a global
 * scalar/vector parameter bag for materials).
 */
class FECACommand_CreateMPC : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_mpc"); }
	virtual FString GetDescription() const override { return TEXT("Create a UMaterialParameterCollection asset. Set overwrite=true to update an existing asset at the same path."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"),      TEXT("string"),  TEXT("Folder path (e.g. '/Game/MPCs/')"), true },
			{ TEXT("name"),      TEXT("string"),  TEXT("Asset name (no extension)"), true },
			{ TEXT("overwrite"), TEXT("boolean"), TEXT("Update existing asset at this path instead of failing (default false)"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),    TEXT("string"),  TEXT("Created asset path") },
			{ TEXT("created"), TEXT("boolean"), TEXT("True for newly-created, false for overwrite") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add (or update) a scalar parameter entry on a MPC asset.
 */
class FECACommand_AddMPCScalarParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_mpc_scalar_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Add or update a scalar parameter on a UMaterialParameterCollection asset."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mpc_path"),     TEXT("string"), TEXT("Asset path to the MPC"), true },
			{ TEXT("parameter"),    TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("default_value"),TEXT("number"), TEXT("Default scalar value (default 0.0)"), false, TEXT("0.0") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("parameter"), TEXT("string"),  TEXT("Parameter name") },
			{ TEXT("added"),     TEXT("boolean"), TEXT("True for newly-added, false when updated existing") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add (or update) a vector parameter entry on a MPC asset.
 */
class FECACommand_AddMPCVectorParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_mpc_vector_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Add or update a vector parameter on a UMaterialParameterCollection asset."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mpc_path"),     TEXT("string"), TEXT("Asset path to the MPC"), true },
			{ TEXT("parameter"),    TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("default_value"),TEXT("object"), TEXT("Default FLinearColor: {r,g,b,a} (defaults to {0,0,0,1})"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("parameter"), TEXT("string"),  TEXT("Parameter name") },
			{ TEXT("added"),     TEXT("boolean"), TEXT("True for newly-added, false when updated existing") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect a UMaterialParameterCollection: list its scalar and vector parameters.
 */
class FECACommand_DumpMPC : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_mpc"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a UMaterialParameterCollection: list scalar and vector parameter definitions."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mpc_path"), TEXT("string"), TEXT("Asset path to the MPC"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),    TEXT("string"), TEXT("Resolved asset path") },
			{ TEXT("scalars"), TEXT("array"),  TEXT("[{parameter, default_value, id}]"), TEXT("object") },
			{ TEXT("vectors"), TEXT("array"),  TEXT("[{parameter, default_value:{r,g,b,a}, id}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a scalar parameter on the current editor world's instance of an MPC.
 * Mutates the runtime instance only; the asset's default is unchanged.
 */
class FECACommand_SetMPCScalarRuntime : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_mpc_scalar_runtime"); }
	virtual FString GetDescription() const override { return TEXT("Set a scalar parameter on the editor world's runtime instance of an MPC. Asset default is unchanged."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mpc_path"),  TEXT("string"), TEXT("Asset path to the MPC"), true },
			{ TEXT("parameter"), TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("value"),     TEXT("number"), TEXT("New scalar value"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a vector parameter on the current editor world's instance of an MPC.
 */
class FECACommand_SetMPCVectorRuntime : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_mpc_vector_runtime"); }
	virtual FString GetDescription() const override { return TEXT("Set a vector parameter on the editor world's runtime instance of an MPC."); }
	virtual FString GetCategory() const override { return TEXT("MaterialParameterCollection"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mpc_path"),  TEXT("string"), TEXT("Asset path to the MPC"), true },
			{ TEXT("parameter"), TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("value"),     TEXT("object"), TEXT("New FLinearColor: {r,g,b,a}"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
