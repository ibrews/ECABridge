// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Take a Slate screenshot of the active SGraphEditor inside an opened Blueprint
 * Asset Editor. Frames the graph (zoom-to-fit) and optionally strips selection
 * overlays so the resulting PNG is slide-ready. Returns the PNG inline as an
 * MCP image content block when file_path is omitted; otherwise writes to disk.
 *
 * Use this instead of take_gameplay_screenshot target=editor_window when you
 * want JUST the BP graph viewport (no tabs, no Details panel, no toolbar).
 */
class FECACommand_TakeBlueprintEditorScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_blueprint_editor_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Take a Slate screenshot of an opened Blueprint editor's graph viewport (UbergraphPage or function graph). Auto-opens the BP editor if closed, focuses the named graph, optionally zoom-to-fits all nodes, and optionally clears selection overlays. Returns PNG inline as an MCP image content block when file_path is omitted. Slide-ready alternative to take_gameplay_screenshot target=editor_window."); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"),  TEXT("string"),  TEXT("UE package path to the Blueprint (e.g., /Game/ThirdPerson/Blueprints/BP_WaveSpawner, with or without .BP_WaveSpawner suffix)"), true },
			{ TEXT("graph_name"),      TEXT("string"),  TEXT("Name of the UbergraphPage or FunctionGraph to focus before capture (default 'EventGraph')"), false, TEXT("EventGraph") },
			{ TEXT("frame_all_nodes"), TEXT("boolean"), TEXT("Zoom-to-fit on all nodes in the graph before capture (default true)"), false, TEXT("true") },
			{ TEXT("hide_overlays"),   TEXT("boolean"), TEXT("Best-effort hide of selection rectangles / debug overlays by clearing the selection set before capture (default true). The BP editor's pinned watch boxes and PIE debug arrows are NOT controlled by this flag — the public Slate API does not expose toggles for them in 5.7/5.8."), false, TEXT("true") },
			{ TEXT("file_path"),       TEXT("string"),  TEXT("Absolute path to save the PNG. If omitted, returns the PNG inline as an MCP image content block."), false },
			{ TEXT("width"),           TEXT("number"),  TEXT("Preferred capture width in pixels. Advisory only — the actual size is the SGraphEditor widget's rendered size, which depends on the BP editor's docked layout."), false, TEXT("1920") },
			{ TEXT("height"),          TEXT("number"),  TEXT("Preferred capture height in pixels. Advisory only — see width."), false, TEXT("1080") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
