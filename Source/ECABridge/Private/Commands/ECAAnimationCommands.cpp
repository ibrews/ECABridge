// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAnimationCommands.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "UObject/SavePackage.h"

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
REGISTER_ECA_COMMAND(FECACommand_CreateAnimationSequence);

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

// ─── create_animation_sequence ───────────────────────────────

namespace CreateAnimSequenceHelpers
{
	/** Parse a FVector3f from a JSON object with x, y, z fields */
	static bool ParseVector3f(const TSharedPtr<FJsonObject>& Obj, FVector3f& OutVec)
	{
		if (!Obj.IsValid())
		{
			return false;
		}
		double X = 0.0, Y = 0.0, Z = 0.0;
		if (!Obj->TryGetNumberField(TEXT("x"), X) ||
			!Obj->TryGetNumberField(TEXT("y"), Y) ||
			!Obj->TryGetNumberField(TEXT("z"), Z))
		{
			return false;
		}
		OutVec = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
		return true;
	}

	/** Parse a FQuat4f from a JSON object. Accepts either quaternion {x,y,z,w} or Euler {pitch,yaw,roll}. */
	static bool ParseRotation(const TSharedPtr<FJsonObject>& Obj, FQuat4f& OutQuat)
	{
		if (!Obj.IsValid())
		{
			return false;
		}

		// Try quaternion first: requires 'w' field to distinguish from Euler
		double W = 0.0;
		if (Obj->TryGetNumberField(TEXT("w"), W))
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			Obj->TryGetNumberField(TEXT("x"), X);
			Obj->TryGetNumberField(TEXT("y"), Y);
			Obj->TryGetNumberField(TEXT("z"), Z);
			OutQuat = FQuat4f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z), static_cast<float>(W));
			OutQuat.Normalize();
			return true;
		}

		// Try Euler: {pitch, yaw, roll} in degrees
		double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
		bool bHasEuler = false;
		bHasEuler |= Obj->TryGetNumberField(TEXT("pitch"), Pitch);
		bHasEuler |= Obj->TryGetNumberField(TEXT("yaw"), Yaw);
		bHasEuler |= Obj->TryGetNumberField(TEXT("roll"), Roll);

		if (bHasEuler)
		{
			FRotator Rot(Pitch, Yaw, Roll);
			FQuat QuatD = Rot.Quaternion();
			OutQuat = FQuat4f(static_cast<float>(QuatD.X), static_cast<float>(QuatD.Y),
				static_cast<float>(QuatD.Z), static_cast<float>(QuatD.W));
			return true;
		}

		return false;
	}

	/** Sparse key data for a single bone */
	struct FSparseKey
	{
		int32 Frame = 0;
		bool bHasLocation = false;
		bool bHasRotation = false;
		bool bHasScale = false;
		FVector3f Location = FVector3f::ZeroVector;
		FQuat4f Rotation = FQuat4f::Identity;
		FVector3f Scale = FVector3f::OneVector;
	};

	/** Linearly interpolate FVector3f */
	static FVector3f LerpVector3f(const FVector3f& A, const FVector3f& B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	/** Spherically interpolate FQuat4f */
	static FQuat4f SlerpQuat4f(const FQuat4f& A, const FQuat4f& B, float Alpha)
	{
		// Use FQuat's Slerp via double-precision and convert back
		FQuat QA(A.X, A.Y, A.Z, A.W);
		FQuat QB(B.X, B.Y, B.Z, B.W);
		FQuat QResult = FQuat::Slerp(QA, QB, static_cast<double>(Alpha));
		return FQuat4f(
			static_cast<float>(QResult.X), static_cast<float>(QResult.Y),
			static_cast<float>(QResult.Z), static_cast<float>(QResult.W));
	}

	/**
	 * Given sparse keys and a frame count, produce dense arrays by interpolating between specified keys.
	 * For frames before the first key, the first key's value is held.
	 * For frames after the last key, the last key's value is held.
	 */
	static void InterpolateSparseKeys(
		const TArray<FSparseKey>& SparseKeys,
		int32 FrameCount,
		const FVector3f& RefLocation,
		const FQuat4f& RefRotation,
		const FVector3f& RefScale,
		TArray<FVector3f>& OutPositions,
		TArray<FQuat4f>& OutRotations,
		TArray<FVector3f>& OutScales)
	{
		OutPositions.SetNum(FrameCount);
		OutRotations.SetNum(FrameCount);
		OutScales.SetNum(FrameCount);

		if (SparseKeys.Num() == 0)
		{
			// No keys provided — fill with reference pose
			for (int32 F = 0; F < FrameCount; ++F)
			{
				OutPositions[F] = RefLocation;
				OutRotations[F] = RefRotation;
				OutScales[F] = RefScale;
			}
			return;
		}

		// Build resolved key arrays (fill in defaults where a channel wasn't specified)
		struct FResolvedKey
		{
			int32 Frame;
			FVector3f Location;
			FQuat4f Rotation;
			FVector3f Scale;
		};

		TArray<FResolvedKey> Resolved;
		Resolved.Reserve(SparseKeys.Num());
		for (const FSparseKey& SK : SparseKeys)
		{
			FResolvedKey RK;
			RK.Frame = SK.Frame;
			RK.Location = SK.bHasLocation ? SK.Location : RefLocation;
			RK.Rotation = SK.bHasRotation ? SK.Rotation : RefRotation;
			RK.Scale = SK.bHasScale ? SK.Scale : RefScale;
			Resolved.Add(RK);
		}

		// Sort by frame
		Resolved.Sort([](const FResolvedKey& A, const FResolvedKey& B) { return A.Frame < B.Frame; });

		int32 KeyIdx = 0;
		for (int32 F = 0; F < FrameCount; ++F)
		{
			// Advance key index so that Resolved[KeyIdx] is the last key at or before F
			while (KeyIdx < Resolved.Num() - 1 && Resolved[KeyIdx + 1].Frame <= F)
			{
				++KeyIdx;
			}

			if (F <= Resolved[0].Frame)
			{
				// Before or at first key — hold first key's value
				OutPositions[F] = Resolved[0].Location;
				OutRotations[F] = Resolved[0].Rotation;
				OutScales[F] = Resolved[0].Scale;
			}
			else if (KeyIdx >= Resolved.Num() - 1)
			{
				// At or past last key — hold last key's value
				OutPositions[F] = Resolved.Last().Location;
				OutRotations[F] = Resolved.Last().Rotation;
				OutScales[F] = Resolved.Last().Scale;
			}
			else
			{
				// Between two keys — interpolate
				const FResolvedKey& KA = Resolved[KeyIdx];
				const FResolvedKey& KB = Resolved[KeyIdx + 1];
				float Alpha = static_cast<float>(F - KA.Frame) / static_cast<float>(KB.Frame - KA.Frame);

				OutPositions[F] = LerpVector3f(KA.Location, KB.Location, Alpha);
				OutRotations[F] = SlerpQuat4f(KA.Rotation, KB.Rotation, Alpha);
				OutScales[F] = LerpVector3f(KA.Scale, KB.Scale, Alpha);
			}
		}
	}
}

