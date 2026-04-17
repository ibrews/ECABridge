// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new MetaHuman Character asset with initial properties.
 * Requires the MetaHumanCharacter plugin.
 */
class FECACommand_CreateMetaHumanCharacter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_metahuman_character"); }
	virtual FString GetDescription() const override { return TEXT("Create a new MetaHumanCharacter asset. Set initial properties like body type (small, medium, tall, muscular), skin tone, eye color, hair style at creation time. The character will be created in unrigged state — assemble/build it via the MetaHuman editor afterward for full mesh generation."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path for the new character (e.g., /Game/Characters/MH_Hero)"), true },
			{ TEXT("body_type"), TEXT("string"), TEXT("Body preset: small, medium, tall, muscular, athletic (optional)"), false },
			{ TEXT("skin_tone"), TEXT("string"), TEXT("Skin tone preset: light, medium_light, medium, medium_dark, dark (optional)"), false },
			{ TEXT("eye_color"), TEXT("string"), TEXT("Eye color preset: blue, green, brown, hazel, gray (optional)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Dump a MetaHumanCharacter's full state: skin, eyes, teeth, makeup, body, wardrobe.
 */
class FECACommand_DumpMetaHumanCharacter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_metahuman_character"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a MetaHumanCharacter asset to JSON: skin tone and colors, eye color and shape, teeth, makeup, body type, rigging state, and any wardrobe items. Requires MetaHumanCharacter plugin."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set any property on a MetaHumanCharacter via reflection — supports nested paths like 'SkinSettings.BaseColorCM'.
 */
class FECACommand_SetMetaHumanProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a MetaHumanCharacter via reflection. Supports nested struct paths. Common properties: SkinSettings, EyesSettings, TeethSettings, MakeupSettings. Use dump_metahuman_character to discover properties."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Property name or dotted path (e.g., 'SkinSettings.BaseColorCM.R')"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Open the MetaHuman Character editor for an asset (triggers the preview viewport).
 * Works for any asset class — generic asset editor opener.
 */
class FECACommand_OpenMetaHumanEditor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("open_metahuman_editor"); }
	virtual FString GetDescription() const override { return TEXT("Open the MetaHuman Character editor on an asset (or any asset via UAssetEditorSubsystem). Makes the MetaHuman visible in the editor preview viewport where skin/eyes/freckles/makeup settings render."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter asset (or any asset)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Trigger the MetaHuman build/assembly pipeline — generates the mesh from data settings.
 */
class FECACommand_BuildMetaHuman : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("build_metahuman"); }
	virtual FString GetDescription() const override { return TEXT("Trigger the MetaHuman Character build/assembly pipeline for a character asset. This generates the actual face and body mesh from the data settings (skin tone, eyes, body type, etc.). Required to see changes in the viewport. Operation may be async — check back after a few seconds."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter to build"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Download MetaHuman texture sources (skin albedo, normal maps) from Epic's service.
 */
class FECACommand_DownloadMetaHumanTextures : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("download_metahuman_textures"); }
	virtual FString GetDescription() const override { return TEXT("Download skin/face texture sources for a MetaHumanCharacter. Required before the character renders with actual skin (otherwise shows pink/blue material zones). Calls UMetaHumanCharacterEditorSubsystem::RequestTextureSources via reflection."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Request auto-rigging for a MetaHumanCharacter (Rig State: Unrigged → Rigged).
 */
class FECACommand_RigMetaHuman : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rig_metahuman"); }
	virtual FString GetDescription() const override { return TEXT("Trigger auto-rigging on a MetaHumanCharacter. rig_type='full' creates Joints + Blend Shapes (needed for face animation), 'joints' creates skeleton only. Default: full."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("rig_type"), TEXT("string"), TEXT("'full' for Joints + Blend Shapes (default, recommended for animation), 'joints' for joints-only rig"), false, TEXT("full") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Spawn a MetaHuman actor in the current level.
 */
class FECACommand_SpawnMetaHumanActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_metahuman_actor"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a MetaHumanCharacter as an actor in the current level. Calls UMetaHumanCharacterEditorSubsystem::SpawnMetaHumanActor. The character must already be built (textures downloaded, rigged) for the actor to render properly."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get all body constraints for a MetaHumanCharacter with current values and ranges.
 */
class FECACommand_GetMetaHumanBodyConstraints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_metahuman_body_constraints"); }
	virtual FString GetDescription() const override { return TEXT("List all body-shape constraints for a MetaHuman (Height, Weight, Chest, Waist, etc.) with current values and valid ranges. Use this to discover what body-shape dimensions can be set."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set body constraints on a MetaHumanCharacter to morph the parametric body.
 */
class FECACommand_SetMetaHumanBodyConstraints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_body_constraints"); }
	virtual FString GetDescription() const override { return TEXT("Set one or more body constraints on a MetaHuman to morph the parametric body shape. Each constraint is {name, target_measurement, active}. Valid names come from get_metahuman_body_constraints (Height, Weight, Chest, etc.). Measurements are in cm."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("constraints"), TEXT("array"), TEXT("Array of {name: string, target_measurement: number, active: boolean}"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a fixed-body-type preset (height × weight grid).
 */
class FECACommand_SetMetaHumanBodyType : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_body_type"); }
	virtual FString GetDescription() const override { return TEXT("Set a MetaHuman body type from Epic's preset grid. Format: {gender}_{height}_{weight}. Gender: f|m. Height: srt|med|tal. Weight: unw|nrw|ovw. Examples: m_tal_ovw (tall muscular male), f_med_nrw (average female), m_srt_unw (short thin male). Also accepts 'BlendableBody'. Calls SetMetaHumanBodyType via reflection."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("body_type"), TEXT("string"), TEXT("Body type preset: f_med_nrw, m_tal_ovw, etc. (19 options + BlendableBody)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Re-run the MetaHuman character editor pipeline at preview quality.
 * Refreshes the rendered preview after changes (face preset, makeup, grooms, etc.).
 */
class FECACommand_RefreshMetaHumanPreview : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("refresh_metahuman_preview"); }
	virtual FString GetDescription() const override { return TEXT("Rebuild the MetaHuman preview at Preview quality. Needed after changing face preset, attaching grooms, changing body constraints — so the rendered character actually reflects the new settings. Calls RunCharacterEditorPipelineForPreview."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Screenshot the MetaHuman character editor's preview viewport.
 */
class FECACommand_TakeMetaHumanEditorScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_metahuman_editor_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Capture a screenshot of the MetaHuman Character editor's preview viewport. This is the viewport that shows the character with full skin/hair/makeup rendering — NOT the main level viewport. Opens the editor first if needed, then captures."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("file_path"), TEXT("string"), TEXT("Absolute path to save the PNG (e.g., D:/Meridian.png)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List available MetaHuman wardrobe items (hair, beard, eyebrows, clothing) from the Engine.
 */
class FECACommand_ListMetaHumanGrooms : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_metahuman_grooms"); }
	virtual FString GetDescription() const override { return TEXT("List available MetaHuman wardrobe items (hair grooms, beards, eyebrows, eyelashes, outfits). Returns assets grouped by slot. Searches the asset registry for UMetaHumanWardrobeItem assets. Use the returned paths with attach_metahuman_groom."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("slot_filter"), TEXT("string"), TEXT("Optional slot filter: Hair, Beard, Eyebrows, Eyelashes, Mustache, Peachfuzz, Clothing. If empty, returns all."), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List available MetaHuman face presets from the Engine.
 */
class FECACommand_ListMetaHumanPresets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_metahuman_presets"); }
	virtual FString GetDescription() const override { return TEXT("List available MetaHuman face preset characters (Ada, Bruce, Celeste, etc.). These are pre-sculpted faces you can apply to your character via set_metahuman_face_preset."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Apply a face preset to a MetaHumanCharacter by copying face-shape properties from a preset character.
 */
class FECACommand_SetMetaHumanFacePreset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_face_preset"); }
	virtual FString GetDescription() const override { return TEXT("Copy face shape and head model settings from a preset MetaHumanCharacter to the target character. Use list_metahuman_presets to find preset names. By default copies HeadModelSettings + FaceEvaluationSettings; optionally copies SkinSettings/EyesSettings too. Does NOT overwrite rig state."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the target MetaHumanCharacter"), true },
			{ TEXT("preset_path"), TEXT("string"), TEXT("Content path to a preset MetaHumanCharacter (e.g., /MetaHumanCharacter/Optional/Presets/Ada)"), true },
			{ TEXT("include_skin"), TEXT("boolean"), TEXT("Also copy SkinSettings (skin tone)"), false, TEXT("false") },
			{ TEXT("include_eyes"), TEXT("boolean"), TEXT("Also copy EyesSettings"), false, TEXT("false") },
			{ TEXT("include_makeup"), TEXT("boolean"), TEXT("Also copy MakeupSettings"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Apply makeup to a MetaHumanCharacter with simple, named params.
 */
class FECACommand_SetMetaHumanMakeup : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_makeup"); }
	virtual FString GetDescription() const override { return TEXT("Apply makeup with a single call. Lip types: None, Natural, Hollywood, Cupid, etc. Eye types: None, ThinLiner, SoftSmokey, CatEye, FullThinLiner. Blush types: None, Angled, Apple, LowSweep, HighCurve. Colors are RGBA 0-1."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("lip_type"), TEXT("string"), TEXT("Lipstick type: None, Natural, Hollywood, Cupid, etc."), false },
			{ TEXT("lip_color"), TEXT("object"), TEXT("Lip color {r,g,b,a} 0-1"), false },
			{ TEXT("eye_type"), TEXT("string"), TEXT("Eye makeup type: None, ThinLiner, SoftSmokey, CatEye, FullThinLiner"), false },
			{ TEXT("eye_color"), TEXT("object"), TEXT("Eye makeup primary color {r,g,b,a} 0-1"), false },
			{ TEXT("blush_type"), TEXT("string"), TEXT("Blush type: None, Angled, Apple, LowSweep, HighCurve"), false },
			{ TEXT("blush_color"), TEXT("object"), TEXT("Blush color {r,g,b,a} 0-1"), false },
			{ TEXT("foundation"), TEXT("boolean"), TEXT("Enable foundation"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Attach a hair groom (or other wardrobe item) to a MetaHumanCharacter.
 */
class FECACommand_AttachMetaHumanGroom : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("attach_metahuman_groom"); }
	virtual FString GetDescription() const override { return TEXT("Attach a UMetaHumanWardrobeItem (hair groom, outfit, etc.) to a MetaHumanCharacter. The wardrobe item must be a pre-made asset. Writes to the character's WardrobeIndividualAssets map under the given slot name. Common slot names: 'Hair', 'Eyebrows', 'Beard', 'Eyelashes', 'Outfit'."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("slot_name"), TEXT("string"), TEXT("Slot name (Hair, Eyebrows, Beard, Eyelashes, Outfit, etc.)"), true },
			{ TEXT("wardrobe_item_path"), TEXT("string"), TEXT("Content path to a UMetaHumanWardrobeItem asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Switch the MetaHuman editor viewport preview mode.
 * Controls whether the character renders with actual skin, the topology/zone overlay, or clay shader.
 */
class FECACommand_SetMetaHumanPreviewMode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_metahuman_preview_mode"); }
	virtual FString GetDescription() const override { return TEXT("Switch the MetaHuman editor viewport preview mode. 'skin' shows fully-textured rendering, 'topology' shows editable zone overlay (default for unrigged characters), 'clay' shows a matte clay shader."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("mode"), TEXT("string"), TEXT("Preview mode: skin, topology, or clay"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Describe a MetaHuman in natural language and apply best-guess settings.
 * Takes descriptive terms like "large white-skinned with purple hair" and maps them to properties.
 */
class FECACommand_DescribeMetaHuman : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_metahuman"); }
	virtual FString GetDescription() const override { return TEXT("Apply a natural-language description to a MetaHumanCharacter. Parses phrases like 'tall muscular dark-skinned with green eyes' and sets the matching properties. Convenience wrapper — use set_metahuman_property for precise control."); }
	virtual FString GetCategory() const override { return TEXT("MetaHuman"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("character_path"), TEXT("string"), TEXT("Content path to the MetaHumanCharacter"), true },
			{ TEXT("description"), TEXT("string"), TEXT("Natural-language description (e.g., 'large white-skinned with purple hair and green eyes')"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
