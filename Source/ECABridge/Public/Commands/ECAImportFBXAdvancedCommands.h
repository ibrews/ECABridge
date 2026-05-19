// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * import_fbx_advanced — exposes the full UFbxImportUI surface (legacy UnFbx path).
 *
 * The existing import_fbx runs the Interchange auto-pipeline and supports a small
 * curated option set. import_fbx_advanced reaches deeper: smoothing groups, normal /
 * tangent import method, LOD count + auto-screen-size, physics-asset creation,
 * NaniteEnabled, animation override naming, etc. Driven by UAssetImportTask so it
 * goes through the standard editor import pipeline and shows up in the message log
 * like a UI-initiated import.
 *
 * Category Asset (always available — UFbxImportUI / UAssetImportTask are core editor
 * types).
 */
class FECACommand_ImportFBXAdvanced : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_fbx_advanced"); }
	virtual FString GetDescription() const override { return TEXT("Import an FBX file via the legacy UnFbx path with the full UFbxImportUI option surface (LODs, normals/tangents, physics asset, animation override, Nanite, etc). Use import_fbx for simple defaults; this command for fine-grained control."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"),         TEXT("string"),  TEXT("Absolute path to the source .fbx file"), true, TEXT("") },
			{ TEXT("destination_path"),    TEXT("string"),  TEXT("Content path for the imported assets (e.g. /Game/Meshes)"), true, TEXT("") },
			{ TEXT("asset_name"),          TEXT("string"),  TEXT("Override base name for the imported asset (defaults to source filename)"), false, TEXT("") },
			{ TEXT("replace_existing"),    TEXT("boolean"), TEXT("Overwrite assets at destination if they already exist"), false, TEXT("true") },
			{ TEXT("save_after_import"),   TEXT("boolean"), TEXT("Save the imported package(s) immediately after import"), false, TEXT("false") },
			// UFbxImportUI top level
			{ TEXT("import_as_skeletal"),  TEXT("boolean"), TEXT("Import as a SkeletalMesh rather than StaticMesh"), false, TEXT("false") },
			{ TEXT("import_materials"),    TEXT("boolean"), TEXT("Import materials referenced by the FBX"), false, TEXT("true") },
			{ TEXT("import_textures"),     TEXT("boolean"), TEXT("Import textures referenced by materials"), false, TEXT("true") },
			{ TEXT("import_animations"),   TEXT("boolean"), TEXT("Import animations (skeletal mesh only)"), false, TEXT("false") },
			{ TEXT("create_physics_asset"),TEXT("boolean"), TEXT("Create a default physics asset for the imported skeletal mesh"), false, TEXT("true") },
			{ TEXT("skeleton_path"),       TEXT("string"),  TEXT("Asset path to an existing USkeleton to bind the imported mesh to (skeletal only)"), false, TEXT("") },
			{ TEXT("override_animation_name"), TEXT("string"), TEXT("Override the generated AnimSequence's name (skeletal+anim only)"), false, TEXT("") },
			// LOD
			{ TEXT("lod_count"),           TEXT("number"),  TEXT("Number of LODs to generate (static mesh; default 0 = auto)"), false, TEXT("0") },
			{ TEXT("auto_compute_lod_screen_size"), TEXT("boolean"), TEXT("Auto-compute LOD screen sizes (static mesh)"), false, TEXT("true") },
			// Static-mesh import data
			{ TEXT("remove_degenerates"),  TEXT("boolean"), TEXT("Remove degenerate triangles (static mesh)"), false, TEXT("true") },
			{ TEXT("build_reversed_index_buffer"), TEXT("boolean"), TEXT("Build the reversed-index buffer (static mesh)"), false, TEXT("true") },
			{ TEXT("generate_lightmap_uvs"), TEXT("boolean"), TEXT("Generate lightmap UVs (static mesh)"), false, TEXT("true") },
			{ TEXT("build_nanite"),        TEXT("boolean"), TEXT("Enable Nanite on the imported static mesh"), false, TEXT("false") },
			{ TEXT("static_mesh_lod_group"), TEXT("string"),TEXT("Static-mesh LOD group name (e.g. LargeProp, SmallProp, HighDetail; empty for default)"), false, TEXT("") },
			// Normals / tangents
			{ TEXT("normal_import_method"), TEXT("string"), TEXT("Normal import: ComputeNormals, ImportNormals, ImportNormalsAndTangents"), false, TEXT("ComputeNormals") },
			{ TEXT("normal_generation_method"), TEXT("string"), TEXT("Normal generation: BuiltIn or MikkTSpace"), false, TEXT("MikkTSpace") },
			// Animation import data
			{ TEXT("anim_remove_redundant_keys"), TEXT("boolean"), TEXT("Remove redundant animation keys"), false, TEXT("true") },
			{ TEXT("anim_use_default_sample_rate"), TEXT("boolean"), TEXT("Resample animation to the project's default sample rate"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
