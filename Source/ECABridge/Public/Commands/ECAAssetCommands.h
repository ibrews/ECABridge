// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Import a texture from a file (PNG, JPG, TGA, etc.)
 */
class FECACommand_ImportTexture : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_texture"); }
	virtual FString GetDescription() const override { return TEXT("Import a texture from an image file (PNG, JPG, TGA, BMP, EXR) into a UAsset"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"), TEXT("string"), TEXT("File path to the source image (PNG, JPG, TGA, etc.)"), true },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Content path for the imported texture (e.g., /Game/Textures/MyTexture)"), true },
			{ TEXT("texture_name"), TEXT("string"), TEXT("Name for the texture asset (defaults to source filename)"), false },
			{ TEXT("compression_settings"), TEXT("string"), TEXT("Compression settings (Default, Normalmap, Masks, Grayscale, HDR, UserInterface2D)"), false, TEXT("Default") },
			{ TEXT("srgb"), TEXT("boolean"), TEXT("Enable sRGB color space"), false, TEXT("true") },
			{ TEXT("lod_group"), TEXT("string"), TEXT("LOD group (World, WorldNormalMap, WorldSpecular, UI, Shadowmap, etc.)"), false, TEXT("World") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new material asset
 */
class FECACommand_CreateMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_material"); }
	virtual FString GetDescription() const override { return TEXT("Create a new material asset"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_name"), TEXT("string"), TEXT("Name for the new material"), true },
			{ TEXT("path"), TEXT("string"), TEXT("Content path for the material"), false, TEXT("/Game/Materials/") },
			{ TEXT("base_color_texture"), TEXT("string"), TEXT("Path to texture for base color (optional)"), false },
			{ TEXT("normal_texture"), TEXT("string"), TEXT("Path to normal map texture (optional)"), false },
			{ TEXT("roughness_texture"), TEXT("string"), TEXT("Path to roughness texture (optional)"), false },
			{ TEXT("metallic_texture"), TEXT("string"), TEXT("Path to metallic texture (optional)"), false },
			{ TEXT("emissive_texture"), TEXT("string"), TEXT("Path to emissive texture (optional)"), false },
			{ TEXT("base_color"), TEXT("object"), TEXT("Base color as {r, g, b, a} if no texture"), false },
			{ TEXT("roughness"), TEXT("number"), TEXT("Roughness value (0-1) if no texture"), false, TEXT("0.5") },
			{ TEXT("metallic"), TEXT("number"), TEXT("Metallic value (0-1) if no texture"), false, TEXT("0.0") },
			{ TEXT("two_sided"), TEXT("boolean"), TEXT("Enable two-sided rendering"), false, TEXT("false") },
			{ TEXT("blend_mode"), TEXT("string"), TEXT("Blend mode (Opaque, Masked, Translucent, Additive, Modulate)"), false, TEXT("Opaque") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a material from textures in one call (import + material creation)
 */
class FECACommand_CreateMaterialFromTextures : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_material_from_textures"); }
	virtual FString GetDescription() const override { return TEXT("Import textures and create a material from them in one operation"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_name"), TEXT("string"), TEXT("Name for the new material"), true },
			{ TEXT("material_path"), TEXT("string"), TEXT("Content path for the material"), false, TEXT("/Game/Materials/") },
			{ TEXT("texture_path"), TEXT("string"), TEXT("Content path for imported textures"), false, TEXT("/Game/Textures/") },
			{ TEXT("base_color_file"), TEXT("string"), TEXT("File path to base color/diffuse image"), false },
			{ TEXT("normal_file"), TEXT("string"), TEXT("File path to normal map image"), false },
			{ TEXT("roughness_file"), TEXT("string"), TEXT("File path to roughness image"), false },
			{ TEXT("metallic_file"), TEXT("string"), TEXT("File path to metallic image"), false },
			{ TEXT("emissive_file"), TEXT("string"), TEXT("File path to emissive image"), false },
			{ TEXT("orm_file"), TEXT("string"), TEXT("File path to ORM (Occlusion/Roughness/Metallic) packed texture"), false },
			{ TEXT("two_sided"), TEXT("boolean"), TEXT("Enable two-sided rendering"), false, TEXT("false") },
			{ TEXT("blend_mode"), TEXT("string"), TEXT("Blend mode (Opaque, Masked, Translucent, Additive, Modulate)"), false, TEXT("Opaque") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get texture info (technical metadata only)
 */
class FECACommand_GetTextureInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_texture_info"); }
	virtual FString GetDescription() const override { return TEXT("Get technical info about a texture (resolution, format, mips). NOTE: For visual description of what a texture looks like, use get_asset_thumbnail + analyze_image instead."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("texture_path"), TEXT("string"), TEXT("Path to the texture asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get material info (technical/node graph metadata only)
 */
class FECACommand_GetMaterialInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_material_info"); }
	virtual FString GetDescription() const override { return TEXT("Get technical info about a material (nodes, parameters). NOTE: For visual description of what a material looks like, use get_asset_thumbnail + analyze_image instead."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the material asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List textures in a path
 */
class FECACommand_ListTextures : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_textures"); }
	virtual FString GetDescription() const override { return TEXT("List all texture assets in a path. TIP: For visual descriptions, use get_asset_thumbnail + analyze_image to describe what textures look like."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"), TEXT("string"), TEXT("Content path to search"), false, TEXT("/Game/") },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Search recursively"), false, TEXT("true") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List materials in a path
 */
class FECACommand_ListMaterials : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_materials"); }
	virtual FString GetDescription() const override { return TEXT("List all material assets in a path. TIP: For visual descriptions, use get_asset_thumbnail + analyze_image to describe what materials look like."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"), TEXT("string"), TEXT("Content path to search"), false, TEXT("/Game/") },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Search recursively"), false, TEXT("true") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a texture parameter on a material instance
 */
class FECACommand_SetMaterialTextureParam : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_texture_param"); }
	virtual FString GetDescription() const override { return TEXT("Set a texture parameter on a material or material instance"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the material asset"), true },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of the texture parameter"), true },
			{ TEXT("texture_path"), TEXT("string"), TEXT("Path to the texture asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set material on an actor's mesh component
 */
class FECACommand_SetActorMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_actor_material"); }
	virtual FString GetDescription() const override { return TEXT("Set a material on an actor's mesh component (StaticMesh, SkeletalMesh, etc.)"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor in the level"), true },
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the material asset"), true },
			{ TEXT("slot_index"), TEXT("number"), TEXT("Material slot index (0-based)"), false, TEXT("0") },
			{ TEXT("component_name"), TEXT("string"), TEXT("Specific component name (if actor has multiple mesh components)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get materials on an actor
 */
class FECACommand_GetActorMaterials : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actor_materials"); }
	virtual FString GetDescription() const override { return TEXT("Get all materials currently assigned to an actor's mesh components"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor in the level"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set material on a static mesh asset (default material)
 */
class FECACommand_SetStaticMeshMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_static_mesh_material"); }
	virtual FString GetDescription() const override { return TEXT("Set the default material on a static mesh asset"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Path to the static mesh asset"), true },
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to the material asset"), true },
			{ TEXT("slot_index"), TEXT("number"), TEXT("Material slot index (0-based)"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a material instance from a parent material
 */
class FECACommand_CreateMaterialInstance : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_material_instance"); }
	virtual FString GetDescription() const override { return TEXT("Create a material instance from a parent material"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("instance_name"), TEXT("string"), TEXT("Name for the new material instance"), true },
			{ TEXT("parent_material"), TEXT("string"), TEXT("Path to the parent material"), true },
			{ TEXT("path"), TEXT("string"), TEXT("Content path for the material instance"), false, TEXT("/Game/Materials/") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set scalar parameter on a material instance
 */
class FECACommand_SetMaterialInstanceScalarParam : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_instance_scalar_param"); }
	virtual FString GetDescription() const override { return TEXT("Set a scalar parameter value on a material instance"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("instance_path"), TEXT("string"), TEXT("Path to the material instance"), true },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of the scalar parameter"), true },
			{ TEXT("value"), TEXT("number"), TEXT("Value to set"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set vector parameter on a material instance
 */
class FECACommand_SetMaterialInstanceVectorParam : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_instance_vector_param"); }
	virtual FString GetDescription() const override { return TEXT("Set a vector/color parameter value on a material instance"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("instance_path"), TEXT("string"), TEXT("Path to the material instance"), true },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of the vector parameter"), true },
			{ TEXT("value"), TEXT("object"), TEXT("Value as {r, g, b, a} or {x, y, z, w}"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set texture parameter on a material instance
 */
class FECACommand_SetMaterialInstanceTextureParam : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_material_instance_texture_param"); }
	virtual FString GetDescription() const override { return TEXT("Set a texture parameter on a material instance"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("instance_path"), TEXT("string"), TEXT("Path to the material instance"), true },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of the texture parameter"), true },
			{ TEXT("texture_path"), TEXT("string"), TEXT("Path to the texture asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Import an OBJ mesh file as a Static Mesh asset
 */
class FECACommand_ImportOBJ : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_obj"); }
	virtual FString GetDescription() const override { return TEXT("Import an OBJ mesh file as a Static Mesh asset"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"), TEXT("string"), TEXT("File path to the source OBJ file"), true },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Content path for the imported mesh (e.g., /Game/Meshes)"), true },
			{ TEXT("mesh_name"), TEXT("string"), TEXT("Name for the mesh asset (defaults to source filename)"), false },
			{ TEXT("scale"), TEXT("number"), TEXT("Uniform scale factor to apply (default 1.0, use 100.0 if OBJ is in meters)"), false, TEXT("1.0") },
			{ TEXT("import_materials"), TEXT("boolean"), TEXT("Import materials from MTL file if present"), false, TEXT("true") },
			{ TEXT("generate_lightmap_uvs"), TEXT("boolean"), TEXT("Generate lightmap UVs"), false, TEXT("true") },
			{ TEXT("auto_generate_collision"), TEXT("boolean"), TEXT("Auto-generate simple collision"), false, TEXT("true") },
			{ TEXT("combine_meshes"), TEXT("boolean"), TEXT("Combine all objects into single mesh"), false, TEXT("true") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Export a texture asset to PNG file
 */
class FECACommand_ExportTexture : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("export_texture"); }
	virtual FString GetDescription() const override { return TEXT("Export a texture asset to a PNG file"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("texture_path"), TEXT("string"), TEXT("Content path to the texture asset (e.g., /Game/Textures/MyTexture)"), true },
			{ TEXT("output_path"), TEXT("string"), TEXT("File path for the output PNG file"), true },
			{ TEXT("mip_level"), TEXT("number"), TEXT("Mip level to export (0 = full resolution)"), false, TEXT("0") },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Export a render target to PNG file
 */
class FECACommand_ExportRenderTarget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("export_render_target"); }
	virtual FString GetDescription() const override { return TEXT("Export a render target asset to a PNG file"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("render_target_path"), TEXT("string"), TEXT("Content path to the render target asset"), true },
			{ TEXT("output_path"), TEXT("string"), TEXT("File path for the output PNG file"), true },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set any property on any UObject asset using reflection
 */
class FECACommand_SetAssetProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_asset_property"); }
	virtual FString GetDescription() const override { return TEXT("Set any UPROPERTY on any asset (UStaticMesh, USkeletalMesh, UTexture, USoundWave, etc.) using reflection. Use get_asset_property to discover available properties."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Property name (e.g., bAllowCPUAccess, LightMapResolution, NaniteSettings). Supports snake_case."), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set. For enums use the enum value name. For bools use true/false. For structs use nested property paths like 'NaniteSettings.bEnabled'."), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get properties from any UObject asset using reflection
 */
class FECACommand_GetAssetProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_asset_property"); }
	virtual FString GetDescription() const override { return TEXT("Get UPROPERTY values from any asset using reflection. Returns all editable properties if no specific property is requested."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Specific property name to get (optional - returns all editable properties if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List all editable properties on an asset class
 */
class FECACommand_ListAssetProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_asset_properties"); }
	virtual FString GetDescription() const override { return TEXT("List all editable UPROPERTY fields on an asset, organized by category. Useful for discovering what properties can be set."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("category"), TEXT("string"), TEXT("Filter by category name (optional)"), false },
			{ TEXT("search"), TEXT("string"), TEXT("Search filter for property names (optional)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete an asset
 */
class FECACommand_DeleteAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_asset"); }
	virtual FString GetDescription() const override { return TEXT("Delete an asset from the project. Use with caution!"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to delete (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("force"), TEXT("boolean"), TEXT("Force delete even if asset has references"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename an asset
 */
class FECACommand_RenameAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rename_asset"); }
	virtual FString GetDescription() const override { return TEXT("Rename an asset (changes the asset name, not the path)"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to rename (e.g., /Game/Meshes/OldName)"), true },
			{ TEXT("new_name"), TEXT("string"), TEXT("New name for the asset (just the name, not full path)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Move an asset to a different folder
 */
class FECACommand_MoveAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("move_asset"); }
	virtual FString GetDescription() const override { return TEXT("Move an asset to a different folder"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to move (e.g., /Game/OldFolder/MyAsset)"), true },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Destination folder path (e.g., /Game/NewFolder)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Duplicate an asset
 */
class FECACommand_DuplicateAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("duplicate_asset"); }
	virtual FString GetDescription() const override { return TEXT("Create a copy of an asset"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to duplicate (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("new_name"), TEXT("string"), TEXT("Name for the duplicated asset"), true },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Destination folder (defaults to same folder as source)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Import an FBX mesh file as a Static Mesh asset using Interchange
 */
class FECACommand_ImportFBX : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_fbx"); }
	virtual FString GetDescription() const override { return TEXT("Import an FBX mesh file as a Static Mesh asset using Unreal's Interchange framework"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_path"), TEXT("string"), TEXT("File path to the source FBX file"), true },
			{ TEXT("destination_path"), TEXT("string"), TEXT("Content path for the imported mesh (e.g., /Game/Meshes)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the imported asset (defaults to source filename)"), false },
			{ TEXT("import_materials"), TEXT("boolean"), TEXT("Import materials from FBX file"), false, TEXT("true") },
			{ TEXT("import_textures"), TEXT("boolean"), TEXT("Import textures from FBX file"), false, TEXT("true") },
			{ TEXT("import_as_skeletal"), TEXT("boolean"), TEXT("Import as Skeletal Mesh instead of Static Mesh (if mesh has bones)"), false, TEXT("false") },
			{ TEXT("import_animations"), TEXT("boolean"), TEXT("Import animations (only valid for skeletal meshes)"), false, TEXT("false") },
			{ TEXT("generate_lightmap_uvs"), TEXT("boolean"), TEXT("Generate lightmap UVs for static meshes"), false, TEXT("true") },
			{ TEXT("auto_generate_collision"), TEXT("boolean"), TEXT("Auto-generate simple collision"), false, TEXT("true") },
			{ TEXT("combine_meshes"), TEXT("boolean"), TEXT("Combine all meshes into single asset"), false, TEXT("true") },
			{ TEXT("convert_scene_unit"), TEXT("boolean"), TEXT("Convert scene units to UE units (centimeters)"), false, TEXT("true") },
			{ TEXT("force_front_x_axis"), TEXT("boolean"), TEXT("Force front axis to X instead of -Y"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get the thumbnail/icon of an asset as a PNG image file.
 * IMPORTANT FOR AI: When generating descriptions of visual assets (meshes, materials, textures, 
 * Niagara effects, etc.), ALWAYS use this tool to get the thumbnail first, then use analyze_image()
 * to describe what it looks like visually. This produces much better semantic search descriptions
 * than just reading metadata. Workflow: 1) get_asset_thumbnail -> 2) analyze_image -> 3) set_asset_association
 */
class FECACommand_GetAssetThumbnail : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_asset_thumbnail"); }
	virtual FString GetDescription() const override { return TEXT("Get the thumbnail/icon image for any asset and save it to a PNG file. IMPORTANT: For generating visual asset descriptions, use this to get the thumbnail, then analyze_image() to describe it visually, then set_asset_association() to register the description for semantic search."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Meshes/MyMesh)"), true },
			{ TEXT("output_path"), TEXT("string"), TEXT("File path to save the thumbnail PNG (e.g., C:/Thumbnails/mesh.png)"), true },
			{ TEXT("size"), TEXT("number"), TEXT("Thumbnail size in pixels (default 256, max 1024)"), false, TEXT("256") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get thumbnails for multiple assets in a single call.
 * IMPORTANT FOR AI: Use this for bulk visual analysis of assets. For each thumbnail, use analyze_image()
 * to describe what it looks like, then set_asset_association() to register for semantic search.
 * Much more efficient than calling get_asset_thumbnail multiple times.
 */
class FECACommand_GetAssetThumbnails : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_asset_thumbnails"); }
	virtual FString GetDescription() const override { return TEXT("Get thumbnails for multiple assets at once. IMPORTANT: For visual asset cataloging, get thumbnails, then analyze_image() each one to describe visually, then set_asset_association() to register descriptions for semantic search. Much more efficient than single-asset calls."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_paths"), TEXT("array"), TEXT("Array of content paths to assets"), true },
			{ TEXT("output_directory"), TEXT("string"), TEXT("Directory to save thumbnail PNGs (files named after assets, e.g., MyMesh.png)"), true },
			{ TEXT("size"), TEXT("number"), TEXT("Thumbnail size in pixels (default 256, max 1024)"), false, TEXT("256") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ============================================================================
// Rosetta Stone: Deep introspection commands
// ============================================================================

/**
 * Dump full asset state to JSON in a single call.
 * Combines list_asset_properties + get_asset_property + metadata into one response.
 * Makes any .uasset fully legible to an LLM without round-trips.
 */
class FECACommand_DumpAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_asset"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a complete asset to JSON: all UPROPERTYs (with values), sub-objects, asset references, and metadata. Single-call alternative to list_asset_properties + get_asset_property."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Characters/Hero)"), true },
			{ TEXT("depth"), TEXT("number"), TEXT("Sub-object recursion depth (default 2, max 5)"), false, TEXT("2") },
			{ TEXT("include_defaults"), TEXT("boolean"), TEXT("Include properties at engine default values (default false — only shows changed properties)"), false, TEXT("false") },
			{ TEXT("include_thumbnail"), TEXT("boolean"), TEXT("Include base64 PNG thumbnail in response (default false)"), false, TEXT("false") },
			{ TEXT("sections"), TEXT("array"), TEXT("Filter response sections: properties, references, metadata, sub_objects. Default: all"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Search the asset registry for assets by class, path, name wildcard.
 * General-purpose asset discovery — find what exists before introspecting.
 */
class FECACommand_FindAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_assets"); }
	virtual FString GetDescription() const override { return TEXT("Search the asset registry by class, path, or name. Returns matching assets with optional metadata. Use to discover assets before introspecting with dump_asset."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_filter"), TEXT("string"), TEXT("Asset class name (e.g., StaticMesh, Material, Blueprint, Texture2D, SoundWave, NiagaraSystem)"), false },
			{ TEXT("path_filter"), TEXT("string"), TEXT("Content path prefix (e.g., /Game/Characters/)"), false, TEXT("/Game/") },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Wildcard name match (e.g., *Hero*, SM_*)"), false },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Search subdirectories"), false, TEXT("true") },
			{ TEXT("max_results"), TEXT("number"), TEXT("Maximum results to return (default 100)"), false, TEXT("100") },
			{ TEXT("include_metadata"), TEXT("boolean"), TEXT("Include disk size and class per result"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get asset dependency graph — what an asset references and what references it.
 */
class FECACommand_GetAssetReferences : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_asset_references"); }
	virtual FString GetDescription() const override { return TEXT("Get asset dependency graph: what this asset references (dependencies) and what references it (referencers). Essential for understanding 'what breaks if I change this?'"); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset"), true },
			{ TEXT("direction"), TEXT("string"), TEXT("dependencies, referencers, or both (default: both)"), false, TEXT("both") },
			{ TEXT("depth"), TEXT("number"), TEXT("Recursion depth for transitive references (default 1, max 3)"), false, TEXT("1") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Validate an asset using UE's built-in validation framework.
 */
class FECACommand_ValidateAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("validate_asset"); }
	virtual FString GetDescription() const override { return TEXT("Run UE's asset validation on any asset and return errors/warnings. Use after making changes to verify correctness."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset to validate"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get class hierarchy: parents, interfaces, and child classes.
 */
class FECACommand_GetClassHierarchy : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_class_hierarchy"); }
	virtual FString GetDescription() const override { return TEXT("Get class hierarchy for any UClass: parent chain, implemented interfaces, direct child classes, and key properties. Essential for understanding what you can cast to and what functions are available."); }
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name (e.g., Character, StaticMeshActor, PlayerController) or full path (/Script/Engine.Character)"), true },
			{ TEXT("include_children"), TEXT("boolean"), TEXT("Include direct child classes (default true)"), false, TEXT("true") },
			{ TEXT("include_functions"), TEXT("boolean"), TEXT("Include BlueprintCallable functions (default false — can be verbose)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
