// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Inspect a UPhysicsAsset: bodies (per bone) + constraints (parent/child bones).
 */
class FECACommand_DumpPhysicsAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_physics_asset"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a UPhysicsAsset: list bodies (bone name + collision shapes summary) and constraints (joint name, parent/child bone)."); }
	virtual FString GetCategory() const override { return TEXT("Physics"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Asset path to a UPhysicsAsset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),             TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("name"),             TEXT("string"),  TEXT("Asset name") },
			{ TEXT("body_count"),       TEXT("integer"), TEXT("Number of USkeletalBodySetup entries") },
			{ TEXT("constraint_count"), TEXT("integer"), TEXT("Number of UPhysicsConstraintTemplate entries") },
			{ TEXT("bodies"),           TEXT("array"),   TEXT("Per-body {bone_name, shape_summary:{sphere, box, sphyl, convex, total}}"), TEXT("object") },
			{ TEXT("constraints"),      TEXT("array"),   TEXT("Per-constraint {joint_name, parent_bone, child_bone, linear_limit, swing1, swing2, twist, motions:{linear_xyz, angular_swing1, angular_swing2, angular_twist}}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect a UPhysicalMaterial: friction, restitution, density, surface type, etc.
 */
class FECACommand_DumpPhysicalMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_physical_material"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a UPhysicalMaterial: friction (static/dynamic), restitution, density, surface type, combine modes."); }
	virtual FString GetCategory() const override { return TEXT("Physics"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physical_material_path"), TEXT("string"), TEXT("Asset path to a UPhysicalMaterial"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),                  TEXT("string"), TEXT("Resolved asset path") },
			{ TEXT("name"),                  TEXT("string"), TEXT("Asset name") },
			{ TEXT("friction"),              TEXT("number"), TEXT("Friction (sliding/static fallback)") },
			{ TEXT("static_friction"),       TEXT("number"), TEXT("StaticFriction") },
			{ TEXT("restitution"),           TEXT("number"), TEXT("Bounciness") },
			{ TEXT("density"),               TEXT("number"), TEXT("Density g/cm³") },
			{ TEXT("raise_mass_to_power"),   TEXT("number"), TEXT("RaiseMassToPower exponent") },
			{ TEXT("friction_combine_mode"), TEXT("string"), TEXT("Average/Min/Multiply/Max (when override set)") },
			{ TEXT("restitution_combine_mode"), TEXT("string"), TEXT("Average/Min/Multiply/Max (when override set)") },
			{ TEXT("surface_type"),          TEXT("string"), TEXT("EPhysicalSurface enum name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new UPhysicalMaterial asset with optional friction/restitution/density.
 * Idempotent: re-creating at the same path updates the existing asset if
 * overwrite=true; otherwise errors.
 */
class FECACommand_CreatePhysicalMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_physical_material"); }
	virtual FString GetDescription() const override { return TEXT("Create a UPhysicalMaterial asset. Sets friction, restitution, density. Set overwrite=true to update an existing asset at the same path."); }
	virtual FString GetCategory() const override { return TEXT("Physics"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path"),         TEXT("string"),  TEXT("Folder path (e.g. '/Game/Physics/')"), true },
			{ TEXT("name"),         TEXT("string"),  TEXT("Asset name (no extension)"), true },
			{ TEXT("friction"),     TEXT("number"),  TEXT("Friction (default 0.7)"), false, TEXT("0.7") },
			{ TEXT("restitution"),  TEXT("number"),  TEXT("Restitution (default 0.3)"), false, TEXT("0.3") },
			{ TEXT("density"),      TEXT("number"),  TEXT("Density g/cm³ (default 1.0)"), false, TEXT("1.0") },
			{ TEXT("overwrite"),    TEXT("boolean"), TEXT("Update existing asset at this path instead of failing (default false)"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),    TEXT("string"),  TEXT("Created asset path") },
			{ TEXT("created"), TEXT("boolean"), TEXT("True for newly-created, false for overwrite") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Spawn a APhysicsConstraintActor in the editor world and bind it to two named
 * actors. The constraint settings come from the named actor's components or
 * actor roots.
 */
class FECACommand_SpawnPhysicsConstraint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_physics_constraint"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a APhysicsConstraintActor at a world location and bind it to two existing actors by name. The constraint's component name fields are left empty (binds to actor root primitive)."); }
	virtual FString GetCategory() const override { return TEXT("Physics"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor1_name"), TEXT("string"), TEXT("Name/label of the first actor to constrain"), true },
			{ TEXT("actor2_name"), TEXT("string"), TEXT("Name/label of the second actor to constrain"), true },
			{ TEXT("location"),    TEXT("object"), TEXT("World location for the constraint: {x,y,z}"), true },
			{ TEXT("label"),       TEXT("string"), TEXT("Optional editor label for the constraint actor"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("constraint_actor"), TEXT("string"), TEXT("Spawned actor path") },
			{ TEXT("actor1"),           TEXT("string"), TEXT("Bound actor1 path") },
			{ TEXT("actor2"),           TEXT("string"), TEXT("Bound actor2 path") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
