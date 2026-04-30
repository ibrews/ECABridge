// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Set static mesh properties on a component
 */
class FECACommand_SetStaticMeshProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_static_mesh_properties"); }
	virtual FString GetDescription() const override { return TEXT("Set static mesh on a StaticMeshComponent in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Component"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the StaticMeshComponent"), true },
			{ TEXT("static_mesh"), TEXT("string"), TEXT("Path to the static mesh asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set physics properties on a component
 */
class FECACommand_SetPhysicsProperties : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_physics_properties"); }
	virtual FString GetDescription() const override { return TEXT("Set physics properties on a component in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Component"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component"), true },
			{ TEXT("simulate_physics"), TEXT("boolean"), TEXT("Enable physics simulation"), false, TEXT("true") },
			{ TEXT("gravity_enabled"), TEXT("boolean"), TEXT("Enable gravity"), false, TEXT("true") },
			{ TEXT("mass"), TEXT("number"), TEXT("Mass in kg"), false, TEXT("1.0") },
			{ TEXT("linear_damping"), TEXT("number"), TEXT("Linear damping"), false, TEXT("0.01") },
			{ TEXT("angular_damping"), TEXT("number"), TEXT("Angular damping"), false, TEXT("0.0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a generic property on a component
 */
class FECACommand_SetComponentProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_component_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a component in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Component"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("Name of the property to set"), true },
			{ TEXT("property_value"), TEXT("any"), TEXT("Value to set (type depends on property)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Read a property value from a Blueprint component template (read complement to set_component_property).
 */
class FECACommand_GetComponentProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_component_property"); }
	virtual FString GetDescription() const override { return TEXT("Read a property value from a Blueprint component template — the same default value that set_component_property writes. Returns the value as a string (Unreal property text format) plus a typed JSON field for primitives. Useful for verifying that a write took effect or inspecting an existing component."); }
	virtual FString GetCategory() const override { return TEXT("Component"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("Property name to read"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set component transform (relative)
 */
class FECACommand_SetComponentTransform : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_component_transform"); }
	virtual FString GetDescription() const override { return TEXT("Set the relative transform of a component in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Component"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name of the component"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Relative location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("Relative rotation {pitch, yaw, roll}"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale {x, y, z}"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get components in a Blueprint
 */
class FECACommand_GetBlueprintComponents : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_blueprint_components"); }
	virtual FString GetDescription() const override { return TEXT("Get all components in a Blueprint"); }
	virtual FString GetCategory() const override { return TEXT("Component"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Blueprint asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
