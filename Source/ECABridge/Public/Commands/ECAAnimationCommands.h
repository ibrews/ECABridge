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
