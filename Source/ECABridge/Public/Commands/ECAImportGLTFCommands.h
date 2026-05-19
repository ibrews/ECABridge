// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * import_gltf — import a glTF (.gltf/.glb) file as Unreal assets via the Interchange
 * framework's glTF translator. Mirrors the structure of import_fbx but constrained to
 * the glTF extensions Khronos publishes for Three.js / web-content interchange.
 *
 * Category "Asset" so it ships unconditionally with the rest of the asset commands —
 * Interchange + its glTF translator are stock engine modules on 5.7 and 5.8.
 */
class FECACommand_ImportGLTF : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_gltf"); }
	virtual FString GetDescription() const override { return TEXT("Import a glTF or GLB file as Unreal assets using the Interchange framework's glTF translator. Returns a summary of imported meshes / materials / textures / animations."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"), TEXT("string"), TEXT("Absolute path to the source .gltf or .glb file"), true, TEXT("") },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Content path for the imported assets (e.g. /Game/Imported/GLTF)"), true, TEXT("") },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Override base name for the imported assets (defaults to the source filename)"), false, TEXT("") },
			{ TEXT("replace_existing"), TEXT("boolean"), TEXT("Overwrite assets at destination if they already exist"), false, TEXT("true") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("source_file"),        TEXT("string"),  TEXT("Original source file path") },
			{ TEXT("destination_path"),   TEXT("string"),  TEXT("Content folder the assets were imported into") },
			{ TEXT("imported_count"),     TEXT("integer"), TEXT("Total number of UObjects produced") },
			{ TEXT("imported_objects"),   TEXT("array"),   TEXT("Per-object entries with name, path, class, type"), TEXT("object") },
			{ TEXT("primary_mesh_path"),  TEXT("string"),  TEXT("Path of the first static or skeletal mesh produced, if any") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
