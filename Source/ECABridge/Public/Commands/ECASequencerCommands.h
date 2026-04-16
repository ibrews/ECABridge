// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── create_level_sequence ─────────────────────────────────────
// Creates a new ULevelSequence asset at the given package path.
class FECACommand_CreateLevelSequence : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_level_sequence"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Level Sequence asset at the given path"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Package directory (e.g. /Game/Cinematics)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new Level Sequence asset"), true },
			{ TEXT("frame_rate"), TEXT("number"), TEXT("Display frame rate (default 30)"), false, TEXT("30") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── add_sequence_actor_binding ────────────────────────────────
// Binds an actor from the level to the sequence so it can be animated.
class FECACommand_AddSequenceActorBinding : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_sequence_actor_binding"); }
	virtual FString GetDescription() const override { return TEXT("Bind an actor from the level to a Level Sequence for animation"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence (e.g. /Game/Cinematics/MySeq)"), true },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level to bind"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── add_sequence_transform_key ────────────────────────────────
// Adds a transform keyframe for a bound actor at a given time.
class FECACommand_AddSequenceTransformKey : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_sequence_transform_key"); }
	virtual FString GetDescription() const override { return TEXT("Add a transform keyframe for a bound actor in a Level Sequence"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence"), true },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the bound actor"), true },
			{ TEXT("time"), TEXT("number"), TEXT("Key time in seconds"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Location {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Rotation {pitch, yaw, roll}"), true },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale {x, y, z} (default 1,1,1)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── add_sequence_camera ───────────────────────────────────────
// Creates a CineCameraActor in the level and binds it to the sequence
// with a camera cut track.
class FECACommand_AddSequenceCamera : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_sequence_camera"); }
	virtual FString GetDescription() const override { return TEXT("Create a CineCameraActor and bind it to a Level Sequence with a camera cut track"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence"), true },
			{ TEXT("camera_name"), TEXT("string"), TEXT("Display name for the camera actor"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── play_sequence ─────────────────────────────────────────────
// Plays a level sequence in the editor viewport.
class FECACommand_PlaySequence : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("play_sequence"); }
	virtual FString GetDescription() const override { return TEXT("Play a Level Sequence in the editor viewport"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence to play"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_sequence_info ─────────────────────────────────────────
// Returns info about a level sequence: duration, frame rate, bound actors, tracks.
class FECACommand_GetSequenceInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_sequence_info"); }
	virtual FString GetDescription() const override { return TEXT("Get info about a Level Sequence: duration, frame rate, bindings, tracks"); }
	virtual FString GetCategory() const override { return TEXT("Sequencer"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
