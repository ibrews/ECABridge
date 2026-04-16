// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── render_sequence ──────────────────────────────────────────
// Renders a Level Sequence using Movie Render Queue (legacy pipeline).
class FECACommand_RenderSequence : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("render_sequence"); }
	virtual FString GetDescription() const override { return TEXT("Render a Level Sequence using Movie Render Queue"); }
	virtual FString GetCategory() const override { return TEXT("MovieRender"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sequence_path"), TEXT("string"), TEXT("Asset path of the Level Sequence to render (e.g. /Game/Cinematics/MySeq)"), true },
			{ TEXT("output_dir"), TEXT("string"), TEXT("Output directory (absolute or relative to project Saved folder)"), false, TEXT("Saved/MovieRenders") },
			{ TEXT("output_format"), TEXT("string"), TEXT("Image format: png, jpg, or exr"), false, TEXT("png") },
			{ TEXT("resolution_x"), TEXT("number"), TEXT("Horizontal resolution in pixels"), false, TEXT("1920") },
			{ TEXT("resolution_y"), TEXT("number"), TEXT("Vertical resolution in pixels"), false, TEXT("1080") },
			{ TEXT("frame_rate"), TEXT("number"), TEXT("Output frame rate"), false, TEXT("30") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_render_status ────────────────────────────────────────
// Returns whether a render is in progress and its completion percentage.
class FECACommand_GetRenderStatus : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_render_status"); }
	virtual FString GetDescription() const override { return TEXT("Check if a Movie Render Queue render is in progress and get completion percentage"); }
	virtual FString GetCategory() const override { return TEXT("MovieRender"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
