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
 * Reparent a widget under a new panel parent. Optionally orders it at child_index.
 */
class FECACommand_MoveWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("move_widget"); }
	virtual FString GetDescription() const override { return TEXT("Move a widget under a new panel parent. Detaches from current parent (panel or content widget) and re-adds under new_parent_name; child_index orders in new parent (-1 = append)."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"),  TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("widget_name"),           TEXT("string"),  TEXT("Name of the widget to move"), true },
			{ TEXT("new_parent_name"),       TEXT("string"),  TEXT("Name of the new parent panel widget"), true },
			{ TEXT("child_index"),           TEXT("integer"), TEXT("Position within new parent's children (-1 = append)"), false, TEXT("-1") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of the moved widget") },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Name of the new parent") },
			{ TEXT("slot_class"),  TEXT("string"), TEXT("Class of the new panel slot") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a widget and its descendants from the tree.
 */
class FECACommand_RemoveWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_widget"); }
	virtual FString GetDescription() const override { return TEXT("Remove a widget (and its descendants) from a Widget Blueprint's tree. If the target is the root, the tree is left empty."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("widget_name"),           TEXT("string"), TEXT("Name of the widget to remove"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_name"), TEXT("string"),  TEXT("Name of the removed widget") },
			{ TEXT("removed"),     TEXT("boolean"), TEXT("True if the widget was removed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Rename a widget. Errors on collision with an existing widget name.
 */
class FECACommand_RenameWidget : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("rename_widget"); }
	virtual FString GetDescription() const override { return TEXT("Rename a widget within a Widget Blueprint. Errors if the new name is already used by another widget in the tree."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("widget_name"),           TEXT("string"), TEXT("Current widget name"), true },
			{ TEXT("new_name"),              TEXT("string"), TEXT("Desired new widget name"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("old_name"), TEXT("string"), TEXT("Previous widget name") },
			{ TEXT("new_name"), TEXT("string"), TEXT("New widget name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Toggle a widget's bIsVariable flag — exposes (or hides) it as a blueprint
 * member variable on the next compile.
 */
class FECACommand_SetWidgetAsVariable : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_widget_as_variable"); }
	virtual FString GetDescription() const override { return TEXT("Set a widget's bIsVariable flag. When true, the next compile generates a blueprint member variable for the widget."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"),  TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("widget_name"),           TEXT("string"),  TEXT("Widget name"), true },
			{ TEXT("is_variable"),           TEXT("boolean"), TEXT("Target value for bIsVariable"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_name"), TEXT("string"),  TEXT("Widget name") },
			{ TEXT("is_variable"), TEXT("boolean"), TEXT("Resulting flag state") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Reparent a Widget Blueprint to a different UUserWidget subclass and recompile.
 */
class FECACommand_ReparentWidgetBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("reparent_widget_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Reparent a Widget Blueprint to a different UUserWidget subclass and recompile."); }
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Path to a UWidgetBlueprint asset"), true },
			{ TEXT("new_parent_class"),      TEXT("string"), TEXT("New parent class (must be a UUserWidget subclass)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("widget_blueprint_path"), TEXT("string"), TEXT("Echo of the asset path") },
			{ TEXT("new_parent_class"),      TEXT("string"), TEXT("New parent class") }
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
