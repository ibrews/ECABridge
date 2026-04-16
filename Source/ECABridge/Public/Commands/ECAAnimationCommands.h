// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── play_animation ───────────────────────────────────────────
// Play an animation asset (UAnimSequence or UAnimMontage) on an actor's
// SkeletalMeshComponent.
class FECACommand_PlayAnimation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("play_animation"); }
	virtual FString GetDescription() const override { return TEXT("Play an animation asset on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("animation_path"), TEXT("string"), TEXT("Asset path to a UAnimSequence or UAnimMontage (e.g. /Game/Animations/Walk)"), true },
			{ TEXT("loop"), TEXT("boolean"), TEXT("Whether to loop the animation (default false)"), false, TEXT("false") },
			{ TEXT("slot_name"), TEXT("string"), TEXT("Slot name for montage playback (optional, e.g. DefaultSlot)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── stop_animation ───────────────────────────────────────────
// Stop any playing animation on an actor's SkeletalMeshComponent.
class FECACommand_StopAnimation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_animation"); }
	virtual FString GetDescription() const override { return TEXT("Stop any playing animation on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_actor_animations ─────────────────────────────────────
// List all animation assets compatible with an actor's skeleton.
class FECACommand_GetActorAnimations : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actor_animations"); }
	virtual FString GetDescription() const override { return TEXT("List all animation assets compatible with an actor's skeleton"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_animation_blueprint ──────────────────────────────────
// Set the Animation Blueprint on an actor's SkeletalMeshComponent.
class FECACommand_SetAnimationBlueprint : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_animation_blueprint"); }
	virtual FString GetDescription() const override { return TEXT("Set the Animation Blueprint on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("anim_bp_path"), TEXT("string"), TEXT("Asset path to the Animation Blueprint (e.g. /Game/Animations/ABP_Character)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_skeleton_info ────────────────────────────────────────
// Get bone hierarchy and morph target info for a skeletal mesh.
class FECACommand_GetSkeletonInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_skeleton_info"); }
	virtual FString GetDescription() const override { return TEXT("Get skeleton info: bone names, bone count, morph targets for a skeletal mesh"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of an actor with a SkeletalMeshComponent (optional if mesh_path provided)"), false },
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to a USkeletalMesh (optional if actor_name provided)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── set_skeletal_mesh ──────────────────────────────────────
// Set a skeletal mesh on a SkeletalMeshActor or any actor with a
// SkeletalMeshComponent.
class FECACommand_SetSkeletalMesh : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_skeletal_mesh"); }
	virtual FString GetDescription() const override { return TEXT("Set a skeletal mesh on an actor's SkeletalMeshComponent"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor in the level"), true },
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Asset path to a USkeletalMesh (e.g. /Game/Characters/SK_Mannequin)"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Specific SkeletalMeshComponent to target (optional, uses first found if omitted)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── create_animation_sequence ───────────────────────────────
// Create a new UAnimSequence asset with programmatic bone keyframes.
class FECACommand_CreateAnimationSequence : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_animation_sequence"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UAnimSequence asset with programmatic bone keyframes"); }
	virtual FString GetCategory() const override { return TEXT("Animation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Package path for the new asset (e.g. /Game/Animations)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new animation asset (e.g. Anim_Wave)"), true },
			{ TEXT("skeleton_path"), TEXT("string"), TEXT("Asset path to the USkeleton (from get_skeleton_info)"), true },
			{ TEXT("frame_count"), TEXT("number"), TEXT("Total number of frames in the animation"), true },
			{ TEXT("frame_rate"), TEXT("number"), TEXT("Frames per second (default 30)"), false, TEXT("30") },
			{ TEXT("bone_tracks"), TEXT("array"), TEXT("Array of bone track objects, each with: bone_name (string), keys (array of {frame, location?, rotation?, scale?})"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
