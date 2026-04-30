// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Focus viewport on actor(s)
 */
class FECACommand_FocusViewport : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("focus_viewport"); }
	virtual FString GetDescription() const override { return TEXT("Focus the editor viewport on one or more actors"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_names"), TEXT("array"), TEXT("Array of actor names to focus on"), false },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Single actor name to focus on"), false },
			{ TEXT("location"), TEXT("object"), TEXT("Location to focus on {x, y, z}"), false },
			{ TEXT("distance"), TEXT("number"), TEXT("Camera distance from target"), false, TEXT("500") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Select actors in the editor
 */
class FECACommand_SelectActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("select_actors"); }
	virtual FString GetDescription() const override { return TEXT("Select actors in the editor"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_names"), TEXT("array"), TEXT("Array of actor names to select"), false },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Single actor name to select"), false },
			{ TEXT("add_to_selection"), TEXT("boolean"), TEXT("Add to current selection instead of replacing"), false, TEXT("false") },
			{ TEXT("deselect_all"), TEXT("boolean"), TEXT("Deselect all actors"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get selected actors
 */
class FECACommand_GetSelectedActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_selected_actors"); }
	virtual FString GetDescription() const override { return TEXT("Get the currently selected actors"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Take a depth screenshot of the viewport
 */
class FECACommand_TakeDepthScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_depth_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Take a depth buffer screenshot from the editor viewport. Returns grayscale image where darker=closer, lighter=farther."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("file_path"), TEXT("string"), TEXT("Path to save the depth image (if not specified, returns base64)"), false },
			{ TEXT("width"), TEXT("number"), TEXT("Image width"), false, TEXT("1024") },
			{ TEXT("height"), TEXT("number"), TEXT("Image height"), false, TEXT("1024") },
			{ TEXT("max_depth"), TEXT("number"), TEXT("Maximum depth in units (default 10000 = 100m). Depths beyond this are clamped to white."), false, TEXT("10000") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Take a screenshot of the active viewport, game window, or editor window.
 * Supports three capture targets:
 *   "pie"             - Game window via FSlateApplication::TakeScreenshot (full composited with all UI)
 *   "editor_viewport" - Editor 3D viewport via GetViewportScreenShot (render target only, no Slate)
 *   "editor_window"   - Full editor window via FSlateApplication::TakeScreenshot (all panels, tabs, graphs)
 * Auto-detects: if PIE is running uses "pie", otherwise uses "editor_viewport".
 */
class FECACommand_TakeGameplayScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_gameplay_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Take a screenshot of the game viewport, editor viewport, or the full editor window. By default auto-detects: captures the PIE game window (with all Slate/UMG/HUD overlays) if playing, otherwise captures the editor 3D viewport. Use the 'target' parameter to explicitly choose what to capture, including 'editor_window' for the full editor UI (Blueprint graphs, widget hierarchies, Details panels, etc)."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("file_path"), TEXT("string"), TEXT("Path to save the screenshot PNG. If not specified, saves to project Saved/Screenshots/ with a timestamp filename."), false },
			{ TEXT("target"), TEXT("string"), TEXT("What to capture: 'pie' (game window with UI overlays), 'editor_viewport' (3D viewport only), or 'editor_window' (full editor UI including all panels/tabs). If not specified, auto-detects: PIE window if playing, otherwise editor viewport."), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Run a console command
 */
class FECACommand_RunConsoleCommand : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("run_console_command"); }
	virtual FString GetDescription() const override { return TEXT("Execute an Unreal console command"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("command"), TEXT("string"), TEXT("The console command to execute"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get current level info
 */
class FECACommand_GetLevelInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_level_info"); }
	virtual FString GetDescription() const override { return TEXT("Get information about the current level"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Open a level
 */
class FECACommand_OpenLevel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("open_level"); }
	virtual FString GetDescription() const override { return TEXT("Open a level in the editor"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("level_path"), TEXT("string"), TEXT("Path to the level asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Save current level
 */
class FECACommand_SaveLevel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_level"); }
	virtual FString GetDescription() const override { return TEXT("Save the current level"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Save a single asset by content path
 */
class FECACommand_SaveAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_asset"); }
	virtual FString GetDescription() const override { return TEXT("Save a single asset to disk by content path. Works for Blueprints, materials, Niagara systems, widgets, textures, and any UAsset. Use save_level for the current map."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the asset (e.g., /Game/Blueprints/BP_Foo)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Save all dirty (modified-in-memory) assets
 */
class FECACommand_SaveDirtyAssets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_dirty_assets"); }
	virtual FString GetDescription() const override { return TEXT("Save all modified-in-memory assets to disk. Like Save All but with no UI prompts. Useful as a one-shot persist after a batch of edits across multiple Blueprints/materials/widgets."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("include_maps"), TEXT("boolean"), TEXT("Also save dirty map (level) packages (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Play in Editor
 */
class FECACommand_PlayInEditor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("play_in_editor"); }
	virtual FString GetDescription() const override { return TEXT("Start Play in Editor session"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("viewport_index"), TEXT("number"), TEXT("Viewport index to play in"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Stop Play in Editor
 */
class FECACommand_StopPlayInEditor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_play_in_editor"); }
	virtual FString GetDescription() const override { return TEXT("Stop the current Play in Editor session"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get editor play state
 */
class FECACommand_GetPlayState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_play_state"); }
	virtual FString GetDescription() const override { return TEXT("Get the current Play in Editor state"); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get a high-level overview of the entire project: folder tree, asset counts by type.
 */
class FECACommand_GetProjectOverview : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_project_overview"); }
	virtual FString GetDescription() const override { return TEXT("Get project overview: content folder tree with asset counts by type and class, total assets. Essential for understanding an unfamiliar project."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"), TEXT("string"), TEXT("Root path to scan (default /Game/)"), false, TEXT("/Game/") },
			{ TEXT("max_depth"), TEXT("number"), TEXT("Maximum folder depth to show (default 3)"), false, TEXT("3") },
			{ TEXT("include_engine_content"), TEXT("boolean"), TEXT("Include /Engine/ content (default false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get world settings: game mode, gravity, kill Z, default pawn, etc.
 */
class FECACommand_GetWorldSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_world_settings"); }
	virtual FString GetDescription() const override { return TEXT("Get world settings for the current level: game mode, default pawn class, gravity, kill Z, world bounds, streaming distances, and other level-wide configuration."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set world settings properties.
 */
class FECACommand_SetWorldSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_world_settings"); }
	virtual FString GetDescription() const override { return TEXT("Set world settings properties: game mode override, default pawn class, kill Z, gravity, etc. Uses reflection — any editable AWorldSettings UPROPERTY can be set."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("property"), TEXT("string"), TEXT("Property name (e.g., DefaultGameMode, KillZ, bEnableWorldBoundsChecks, GlobalGravityZ)"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// Note: get_performance_stats is in ECAEnvironmentCommands
