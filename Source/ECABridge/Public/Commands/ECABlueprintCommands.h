// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new Blueprint asset
 */
class FECACommand_CreateBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Blueprint asset"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_name"), TEXT("string"), TEXT("Name for the new Blueprint"), true },
			{ TEXT("parent_class"), TEXT("string"), TEXT("Parent class (Actor, Pawn, Character, or class path)"), false, TEXT("Actor") },
			{ TEXT("path"), TEXT("string"), TEXT("Content path for the Blueprint"), false, TEXT("/Game/Blueprints/") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a component to a Blueprint
 */
class FECACommand_AddBlueprintComponent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_component"); }
	virtual FString GetDescription() const override { return TEXT("Add a component to a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_type"), TEXT("string"), TEXT("Type of component (StaticMesh, PointLight, SpotLight, Camera, etc.)"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name for the component"), false, TEXT("NewComponent") },
			{ TEXT("attach_to"), TEXT("string"), TEXT("Name of component to attach to"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Compile a Blueprint
 */
class FECACommand_CompileBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("compile_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Compile a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a variable to a Blueprint
 */
class FECACommand_AddBlueprintVariable : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_blueprint_variable"); }
	virtual FString GetDescription() const override { return TEXT("Add a member variable to a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("variable_name"), TEXT("string"), TEXT("Name for the variable"), true },
			{ TEXT("variable_type"), TEXT("string"), TEXT("Type of variable (Boolean, Integer, Float, String, Vector, Rotator, Transform)"), true },
			{ TEXT("default_value"), TEXT("any"), TEXT("Default value for the variable"), false },
			{ TEXT("is_instance_editable"), TEXT("boolean"), TEXT("Allow editing per instance"), false, TEXT("false") },
			{ TEXT("is_blueprint_read_only"), TEXT("boolean"), TEXT("Make read-only in Blueprints"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Spawn a Blueprint actor in the level
 */
class FECACommand_SpawnBlueprintActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_blueprint_actor"); }
	virtual FString GetDescription() const override { return TEXT("Spawn an instance of a Blueprint in the level"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), false },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Display name for the spawned actor"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get Blueprint info
 */
class FECACommand_GetBlueprintInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_blueprint_info"); }
	virtual FString GetDescription() const override { return TEXT("Get information about a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List all Blueprints in a path
 */
class FECACommand_ListBlueprints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_blueprints"); }
	virtual FString GetDescription() const override { return TEXT("List all Blueprint assets in a path"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"), TEXT("string"), TEXT("Content path to search"), false, TEXT("/Game/") },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Search recursively"), false, TEXT("true") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Open Blueprint editor
 */
class FECACommand_OpenBlueprintEditor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("open_blueprint_editor"); }
	virtual FString GetDescription() const override { return TEXT("Open a Blueprint in the Blueprint editor"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete a function from a Blueprint
 */
class FECACommand_DeleteBlueprintFunction : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_blueprint_function"); }
	virtual FString GetDescription() const override { return TEXT("Delete a function graph from a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Blueprint"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("function_name"), TEXT("string"), TEXT("Name of the function to delete"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
