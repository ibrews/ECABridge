// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * validate_before_submit — run the editor's data validators across the contents of a
 * pending changelist. Submit-guard helper: refuses to call submit_changelist itself,
 * just returns pass/fail + per-asset error/warning details so the caller can decide.
 *
 * Lives in the SourceControlValidation category so the runtime registry can drop it
 * when the DataValidation engine plugin isn't loaded (Build.cs gates it behind
 * WITH_ECA_DATAVALIDATION).
 */
class FECACommand_ValidateBeforeSubmit : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("validate_before_submit"); }
	virtual FString GetDescription() const override { return TEXT("Run UEditorValidatorSubsystem across the assets in a pending changelist; returns overall pass/fail and per-asset errors/warnings. Does NOT submit; the caller should consult the result and then call submit_changelist."); }
	virtual FString GetCategory() const override { return TEXT("SourceControlValidation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("changelist_id"), TEXT("string"), TEXT("Identifier of the pending changelist to validate"), true, TEXT("") },
			{ TEXT("max_assets"), TEXT("number"), TEXT("Cap on number of assets to validate (default 100)"), false, TEXT("100") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
