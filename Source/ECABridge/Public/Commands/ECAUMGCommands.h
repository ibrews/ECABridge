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
