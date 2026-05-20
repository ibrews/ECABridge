// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * list_validation_rules — enumerate the UEditorValidatorBase classes registered with
 * the editor's UEditorValidatorSubsystem. The same set you'd see under
 * Project Settings → Editor → Asset Validation.
 *
 * Category SourceControlValidation so the existing WITH_ECA_DATAVALIDATION runtime
 * gate also drops this command when the DataValidation plugin isn't loaded.
 */
class FECACommand_ListValidationRules : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_validation_rules"); }
	virtual FString GetDescription() const override { return TEXT("List all UEditorValidatorBase validators currently registered with the editor's UEditorValidatorSubsystem. Returns class name, package path, and enabled state for each."); }
	virtual FString GetCategory() const override { return TEXT("SourceControlValidation"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * run_asset_validator — run UEditorValidatorSubsystem::ValidateAssetsWithSettings against
 * an arbitrary list of asset paths. Returns the same per-asset detail object validate_before_submit
 * produces, but without the changelist-resolution detour. Useful for "hey LLM, validate these
 * three assets you just imported" workflows.
 */
class FECACommand_RunAssetValidator : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("run_asset_validator"); }
	virtual FString GetDescription() const override { return TEXT("Run UEditorValidatorSubsystem across a caller-supplied list of asset object paths and return pass/fail + per-asset error/warning details."); }
	virtual FString GetCategory() const override { return TEXT("SourceControlValidation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_paths"), TEXT("array"), TEXT("List of asset object paths (e.g. /Game/Foo/Bar) to validate. Package paths are also accepted."), true, TEXT("") },
			{ TEXT("usecase"), TEXT("string"), TEXT("Validation usecase: Manual, Save, PreSubmit, Commandlet, Script (default Manual)"), false, TEXT("Manual") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * dump_validation_report — same validator pass as run_asset_validator, but takes a
 * content-folder path and walks the AssetRegistry to assemble the asset list. Returns
 * only the per-asset summary (no per-class details) so it stays compact even when the
 * folder contains hundreds of assets.
 */
class FECACommand_DumpValidationReport : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_validation_report"); }
	virtual FString GetDescription() const override { return TEXT("Walk a content-folder path, validate every asset under it via UEditorValidatorSubsystem, and return a compact per-asset pass/fail summary (no per-error detail). Use run_asset_validator for full error text."); }
	virtual FString GetCategory() const override { return TEXT("SourceControlValidation"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("content_path"), TEXT("string"), TEXT("Content folder path to recursively validate (e.g. /Game/Levels)"), true, TEXT("") },
			{ TEXT("max_assets"), TEXT("number"), TEXT("Cap on number of assets to validate (default 200)"), false, TEXT("200") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
