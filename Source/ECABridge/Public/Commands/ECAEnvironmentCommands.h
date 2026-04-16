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

// ─── enable_physics_simulation ─────────────────────────────
// Enable/disable physics simulation on an actor's components.
class FECACommand_EnablePhysicsSimulation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("enable_physics_simulation"); }
	virtual FString GetDescription() const override { return TEXT("Enable or disable physics simulation on an actor's primitive components"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name or label of the target actor"), true },
			{ TEXT("enable"), TEXT("boolean"), TEXT("Whether to enable physics simulation (default: true)"), false, TEXT("true") },
			{ TEXT("component_name"), TEXT("string"), TEXT("Specific primitive component name — if omitted, affects all primitive components"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── apply_impulse ─────────────────────────────────────────
// Apply a physics impulse to an actor (push/launch/hit effect).
class FECACommand_ApplyImpulse : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("apply_impulse"); }
	virtual FString GetDescription() const override { return TEXT("Apply a physics impulse to an actor for push, launch, or hit effects"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name or label of the target actor"), true },
			{ TEXT("impulse"), TEXT("object"), TEXT("Impulse vector as {x, y, z}"), true },
			{ TEXT("location"), TEXT("object"), TEXT("World position for impulse application as {x, y, z} — defaults to actor center"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_actor_visibility ──────────────────────────────────
// Show or hide actors.
class FECACommand_SetActorVisibility : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_actor_visibility"); }
	virtual FString GetDescription() const override { return TEXT("Show or hide an actor in the game"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name or label of the target actor"), true },
			{ TEXT("visible"), TEXT("boolean"), TEXT("Whether the actor should be visible"), true },
			{ TEXT("affect_children"), TEXT("boolean"), TEXT("Whether to propagate visibility to child components (default: true)"), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── batch_spawn_actors ────────────────────────────────────
// Spawn multiple actors in one call — useful for grids, arrays, crowds.
class FECACommand_BatchSpawnActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("batch_spawn_actors"); }
	virtual FString GetDescription() const override { return TEXT("Spawn multiple actors in one call — useful for building grids, arrays, or crowds"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_type"), TEXT("string"), TEXT("Type of actor to spawn (e.g. StaticMeshActor)"), true },
			{ TEXT("count"), TEXT("number"), TEXT("Number of actors to spawn"), true },
			{ TEXT("base_location"), TEXT("object"), TEXT("Starting location as {x, y, z}"), true },
			{ TEXT("spacing"), TEXT("object"), TEXT("Offset between each spawned actor as {x, y, z} (default: {x:200, y:0, z:0})"), false, TEXT("{x:200, y:0, z:0}") },
			{ TEXT("base_name"), TEXT("string"), TEXT("Base label for spawned actors — each gets an incrementing suffix (default: SpawnedActor)"), false, TEXT("SpawnedActor") },
			{ TEXT("mesh"), TEXT("string"), TEXT("Asset path to a static mesh to assign (optional)"), false },
			{ TEXT("material"), TEXT("string"), TEXT("Asset path to a material to apply (optional)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── teleport_actor ────────────────────────────────────────
// Instantly move an actor to a new location without physics interpolation.
class FECACommand_TeleportActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("teleport_actor"); }
	virtual FString GetDescription() const override { return TEXT("Instantly move an actor to a new location without physics interpolation"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name or label of the target actor"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Target location as {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Target rotation as {pitch, yaw, roll} (optional — keeps current rotation if omitted)"), false },
			{ TEXT("sweep"), TEXT("boolean"), TEXT("Whether to sweep to the target location, stopping at collision (default: false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── generate_grid ─────────────────────────────────────────
// Procedurally generate a grid of static mesh actors.
class FECACommand_GenerateGrid : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("generate_grid"); }
	virtual FString GetDescription() const override { return TEXT("Procedurally generate a grid of static mesh actors (for floors, walls, mazes)"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to the static mesh (default: /Engine/BasicShapes/Cube)"), false, TEXT("/Engine/BasicShapes/Cube") },
			{ TEXT("rows"), TEXT("number"), TEXT("Number of rows in the grid"), true },
			{ TEXT("columns"), TEXT("number"), TEXT("Number of columns in the grid"), true },
			{ TEXT("spacing"), TEXT("number"), TEXT("Distance between each actor center in world units (default: 200)"), false, TEXT("200") },
			{ TEXT("origin"), TEXT("object"), TEXT("Grid origin as {x, y, z} (default: 0,0,0)"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale for each actor as {x, y, z} (default: 1,1,1)"), false, TEXT("{x:1, y:1, z:1}") },
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path to a material to apply to each mesh"), false },
			{ TEXT("name_prefix"), TEXT("string"), TEXT("Prefix for actor labels (default: Grid)"), false, TEXT("Grid") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── generate_circle ───────────────────────────────────────
// Spawn actors arranged in a circle.
class FECACommand_GenerateCircle : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("generate_circle"); }
	virtual FString GetDescription() const override { return TEXT("Spawn actors arranged in a circle (useful for arenas, stages, turrets)"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to the static mesh (default: /Engine/BasicShapes/Cube)"), false, TEXT("/Engine/BasicShapes/Cube") },
			{ TEXT("count"), TEXT("number"), TEXT("Number of actors to place around the circle"), true },
			{ TEXT("radius"), TEXT("number"), TEXT("Circle radius in world units (default: 500)"), false, TEXT("500") },
			{ TEXT("center"), TEXT("object"), TEXT("Circle center as {x, y, z} (default: 0,0,0)"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale for each actor as {x, y, z} (default: 1,1,1)"), false, TEXT("{x:1, y:1, z:1}") },
			{ TEXT("face_center"), TEXT("boolean"), TEXT("Rotate each actor to face the circle center (default: true)"), false, TEXT("true") },
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path to a material to apply to each mesh"), false },
			{ TEXT("name_prefix"), TEXT("string"), TEXT("Prefix for actor labels (default: Circle)"), false, TEXT("Circle") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── destroy_actors_by_pattern ─────────────────────────────
// Delete multiple actors matching a name pattern.
class FECACommand_DestroyActorsByPattern : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("destroy_actors_by_pattern"); }
	virtual FString GetDescription() const override { return TEXT("Delete multiple actors matching a name pattern — useful for cleaning up procedurally generated content"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_pattern"), TEXT("string"), TEXT("Wildcard pattern to match actor labels (supports * wildcard)"), true },
			{ TEXT("dry_run"), TEXT("boolean"), TEXT("If true, just report what would be deleted without actually deleting (default: false)"), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── take_camera_screenshot ────────────────────────────────
// Capture a screenshot from a specific camera actor's perspective.
class FECACommand_TakeCameraScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("take_camera_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Capture a screenshot from a specific camera actor's perspective and save as PNG"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("camera_name"), TEXT("string"), TEXT("Name or label of the camera actor to capture from"), true },
			{ TEXT("filename"), TEXT("string"), TEXT("Output filename (saved to Saved/Screenshots/)"), true },
			{ TEXT("resolution_x"), TEXT("number"), TEXT("Horizontal resolution in pixels (default: 1920)"), false, TEXT("1920") },
			{ TEXT("resolution_y"), TEXT("number"), TEXT("Vertical resolution in pixels (default: 1080)"), false, TEXT("1080") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_decal ──────────────────────────────────────────
// Spawn a decal actor that projects a material onto nearby surfaces.
class FECACommand_SpawnDecal : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_decal"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a decal actor that projects a material onto nearby surfaces"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path to the decal material"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation as {pitch, yaw, roll} (default: points down, i.e. {-90,0,0})"), false },
			{ TEXT("size"), TEXT("object"), TEXT("Decal extent as {x, y, z} (default: {128, 256, 256})"), false, TEXT("{x:128, y:256, z:256}") },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_text_render ────────────────────────────────────
// Spawn 3D text in the scene.
class FECACommand_SpawnTextRender : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_text_render"); }
	virtual FString GetDescription() const override { return TEXT("Spawn 3D text in the scene using a TextRenderActor"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("text"), TEXT("string"), TEXT("The text string to render in 3D"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation as {pitch, yaw, roll}"), false },
			{ TEXT("color"), TEXT("object"), TEXT("Text color as {r, g, b} with values 0-255 (default: white)"), false },
			{ TEXT("size"), TEXT("number"), TEXT("World size of the text (default: 100)"), false, TEXT("100") },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── describe_actor ───────────────────────────────────────
// Give a comprehensive description of any actor: class, components, properties.
class FECACommand_DescribeActor : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_actor"); }
	virtual FString GetDescription() const override { return TEXT("Give a comprehensive description of any actor: class, all components with their types and key properties, materials, meshes, lights, etc."); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name or label of the target actor"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── clone_actor_array ────────────────────────────────────
// Clone an actor N times in a line/grid with optional transforms.
class FECACommand_CloneActorArray : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("clone_actor_array"); }
	virtual FString GetDescription() const override { return TEXT("Clone an actor N times in a line or grid with configurable spacing and rotation increment"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("source_actor"), TEXT("string"), TEXT("Name or label of the actor to clone"), true },
			{ TEXT("count"), TEXT("number"), TEXT("Number of clones to create"), true },
			{ TEXT("spacing"), TEXT("object"), TEXT("Offset between each clone as {x, y, z} (default: {x:200, y:0, z:0})"), false, TEXT("{x:200, y:0, z:0}") },
			{ TEXT("rotation_increment"), TEXT("object"), TEXT("Rotation added to each successive clone as {pitch, yaw, roll}"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_audio_source ──────────────────────────────────
// Spawn an ambient sound actor at a location.
class FECACommand_SpawnAudioSource : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_audio_source"); }
	virtual FString GetDescription() const override { return TEXT("Spawn an ambient sound actor at a location with configurable volume and attenuation"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("sound_path"), TEXT("string"), TEXT("Asset path to a USoundBase/USoundWave/USoundCue asset"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false },
			{ TEXT("auto_play"), TEXT("boolean"), TEXT("Whether the sound should auto-play (default: true)"), false, TEXT("true") },
			{ TEXT("volume"), TEXT("number"), TEXT("Volume multiplier (default: 1.0)"), false, TEXT("1.0") },
			{ TEXT("attenuation_radius"), TEXT("number"), TEXT("Inner/outer attenuation radius in cm (optional)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── spawn_trigger_box ───────────────────────────────────
// Spawn a trigger box volume.
class FECACommand_SpawnTriggerBox : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_trigger_box"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a trigger box volume at a location with configurable extent"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location as {x, y, z}"), true },
			{ TEXT("extent"), TEXT("object"), TEXT("Half-size of the box as {x, y, z} (default: {x:100, y:100, z:100})"), false, TEXT("{x:100, y:100, z:100}") },
			{ TEXT("name"), TEXT("string"), TEXT("Actor label in the editor"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── align_actors ────────────────────────────────────────
// Align multiple actors to a common position/axis.
class FECACommand_AlignActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("align_actors"); }
	virtual FString GetDescription() const override { return TEXT("Align multiple actors to a common position along an axis, or snap them to the ground"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_names"), TEXT("array"), TEXT("Array of actor name/label strings to align"), true },
			{ TEXT("align_axis"), TEXT("string"), TEXT("Axis to align on: x, y, z, or ground"), true },
			{ TEXT("target_value"), TEXT("number"), TEXT("Specific coordinate value to align to (defaults to average of all actors)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── distribute_actors ───────────────────────────────────
// Distribute actors evenly between two points or along an axis.
class FECACommand_DistributeActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("distribute_actors"); }
	virtual FString GetDescription() const override { return TEXT("Distribute actors evenly between two points or along a specified axis"); }
	virtual FString GetCategory() const override { return TEXT("Environment"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_names"), TEXT("array"), TEXT("Array of actor name/label strings to distribute"), true },
			{ TEXT("start_location"), TEXT("object"), TEXT("Start point as {x, y, z} — used with end_location for full repositioning"), false },
			{ TEXT("end_location"), TEXT("object"), TEXT("End point as {x, y, z} — used with start_location for full repositioning"), false },
			{ TEXT("axis"), TEXT("string"), TEXT("Distribute along this axis (x, y, or z) using existing positions — alternative to start/end"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
