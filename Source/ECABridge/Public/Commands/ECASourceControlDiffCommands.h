// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * diff_asset_against_depot — compare a local asset against its head revision in source
 * control. Returns sizes and SHA1 hashes on both sides, plus a textual unified diff if
 * the file looks like text (.ini, .json, .cs, .py, …). For binary content (the common
 * .uasset case) the response includes a "binary, diff via UE diff tool" note and the
 * caller is expected to open the asset diff in the editor instead.
 */
class FECACommand_DiffAssetAgainstDepot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("diff_asset_against_depot"); }
	virtual FString GetDescription() const override { return TEXT("Diff a local asset against the head revision in source control. Returns local/depot size + SHA1, and a unified text diff for text files. Binary assets get a marker and should be diffed via the editor."); }
	virtual FString GetCategory() const override { return TEXT("SourceControl"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Asset path (e.g. /Game/Foo/Bar)"), true, TEXT("") },
			{ TEXT("max_text_bytes"), TEXT("number"), TEXT("Maximum bytes to load when producing a text diff (default 65536)"), false, TEXT("65536") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
