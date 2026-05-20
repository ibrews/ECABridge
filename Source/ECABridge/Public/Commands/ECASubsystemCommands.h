// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Enumerate every active UEditorSubsystem on GEditor.
 */
class FECACommand_ListEditorSubsystems : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_editor_subsystems"); }
	virtual FString GetDescription() const override { return TEXT("List every active UEditorSubsystem (class name, package, path). Use dump_subsystem to introspect a specific one."); }
	virtual FString GetCategory() const override { return TEXT("Subsystem"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_filter"), TEXT("string"), TEXT("Substring filter on class name (case-insensitive)"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),      TEXT("integer"), TEXT("Number of subsystems returned") },
			{ TEXT("subsystems"), TEXT("array"),   TEXT("[{class, path, package}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Enumerate every active UEngineSubsystem on GEngine.
 */
class FECACommand_ListEngineSubsystems : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_engine_subsystems"); }
	virtual FString GetDescription() const override { return TEXT("List every active UEngineSubsystem (class name, package, path). Use dump_subsystem to introspect a specific one."); }
	virtual FString GetCategory() const override { return TEXT("Subsystem"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_filter"), TEXT("string"), TEXT("Substring filter on class name (case-insensitive)"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),      TEXT("integer"), TEXT("Number of subsystems returned") },
			{ TEXT("subsystems"), TEXT("array"),   TEXT("[{class, path, package}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect a single subsystem instance: list its UFUNCTIONs (callable by name)
 * and a summary of its UPROPERTYs (name, type, value-if-simple).
 *
 * Resolution order:
 *   1. UEditorSubsystem subclass on GEditor
 *   2. UEngineSubsystem subclass on GEngine
 *   3. UWorldSubsystem subclass on the editor world
 *
 * No mutation; output is purely reflective.
 */
class FECACommand_DumpSubsystem : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_subsystem"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a subsystem (editor / engine / world) by class name: list its UFUNCTIONs and UPROPERTYs via reflection."); }
	virtual FString GetCategory() const override { return TEXT("Subsystem"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_name"),       TEXT("string"),  TEXT("Subsystem class name (e.g. 'UEditorActorSubsystem', 'EditorActorSubsystem')"), true },
			{ TEXT("include_inherited"),TEXT("boolean"), TEXT("Include UFUNCTIONs/UPROPERTYs inherited from parent classes (default false)"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("class"),      TEXT("string"),  TEXT("Resolved class name") },
			{ TEXT("kind"),       TEXT("string"),  TEXT("editor / engine / world / other") },
			{ TEXT("path"),       TEXT("string"),  TEXT("Subsystem instance path") },
			{ TEXT("functions"),  TEXT("array"),   TEXT("[{name, return_type, params:[{name, type}]}]"), TEXT("object") },
			{ TEXT("properties"), TEXT("array"),   TEXT("[{name, type, value_summary}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
