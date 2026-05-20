// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Walk the Slate widget tree of an editor window and return a flat list of
 * matching widgets. This is the agent's "discovery" tool — find clickable /
 * focusable widgets by accessible text, widget type, etc.
 *
 * IMPORTANT: the returned `widget_id` values are valid only within the same
 * MCP session AND only until the Slate tree is rebuilt (which happens on tab
 * switch, redraw, modal open, panel rearrangement). Always call
 * find_slate_widgets immediately before clicking / typing / screenshotting
 * via widget_id.
 */
class FECACommand_FindSlateWidgets : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_slate_widgets"); }
	virtual FString GetDescription() const override { return TEXT("Walk the Slate widget tree of an editor window and return matching widgets. Use to discover clickable/focusable widgets. widget_id is transient: stays valid only until the Slate tree rebuilds (tab switch, redraw, modal open). Call this immediately before click/type/screenshot via widget_id."); }
	virtual FString GetCategory() const override { return TEXT("Slate Input"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("text_contains"), TEXT("string"), TEXT("Case-insensitive substring match against widget accessible text and type name"), false },
			{ TEXT("widget_type"), TEXT("string"), TEXT("Exact match on Slate widget type (e.g., 'SButton', 'SDockTab', 'SCheckBox')"), false },
			{ TEXT("window_title"), TEXT("string"), TEXT("Restrict search to the window with this title. Default: active top-level editor window."), false },
			{ TEXT("max_results"), TEXT("number"), TEXT("Maximum widgets to return"), false, TEXT("50") },
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("window_title"), TEXT("string"),  TEXT("Title of the window the search ran against") },
			{ TEXT("widgets"),      TEXT("array"),   TEXT("Matched widgets: widget_id, type, accessible_text, geometry {x,y,w,h}, is_enabled, is_visible"), TEXT("object") },
			{ TEXT("count"),        TEXT("integer"), TEXT("Number of widgets returned") },
			{ TEXT("truncated"),    TEXT("boolean"), TEXT("True if more matches existed than max_results") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Synthesize a mouse-down + mouse-up sequence on a Slate widget (or absolute
 * window pixel). Routes through FSlateApplication::ProcessMouseMoveEvent /
 * ProcessMouseButtonDownEvent / ProcessMouseButtonUpEvent so the click hits
 * the in-process Slate hit-test grid — no OS-level pointer movement.
 */
class FECACommand_ClickSlateWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("click_slate_widget"); }
	virtual FString GetDescription() const override { return TEXT("Synthesize a mouse click on a Slate widget. Targets: widget_id (from find_slate_widgets), accessible_text (re-walks the tree), or window_pixel {x,y}. Routes through FSlateApplication's synthetic input pipeline — no OS-level pointer movement."); }
	virtual FString GetCategory() const override { return TEXT("Slate Input"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_id"), TEXT("number"), TEXT("Widget id from a recent find_slate_widgets call"), false },
			{ TEXT("accessible_text"), TEXT("string"), TEXT("Re-walk the tree and click the first widget whose GetAccessibleText() matches exactly"), false },
			{ TEXT("window_pixel"), TEXT("object"), TEXT("Click at absolute window-relative pixel coords {x, y}"), false },
			{ TEXT("button"), TEXT("string"), TEXT("Mouse button: 'left' (default), 'right', 'middle'"), false, TEXT("left") },
			{ TEXT("modifiers"), TEXT("array"), TEXT("Modifier keys held during click: any of 'ctrl', 'shift', 'alt', 'cmd'"), false },
			{ TEXT("double_click"), TEXT("boolean"), TEXT("Send the down/up sequence twice (default false)"), false, TEXT("false") },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Synthesize keyboard text input (a string of characters). Routes individual
 * characters via FSlateApplication::ProcessKeyCharEvent; newlines (\n) are
 * sent as Enter via ProcessKeyDownEvent/UpEvent for reliability.
 */
class FECACommand_TypeSlateText : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("type_slate_text"); }
	virtual FString GetDescription() const override { return TEXT("Type a string of characters into the focused (or specified) Slate widget. Supports escapes: \\n for Enter, \\t for Tab."); }
	virtual FString GetCategory() const override { return TEXT("Slate Input"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("text"), TEXT("string"), TEXT("Text to type. Special escapes: \\n = Enter, \\t = Tab."), true },
			{ TEXT("into_focused"), TEXT("boolean"), TEXT("Type into the currently-focused widget (default true). If false and no target widget given, this is the only path."), false, TEXT("true") },
			{ TEXT("widget_id"), TEXT("number"), TEXT("Focus this widget (from find_slate_widgets) before typing"), false },
			{ TEXT("accessible_text"), TEXT("string"), TEXT("Re-walk tree to find a widget with matching accessible text, focus it, then type"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Press a key chord (e.g., Ctrl+S, F5, Cmd+Tab). Routes a single
 * key-down/key-up pair via FSlateApplication, with modifier state baked
 * into the FKeyEvent's FModifierKeysState.
 */
class FECACommand_SlateKeyChord : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("slate_key_chord"); }
	virtual FString GetDescription() const override { return TEXT("Press a key chord. 'key' is a UE canonical key name (S, F5, Tab, Enter, Escape, etc.); modifiers are 'ctrl', 'shift', 'alt', 'cmd'. Accepts common aliases (Return->Enter, Esc->Escape)."); }
	virtual FString GetCategory() const override { return TEXT("Slate Input"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("key"), TEXT("string"), TEXT("Main key: UE canonical name (S, F5, Tab, Enter, Escape, ...). Aliases accepted: Return/Esc."), true },
			{ TEXT("modifiers"), TEXT("array"), TEXT("Modifier keys: any of 'ctrl', 'shift', 'alt', 'cmd'"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Capture just the rectangle of a specific Slate widget, with optional
 * padding. Pairs naturally with find_slate_widgets for "show me this widget"
 * agent flows. Encodes PNG and returns inline (MakeImageContent) unless
 * file_path is given.
 */
class FECACommand_TakeSlateWidgetScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_slate_widget_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Screenshot just the rectangle of a specific Slate widget (by widget_id or accessible_text), with optional pixel padding. Encodes PNG; returns inline if file_path is omitted."); }
	virtual FString GetCategory() const override { return TEXT("Slate Input"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_id"), TEXT("number"), TEXT("Widget id from a recent find_slate_widgets call"), false },
			{ TEXT("accessible_text"), TEXT("string"), TEXT("Re-walk tree to find a widget with matching accessible text"), false },
			{ TEXT("file_path"), TEXT("string"), TEXT("If omitted, returns inline base64 PNG via MakeImageContent"), false },
			{ TEXT("padding"), TEXT("number"), TEXT("Pixels of padding around the widget bounds"), false, TEXT("8") },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
