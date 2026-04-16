// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Get all actors in the current level
 */
class FECACommand_GetActorsInLevel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actors_in_level"); }
	virtual FString GetDescription() const override { return TEXT("Get all actors in the current level with optional class filtering"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_filter"), TEXT("string"), TEXT("Filter actors by class name (partial match)"), false },
			{ TEXT("include_hidden"), TEXT("boolean"), TEXT("Include hidden actors"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new actor in the level
 */
class FECACommand_CreateActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_actor"); }
	virtual FString GetDescription() const override { return TEXT("Create a new actor in the current level"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_type"), TEXT("string"), TEXT("Type of actor (StaticMeshActor, PointLight, SpotLight, DirectionalLight, CameraActor, or class path)"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Display name for the actor"), false },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale {x, y, z}"), false },
			{ TEXT("mesh"), TEXT("string"), TEXT("Mesh path for StaticMeshActor"), false },
			{ TEXT("folder"), TEXT("string"), TEXT("Outliner folder path"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete an actor from the level
 */
class FECACommand_DeleteActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_actor"); }
	virtual FString GetDescription() const override { return TEXT("Delete an actor from the current level"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor to delete"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set actor transform (location, rotation, scale)
 */
class FECACommand_SetActorTransform : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_actor_transform"); }
	virtual FString GetDescription() const override { return TEXT("Set the transform of an actor"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor"), true },
			{ TEXT("location"), TEXT("object"), TEXT("New location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("New rotation {pitch, yaw, roll}"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("New scale {x, y, z}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get actor properties
 */
class FECACommand_GetActorProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actor_properties"); }
	virtual FString GetDescription() const override { return TEXT("Get properties of an actor"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor"), true },
			{ TEXT("properties"), TEXT("array"), TEXT("Specific property names to retrieve (optional, returns all if not specified)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set actor property
 */
class FECACommand_SetActorProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_actor_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on an actor. Supports StaticMesh, Material, and generic UE properties via reflection."); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_path"), TEXT("string"), TEXT("Path to the actor (from get_selected_actors). Preferred over actor_name."), false },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor (legacy, use actor_path instead)"), false },
			{ TEXT("property_name"), TEXT("string"), TEXT("Property: 'StaticMesh', 'Material', 'Material[0]', 'Mobility', 'Hidden', 'Folder', or any UProperty name"), true },
			{ TEXT("property_value"), TEXT("any"), TEXT("Value to set (asset path for objects, or primitive value)"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Optional: target a specific component instead of the actor"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Find actors by various criteria
 */
class FECACommand_FindActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_actors"); }
	virtual FString GetDescription() const override { return TEXT("Find actors by name pattern, class, tag, or location"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_pattern"), TEXT("string"), TEXT("Name pattern to match (supports * wildcard)"), false },
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name to filter by"), false },
			{ TEXT("tag"), TEXT("string"), TEXT("Actor tag to filter by"), false },
			{ TEXT("near_location"), TEXT("object"), TEXT("Find actors near this location {x, y, z}"), false },
			{ TEXT("radius"), TEXT("number"), TEXT("Radius for location search"), false, TEXT("1000") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Duplicate an actor
 */
class FECACommand_DuplicateActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("duplicate_actor"); }
	virtual FString GetDescription() const override { return TEXT("Duplicate an existing actor"); }
	virtual FString GetCategory() const override { return TEXT("Actor"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor to duplicate"), true },
			{ TEXT("new_name"), TEXT("string"), TEXT("Name for the duplicated actor"), false },
			{ TEXT("offset"), TEXT("object"), TEXT("Position offset from original {x, y, z}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};


