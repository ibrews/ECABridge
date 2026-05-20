// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Polymorphic widget instantiation. Replaces type-specific add_*_to_widget when
 * the widget class isn't known up front.
 */
class FECACommand_AddWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_widget"); }
	virtual FString GetDescription() const override { return TEXT("Construct a widget of any UWidget subclass and attach it to a Widget Blueprint. If parent_widget_name is omitted and the tree is empty the new widget becomes the root; if omitted and a root exists it is added under the root. child_index orders within the parent's child list (-1 = append)."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"),  TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("widget_class"),          TEXT("string"),  TEXT("Class path or unqualified UMG name (e.g. 'Button', '/Script/UMG.Button')"), true },
			{ TEXT("widget_name"),           TEXT("string"),  TEXT("Name for the new widget instance"), true },
			{ TEXT("parent_widget_name"),    TEXT("string"),  TEXT("Existing panel widget to attach under (defaults to root, or promotes to root if tree empty)"), false },
			{ TEXT("child_index"),           TEXT("integer"), TEXT("Position within parent's children (0 = first, -1 = append)"), false, TEXT("-1") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_name"),  TEXT("string"),  TEXT("Name of the new widget") },
			{ TEXT("widget_class"), TEXT("string"),  TEXT("Class of the new widget") },
			{ TEXT("parent_name"),  TEXT("string"),  TEXT("Name of the parent widget (empty when new widget is root)") },
			{ TEXT("slot_class"),   TEXT("string"),  TEXT("Class of the resulting panel slot (empty when new widget is root)") },
			{ TEXT("is_root"),      TEXT("boolean"), TEXT("True if the new widget became the tree root") },
			{ TEXT("is_variable"),  TEXT("boolean"), TEXT("Whether the new widget is exposed as a blueprint variable") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Place a child widget into a host widget's named slot. Supports widgets that
 * implement INamedSlotInterface (e.g. NamedSlot) and UContentWidget hosts
 * exposed via the synthetic "Content" slot (Button, Border, SizeBox, etc.).
 */
class FECACommand_SetNamedSlotContent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_named_slot_content"); }
	virtual FString GetDescription() const override { return TEXT("Set a child widget into a host widget's named slot. The host may be a widget implementing INamedSlotInterface or a UContentWidget (use slot_name='Content')."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("host_widget_name"),      TEXT("string"), TEXT("Name of the host widget exposing the slot"), true },
			{ TEXT("slot_name"),             TEXT("string"), TEXT("Named slot identifier (use 'Content' for UContentWidget hosts)"), true },
			{ TEXT("widget_class"),          TEXT("string"), TEXT("Class path of the new content widget"), true },
			{ TEXT("widget_name"),           TEXT("string"), TEXT("Name for the new content widget"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_name"),  TEXT("string"),  TEXT("Name of the new content widget") },
			{ TEXT("widget_class"), TEXT("string"),  TEXT("Class of the new content widget") },
			{ TEXT("parent_name"),  TEXT("string"),  TEXT("Host widget name") },
			{ TEXT("slot_class"),   TEXT("string"),  TEXT("Resulting slot class (empty for INamedSlotInterface hosts)") },
			{ TEXT("is_root"),      TEXT("boolean"), TEXT("Always false for named-slot content") },
			{ TEXT("is_variable"),  TEXT("boolean"), TEXT("Whether the new widget is a blueprint variable") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List named-slot bindings (NamedSlotInterface implementers + UContentWidget content slots).
 */
class FECACommand_GetNamedSlots : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_named_slots"); }
	virtual FString GetDescription() const override { return TEXT("List named-slot bindings on a Widget Blueprint. Includes widgets implementing INamedSlotInterface and UContentWidget hosts with their single 'Content' slot."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Path to a UWidgetBlueprint asset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_blueprint_path"), TEXT("string"),  TEXT("Echo of the requested path") },
			{ TEXT("slots"),                 TEXT("array"),   TEXT("Array of { slot_name, host_widget_name, content_widget_name } entries") },
			{ TEXT("count"),                 TEXT("integer"), TEXT("Number of slots returned") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
