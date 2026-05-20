// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// =============================================================================
// PhysicsAsset authoring commands (Batch X). Distinct category "PhysicsAsset"
// to keep skeletal-physics authoring separate from dump/material/actor-level
// commands in the "Physics" category. See:
//   knowledge/intelligence/tools/ecabridge-physics-asset-port.md
// for the canonical 17-tool spec.
// =============================================================================

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

class FECACommand_CreatePhysicsAssetFromMesh : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_physics_asset_from_mesh"); }
	virtual FString GetDescription() const override { return TEXT("Create a UPhysicsAsset for a USkeletalMesh. Walks the mesh's bones and creates one default body per bone (no auto-constraints). Use add_constraint after to wire up joints."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("mesh_path"),      TEXT("string"),  TEXT("Asset path to a USkeletalMesh"), true },
			{ TEXT("assign_to_mesh"), TEXT("boolean"), TEXT("If true, sets USkeletalMesh::PhysicsAsset to the new asset"), false, TEXT("false") },
			{ TEXT("dest_path"),      TEXT("string"),  TEXT("Optional destination folder (must end with '/'). Defaults to <MeshFolder>/<MeshName>_PhysicsAsset"), false },
			{ TEXT("overwrite"),      TEXT("boolean"), TEXT("If false and destination exists, error out"), false, TEXT("false") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),             TEXT("string"),  TEXT("Created asset path") },
			{ TEXT("created"),          TEXT("boolean"), TEXT("True for newly-created, false when overwriting") },
			{ TEXT("body_count"),       TEXT("integer"), TEXT("Number of bodies created") },
			{ TEXT("constraint_count"), TEXT("integer"), TEXT("Number of auto-generated constraints (always 0 — use add_constraint)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddBody : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_body"); }
	virtual FString GetDescription() const override { return TEXT("Add an empty USkeletalBodySetup for a bone that doesn't already have one. Use set_body_sphere/capsule/box to populate the shape afterward."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone name (must exist on the bound skeleton)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"), TEXT("string"), TEXT("Bone the body was created for") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemoveBody : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_body"); }
	virtual FString GetDescription() const override { return TEXT("Remove the body for a bone and any constraints referencing it."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone whose body is to be removed"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"),            TEXT("string"), TEXT("Bone whose body was removed") },
			{ TEXT("removed_constraints"),  TEXT("array"),  TEXT("Names of constraints removed as a consequence"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -----------------------------------------------------------------------------
// Shape introspection
// -----------------------------------------------------------------------------

class FECACommand_GetBodyNames : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_body_names"); }
	virtual FString GetDescription() const override { return TEXT("List all bones in the physics asset that have a body."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_names"), TEXT("array"), TEXT("Bones that have a body"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetBodyShapes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_body_shapes"); }
	virtual FString GetDescription() const override { return TEXT("List all collision primitives on a body. Per-shape: name, type (Sphere/Capsule/Box), center, rotation, radius/length/extent."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"),       true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"), TEXT("string"), TEXT("Bone whose shapes are listed") },
			{ TEXT("shapes"),    TEXT("array"),  TEXT("Per-shape {shape_name, shape_type, center, rotation, radius?, length?, extent_x/y/z?}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -----------------------------------------------------------------------------
// Shape authoring (upsert by shape_name)
// -----------------------------------------------------------------------------

class FECACommand_SetBodySphere : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_body_sphere"); }
	virtual FString GetDescription() const override { return TEXT("Add or replace a sphere collision shape on a body. Identity is shape_name; replaces any prior shape with that name across all primitive arrays on the body."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("shape_name"),         TEXT("string"), TEXT("Unique shape name on the body"), true },
			{ TEXT("center"),             TEXT("object"), TEXT("Bone-local {x,y,z} in cm"), true },
			{ TEXT("radius"),             TEXT("number"), TEXT("Sphere radius (>0)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({ { TEXT("shape_name"), TEXT("string"), TEXT("Applied shape name") } });
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetBodyCapsule : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_body_capsule"); }
	virtual FString GetDescription() const override { return TEXT("Add or replace a capsule (sphyl) collision shape. Long axis is local Z after rotation. Total height = length + 2*radius."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("shape_name"),         TEXT("string"), TEXT("Unique shape name on the body"), true },
			{ TEXT("center"),             TEXT("object"), TEXT("Bone-local {x,y,z}"), true },
			{ TEXT("rotation"),           TEXT("object"), TEXT("Bone-local {pitch,yaw,roll} degrees"), true },
			{ TEXT("radius"),             TEXT("number"), TEXT("End-cap radius (>0)"), true },
			{ TEXT("length"),             TEXT("number"), TEXT("Cylinder section length (>=0)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({ { TEXT("shape_name"), TEXT("string"), TEXT("Applied shape name") } });
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetBodyBox : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_body_box"); }
	virtual FString GetDescription() const override { return TEXT("Add or replace a box collision shape with full extents on local X/Y/Z."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("shape_name"),         TEXT("string"), TEXT("Unique shape name on the body"), true },
			{ TEXT("center"),             TEXT("object"), TEXT("Bone-local {x,y,z}"), true },
			{ TEXT("rotation"),           TEXT("object"), TEXT("Bone-local {pitch,yaw,roll} degrees"), true },
			{ TEXT("extent_x"),           TEXT("number"), TEXT("Full extent local X (>0)"), true },
			{ TEXT("extent_y"),           TEXT("number"), TEXT("Full extent local Y (>0)"), true },
			{ TEXT("extent_z"),           TEXT("number"), TEXT("Full extent local Z (>0)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({ { TEXT("shape_name"), TEXT("string"), TEXT("Applied shape name") } });
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemoveBodyShape : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_body_shape"); }
	virtual FString GetDescription() const override { return TEXT("Remove a shape from a body by name. Searches sphere/box/capsule arrays."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("shape_name"),         TEXT("string"), TEXT("Name of shape to remove"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("shape_name"), TEXT("string"),  TEXT("Shape name requested") },
			{ TEXT("removed"),    TEXT("boolean"), TEXT("True if a shape was found and removed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -----------------------------------------------------------------------------
// Body properties
// -----------------------------------------------------------------------------

class FECACommand_GetBodyPhysicsMode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_body_physics_mode"); }
	virtual FString GetDescription() const override { return TEXT("Get the simulation mode of a body: Default | Kinematic | Simulated."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"), TEXT("string"), TEXT("Bone whose mode was queried") },
			{ TEXT("mode"),      TEXT("string"), TEXT("Default | Kinematic | Simulated") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetBodyPhysicsMode : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_body_physics_mode"); }
	virtual FString GetDescription() const override { return TEXT("Set the simulation mode of a body: Default | Kinematic | Simulated."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("mode"),               TEXT("string"), TEXT("Default | Kinematic | Simulated"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"), TEXT("string"), TEXT("Bone updated") },
			{ TEXT("mode"),      TEXT("string"), TEXT("Applied mode (echo)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetBodyMassScale : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_body_mass_scale"); }
	virtual FString GetDescription() const override { return TEXT("Get the mass-scale multiplier on a body's default instance."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"),  TEXT("string"), TEXT("Bone queried") },
			{ TEXT("mass_scale"), TEXT("number"), TEXT("Multiplier applied to computed mass") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetBodyMassScale : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_body_mass_scale"); }
	virtual FString GetDescription() const override { return TEXT("Set the mass-scale multiplier on a body's default instance (must be > 0)."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone_name"),          TEXT("string"), TEXT("Bone with a body"), true },
			{ TEXT("mass_scale"),         TEXT("number"), TEXT("Mass multiplier (>0)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone_name"),  TEXT("string"), TEXT("Bone updated") },
			{ TEXT("mass_scale"), TEXT("number"), TEXT("Applied mass-scale (echo)") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -----------------------------------------------------------------------------
// Constraint CRUD
// -----------------------------------------------------------------------------

class FECACommand_GetConstraints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_constraints"); }
	virtual FString GetDescription() const override { return TEXT("List all constraints on the physics asset, including swing1/swing2/twist motion + limit angles."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("constraints"), TEXT("array"), TEXT("Per-constraint {bone1_name (child), bone2_name (parent), swing1/swing2/twist motion + limit_degrees}"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_AddConstraint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_constraint"); }
	virtual FString GetDescription() const override { return TEXT("Add a default angular constraint between two bones (bone1 = child, bone2 = parent). Both bones must have bodies."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone1_name"),         TEXT("string"), TEXT("Child bone (must have a body)"), true },
			{ TEXT("bone2_name"),         TEXT("string"), TEXT("Parent bone (must have a body)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone1_name"), TEXT("string"), TEXT("Child bone") },
			{ TEXT("bone2_name"), TEXT("string"), TEXT("Parent bone") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetConstraintLimits : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_constraint_limits"); }
	virtual FString GetDescription() const override { return TEXT("Update angular limits on an existing constraint. Missing motion/limit params leave the existing value untouched."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"),     TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone1_name"),             TEXT("string"), TEXT("Child bone"), true },
			{ TEXT("bone2_name"),             TEXT("string"), TEXT("Parent bone"), true },
			{ TEXT("swing1_motion"),          TEXT("string"), TEXT("Free | Limited | Locked"), false },
			{ TEXT("swing1_limit_degrees"),   TEXT("number"), TEXT("[0,180] when motion is Limited"), false },
			{ TEXT("swing2_motion"),          TEXT("string"), TEXT("Free | Limited | Locked"), false },
			{ TEXT("swing2_limit_degrees"),   TEXT("number"), TEXT("[0,180] when motion is Limited"), false },
			{ TEXT("twist_motion"),           TEXT("string"), TEXT("Free | Limited | Locked"), false },
			{ TEXT("twist_limit_degrees"),    TEXT("number"), TEXT("[0,180] when motion is Limited"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone1_name"),           TEXT("string"), TEXT("Child bone") },
			{ TEXT("bone2_name"),           TEXT("string"), TEXT("Parent bone") },
			{ TEXT("swing1_motion"),        TEXT("string"), TEXT("Free | Limited | Locked") },
			{ TEXT("swing1_limit_degrees"), TEXT("number"), TEXT("Current swing1 limit") },
			{ TEXT("swing2_motion"),        TEXT("string"), TEXT("Free | Limited | Locked") },
			{ TEXT("swing2_limit_degrees"), TEXT("number"), TEXT("Current swing2 limit") },
			{ TEXT("twist_motion"),         TEXT("string"), TEXT("Free | Limited | Locked") },
			{ TEXT("twist_limit_degrees"),  TEXT("number"), TEXT("Current twist limit") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemoveConstraint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_constraint"); }
	virtual FString GetDescription() const override { return TEXT("Remove a constraint by (bone1,bone2) pair. Looks up either ordering."); }
	virtual FString GetCategory() const override { return TEXT("PhysicsAsset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to UPhysicsAsset"), true },
			{ TEXT("bone1_name"),         TEXT("string"), TEXT("Child bone"), true },
			{ TEXT("bone2_name"),         TEXT("string"), TEXT("Parent bone"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("bone1_name"), TEXT("string"),  TEXT("Child bone") },
			{ TEXT("bone2_name"), TEXT("string"),  TEXT("Parent bone") },
			{ TEXT("removed"),    TEXT("boolean"), TEXT("True if a matching constraint was removed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

