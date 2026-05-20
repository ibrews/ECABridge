// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// load_lut_texture ─ load a UTexture LUT asset by path (validates as 16x16x16 or similar)
class FECACommand_LoadLutTexture : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("load_lut_texture"); }
	virtual FString GetDescription() const override { return TEXT("Resolve a color-grading LUT texture by asset path, validate format, return metadata"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("texture_path"), TEXT("string"), TEXT("Asset path of the LUT texture (e.g. /Engine/EngineMaterials/LUT_Sample)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// apply_lut_to_volume ─ assign a LUT texture + intensity to a PostProcessVolume
class FECACommand_ApplyLutToVolume : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("apply_lut_to_volume"); }
	virtual FString GetDescription() const override { return TEXT("Assign a color-grading LUT texture and intensity to a PostProcessVolume"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the PostProcessVolume. Defaults to first in level."), false },
			{ TEXT("texture_path"), TEXT("string"), TEXT("Asset path of the LUT texture"), true },
			{ TEXT("intensity"), TEXT("number"), TEXT("LUT intensity 0..1"), false, TEXT("1.0") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// dump_color_grading ─ dump the SCCH+wheel grading parameters of a PostProcessVolume
class FECACommand_DumpColorGrading : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_color_grading"); }
	virtual FString GetDescription() const override { return TEXT("Dump color-grading wheel parameters (Saturation/Contrast/Gamma/Gain/Offset for Global/Shadows/Midtones/Highlights) of a PostProcessVolume"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the PostProcessVolume. Defaults to first in level."), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
