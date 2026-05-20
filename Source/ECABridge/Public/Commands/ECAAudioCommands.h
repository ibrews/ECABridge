// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Read-only inspection of a USoundCue asset: graph node summary, output sound
 * class, attenuation reference, max distance, base volume/pitch multipliers.
 */
class FECACommand_DumpSoundCue : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_sound_cue"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a USoundCue: enumerate its USoundNode tree (class names + child counts), and report its USoundClass, USoundAttenuation, base volume/pitch, and max distance."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sound_cue_path"), TEXT("string"), TEXT("Asset path to a USoundCue (e.g. '/Game/Audio/SC_MySound')"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),             TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("name"),             TEXT("string"),  TEXT("Asset name") },
			{ TEXT("duration"),         TEXT("number"),  TEXT("Sound duration in seconds (sum of leaf wave durations)") },
			{ TEXT("max_distance"),     TEXT("number"),  TEXT("Max audible distance in cm") },
			{ TEXT("volume_multiplier"),TEXT("number"),  TEXT("Cue VolumeMultiplier") },
			{ TEXT("pitch_multiplier"), TEXT("number"),  TEXT("Cue PitchMultiplier") },
			{ TEXT("sound_class"),      TEXT("string"),  TEXT("USoundClass asset path or empty") },
			{ TEXT("attenuation"),      TEXT("string"),  TEXT("USoundAttenuation asset path or empty") },
			{ TEXT("nodes"),            TEXT("array"),   TEXT("Graph nodes: {class, child_count, leaf_info}"), TEXT("object") },
			{ TEXT("node_count"),       TEXT("integer"), TEXT("Number of USoundNode instances in the cue graph") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Read-only inspection of a USoundClass: properties, child class names,
 * passive sound mix references.
 */
class FECACommand_DumpSoundClass : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_sound_class"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a USoundClass: volume, pitch, low-pass-filter freq, output target, child class paths."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sound_class_path"), TEXT("string"), TEXT("Asset path to a USoundClass"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),               TEXT("string"), TEXT("Resolved asset path") },
			{ TEXT("name"),               TEXT("string"), TEXT("Asset name") },
			{ TEXT("volume"),             TEXT("number"), TEXT("Properties.Volume") },
			{ TEXT("pitch"),              TEXT("number"), TEXT("Properties.Pitch") },
			{ TEXT("lowpass_filter_freq"),TEXT("number"), TEXT("Properties.LowPassFilterFrequency") },
			{ TEXT("output_target"),      TEXT("string"), TEXT("Master/BGM/SFX/Voice/etc. (EAudioOutputTarget enum name)") },
			{ TEXT("child_classes"),      TEXT("array"),  TEXT("Child USoundClass asset paths"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Read-only inspection of a USoundAttenuation asset: spatialization, falloff,
 * distance algorithm, occlusion, focus, reverb send.
 */
class FECACommand_DumpSoundAttenuation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_sound_attenuation"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a USoundAttenuation: spatialization, falloff distances + algorithm, air absorption, occlusion, reverb send."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("attenuation_path"), TEXT("string"), TEXT("Asset path to a USoundAttenuation"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Enumerate USoundBase-derived assets (SoundCue, SoundWave, MetaSoundSource, etc.)
 * matching an optional path prefix and name wildcard.
 */
class FECACommand_ListSoundAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_sound_assets"); }
	virtual FString GetDescription() const override { return TEXT("List USoundBase-derived assets (SoundCue, SoundWave, MetaSoundSource, etc.) in the project. Filter by path prefix, name wildcard, and class name."); }
	virtual FString GetCategory() const override { return TEXT("Audio"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"),  TEXT("string"),  TEXT("Package path prefix (default '/Game/')"), false, TEXT("/Game/") },
			{ TEXT("name_filter"),  TEXT("string"),  TEXT("Wildcard filter on asset name (e.g. 'SC_*')"), false },
			{ TEXT("class_filter"), TEXT("string"),  TEXT("Restrict to a specific subclass (SoundCue / SoundWave / MetaSoundSource / SoundClass / SoundAttenuation)"), false },
			{ TEXT("max_results"),  TEXT("integer"), TEXT("Cap results (default 200, max 5000)"), false, TEXT("200") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("total_found"), TEXT("integer"), TEXT("Number of matching assets") },
			{ TEXT("assets"),      TEXT("array"),   TEXT("Per-asset {path, name, class}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
