// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAnimationCommands.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"

// ─── Helpers ──────────────────────────────────────────────────

namespace AnimationCommandHelpers
{
	/** Find the first USkeletalMeshComponent on an actor */
	static USkeletalMeshComponent* GetSkeletalMeshComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		USkeletalMeshComponent* SkelComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
		return SkelComp;
	}

	/** Load an animation asset by path, trying common path variants */
	static UAnimationAsset* LoadAnimationAsset(const FString& AnimPath)
	{
		UAnimationAsset* Asset = LoadObject<UAnimationAsset>(nullptr, *AnimPath);
		if (!Asset)
		{
			// Try appending the short name as the sub-object name
			FString FullPath = AnimPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Asset = LoadObject<UAnimationAsset>(nullptr, *FullPath);
		}
		return Asset;
	}

	/** Load a skeletal mesh by path, trying common path variants */
	static USkeletalMesh* LoadSkeletalMesh(const FString& MeshPath)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!Mesh)
		{
			FString FullPath = MeshPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Mesh = LoadObject<USkeletalMesh>(nullptr, *FullPath);
		}
		return Mesh;
	}
}

// ─── REGISTER ─────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_PlayAnimation);
REGISTER_ECA_COMMAND(FECACommand_StopAnimation);
REGISTER_ECA_COMMAND(FECACommand_GetActorAnimations);
REGISTER_ECA_COMMAND(FECACommand_SetAnimationBlueprint);
REGISTER_ECA_COMMAND(FECACommand_GetSkeletonInfo);

// ─── play_animation ───────────────────────────────────────────

FECACommandResult FECACommand_PlayAnimation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, AnimationPath;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("animation_path"), AnimationPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: animation_path"));

	bool bLoop = false;
	GetBoolParam(Params, TEXT("loop"), bLoop, /*bRequired=*/false);

	FString SlotName;
	GetStringParam(Params, TEXT("slot_name"), SlotName, /*bRequired=*/false);

	// Find the actor
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	// Get skeletal mesh component
	USkeletalMeshComponent* SkelComp = AnimationCommandHelpers::GetSkeletalMeshComponent(Actor);
	if (!SkelComp)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));

	// Load the animation asset
	UAnimationAsset* AnimAsset = AnimationCommandHelpers::LoadAnimationAsset(AnimationPath);
	if (!AnimAsset)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load animation asset at: %s"), *AnimationPath));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("animation_path"), AnimAsset->GetPathName());
	Result->SetBoolField(TEXT("loop"), bLoop);

	// Check if this is a montage
	UAnimMontage* Montage = Cast<UAnimMontage>(AnimAsset);
	if (Montage)
	{
		// Play as montage via the AnimInstance
		UAnimInstance* AnimInstance = SkelComp->GetAnimInstance();
		if (!AnimInstance)
		{
			// If there's no anim instance, we can't play montages.
			// Try to set up a basic one if an anim BP is assigned.
			return FECACommandResult::Error(
				TEXT("No AnimInstance on the SkeletalMeshComponent. A montage requires an Animation Blueprint. ")
				TEXT("Use set_animation_blueprint first, or use a UAnimSequence instead."));
		}

		float Duration = AnimInstance->Montage_Play(Montage, 1.0f);
		if (Duration <= 0.0f)
			return FECACommandResult::Error(TEXT("Montage_Play returned 0 - montage failed to play"));

		Result->SetStringField(TEXT("play_type"), TEXT("montage"));
		Result->SetNumberField(TEXT("duration"), Duration);

		if (!SlotName.IsEmpty())
		{
			Result->SetStringField(TEXT("slot_name"), SlotName);
		}
	}
	else
	{
		// Play as a simple animation sequence on the component directly
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimAsset);
		if (!AnimSeq)
			return FECACommandResult::Error(FString::Printf(
				TEXT("Animation asset '%s' is neither a UAnimSequence nor a UAnimMontage"), *AnimationPath));

		SkelComp->PlayAnimation(AnimSeq, bLoop);

		Result->SetStringField(TEXT("play_type"), TEXT("sequence"));
		Result->SetNumberField(TEXT("duration"), AnimSeq->GetPlayLength());
	}

	return FECACommandResult::Success(Result);
}

