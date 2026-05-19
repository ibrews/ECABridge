// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Promote a node's inline UPCGSettings into a standalone settings asset (mirror of
 * the editor's "Convert to Settings Asset" action), then rewire the node to use
 * a UPCGSettingsInstance referencing the new asset.
 */
class FECACommand_ExtractPCGSettingsAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("extract_pcg_settings_asset"); }
	virtual FString GetDescription() const override { return TEXT("Save a node's inline settings as a standalone UPCGSettings asset and rewire the source node to a UPCGSettingsInstance pointing at it. Other nodes can then reference the same asset for shared configuration."); }
	virtual FString GetCategory() const override { return TEXT("PCG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("graph_path"), TEXT("string"), TEXT("Path to the PCGGraph asset that owns the node"), true },
			{ TEXT("node_id"),    TEXT("string"), TEXT("Stable name of the node whose settings to extract"), true },
			{ TEXT("asset_path"), TEXT("string"), TEXT("Full content path for the new settings asset (e.g. /Game/PCG/MySettings)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("graph_path"),     TEXT("string"), TEXT("Source PCGGraph path") },
			{ TEXT("node_id"),        TEXT("string"), TEXT("Source node id") },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Class of the saved settings asset") },
			{ TEXT("asset_path"),     TEXT("string"), TEXT("Saved settings-asset path") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
