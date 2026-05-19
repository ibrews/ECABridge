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
