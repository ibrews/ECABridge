// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Assign a ViewModel class to a Widget Blueprint.
 */
class FECACommand_SetWidgetViewmodel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_widget_viewmodel"); }
	virtual FString GetDescription() const override { return TEXT("Assign a ViewModel class to a Widget Blueprint for MVVM binding"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("viewmodel_class"), TEXT("string"), TEXT("Name of the ViewModel class to assign"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Bind a TextBlock or RichTextBlock's Text property to a ViewModel property.
 */
class FECACommand_BindTextToViewmodel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("bind_text_to_viewmodel"); }
	virtual FString GetDescription() const override { return TEXT("Bind a text widget's Text property to a ViewModel property"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the TextBlock widget to bind"), true },
			{ TEXT("viewmodel_property"), TEXT("string"), TEXT("Name of the ViewModel property to bind from"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Bind a widget's Visibility to a ViewModel bool using Conv_BoolToSlateVisibility.
 * Supports all 20 true/false ESlateVisibility combinations.
 */
class FECACommand_BindVisibilityToViewmodel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("bind_visibility_to_viewmodel"); }
	virtual FString GetDescription() const override { return TEXT("Bind a widget's Visibility to a ViewModel bool via Conv_BoolToSlateVisibility with configurable true/false visibility values"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to bind"), true },
			{ TEXT("viewmodel_property"), TEXT("string"), TEXT("Name of the ViewModel bool property"), true },
			{ TEXT("true_visibility"), TEXT("string"), TEXT("Visibility when bool is true (Visible, Collapsed, Hidden, HitTestInvisible, SelfHitTestInvisible)"), false, TEXT("Visible") },
			{ TEXT("false_visibility"), TEXT("string"), TEXT("Visibility when bool is false"), false, TEXT("Collapsed") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Bind an Image widget's Brush property to a ViewModel FSlateBrush property.
 */
class FECACommand_BindImageToViewmodel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("bind_image_to_viewmodel"); }
	virtual FString GetDescription() const override { return TEXT("Bind an Image widget's Brush to a ViewModel FSlateBrush property"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the Image widget to bind"), true },
			{ TEXT("viewmodel_property"), TEXT("string"), TEXT("Name of the ViewModel FSlateBrush property"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Query all ViewModels and MVVM bindings on a Widget Blueprint.
 */
class FECACommand_GetViewmodelBindings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_viewmodel_bindings"); }
	virtual FString GetDescription() const override { return TEXT("Get all ViewModel assignments and MVVM bindings on a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove an MVVM binding by index or by source/destination property match.
 */
class FECACommand_RemoveViewmodelBinding : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_viewmodel_binding"); }
	virtual FString GetDescription() const override { return TEXT("Remove an MVVM binding by index or by matching source/destination property names"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("binding_index"), TEXT("number"), TEXT("Index of the binding to remove (use get_viewmodel_bindings to find indices)"), false },
			{ TEXT("source_property"), TEXT("string"), TEXT("Source ViewModel property name to match for removal"), false },
			{ TEXT("destination_property"), TEXT("string"), TEXT("Destination widget property name to match for removal"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a generic MVVM binding between a ViewModel property and any widget property.
 * Supports self-bindings, multi-ViewModel lookup by name, and optional conversion functions.
 */
class FECACommand_AddViewmodelBinding : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_viewmodel_binding"); }
	virtual FString GetDescription() const override { return TEXT("Add a generic MVVM binding from a ViewModel property to any widget property, with optional conversion function"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("source_property"), TEXT("string"), TEXT("ViewModel property name to bind from"), true },
			{ TEXT("destination_property"), TEXT("string"), TEXT("Destination property or setter name on the target widget"), true },
			{ TEXT("destination_widget"), TEXT("string"), TEXT("Child widget name, or 'self' to bind to the widget itself (default: self)"), false, TEXT("self") },
			{ TEXT("viewmodel_name"), TEXT("string"), TEXT("ViewModel name to bind from (default: first ViewModel)"), false },
			{ TEXT("binding_mode"), TEXT("string"), TEXT("Binding mode: OneWayToDestination, TwoWay, OneWayToSource, OneTimeToDestination (default: OneWayToDestination)"), false, TEXT("OneWayToDestination") },
			{ TEXT("conversion_function"), TEXT("string"), TEXT("Optional conversion function (e.g. 'ClassName::FuncName' or 'FuncName' for BP-local)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List all bindable properties on a specific widget within a Widget Blueprint.
 */
class FECACommand_GetWidgetMVVMProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_widget_mvvm_properties"); }
	virtual FString GetDescription() const override { return TEXT("Get all bindable properties on a widget within a Widget Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("MVVM"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint asset"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the widget to inspect"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
