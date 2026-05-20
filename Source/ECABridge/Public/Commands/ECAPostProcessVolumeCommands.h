// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// create_post_process_volume ─ spawn a PostProcessVolume actor
class FECACommand_CreatePostProcessVolume : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_post_process_volume"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a PostProcessVolume actor in the level"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Actor label for the new PostProcessVolume"), false },
			{ TEXT("unbound"), TEXT("boolean"), TEXT("Apply to entire level regardless of position (default true)"), false, TEXT("true") },
			{ TEXT("priority"), TEXT("number"), TEXT("Volume priority (higher wins). Default 0."), false },
			{ TEXT("location"), TEXT("object"), TEXT("World location {x,y,z}"), false },
			{ TEXT("extent"), TEXT("object"), TEXT("Box extent {x,y,z} (applies only when unbound=false)"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// set_post_process_setting ─ set any FPostProcessSettings field by reflective name
class FECACommand_SetPostProcessSetting : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_post_process_setting"); }
	virtual FString GetDescription() const override { return TEXT("Set a single FPostProcessSettings property by name (reflective). Auto-flips the matching bOverride_ flag."); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the PostProcessVolume. Defaults to first in level."), false },
			{ TEXT("setting_name"), TEXT("string"), TEXT("FPostProcessSettings property name (e.g. BloomIntensity, FilmGrainIntensity, MotionBlurAmount)"), true },
			{ TEXT("value"), TEXT("object"), TEXT("Value — number, boolean, {r,g,b,a} color, or {x,y,z,w} vector"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// dump_post_process_settings ─ list overridden FPostProcessSettings properties
class FECACommand_DumpPostProcessSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_post_process_settings"); }
	virtual FString GetDescription() const override { return TEXT("Dump every overridden FPostProcessSettings property on a PostProcessVolume"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the PostProcessVolume. Defaults to first in level."), false },
			{ TEXT("include_defaults"), TEXT("boolean"), TEXT("If true, dumps every property regardless of override flag"), false, TEXT("false") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
