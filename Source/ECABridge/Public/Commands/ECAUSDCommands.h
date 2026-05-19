// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * import_usd — import a USD (.usd/.usda/.usdc/.usdz) file via the USDImporter plugin's
 * stage-importer factory. Runs through UAssetImportTask so the editor's standard
 * import pipeline handles asset cache + factory auto-detection.
 *
 * Optional-dep gated: WITH_ECA_USD is set by the USDStageImporter build-time check,
 * and the "USD" category is dropped at runtime when the USDStageImporter module
 * isn't loaded (e.g. user disabled the plugin in their .uproject).
 */
class FECACommand_ImportUSD : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_usd"); }
	virtual FString GetDescription() const override { return TEXT("Import a USD file (.usd/.usda/.usdc/.usdz) via the USDImporter plugin. Returns the list of imported objects."); }
	virtual FString GetCategory() const override { return TEXT("USD"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"),      TEXT("string"),  TEXT("Absolute path to the .usd / .usda / .usdc / .usdz file"), true, TEXT("") },
			{ TEXT("destination_path"), TEXT("string"),  TEXT("Content path for the imported assets (e.g. /Game/Imported/USD)"), true, TEXT("") },
			{ TEXT("asset_name"),       TEXT("string"),  TEXT("Override base name for the imported scene (defaults to source filename)"), false, TEXT("") },
			{ TEXT("replace_existing"), TEXT("boolean"), TEXT("Overwrite assets at destination if they already exist"), false, TEXT("true") },
			{ TEXT("save_after_import"),TEXT("boolean"), TEXT("Save imported packages immediately after import"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * export_actor_as_usd — export the StaticMesh / SkeletalMesh referenced by an actor
 * (or any asset path) out to a .usd / .usda / .usdc file using the USDImporter plugin's
 * exporters. UExporter::RunAssetExportTask handles the per-asset-class dispatch.
 */
class FECACommand_ExportActorAsUSD : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("export_actor_as_usd"); }
	virtual FString GetDescription() const override { return TEXT("Export a UObject (asset or actor's referenced asset) as USD via UExporter::RunAssetExportTask. The target extension picks the USD flavor (.usd / .usda / .usdc)."); }
	virtual FString GetCategory() const override { return TEXT("USD"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"),  TEXT("string"), TEXT("Content path to the asset to export (e.g. /Game/Meshes/MyMesh), OR an actor label if 'is_actor_label' is true"), true, TEXT("") },
			{ TEXT("output_path"), TEXT("string"), TEXT("Absolute output file path (.usd / .usda / .usdc / .usdz)"), true, TEXT("") },
			{ TEXT("is_actor_label"), TEXT("boolean"), TEXT("If true, asset_path is interpreted as an actor label and the actor's underlying mesh asset is exported"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
