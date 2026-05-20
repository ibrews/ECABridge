// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Edit fields on a USoundAttenuation asset.
 *
 * The first inspection batch shipped dump_sound_attenuation; this is its mutator
 * counterpart. Accepts a sparse JSON object of fields to update — only the
 * fields present in the request are touched. Marks the asset dirty.
 */
class FECACommand_SetSoundAttenuation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sound_attenuation"); }
	virtual FString GetDescription() const override { return TEXT("Edit a USoundAttenuation asset's FSoundAttenuationSettings. Accepts sparse fields: falloff_distance, attenuate, spatialize, attenuate_with_lpf, enable_listener_focus, enable_occlusion, enable_reverb_send, lpf_radius_min, lpf_radius_max, occlusion_volume_attenuation, reverb_distance_min, reverb_distance_max, reverb_wet_level_min, reverb_wet_level_max. Marks the asset dirty."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("attenuation_path"),             TEXT("string"),  TEXT("Asset path to a USoundAttenuation"), true },
			{ TEXT("falloff_distance"),             TEXT("number"),  TEXT("Linear falloff distance in cm"), false },
			{ TEXT("attenuate"),                    TEXT("boolean"), TEXT("Top-level bAttenuate toggle"), false },
			{ TEXT("spatialize"),                   TEXT("boolean"), TEXT("Top-level bSpatialize toggle"), false },
			{ TEXT("attenuate_with_lpf"),           TEXT("boolean"), TEXT("Enable distance-based low-pass filtering"), false },
			{ TEXT("enable_listener_focus"),        TEXT("boolean"), TEXT("Enable listener-focus weighting"), false },
			{ TEXT("enable_occlusion"),             TEXT("boolean"), TEXT("Enable occlusion check + attenuation"), false },
			{ TEXT("enable_reverb_send"),           TEXT("boolean"), TEXT("Enable reverb send"), false },
			{ TEXT("lpf_radius_min"),               TEXT("number"),  TEXT("Distance at which LPF starts attenuating"), false },
			{ TEXT("lpf_radius_max"),               TEXT("number"),  TEXT("Distance at which LPF fully kicks in"), false },
			{ TEXT("occlusion_volume_attenuation"), TEXT("number"),  TEXT("Linear volume multiplier when fully occluded"), false },
			{ TEXT("reverb_distance_min"),          TEXT("number"),  TEXT("Distance where reverb send starts"), false },
			{ TEXT("reverb_distance_max"),          TEXT("number"),  TEXT("Distance where reverb send ends"), false },
			{ TEXT("reverb_wet_level_min"),         TEXT("number"),  TEXT("Reverb send wet level at min distance"), false },
			{ TEXT("reverb_wet_level_max"),         TEXT("number"),  TEXT("Reverb send wet level at max distance"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),           TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("fields_updated"), TEXT("array"),   TEXT("Names of fields actually modified"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Edit properties on a USoundClass asset.
 *
 * Companion to dump_sound_class. Accepts sparse fields under FSoundClassProperties.
 * Marks the asset dirty.
 */
class FECACommand_SetSoundClassProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sound_class_properties"); }
	virtual FString GetDescription() const override { return TEXT("Edit a USoundClass's FSoundClassProperties. Accepts sparse fields: volume, pitch, lowpass_filter_freq, apply_effects, always_play, is_ui, is_music, reverb, center_channel_only. Marks the asset dirty."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sound_class_path"),    TEXT("string"),  TEXT("Asset path to a USoundClass"), true },
			{ TEXT("volume"),              TEXT("number"),  TEXT("Master volume multiplier (1.0 = no change)"), false },
			{ TEXT("pitch"),               TEXT("number"),  TEXT("Master pitch multiplier (1.0 = no change)"), false },
			{ TEXT("lowpass_filter_freq"), TEXT("number"),  TEXT("Low-pass filter frequency in Hz"), false },
			{ TEXT("apply_effects"),       TEXT("boolean"), TEXT("Apply submix effects"), false },
			{ TEXT("always_play"),         TEXT("boolean"), TEXT("Always play (bypass concurrency)"), false },
			{ TEXT("is_ui"),               TEXT("boolean"), TEXT("Treat as UI sound (pauses with editor pause)"), false },
			{ TEXT("is_music"),            TEXT("boolean"), TEXT("Treat as music for music-specific routing"), false },
			{ TEXT("reverb"),              TEXT("boolean"), TEXT("Enable reverb send"), false },
			{ TEXT("center_channel_only"), TEXT("boolean"), TEXT("Route to center channel only"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),           TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("fields_updated"), TEXT("array"),   TEXT("Names of fields actually modified"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Edit base USoundCue properties (volume / pitch multipliers and asset refs).
 *
 * USoundCue's graph structure is edited via the existing add_sound_node /
 * dump_sound_cue commands; this one only touches the cue's scalar/asset fields.
 */
class FECACommand_SetSoundCueProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sound_cue_properties"); }
	virtual FString GetDescription() const override { return TEXT("Edit a USoundCue's base properties: volume_multiplier, pitch_multiplier, sound_class (asset path or empty to clear), attenuation_settings (asset path or empty), override_attenuation. Marks the asset dirty."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sound_cue_path"),       TEXT("string"),  TEXT("Asset path to a USoundCue"), true },
			{ TEXT("volume_multiplier"),    TEXT("number"),  TEXT("Cue VolumeMultiplier"), false },
			{ TEXT("pitch_multiplier"),     TEXT("number"),  TEXT("Cue PitchMultiplier"), false },
			{ TEXT("sound_class"),          TEXT("string"),  TEXT("USoundClass asset path (empty string clears)"), false },
			{ TEXT("attenuation_settings"), TEXT("string"),  TEXT("USoundAttenuation asset path (empty string clears)"), false },
			{ TEXT("override_attenuation"), TEXT("boolean"), TEXT("Cue bOverrideAttenuation toggle"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),           TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("fields_updated"), TEXT("array"),   TEXT("Names of fields actually modified"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
