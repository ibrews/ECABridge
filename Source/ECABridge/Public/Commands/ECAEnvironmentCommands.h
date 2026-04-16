// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── spawn_static_mesh_wall ─────────────────────────────────
// Quickly spawn a wall/floor from a static mesh with optional material.
class FECACommand_SpawnStaticMeshWall : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_static_mesh_wall"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a wall or floor from a static mesh with configurable scale, location, rotation, and material"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to the static mesh (default: /Engine/BasicShapes/Cube)"), false, TEXT("/Engine/BasicShapes/Cube") },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation as {pitch, yaw, roll}"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale as {x, y, z} (default: {x:5, y:0.1, z:3} for wall proportions)"), false, TEXT("{x:5, y:0.1, z:3}") },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false },
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path to a material to apply to the mesh"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_fog ─────────────────────────────────────────────
// Add or configure exponential height fog in the scene.
class FECACommand_CreateFog : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_fog"); }
	virtual FString GetDescription() const override { return TEXT("Add or configure exponential height fog in the scene"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("fog_density"), TEXT("number"), TEXT("Fog density (default: 0.02)"), false, TEXT("0.02") },
			{ TEXT("fog_height"), TEXT("number"), TEXT("Fog base height in world units (default: 0)"), false, TEXT("0") },
			{ TEXT("fog_color"), TEXT("object"), TEXT("Fog inscattering color as {r, g, b} with values 0-255"), false },
			{ TEXT("start_distance"), TEXT("number"), TEXT("Distance from the camera at which fog starts"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_sky_settings ───────────────────────────────────────
// Configure sky atmosphere and directional light for time-of-day.
class FECACommand_SetSkySettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sky_settings"); }
	virtual FString GetDescription() const override { return TEXT("Configure sky atmosphere and directional light for time of day by rotating the sun"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sun_pitch"), TEXT("number"), TEXT("Sun pitch angle in degrees (-90 to 90; noon is roughly -30)"), true },
			{ TEXT("sun_yaw"), TEXT("number"), TEXT("Sun yaw angle in degrees (default: 0)"), false, TEXT("0") },
			{ TEXT("intensity_multiplier"), TEXT("number"), TEXT("Multiplier for the directional light intensity (default: 1)"), false, TEXT("1") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_particle_effect ──────────────────────────────────
// Spawn a Niagara particle system at a location.
class FECACommand_SpawnParticleEffect : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_particle_effect"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a Niagara particle system at a location"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Asset path to the Niagara system"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false },
			{ TEXT("auto_activate"), TEXT("boolean"), TEXT("Whether the system should auto-activate (default: true)"), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_world_gravity ──────────────────────────────────────
// Change world gravity settings.
class FECACommand_SetWorldGravity : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_world_gravity"); }
	virtual FString GetDescription() const override { return TEXT("Change the world gravity Z value"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("gravity_z"), TEXT("number"), TEXT("Gravity Z-axis value in cm/s^2 (default: -980)"), false, TEXT("-980") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_scene_stats ────────────────────────────────────────
// Return scene statistics: actor counts by type, light count, etc.
class FECACommand_GetSceneStats : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_scene_stats"); }
	virtual FString GetDescription() const override { return TEXT("Return scene statistics: actor count by type, light count, and general scene info"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── batch_set_actor_property ───────────────────────────────
// Set the same property on multiple actors at once.
class FECACommand_BatchSetActorProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("batch_set_actor_property"); }
	virtual FString GetDescription() const override { return TEXT("Set the same property on multiple actors at once (e.g. visibility, hidden-in-game)"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_names"), TEXT("array"), TEXT("Array of actor name/label strings"), true },
			{ TEXT("property_name"), TEXT("string"), TEXT("Name of the property to set (e.g. bHidden)"), true },
			{ TEXT("property_value"), TEXT("any"), TEXT("Value to assign to the property"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_spline_path ─────────────────────────────────────
// Create a spline actor from a set of points.
class FECACommand_CreateSplinePath : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_spline_path"); }
	virtual FString GetDescription() const override { return TEXT("Create a spline actor from a set of points, useful for camera paths, AI navigation, or procedural placement"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("points"), TEXT("array"), TEXT("Array of {x, y, z} objects defining the spline path"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false },
			{ TEXT("closed"), TEXT("boolean"), TEXT("Whether the spline forms a closed loop (default: false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
