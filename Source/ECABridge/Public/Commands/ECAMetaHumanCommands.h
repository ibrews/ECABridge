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
	virtual FString GetDescription() const override { return TEXT("Trigger auto-rigging on a MetaHumanCharacter — generates the skeleton needed for animation playback. Calls UMetaHumanCharacterEditorSubsystem::RequestAutoRigging via reflection."); }
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