// ─── stop_animation ───────────────────────────────────────────

FECACommandResult FECACommand_StopAnimation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	USkeletalMeshComponent* SkelComp = AnimationCommandHelpers::GetSkeletalMeshComponent(Actor);
	if (!SkelComp)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));

	// Stop any montage playing on the anim instance
	UAnimInstance* AnimInstance = SkelComp->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.25f);
	}

	// Stop the component-level animation
	SkelComp->Stop();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetBoolField(TEXT("stopped"), true);

	return FECACommandResult::Success(Result);
}

// ─── get_actor_animations ─────────────────────────────────────

FECACommandResult FECACommand_GetActorAnimations::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	USkeletalMeshComponent* SkelComp = AnimationCommandHelpers::GetSkeletalMeshComponent(Actor);
	if (!SkelComp)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));

	USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMeshAsset();
	if (!SkelMesh)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no skeletal mesh assigned"), *ActorName));

	USkeleton* Skeleton = SkelMesh->GetSkeleton();
	if (!Skeleton)
		return FECACommandResult::Error(FString::Printf(TEXT("Skeletal mesh '%s' has no skeleton"), *SkelMesh->GetName()));

	// Query the asset registry for UAnimSequence assets compatible with this skeleton
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	FString SkeletonPath = Skeleton->GetPathName();

	TArray<TSharedPtr<FJsonValue>> AnimationsArray;
	for (const FAssetData& AssetData : AssetList)
	{
		// Check if the animation's skeleton tag matches our skeleton
		auto SkeletonTagResult = AssetData.TagsAndValues.FindTag(TEXT("Skeleton"));
		if (SkeletonTagResult.IsSet())
		{
			FString TagValue = SkeletonTagResult.AsString();
			if (TagValue.Contains(Skeleton->GetName()))
			{
				TSharedPtr<FJsonObject> AnimObj = MakeShared<FJsonObject>();
				AnimObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
				AnimObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
				AnimObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
				AnimationsArray.Add(MakeShared<FJsonValueObject>(AnimObj));
			}
		}
	}

	// Also search for UAnimMontage assets
	FARFilter MontageFilter;
	MontageFilter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	MontageFilter.bRecursiveClasses = true;

	TArray<FAssetData> MontageList;
	AssetRegistry.GetAssets(MontageFilter, MontageList);

	for (const FAssetData& AssetData : MontageList)
	{
		auto SkeletonTag = AssetData.TagsAndValues.FindTag(TEXT("Skeleton"));
		if (SkeletonTag.IsSet())
		{
			FString TagValue = SkeletonTag.AsString();
			if (TagValue.Contains(Skeleton->GetName()))
			{
				TSharedPtr<FJsonObject> AnimObj = MakeShared<FJsonObject>();
				AnimObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
				AnimObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
				AnimObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
				AnimationsArray.Add(MakeShared<FJsonValueObject>(AnimObj));
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("skeleton_name"), Skeleton->GetName());
	Result->SetStringField(TEXT("skeleton_path"), SkeletonPath);
	Result->SetNumberField(TEXT("animation_count"), AnimationsArray.Num());
	Result->SetArrayField(TEXT("animations"), AnimationsArray);

	return FECACommandResult::Success(Result);
}

// ─── set_animation_blueprint ──────────────────────────────────

FECACommandResult FECACommand_SetAnimationBlueprint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, AnimBPPath;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("anim_bp_path"), AnimBPPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: anim_bp_path"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	USkeletalMeshComponent* SkelComp = AnimationCommandHelpers::GetSkeletalMeshComponent(Actor);
	if (!SkelComp)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));

	// Load the Animation Blueprint
	// Try loading as UAnimBlueprint first to get the generated class
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
	if (!AnimBP)
	{
		// Try with sub-object syntax
		FString FullPath = AnimBPPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		AnimBP = LoadObject<UAnimBlueprint>(nullptr, *FullPath);
	}

	if (!AnimBP)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Animation Blueprint at: %s"), *AnimBPPath));

	UAnimBlueprintGeneratedClass* GeneratedClass = AnimBP->GetAnimBlueprintGeneratedClass();
	if (!GeneratedClass)
		return FECACommandResult::Error(FString::Printf(
			TEXT("Animation Blueprint '%s' has no generated class. It may need to be compiled first."), *AnimBPPath));

	// Set the anim instance class on the skeletal mesh component
	SkelComp->SetAnimInstanceClass(GeneratedClass);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("anim_bp_path"), AnimBP->GetPathName());
	Result->SetStringField(TEXT("anim_bp_name"), AnimBP->GetName());
	Result->SetStringField(TEXT("generated_class"), GeneratedClass->GetName());

	return FECACommandResult::Success(Result);
}