FECACommandResult FECACommand_CreateAnimationSequence::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// ── Parse required parameters ──
	FString PackagePath, AssetName, SkeletonPath;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: package_path"));
	if (!GetStringParam(Params, TEXT("asset_name"), AssetName))
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_name"));
	if (!GetStringParam(Params, TEXT("skeleton_path"), SkeletonPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: skeleton_path"));

	int32 FrameCount = 0;
	if (!GetIntParam(Params, TEXT("frame_count"), FrameCount))
		return FECACommandResult::Error(TEXT("Missing required parameter: frame_count"));
	if (FrameCount < 1)
		return FECACommandResult::Error(TEXT("frame_count must be at least 1"));

	double FrameRateValue = 30.0;
	GetFloatParam(Params, TEXT("frame_rate"), FrameRateValue, /*bRequired=*/false);
	if (FrameRateValue <= 0.0)
		return FECACommandResult::Error(TEXT("frame_rate must be positive"));

	const TArray<TSharedPtr<FJsonValue>>* BoneTracksArray = nullptr;
	if (!GetArrayParam(Params, TEXT("bone_tracks"), BoneTracksArray))
		return FECACommandResult::Error(TEXT("Missing required parameter: bone_tracks"));
	if (!BoneTracksArray || BoneTracksArray->Num() == 0)
		return FECACommandResult::Error(TEXT("bone_tracks must be a non-empty array"));

	// ── Load the skeleton ──
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		// Try with sub-object syntax
		FString FullPath = SkeletonPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString ShortName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + ShortName;
		}
		Skeleton = LoadObject<USkeleton>(nullptr, *FullPath);
	}
	if (!Skeleton)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USkeleton at: %s"), *SkeletonPath));

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	// ── Create the package and UAnimSequence ──
	FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package at: %s"), *FullPackagePath));

	Package->FullyLoad();

	UAnimSequence* AnimSequence = NewObject<UAnimSequence>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!AnimSequence)
		return FECACommandResult::Error(TEXT("Failed to create UAnimSequence object"));

	AnimSequence->SetSkeleton(Skeleton);

	// ── Get the animation data controller ──
	IAnimationDataController& Controller = AnimSequence->GetController();

	Controller.OpenBracket(FText::FromString(TEXT("Create Animation Sequence via ECA")));

	// Set frame rate (use integer frame rate, denominator 1)
	int32 FrameRateInt = FMath::RoundToInt32(FrameRateValue);
	if (FrameRateInt < 1) FrameRateInt = 1;
	Controller.SetFrameRate(FFrameRate(FrameRateInt, 1));

	// Set number of frames (the length in frames; UE uses FFrameNumber which is frame count - 1 for zero-indexed,
	// but SetNumberOfFrames expects the total number of keys which equals FrameCount)
	Controller.SetNumberOfFrames(FFrameNumber(FrameCount - 1));

	// ── Process bone tracks ──
	TArray<FString> ProcessedBones;
	TArray<FString> Warnings;

	for (const TSharedPtr<FJsonValue>& TrackValue : *BoneTracksArray)
	{
		const TSharedPtr<FJsonObject>* TrackObjPtr = nullptr;
		if (!TrackValue.IsValid() || !TrackValue->TryGetObject(TrackObjPtr) || !TrackObjPtr || !TrackObjPtr->IsValid())
		{
			Warnings.Add(TEXT("Skipped a bone_tracks entry: not a valid object"));
			continue;
		}
		const TSharedPtr<FJsonObject>& TrackObj = *TrackObjPtr;

		FString BoneName;
		if (!TrackObj->TryGetStringField(TEXT("bone_name"), BoneName) || BoneName.IsEmpty())
		{
			Warnings.Add(TEXT("Skipped a bone_tracks entry: missing bone_name"));
			continue;
		}

		FName BoneFName(*BoneName);

		// Validate the bone exists in the skeleton
		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
		if (BoneIndex == INDEX_NONE)
		{
			Warnings.Add(FString::Printf(TEXT("Bone '%s' not found in skeleton, skipping"), *BoneName));
			continue;
		}

		// Get reference pose for this bone (used as default for unspecified channels)
		FTransform RefPose = RefSkeleton.GetRefBonePose()[BoneIndex];
		FVector3f RefLocation(
			static_cast<float>(RefPose.GetLocation().X),
			static_cast<float>(RefPose.GetLocation().Y),
			static_cast<float>(RefPose.GetLocation().Z));
		FQuat RefQuatD = RefPose.GetRotation();
		FQuat4f RefRotation(
			static_cast<float>(RefQuatD.X), static_cast<float>(RefQuatD.Y),
			static_cast<float>(RefQuatD.Z), static_cast<float>(RefQuatD.W));
		FVector3f RefScale(
			static_cast<float>(RefPose.GetScale3D().X),
			static_cast<float>(RefPose.GetScale3D().Y),
			static_cast<float>(RefPose.GetScale3D().Z));

		// Parse sparse keys
		const TArray<TSharedPtr<FJsonValue>>* KeysArray = nullptr;
		if (!TrackObj->TryGetArrayField(TEXT("keys"), KeysArray) || !KeysArray || KeysArray->Num() == 0)
		{
			Warnings.Add(FString::Printf(TEXT("Bone '%s' has no keys, skipping"), *BoneName));
			continue;
		}

		TArray<CreateAnimSequenceHelpers::FSparseKey> SparseKeys;
		SparseKeys.Reserve(KeysArray->Num());

		for (const TSharedPtr<FJsonValue>& KeyValue : *KeysArray)
		{
			const TSharedPtr<FJsonObject>* KeyObjPtr = nullptr;
			if (!KeyValue.IsValid() || !KeyValue->TryGetObject(KeyObjPtr) || !KeyObjPtr || !KeyObjPtr->IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& KeyObj = *KeyObjPtr;

			CreateAnimSequenceHelpers::FSparseKey SK;

			double FrameD = 0.0;
			if (!KeyObj->TryGetNumberField(TEXT("frame"), FrameD))
			{
				continue; // frame is required for each key
			}
			SK.Frame = FMath::RoundToInt32(FrameD);

			// Clamp frame to valid range
			SK.Frame = FMath::Clamp(SK.Frame, 0, FrameCount - 1);

			// Parse optional location
			const TSharedPtr<FJsonObject>* LocObj = nullptr;
			if (KeyObj->TryGetObjectField(TEXT("location"), LocObj) && LocObj && LocObj->IsValid())
			{
				SK.bHasLocation = CreateAnimSequenceHelpers::ParseVector3f(*LocObj, SK.Location);
			}

			// Parse optional rotation
			const TSharedPtr<FJsonObject>* RotObj = nullptr;
			if (KeyObj->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj && RotObj->IsValid())
			{
				SK.bHasRotation = CreateAnimSequenceHelpers::ParseRotation(*RotObj, SK.Rotation);
			}

			// Parse optional scale
			const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
			if (KeyObj->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj && ScaleObj->IsValid())
			{
				SK.bHasScale = CreateAnimSequenceHelpers::ParseVector3f(*ScaleObj, SK.Scale);
			}

			SparseKeys.Add(SK);
		}

		if (SparseKeys.Num() == 0)
		{
			Warnings.Add(FString::Printf(TEXT("Bone '%s' had no valid keys after parsing, skipping"), *BoneName));
			continue;
		}

		// Interpolate sparse keys into dense arrays
		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;
		TArray<FVector3f> Scales;
		CreateAnimSequenceHelpers::InterpolateSparseKeys(
			SparseKeys, FrameCount,
			RefLocation, RefRotation, RefScale,
			Positions, Rotations, Scales);

		// Add bone curve and set keys
		bool bCurveAdded = Controller.AddBoneCurve(BoneFName);
		if (!bCurveAdded)
		{
			// Curve may already exist (e.g. if bone is part of the default tracks)
			// Proceed to set keys anyway — SetBoneTrackKeys will work on existing curves
		}

		Controller.SetBoneTrackKeys(BoneFName, Positions, Rotations, Scales);
		ProcessedBones.Add(BoneName);
	}

	Controller.CloseBracket();

	// Force refresh to update SequenceLength and compressed data
	AnimSequence->RefreshCacheData();

	// ── Save the package ──
	AnimSequence->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(AnimSequence);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
	bool bSaved = UPackage::SavePackage(Package, AnimSequence, *PackageFilename, SaveArgs);

	// ── Build result ──
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AnimSequence->GetPathName());
	Result->SetStringField(TEXT("package_path"), FullPackagePath);
	Result->SetStringField(TEXT("asset_name"), AssetName);
	Result->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
	Result->SetNumberField(TEXT("frame_count"), FrameCount);
	Result->SetNumberField(TEXT("frame_rate"), FrameRateInt);
	Result->SetNumberField(TEXT("bone_tracks_processed"), ProcessedBones.Num());
	Result->SetBoolField(TEXT("saved_to_disk"), bSaved);

	TArray<TSharedPtr<FJsonValue>> BonesJsonArray;
	for (const FString& BN : ProcessedBones)
	{
		BonesJsonArray.Add(MakeShared<FJsonValueString>(BN));
	}
	Result->SetArrayField(TEXT("bones"), BonesJsonArray);

	if (Warnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarningsJsonArray;
		for (const FString& W : Warnings)
		{
			WarningsJsonArray.Add(MakeShared<FJsonValueString>(W));
		}
		Result->SetArrayField(TEXT("warnings"), WarningsJsonArray);
	}

	if (!bSaved)
	{
		Result->SetStringField(TEXT("save_warning"),
			TEXT("Asset was created in memory but could not be saved to disk. Use Save All in the editor."));
	}

	return FECACommandResult::Success(Result);
}
