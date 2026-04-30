// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new UMG Widget Blueprint
 */
class FECACommand_CreateUMGWidgetBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_umg_widget_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UMG Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name for the new Widget Blueprint"), true },
			{ TEXT("parent_class"), TEXT("string"), TEXT("Parent class (UserWidget, etc.)"), false, TEXT("UserWidget") },
			{ TEXT("path"), TEXT("string"), TEXT("Content path for the Widget Blueprint"), false, TEXT("/Game/UI/") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a Text Block widget to a Widget Blueprint
 */
class FECACommand_AddTextBlockToWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_text_block_to_widget"); }
	virtual FString GetDescription() const override { return TEXT("Add a Text Block widget to a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("text_block_name"), TEXT("string"), TEXT("Name for the Text Block"), true },
			{ TEXT("text"), TEXT("string"), TEXT("Initial text content"), false, TEXT("Text") },
			{ TEXT("position"), TEXT("object"), TEXT("Position in canvas panel {x, y}"), false },
			{ TEXT("size"), TEXT("object"), TEXT("Size of the text block {width, height}"), false },
			{ TEXT("font_size"), TEXT("number"), TEXT("Font size in points"), false, TEXT("12") },
			{ TEXT("color"), TEXT("object"), TEXT("Text color {r, g, b, a} (0.0-1.0)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a Button widget to a Widget Blueprint
 */
class FECACommand_AddButtonToWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_button_to_widget"); }
	virtual FString GetDescription() const override { return TEXT("Add a Button widget to a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("button_name"), TEXT("string"), TEXT("Name for the Button"), true },
			{ TEXT("text"), TEXT("string"), TEXT("Button text"), false, TEXT("Button") },
			{ TEXT("position"), TEXT("object"), TEXT("Position in canvas panel {x, y}"), false },
			{ TEXT("size"), TEXT("object"), TEXT("Size of the button {width, height}"), false },
			{ TEXT("font_size"), TEXT("number"), TEXT("Font size for button text"), false, TEXT("12") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a widget instance to the game viewport
 */
class FECACommand_AddWidgetToViewport : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_widget_to_viewport"); }
	virtual FString GetDescription() const override { return TEXT("Add an instance of a Widget Blueprint to the game viewport"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("z_order"), TEXT("number"), TEXT("Z-order for widget display"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Bind an event to a widget (e.g., button click)
 */
class FECACommand_BindWidgetEvent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("bind_widget_event"); }
	virtual FString GetDescription() const override { return TEXT("Bind an event to a widget (e.g., OnClicked for buttons)"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to bind"), true },
			{ TEXT("event_name"), TEXT("string"), TEXT("Name of the event (OnClicked, OnHovered, etc.)"), true },
			{ TEXT("function_name"), TEXT("string"), TEXT("Name of the function to call"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set up text block binding for dynamic updates
 */
class FECACommand_SetTextBlockBinding : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_text_block_binding"); }
	virtual FString GetDescription() const override { return TEXT("Set up a text binding for dynamic text updates"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("text_block_name"), TEXT("string"), TEXT("Name of the Text Block widget"), true },
			{ TEXT("binding_function"), TEXT("string"), TEXT("Name of the function that returns the text"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a generic UMG widget element of any UMG class.
 * Use this for ProgressBar, VerticalBox, HorizontalBox, CanvasPanel, Border, Spacer, Overlay,
 * ScrollBox, SizeBox, ScaleBox, Slider, CheckBox, GridPanel, UniformGridPanel, RichTextBlock,
 * Throbber, CircularThrobber, NamedSlot, BackgroundBlur, RetainerBox, and any plugin widget.
 * The dedicated commands (add_text_block_to_widget, add_button_to_widget, add_image_to_widget)
 * remain for those specific types because they pre-fill richer initial settings.
 */
class FECACommand_AddWidgetElement : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_widget_element"); }
	virtual FString GetDescription() const override { return TEXT("Add a UMG widget element of any class to a Widget Blueprint. Resolves widget_type as either an unqualified UMG class name (e.g., 'ProgressBar', 'VerticalBox', 'Border') or a fully qualified path (e.g., '/Script/UMG.ProgressBar'). If parent_name is omitted, attaches to the root canvas. Optional 'properties' is a JSON object of property names to values, applied via reflection (e.g., {Percent: 0.75} for ProgressBar)."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_type"), TEXT("string"), TEXT("UMG class name (ProgressBar, VerticalBox, Border, etc.) or full /Script/UMG.X path"), true },
			{ TEXT("element_name"), TEXT("string"), TEXT("Name for the new element"), true },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Optional name of an existing panel to attach to (defaults to the root)"), false },
			{ TEXT("position"), TEXT("object"), TEXT("Position {x, y} when attached to a CanvasPanel"), false },
			{ TEXT("size"), TEXT("object"), TEXT("Size {width, height} when attached to a CanvasPanel"), false },
			{ TEXT("properties"), TEXT("object"), TEXT("Optional JSON properties to set on the new widget (e.g., {Percent: 0.5} for ProgressBar)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add an Image widget to a Widget Blueprint
 */
class FECACommand_AddImageToWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_image_to_widget"); }
	virtual FString GetDescription() const override { return TEXT("Add an Image widget to a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("image_name"), TEXT("string"), TEXT("Name for the Image widget"), true },
			{ TEXT("texture_path"), TEXT("string"), TEXT("Path to the texture asset"), false },
			{ TEXT("position"), TEXT("object"), TEXT("Position in canvas panel {x, y}"), false },
			{ TEXT("size"), TEXT("object"), TEXT("Size of the image {width, height}"), false },
			{ TEXT("color_and_opacity"), TEXT("object"), TEXT("Color tint {r, g, b, a}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get information about a Widget Blueprint
 */
/**
 * Dump full widget tree hierarchy with properties, slot settings, and bindings.
 */
class FECACommand_DumpWidgetTree : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_widget_tree"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a Widget Blueprint's full widget tree to JSON: hierarchical parent-child structure, widget classes, slot properties (padding, alignment, anchors), visibility, and is_variable status. Makes any UMG widget fully legible."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("include_slot_properties"), TEXT("boolean"), TEXT("Include slot properties like padding, alignment, size rules (default true)"), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetWidgetInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_widget_info"); }
	virtual FString GetDescription() const override { return TEXT("Get information about a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
