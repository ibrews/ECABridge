// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Wrap a widget with a container widget (e.g., insert a SizeBox between a widget and its parent).
 * Follows the same algorithm as FWidgetBlueprintEditorUtils::WrapWidgets.
 */
class FECACommand_WrapWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("wrap_widget"); }
	virtual FString GetDescription() const override { return TEXT("Wrap a widget with a container widget (e.g., insert a SizeBox between a widget and its parent)"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to wrap"), true },
			{ TEXT("wrapper_class"), TEXT("string"), TEXT("Class name of the wrapper widget (e.g., SizeBox, Overlay, Border, ScaleBox)"), true },
			{ TEXT("wrapper_name"), TEXT("string"), TEXT("Name for the new wrapper widget (auto-generated if omitted)"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Unwrap a container widget by removing it and promoting its single child to take its place.
 * Follows the same algorithm as FWidgetBlueprintEditorUtils::ReplaceWidgetWithChildren.
 */
class FECACommand_UnwrapWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("unwrap_widget"); }
	virtual FString GetDescription() const override { return TEXT("Remove a wrapper widget and promote its single child to take its place in the hierarchy"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the wrapper widget to remove"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Move a widget from its current parent to a different parent widget.
 * Note: Slot properties (padding, alignment, size rule) are lost on reparent since a new slot is created.
 */
class FECACommand_ReparentWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("reparent_widget"); }
	virtual FString GetDescription() const override { return TEXT("Move a widget from its current parent to a different parent widget. Slot properties are lost on reparent."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to move"), true },
			{ TEXT("new_parent_name"), TEXT("string"), TEXT("Name of the target parent widget"), true },
			{ TEXT("insert_index"), TEXT("number"), TEXT("Position in new parent (-1 or omit to append at end)"), false, TEXT("-1") },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set layout properties on a widget's slot (padding, alignment, size rule).
 * Slot type is auto-detected and properties are applied accordingly.
 */
class FECACommand_SetSlotProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_slot_property"); }
	virtual FString GetDescription() const override { return TEXT("Set layout properties on a widget's slot (padding, alignment, size rule). Slot type is auto-detected."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the child widget whose slot to modify"), true },
			{ TEXT("padding"), TEXT("object"), TEXT("Padding as {left, top, right, bottom}"), false },
			{ TEXT("horizontal_alignment"), TEXT("string"), TEXT("Horizontal alignment: Fill, Left, Center, Right"), false },
			{ TEXT("vertical_alignment"), TEXT("string"), TEXT("Vertical alignment: Fill, Top, Center, Bottom"), false },
			{ TEXT("size_rule"), TEXT("string"), TEXT("Size rule: Auto or Fill (only for VerticalBox/HorizontalBox slots)"), false },
			{ TEXT("size_value"), TEXT("number"), TEXT("Fill weight when size_rule is Fill (default 1.0)"), false, TEXT("1.0") },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set properties directly on a widget (not its slot).
 * Supports common widget properties like Visibility, RenderOpacity,
 * and type-specific properties like SizeBox overrides.
 */
class FECACommand_SetWidgetProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_widget_property"); }
	virtual FString GetDescription() const override { return TEXT("Set properties on a widget (Visibility, SizeBox overrides, Text, etc.). Use set_slot_property for layout/padding."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to modify"), true },
			{ TEXT("properties"), TEXT("object"), TEXT("Key-value pairs of properties to set (e.g., {\"WidthOverride\": 400, \"Visibility\": \"Collapsed\"})"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a child widget of any class to a parent widget in a Widget Blueprint.
 * Works with any UWidget subclass — engine types (TextBlock, Image, SizeBox) and
 * project types (CommonTextBlock, WBP_MyWidget_C, etc.).
 */
class FECACommand_AddChildWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_child_widget"); }
	virtual FString GetDescription() const override { return TEXT("Add a child widget of any class to a parent widget. Works with engine and project widget types."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Name of the parent widget to add the child to"), true },
			{ TEXT("widget_class"), TEXT("string"), TEXT("Class name of the widget to create (e.g., TextBlock, CommonTextBlock, SizeBox, WBP_MyWidget_C)"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name for the new widget"), true },
			{ TEXT("insert_index"), TEXT("number"), TEXT("Position in parent (-1 or omit to append at end)"), false, TEXT("-1") },
			{ TEXT("skip_compile"), TEXT("boolean"), TEXT("Skip blueprint recompile (batch multiple adds, then call compile_widget_blueprint)"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Compile a Widget Blueprint. Use after batching operations with skip_compile=true.
 */
class FECACommand_CompileWidgetBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("compile_widget_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Compile a Widget Blueprint. Use after batching add_child_widget calls with skip_compile=true."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a widget and all its descendants from a Widget Blueprint.
 * The widget is detached from its parent and fully removed from the WidgetTree.
 */
class FECACommand_RemoveWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_widget"); }
	virtual FString GetDescription() const override { return TEXT("Remove a widget and all its descendants from a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to remove"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
