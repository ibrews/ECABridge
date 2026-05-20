// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * list_livelink_subjects — enumerate every LiveLink subject currently registered
 * with the editor's ILiveLinkClient modular feature.
 *
 * Returns subject name, source GUID, source machine, source type, role class,
 * enabled flag, virtual-subject flag, and state. Useful for verifying that a
 * tracker / source is delivering data before driving any LiveLink-bound asset.
 *
 * If the LiveLink plugin is disabled in the project, the modular-feature query
 * returns nullptr and this command returns an empty subjects array with a
 * descriptive `message` field.
 */
class FECACommand_ListLiveLinkSubjects : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_livelink_subjects"); }
	virtual FString GetDescription() const override { return TEXT("List all LiveLink subjects currently registered with the editor. Returns name, source, role, enabled, virtual, and state for each subject. Optional include_disabled (default true) and include_virtual (default true) toggle which subjects are returned."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("include_disabled"), TEXT("boolean"), TEXT("Include disabled subjects (default true)"), false, TEXT("true") },
			{ TEXT("include_virtual"), TEXT("boolean"), TEXT("Include virtual subjects (default true)"), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * dump_livelink_data — return current static + frame data for a single LiveLink subject.
 *
 * Resolves the subject by name (most-recently-added source wins on collision) and
 * returns its role class, state, source GUID, and a JSON-flattened representation of
 * the static data struct. Frame data isn't deeply serialized — instead the most
 * recent N frame timestamps (default 5) are included so the caller can confirm the
 * subject is producing data without requiring a Blueprint evaluator.
 */
class FECACommand_DumpLiveLinkData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_livelink_data"); }
	virtual FString GetDescription() const override { return TEXT("Dump the current LiveLink data for one subject: role, state, source info, and the most recent N frame timestamps (default 5). Pass subject_name (required). Use list_livelink_subjects first to enumerate valid names."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("subject_name"), TEXT("string"), TEXT("Subject name to inspect"), true, TEXT("") },
			{ TEXT("recent_frames"), TEXT("number"), TEXT("Number of most recent frame timestamps to include (default 5)"), false, TEXT("5") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Enumerate ULiveLinkPreset assets in the project.
 *
 * A LiveLink preset bundles a set of sources + subjects so they can be re-applied
 * to the LiveLink client in one shot — the backbone of cinematic face/body capture
 * setups where the same Take Recorder rig needs to come up identically every session.
 *
 * Returns one entry per preset with source count, subject count, and asset path.
 * If the LiveLink plugin isn't loaded, returns an empty list with plugin_available=false.
 */
class FECACommand_ListLiveLinkPresets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_livelink_presets"); }
	virtual FString GetDescription() const override { return TEXT("List every ULiveLinkPreset asset in the project. Returns source count, subject count, and asset path."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Package path prefix (default '/Game/')"), false, TEXT("/Game/") },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Wildcard filter on asset name"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),   TEXT("integer"), TEXT("Number of presets returned") },
			{ TEXT("presets"), TEXT("array"),   TEXT("[{path, name, source_count, subject_count}]"), TEXT("object") },
			{ TEXT("plugin_available"), TEXT("boolean"), TEXT("True if the LiveLink plugin is loaded in this editor session") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect a single ULiveLinkPreset: per-source type + per-subject name and role.
 *
 * Reflectively reads the preset's Sources and Subjects arrays. Source entries
 * surface the factory class and friendly name; subject entries surface the
 * subject name and role class.
 */
class FECACommand_DumpLiveLinkPreset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_livelink_preset"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a ULiveLinkPreset: enumerate its source factories + friendly names and its subject names + role class."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("preset_path"), TEXT("string"), TEXT("Asset path to a ULiveLinkPreset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),     TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("name"),     TEXT("string"),  TEXT("Asset name") },
			{ TEXT("sources"),  TEXT("array"),   TEXT("[{factory, source_type}]"), TEXT("object") },
			{ TEXT("subjects"), TEXT("array"),   TEXT("[{name, role, enabled}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Apply a ULiveLinkPreset to the active LiveLink client.
 *
 * Calls the preset's AddToClient (additive, keeps existing sources/subjects) or
 * ApplyToClient (replaces everything). Returns synchronously with success/fail.
 *
 * Note: AddToClient/ApplyToClient is the synchronous form; the latent variant
 * (ApplyToClientLatent) requires a world context and is not exposed here.
 */
class FECACommand_ApplyLiveLinkPreset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("apply_livelink_preset"); }
	virtual FString GetDescription() const override { return TEXT("Apply a ULiveLinkPreset to the active LiveLink client. Pass preset_path (required) and optionally additive=true to call AddToClient instead of ApplyToClient (default additive=true)."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("preset_path"), TEXT("string"),  TEXT("Asset path to a ULiveLinkPreset"), true },
			{ TEXT("additive"),    TEXT("boolean"), TEXT("If true (default), call AddToClient — preserves existing sources. If false, call ApplyToClient — wipes existing sources/subjects first."), false, TEXT("true") },
			{ TEXT("recreate"),    TEXT("boolean"), TEXT("Only used when additive=true. If true (default), already-existing sources/subjects from this preset are recreated."), false, TEXT("true") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),    TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("applied"), TEXT("boolean"), TEXT("True if the preset was applied successfully") },
			{ TEXT("mode"),    TEXT("string"),  TEXT("'AddToClient' or 'ApplyToClient'") },
			{ TEXT("plugin_available"), TEXT("boolean"), TEXT("True if the LiveLink plugin is loaded") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rebuild a ULiveLinkPreset's source/subject list from the active LiveLink client.
 *
 * Useful for capturing the current LiveLink session state back into a preset
 * asset for reuse next session. Marks the preset dirty; caller should save_asset
 * to persist.
 */
class FECACommand_BuildLiveLinkPresetFromClient : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("build_livelink_preset_from_client"); }
	virtual FString GetDescription() const override { return TEXT("Capture the current LiveLink client state (sources + subjects) into an existing ULiveLinkPreset asset. Marks the asset dirty."); }
	virtual FString GetCategory() const override { return TEXT("LiveLink"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("preset_path"), TEXT("string"), TEXT("Asset path to a ULiveLinkPreset to overwrite"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),          TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("source_count"),  TEXT("integer"), TEXT("Number of sources captured into the preset") },
			{ TEXT("subject_count"), TEXT("integer"), TEXT("Number of subjects captured into the preset") },
			{ TEXT("plugin_available"), TEXT("boolean"), TEXT("True if the LiveLink plugin is loaded") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