// ─── get_skeleton_info ────────────────────────────────────────

FECACommandResult FECACommand_GetSkeletonInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, MeshPath;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);
	GetStringParam(Params, TEXT("mesh_path"), MeshPath, /*bRequired=*/false);

	if (ActorName.IsEmpty() && MeshPath.IsEmpty())
		return FECACommandResult::Error(TEXT("At least one of 'actor_name' or 'mesh_path' must be provided"));

	USkeletalMesh* SkelMesh = nullptr;

	if (!ActorName.IsEmpty())
	{
		AActor* Actor = FindActorByName(ActorName);
		if (!Actor)
			return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

		USkeletalMeshComponent* SkelComp = AnimationCommandHelpers::GetSkeletalMeshComponent(Actor);
		if (!SkelComp)
			return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));

		SkelMesh = SkelComp->GetSkeletalMeshAsset();
		if (!SkelMesh)
			return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no skeletal mesh assigned"), *ActorName));
	}
	else
	{
		SkelMesh = AnimationCommandHelpers::LoadSkeletalMesh(MeshPath);
		if (!SkelMesh)
			return FECACommandResult::Error(FString::Printf(TEXT("Could not load skeletal mesh at: %s"), *MeshPath));
	}

	USkeleton* Skeleton = SkelMesh->GetSkeleton();
	if (!Skeleton)
		return FECACommandResult::Error(FString::Printf(TEXT("Skeletal mesh '%s' has no skeleton"), *SkelMesh->GetName()));

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 BoneCount = RefSkeleton.GetRawBoneNum();

	// Build bone hierarchy
	TArray<TSharedPtr<FJsonValue>> BonesArray;
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShared<FJsonObject>();
		BoneObj->SetStringField(TEXT("name"), RefSkeleton.GetBoneName(BoneIndex).ToString());
		BoneObj->SetNumberField(TEXT("index"), BoneIndex);
		BoneObj->SetNumberField(TEXT("parent_index"), RefSkeleton.GetParentIndex(BoneIndex));

		if (RefSkeleton.GetParentIndex(BoneIndex) >= 0)
		{
			BoneObj->SetStringField(TEXT("parent_name"),
				RefSkeleton.GetBoneName(RefSkeleton.GetParentIndex(BoneIndex)).ToString());
		}

		BonesArray.Add(MakeShared<FJsonValueObject>(BoneObj));
	}

	// Gather morph target names
	TArray<TSharedPtr<FJsonValue>> MorphTargetsArray;
	const TArray<TObjectPtr<UMorphTarget>>& MorphTargets = SkelMesh->GetMorphTargets();
	for (const TObjectPtr<UMorphTarget>& MorphTarget : MorphTargets)
	{
		if (MorphTarget)
		{
			MorphTargetsArray.Add(MakeShared<FJsonValueString>(MorphTarget->GetName()));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("mesh_name"), SkelMesh->GetName());
	Result->SetStringField(TEXT("mesh_path"), SkelMesh->GetPathName());
	Result->SetStringField(TEXT("skeleton_name"), Skeleton->GetName());
	Result->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
	Result->SetNumberField(TEXT("bone_count"), BoneCount);
	Result->SetArrayField(TEXT("bones"), BonesArray);
	Result->SetNumberField(TEXT("morph_target_count"), MorphTargetsArray.Num());
	Result->SetArrayField(TEXT("morph_targets"), MorphTargetsArray);

	if (!ActorName.IsEmpty())
	{
		Result->SetStringField(TEXT("actor_name"), ActorName);
	}

	return FECACommandResult::Success(Result);
}
