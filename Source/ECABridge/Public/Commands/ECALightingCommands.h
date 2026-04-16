// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── set_light_properties ────────────────────────────────────
// Set detailed properties on a light actor (intensity, color, attenuation,
// shadows, temperature, source radius, cone angles for spotlights, etc.).
class FECACommand_SetLightProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_light_properties"); }
	virtual FString GetDescription() const override { return TEXT("Set detailed properties on a light actor (intensity, color, attenuation, shadows, temperature, cone angles)"); }
	virtual FString GetCategory() const override { return TEXT("Lighting"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the light actor in the level"), true },
			{ TEXT("intensity"), TEXT("number"), TEXT("Light intensity (in candelas/lumens depending on light type)"), false },
			{ TEXT("color"), TEXT("object"), TEXT("Light color as {r, g, b} with values 0-255"), false },
			{ TEXT("attenuation_radius"), TEXT("number"), TEXT("Attenuation radius (not applicable to directional lights)"), false },
			{ TEXT("cast_shadows"), TEXT("boolean"), TEXT("Whether the light casts shadows"), false },
			{ TEXT("temperature"), TEXT("number"), TEXT("Color temperature in Kelvin (e.g. 6500 for daylight)"), false },
			{ TEXT("source_radius"), TEXT("number"), TEXT("Source radius for soft shadows (point/spot lights only)"), false },
			{ TEXT("soft_source_radius"), TEXT("number"), TEXT("Soft source radius for softer shadow penumbra (point/spot lights only)"), false },
			{ TEXT("inner_cone_angle"), TEXT("number"), TEXT("Inner cone angle in degrees (spot lights only)"), false },
			{ TEXT("outer_cone_angle"), TEXT("number"), TEXT("Outer cone angle in degrees (spot lights only)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_light_properties ────────────────────────────────────
// Get all light properties from a light actor.
class FECACommand_GetLightProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_light_properties"); }
	virtual FString GetDescription() const override { return TEXT("Get all light properties from a light actor (intensity, color, attenuation, shadow settings, type, mobility)"); }
	virtual FString GetCategory() const override { return TEXT("Lighting"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the light actor in the level"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_light_rig ────────────────────────────────────────
// Create a standard 3-point lighting setup (key, fill, rim) around a target.
class FECACommand_CreateLightRig : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_light_rig"); }
	virtual FString GetDescription() const override { return TEXT("Create a standard 3-point lighting setup (key, fill, rim) around a target location"); }
	virtual FString GetCategory() const override { return TEXT("Lighting"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("target_location"), TEXT("object"), TEXT("Target location as {x, y, z}"), false },
			{ TEXT("target_name"), TEXT("string"), TEXT("Name of an actor to light (used to derive target location if target_location is not specified)"), false },
			{ TEXT("key_intensity"), TEXT("number"), TEXT("Key light intensity"), false, TEXT("5000") },
			{ TEXT("fill_ratio"), TEXT("number"), TEXT("Fill light intensity as a ratio of key intensity"), false, TEXT("0.5") },
			{ TEXT("rim_intensity"), TEXT("number"), TEXT("Rim/back light intensity"), false, TEXT("3000") },
			{ TEXT("radius"), TEXT("number"), TEXT("Distance of lights from target"), false, TEXT("300") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_post_process_settings ───────────────────────────────
// Configure post-process volume settings (bloom, exposure, AO, vignette, LUT).
class FECACommand_SetPostProcessSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_post_process_settings"); }
	virtual FString GetDescription() const override { return TEXT("Configure post-process volume settings (bloom, exposure, AO, vignette, color grading LUT)"); }
	virtual FString GetCategory() const override { return TEXT("Lighting"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of a PostProcessVolume actor (finds first PPV in level if not specified)"), false },
			{ TEXT("bloom_intensity"), TEXT("number"), TEXT("Bloom intensity (0.0 to 8.0+)"), false },
			{ TEXT("exposure_compensation"), TEXT("number"), TEXT("Exposure compensation in EV (-15 to 15)"), false },
			{ TEXT("ambient_occlusion_intensity"), TEXT("number"), TEXT("Ambient occlusion intensity (0.0 to 1.0)"), false },
			{ TEXT("auto_exposure"), TEXT("boolean"), TEXT("Enable or disable auto exposure"), false },
			{ TEXT("vignette_intensity"), TEXT("number"), TEXT("Vignette intensity (0.0 to 1.0)"), false },
			{ TEXT("color_grading_lut"), TEXT("string"), TEXT("Asset path to a color grading LUT texture"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
