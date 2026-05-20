// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPhysicsAssetCommands.h"
#include "Commands/ECACommand.h"
#include "ECAPhysicsHelpers.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ShapeElem.h"

#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_CreatePhysicsAssetFromMesh)
REGISTER_ECA_COMMAND(FECACommand_AddBody)
REGISTER_ECA_COMMAND(FECACommand_RemoveBody)
REGISTER_ECA_COMMAND(FECACommand_GetBodyNames)
REGISTER_ECA_COMMAND(FECACommand_GetBodyShapes)
REGISTER_ECA_COMMAND(FECACommand_SetBodySphere)
REGISTER_ECA_COMMAND(FECACommand_SetBodyCapsule)
REGISTER_ECA_COMMAND(FECACommand_SetBodyBox)
REGISTER_ECA_COMMAND(FECACommand_RemoveBodyShape)
REGISTER_ECA_COMMAND(FECACommand_GetBodyPhysicsMode)
REGISTER_ECA_COMMAND(FECACommand_SetBodyPhysicsMode)
REGISTER_ECA_COMMAND(FECACommand_GetBodyMassScale)
REGISTER_ECA_COMMAND(FECACommand_SetBodyMassScale)
REGISTER_ECA_COMMAND(FECACommand_GetConstraints)
REGISTER_ECA_COMMAND(FECACommand_AddConstraint)
REGISTER_ECA_COMMAND(FECACommand_SetConstraintLimits)
REGISTER_ECA_COMMAND(FECACommand_RemoveConstraint)

// =============================================================================
// Local helpers
// =============================================================================
namespace ECAPhysicsAssetLocal
{
	static USkeletalBodySetup* FindBodyForBone(UPhysicsAsset* PA, FName BoneName, int32* OutIndex = nullptr)
	{
		if (!PA) return nullptr;
		const int32 Idx = PA->FindBodyIndex(BoneName);
		if (OutIndex) *OutIndex = Idx;
		if (Idx == INDEX_NONE) return nullptr;
		return PA->SkeletalBodySetups.IsValidIndex(Idx) ? PA->SkeletalBodySetups[Idx] : nullptr;
	}

	static bool SkeletonContainsBone(USkeletalMesh* Mesh, FName BoneName)
	{
		if (!Mesh) return false;
		const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
		return RefSkel.FindBoneIndex(BoneName) != INDEX_NONE;
	}

	// Resolve the skeleton mesh associated with a physics asset (for bone-name
	// validation). Returns nullptr if no preview mesh is set; callers should
	// then treat bone-name input as trusted.
	static USkeletalMesh* GetPreviewMesh(UPhysicsAsset* PA)
	{
		if (!PA) return nullptr;
#if WITH_EDITORONLY_DATA
		return PA->PreviewSkeletalMesh.LoadSynchronous();
#else
		return nullptr;
#endif
	}

	static void MarkAssetDirtyAndRefresh(UPhysicsAsset* PA)
	{
		if (!PA) return;
		PA->UpdateBodySetupIndexMap();
		PA->UpdateBoundsBodiesArray();
#if WITH_EDITOR
		PA->RefreshPhysicsAssetChange();
#endif
		PA->MarkPackageDirty();
	}
}

// =============================================================================
// create_physics_asset_from_mesh
// =============================================================================
FECACommandResult FECACommand_CreatePhysicsAssetFromMesh::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshPath;
	if (!GetStringParam(Params, TEXT("mesh_path"), MeshPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mesh_path"));
	}

	bool bAssignToMesh = false;
	bool bOverwrite = false;
	FString DestPath;
	GetBoolParam(Params, TEXT("assign_to_mesh"), bAssignToMesh, false);
	GetBoolParam(Params, TEXT("overwrite"), bOverwrite, false);
	GetStringParam(Params, TEXT("dest_path"), DestPath, false);

	USkeletalMesh* Mesh = ECAPhysicsHelpers::LoadAssetTolerant<USkeletalMesh>(MeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USkeletalMesh at: %s"), *MeshPath));
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	const int32 BoneCount = RefSkel.GetNum();
	if (BoneCount <= 0)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Skeletal mesh '%s' has no bones"), *MeshPath));
	}

	// Compute destination package path
	const FString MeshPackage = Mesh->GetOutermost()->GetName(); // e.g. /Game/Foo/SK_Bar
	const FString MeshShortName = FPackageName::GetShortName(MeshPackage);
	const FString MeshFolder = FPackageName::GetLongPackagePath(MeshPackage); // /Game/Foo

	FString Folder = DestPath;
	if (Folder.IsEmpty())
	{
		Folder = MeshFolder + TEXT("/");
	}
	else if (!Folder.EndsWith(TEXT("/")))
	{
		Folder += TEXT("/");
	}

	const FString AssetName = MeshShortName + TEXT("_PhysicsAsset");
	const FString PackagePath = Folder + AssetName;
	const FString ObjectPath = PackagePath + TEXT(".") + AssetName;

	UPhysicsAsset* ExistingPA = nullptr;
	if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath))
	{
		ExistingPA = Cast<UPhysicsAsset>(Existing);
		if (!ExistingPA)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Asset exists at '%s' but is %s, not UPhysicsAsset"), *ObjectPath, *Existing->GetClass()->GetName()));
		}
		if (!bOverwrite)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("UPhysicsAsset already exists at '%s'. Pass overwrite=true to replace."), *ObjectPath));
		}
	}

	UPhysicsAsset* PA = ExistingPA;
	if (!PA)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return FECACommandResult::Error(TEXT("Failed to create package"));
		}
		PA = NewObject<UPhysicsAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!PA)
		{
			return FECACommandResult::Error(TEXT("Failed to create UPhysicsAsset"));
		}
		FAssetRegistryModule::AssetCreated(PA);
	}
	else
	{
		// Overwrite path: drop all existing bodies + constraints
		PA->SkeletalBodySetups.Empty();
		PA->ConstraintSetup.Empty();
		PA->BodySetupIndexMap.Empty();
	}

#if WITH_EDITORONLY_DATA
	PA->PreviewSkeletalMesh = Mesh;
#endif

	int32 BodiesCreated = 0;
	for (int32 BoneIdx = 0; BoneIdx < BoneCount; ++BoneIdx)
	{
		const FName BoneName = RefSkel.GetBoneName(BoneIdx);
		if (BoneName.IsNone()) continue;

		USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PA, NAME_None, RF_Transactional);
		Body->BoneName = BoneName;
		Body->PhysicsType = PhysType_Default;
		Body->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;

		// Seed with a default unit sphere centered on the bone so the body is
		// physically meaningful out of the gate; callers replace it via
		// set_body_sphere/capsule/box.
		FKSphereElem Default(2.0f);
		Default.Center = FVector::ZeroVector;
		Default.SetName(FName(*FString::Printf(TEXT("Default_%s"), *BoneName.ToString())));
		Body->AggGeom.SphereElems.Add(Default);

		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();

		PA->SkeletalBodySetups.Add(Body);
		++BodiesCreated;
	}

	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	if (bAssignToMesh)
	{
		Mesh->Modify();
#if WITH_EDITORONLY_DATA
		Mesh->SetPhysicsAsset(PA);
#endif
		Mesh->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), PA->GetPathName());
	Result->SetBoolField(TEXT("created"), ExistingPA == nullptr);
	Result->SetNumberField(TEXT("body_count"), BodiesCreated);
	Result->SetNumberField(TEXT("constraint_count"), 0);
	return FECACommandResult::Success(Result);
}

// =============================================================================
// add_body
// =============================================================================
FECACommandResult FECACommand_AddBody::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, BoneNameStr;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	}
	if (!GetStringParam(Params, TEXT("bone_name"), BoneNameStr))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone_name"));
	}

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	}

	const FName BoneName(*BoneNameStr);
	if (PA->FindBodyIndex(BoneName) != INDEX_NONE)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Body for bone '%s' already exists"), *BoneNameStr));
	}

	USkeletalMesh* PreviewMesh = ECAPhysicsAssetLocal::GetPreviewMesh(PA);
	if (PreviewMesh && !ECAPhysicsAssetLocal::SkeletonContainsBone(PreviewMesh, BoneName))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Bone '%s' not found on preview skeleton of '%s'"), *BoneNameStr, *PAPath));
	}

	PA->Modify();
	USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PA, NAME_None, RF_Transactional);
	Body->BoneName = BoneName;
	Body->PhysicsType = PhysType_Default;
	Body->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
	PA->SkeletalBodySetups.Add(Body);

	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneNameStr);
	return FECACommandResult::Success(Result);
}

// =============================================================================
// remove_body
// =============================================================================
FECACommandResult FECACommand_RemoveBody::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, BoneNameStr;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	}
	if (!GetStringParam(Params, TEXT("bone_name"), BoneNameStr))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone_name"));
	}

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	}

	const FName BoneName(*BoneNameStr);
	const int32 BodyIdx = PA->FindBodyIndex(BoneName);
	if (BodyIdx == INDEX_NONE)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("No body found for bone '%s'"), *BoneNameStr));
	}

	PA->Modify();

	// Collect + drop constraints touching this bone (either side)
	TArray<TSharedPtr<FJsonValue>> RemovedConstraints;
	for (int32 i = PA->ConstraintSetup.Num() - 1; i >= 0; --i)
	{
		UPhysicsConstraintTemplate* CT = PA->ConstraintSetup[i];
		if (!CT) continue;
		const FConstraintInstance& CI = CT->DefaultInstance;
		if (CI.GetChildBoneName() == BoneName || CI.GetParentBoneName() == BoneName)
		{
			RemovedConstraints.Add(MakeShared<FJsonValueString>(CI.JointName.ToString()));
			PA->ConstraintSetup.RemoveAt(i);
		}
	}

	PA->SkeletalBodySetups.RemoveAt(BodyIdx);
	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneNameStr);
	Result->SetArrayField(TEXT("removed_constraints"), RemovedConstraints);
	return FECACommandResult::Success(Result);
}

// =============================================================================
// get_body_names
// =============================================================================
FECACommandResult FECACommand_GetBodyNames::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	}

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	}

	TArray<TSharedPtr<FJsonValue>> Names;
	for (USkeletalBodySetup* Body : PA->SkeletalBodySetups)
	{
		if (!Body) continue;
		Names.Add(MakeShared<FJsonValueString>(Body->BoneName.ToString()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("bone_names"), Names);
	return FECACommandResult::Success(Result);
}

// =============================================================================
// get_body_shapes
// =============================================================================
namespace ECAPhysicsAssetLocal
{
	static TSharedPtr<FJsonObject> VecToJson(const FVector& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		O->SetNumberField(TEXT("z"), V.Z);
		return O;
	}
	static TSharedPtr<FJsonObject> RotToJson(const FRotator& R)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("pitch"), R.Pitch);
		O->SetNumberField(TEXT("yaw"),   R.Yaw);
		O->SetNumberField(TEXT("roll"),  R.Roll);
		return O;
	}
}

FECACommandResult FECACommand_GetBodyShapes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, BoneNameStr;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	}
	if (!GetStringParam(Params, TEXT("bone_name"), BoneNameStr))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone_name"));
	}

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	}

	const FName BoneName(*BoneNameStr);
	USkeletalBodySetup* Body = ECAPhysicsAssetLocal::FindBodyForBone(PA, BoneName);
	if (!Body)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("No body found for bone '%s'"), *BoneNameStr));
	}

	TArray<TSharedPtr<FJsonValue>> Shapes;
	for (const FKSphereElem& E : Body->AggGeom.SphereElems)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("shape_name"), E.GetName().ToString());
		O->SetStringField(TEXT("shape_type"), TEXT("Sphere"));
		O->SetObjectField(TEXT("center"), ECAPhysicsAssetLocal::VecToJson(E.Center));
		O->SetNumberField(TEXT("radius"), E.Radius);
		Shapes.Add(MakeShared<FJsonValueObject>(O));
	}
	for (const FKSphylElem& E : Body->AggGeom.SphylElems)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("shape_name"), E.GetName().ToString());
		O->SetStringField(TEXT("shape_type"), TEXT("Capsule"));
		O->SetObjectField(TEXT("center"),   ECAPhysicsAssetLocal::VecToJson(E.Center));
		O->SetObjectField(TEXT("rotation"), ECAPhysicsAssetLocal::RotToJson(E.Rotation));
		O->SetNumberField(TEXT("radius"), E.Radius);
		O->SetNumberField(TEXT("length"), E.Length);
		Shapes.Add(MakeShared<FJsonValueObject>(O));
	}
	for (const FKBoxElem& E : Body->AggGeom.BoxElems)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("shape_name"), E.GetName().ToString());
		O->SetStringField(TEXT("shape_type"), TEXT("Box"));
		O->SetObjectField(TEXT("center"),   ECAPhysicsAssetLocal::VecToJson(E.Center));
		O->SetObjectField(TEXT("rotation"), ECAPhysicsAssetLocal::RotToJson(E.Rotation));
		O->SetNumberField(TEXT("extent_x"), E.X);
		O->SetNumberField(TEXT("extent_y"), E.Y);
		O->SetNumberField(TEXT("extent_z"), E.Z);
		Shapes.Add(MakeShared<FJsonValueObject>(O));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneNameStr);
	Result->SetArrayField(TEXT("shapes"), Shapes);
	return FECACommandResult::Success(Result);
}

// =============================================================================
// Tasks 3-5 implementations
// =============================================================================
namespace ECAPhysicsAssetLocal
{
	static bool RemoveShapeByName(USkeletalBodySetup* Body, const FName& ShapeName)
	{
		if (!Body) return false;
		for (int32 i = Body->AggGeom.SphereElems.Num() - 1; i >= 0; --i)
		{
			if (Body->AggGeom.SphereElems[i].GetName() == ShapeName)
			{ Body->AggGeom.SphereElems.RemoveAt(i); return true; }
		}
		for (int32 i = Body->AggGeom.SphylElems.Num() - 1; i >= 0; --i)
		{
			if (Body->AggGeom.SphylElems[i].GetName() == ShapeName)
			{ Body->AggGeom.SphylElems.RemoveAt(i); return true; }
		}
		for (int32 i = Body->AggGeom.BoxElems.Num() - 1; i >= 0; --i)
		{
			if (Body->AggGeom.BoxElems[i].GetName() == ShapeName)
			{ Body->AggGeom.BoxElems.RemoveAt(i); return true; }
		}
		return false;
	}

	struct FShapeContext
	{
		UPhysicsAsset* PA = nullptr;
		USkeletalBodySetup* Body = nullptr;
		FName ShapeName;
		FString BoneNameStr;
	};

	static FECACommandResult ResolveShapeContext(IECACommand* Cmd, const TSharedPtr<FJsonObject>& Params, FShapeContext& Out)
	{
		FString PAPath;
		if (!Params.IsValid() || !Params->TryGetStringField(TEXT("physics_asset_path"), PAPath) || PAPath.IsEmpty())
			return FECACommandResult::ValidationError(Cmd, TEXT("Missing required parameter: physics_asset_path"));
		if (!Params->TryGetStringField(TEXT("bone_name"), Out.BoneNameStr) || Out.BoneNameStr.IsEmpty())
			return FECACommandResult::ValidationError(Cmd, TEXT("Missing required parameter: bone_name"));
		FString ShapeNameStr;
		if (!Params->TryGetStringField(TEXT("shape_name"), ShapeNameStr) || ShapeNameStr.IsEmpty())
			return FECACommandResult::ValidationError(Cmd, TEXT("Missing required parameter: shape_name"));
		Out.PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
		if (!Out.PA)
			return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
		Out.Body = FindBodyForBone(Out.PA, FName(*Out.BoneNameStr));
		if (!Out.Body)
			return FECACommandResult::Error(FString::Printf(TEXT("No body found for bone '%s'"), *Out.BoneNameStr));
		Out.ShapeName = FName(*ShapeNameStr);
		return FECACommandResult::Success();
	}

	static FECACommandResult ResolveBoneBody(IECACommand* Cmd, const TSharedPtr<FJsonObject>& Params,
	                                          UPhysicsAsset*& OutPA, USkeletalBodySetup*& OutBody, FString& OutBoneStr)
	{
		FString PAPath;
		if (!Params.IsValid() || !Params->TryGetStringField(TEXT("physics_asset_path"), PAPath) || PAPath.IsEmpty())
			return FECACommandResult::ValidationError(Cmd, TEXT("Missing required parameter: physics_asset_path"));
		if (!Params->TryGetStringField(TEXT("bone_name"), OutBoneStr) || OutBoneStr.IsEmpty())
			return FECACommandResult::ValidationError(Cmd, TEXT("Missing required parameter: bone_name"));
		OutPA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
		if (!OutPA)
			return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
		OutBody = FindBodyForBone(OutPA, FName(*OutBoneStr));
		if (!OutBody)
			return FECACommandResult::Error(FString::Printf(TEXT("No body found for bone '%s'"), *OutBoneStr));
		return FECACommandResult::Success();
	}

	static void DirtyBody(USkeletalBodySetup* Body, UPhysicsAsset* PA)
	{
		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();
		Body->MarkPackageDirty();
		MarkAssetDirtyAndRefresh(PA);
	}

	static FString PhysModeToString(EPhysicsType M)
	{
		switch (M)
		{
			case PhysType_Default:   return TEXT("Default");
			case PhysType_Kinematic: return TEXT("Kinematic");
			case PhysType_Simulated: return TEXT("Simulated");
			default:                 return TEXT("Default");
		}
	}
	static bool StringToPhysMode(const FString& S, EPhysicsType& Out)
	{
		if (S == TEXT("Default"))   { Out = PhysType_Default;   return true; }
		if (S == TEXT("Kinematic")) { Out = PhysType_Kinematic; return true; }
		if (S == TEXT("Simulated")) { Out = PhysType_Simulated; return true; }
		return false;
	}

	static FString MotionToString(EAngularConstraintMotion M)
	{
		switch (M)
		{
			case ACM_Free:    return TEXT("Free");
			case ACM_Limited: return TEXT("Limited");
			case ACM_Locked:  return TEXT("Locked");
			default:          return TEXT("Free");
		}
	}
	static bool StringToMotion(const FString& S, EAngularConstraintMotion& Out)
	{
		if (S == TEXT("Free"))    { Out = ACM_Free;    return true; }
		if (S == TEXT("Limited")) { Out = ACM_Limited; return true; }
		if (S == TEXT("Locked"))  { Out = ACM_Locked;  return true; }
		return false;
	}

	static int32 FindConstraintByPair(UPhysicsAsset* PA, FName B1, FName B2)
	{
		if (!PA) return INDEX_NONE;
		int32 Idx = PA->FindConstraintIndex(B1, B2);
		if (Idx != INDEX_NONE) return Idx;
		return PA->FindConstraintIndex(B2, B1);
	}

	static void FillConstraintJson(const FConstraintInstance& CI, FName B1, FName B2, const TSharedPtr<FJsonObject>& O)
	{
		O->SetStringField(TEXT("bone1_name"), B1.ToString());
		O->SetStringField(TEXT("bone2_name"), B2.ToString());
		O->SetStringField(TEXT("swing1_motion"),        MotionToString(CI.GetAngularSwing1Motion()));
		O->SetNumberField(TEXT("swing1_limit_degrees"), CI.GetAngularSwing1Limit());
		O->SetStringField(TEXT("swing2_motion"),        MotionToString(CI.GetAngularSwing2Motion()));
		O->SetNumberField(TEXT("swing2_limit_degrees"), CI.GetAngularSwing2Limit());
		O->SetStringField(TEXT("twist_motion"),         MotionToString(CI.GetAngularTwistMotion()));
		O->SetNumberField(TEXT("twist_limit_degrees"),  CI.GetAngularTwistLimit());
	}
}

// === Task 3: set_body_sphere ===
FECACommandResult FECACommand_SetBodySphere::Execute(const TSharedPtr<FJsonObject>& Params)
{
	ECAPhysicsAssetLocal::FShapeContext Ctx;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveShapeContext(this, Params, Ctx);
	if (!R.bSuccess) return R;
	FVector Center;
	if (!GetVectorParam(Params, TEXT("center"), Center))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: center"));
	double Radius = 0.0;
	if (!GetFloatParam(Params, TEXT("radius"), Radius))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: radius"));
	if (Radius <= 0.0) return FECACommandResult::Error(TEXT("radius must be > 0"));

	Ctx.PA->Modify();
	ECAPhysicsAssetLocal::RemoveShapeByName(Ctx.Body, Ctx.ShapeName);
	FKSphereElem Elem(static_cast<float>(Radius));
	Elem.Center = Center;
	Elem.SetName(Ctx.ShapeName);
	Ctx.Body->AggGeom.SphereElems.Add(Elem);
	ECAPhysicsAssetLocal::DirtyBody(Ctx.Body, Ctx.PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("shape_name"), Ctx.ShapeName.ToString());
	return FECACommandResult::Success(Result);
}

// === Task 3: set_body_capsule ===
FECACommandResult FECACommand_SetBodyCapsule::Execute(const TSharedPtr<FJsonObject>& Params)
{
	ECAPhysicsAssetLocal::FShapeContext Ctx;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveShapeContext(this, Params, Ctx);
	if (!R.bSuccess) return R;
	FVector Center; FRotator Rotation;
	if (!GetVectorParam(Params, TEXT("center"), Center))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: center"));
	if (!GetRotatorParam(Params, TEXT("rotation"), Rotation))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: rotation"));
	double Radius = 0.0, Length = 0.0;
	if (!GetFloatParam(Params, TEXT("radius"), Radius))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: radius"));
	if (!GetFloatParam(Params, TEXT("length"), Length))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: length"));
	if (Radius <= 0.0) return FECACommandResult::Error(TEXT("radius must be > 0"));
	if (Length < 0.0)  return FECACommandResult::Error(TEXT("length must be >= 0"));

	Ctx.PA->Modify();
	ECAPhysicsAssetLocal::RemoveShapeByName(Ctx.Body, Ctx.ShapeName);
	FKSphylElem Elem(static_cast<float>(Radius), static_cast<float>(Length));
	Elem.Center = Center; Elem.Rotation = Rotation;
	Elem.SetName(Ctx.ShapeName);
	Ctx.Body->AggGeom.SphylElems.Add(Elem);
	ECAPhysicsAssetLocal::DirtyBody(Ctx.Body, Ctx.PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("shape_name"), Ctx.ShapeName.ToString());
	return FECACommandResult::Success(Result);
}

// === Task 3: set_body_box ===
FECACommandResult FECACommand_SetBodyBox::Execute(const TSharedPtr<FJsonObject>& Params)
{
	ECAPhysicsAssetLocal::FShapeContext Ctx;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveShapeContext(this, Params, Ctx);
	if (!R.bSuccess) return R;
	FVector Center; FRotator Rotation;
	if (!GetVectorParam(Params, TEXT("center"), Center))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: center"));
	if (!GetRotatorParam(Params, TEXT("rotation"), Rotation))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: rotation"));
	double X = 0.0, Y = 0.0, Z = 0.0;
	if (!GetFloatParam(Params, TEXT("extent_x"), X) ||
	    !GetFloatParam(Params, TEXT("extent_y"), Y) ||
	    !GetFloatParam(Params, TEXT("extent_z"), Z))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: extent_x/y/z"));
	if (X <= 0.0 || Y <= 0.0 || Z <= 0.0)
		return FECACommandResult::Error(TEXT("extent_x/y/z must all be > 0"));

	Ctx.PA->Modify();
	ECAPhysicsAssetLocal::RemoveShapeByName(Ctx.Body, Ctx.ShapeName);
	FKBoxElem Elem(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
	Elem.Center = Center; Elem.Rotation = Rotation;
	Elem.SetName(Ctx.ShapeName);
	Ctx.Body->AggGeom.BoxElems.Add(Elem);
	ECAPhysicsAssetLocal::DirtyBody(Ctx.Body, Ctx.PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("shape_name"), Ctx.ShapeName.ToString());
	return FECACommandResult::Success(Result);
}

// === Task 3: remove_body_shape ===
FECACommandResult FECACommand_RemoveBodyShape::Execute(const TSharedPtr<FJsonObject>& Params)
{
	ECAPhysicsAssetLocal::FShapeContext Ctx;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveShapeContext(this, Params, Ctx);
	if (!R.bSuccess) return R;
	Ctx.PA->Modify();
	const bool bRemoved = ECAPhysicsAssetLocal::RemoveShapeByName(Ctx.Body, Ctx.ShapeName);
	if (bRemoved) ECAPhysicsAssetLocal::DirtyBody(Ctx.Body, Ctx.PA);
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("shape_name"), Ctx.ShapeName.ToString());
	Result->SetBoolField(TEXT("removed"), bRemoved);
	return FECACommandResult::Success(Result);
}

// === Task 4: get/set_body_physics_mode ===
FECACommandResult FECACommand_GetBodyPhysicsMode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UPhysicsAsset* PA = nullptr; USkeletalBodySetup* Body = nullptr; FString BoneStr;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveBoneBody(this, Params, PA, Body, BoneStr);
	if (!R.bSuccess) return R;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneStr);
	Result->SetStringField(TEXT("mode"), ECAPhysicsAssetLocal::PhysModeToString(Body->PhysicsType));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_SetBodyPhysicsMode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UPhysicsAsset* PA = nullptr; USkeletalBodySetup* Body = nullptr; FString BoneStr;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveBoneBody(this, Params, PA, Body, BoneStr);
	if (!R.bSuccess) return R;
	FString ModeStr;
	if (!GetStringParam(Params, TEXT("mode"), ModeStr))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mode"));
	EPhysicsType Mode;
	if (!ECAPhysicsAssetLocal::StringToPhysMode(ModeStr, Mode))
		return FECACommandResult::ValidationError(this, FString::Printf(TEXT("Invalid mode '%s' (expected Default | Kinematic | Simulated)"), *ModeStr));

	PA->Modify();
	Body->PhysicsType = Mode;
	Body->MarkPackageDirty();
	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneStr);
	Result->SetStringField(TEXT("mode"), ECAPhysicsAssetLocal::PhysModeToString(Body->PhysicsType));
	return FECACommandResult::Success(Result);
}

// === Task 4: get/set_body_mass_scale ===
FECACommandResult FECACommand_GetBodyMassScale::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UPhysicsAsset* PA = nullptr; USkeletalBodySetup* Body = nullptr; FString BoneStr;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveBoneBody(this, Params, PA, Body, BoneStr);
	if (!R.bSuccess) return R;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneStr);
	Result->SetNumberField(TEXT("mass_scale"), Body->DefaultInstance.MassScale);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_SetBodyMassScale::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UPhysicsAsset* PA = nullptr; USkeletalBodySetup* Body = nullptr; FString BoneStr;
	FECACommandResult R = ECAPhysicsAssetLocal::ResolveBoneBody(this, Params, PA, Body, BoneStr);
	if (!R.bSuccess) return R;
	double MassScale = 0.0;
	if (!GetFloatParam(Params, TEXT("mass_scale"), MassScale))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mass_scale"));
	if (MassScale <= 0.0) return FECACommandResult::Error(TEXT("mass_scale must be > 0"));

	PA->Modify();
	Body->DefaultInstance.MassScale = static_cast<float>(MassScale);
	Body->MarkPackageDirty();
	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone_name"), BoneStr);
	Result->SetNumberField(TEXT("mass_scale"), Body->DefaultInstance.MassScale);
	return FECACommandResult::Success(Result);
}

// === Task 5: get_constraints ===
FECACommandResult FECACommand_GetConstraints::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA) return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (UPhysicsConstraintTemplate* CT : PA->ConstraintSetup)
	{
		if (!CT) continue;
		const FConstraintInstance& CI = CT->DefaultInstance;
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		ECAPhysicsAssetLocal::FillConstraintJson(CI, CI.GetChildBoneName(), CI.GetParentBoneName(), O);
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("constraints"), Arr);
	return FECACommandResult::Success(Result);
}

// === Task 5: add_constraint ===
FECACommandResult FECACommand_AddConstraint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, B1Str, B2Str;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	if (!GetStringParam(Params, TEXT("bone1_name"), B1Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone1_name"));
	if (!GetStringParam(Params, TEXT("bone2_name"), B2Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone2_name"));

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA) return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));

	const FName B1(*B1Str), B2(*B2Str);
	if (PA->FindBodyIndex(B1) == INDEX_NONE)
		return FECACommandResult::Error(FString::Printf(TEXT("bone1 '%s' has no body"), *B1Str));
	if (PA->FindBodyIndex(B2) == INDEX_NONE)
		return FECACommandResult::Error(FString::Printf(TEXT("bone2 '%s' has no body"), *B2Str));
	if (ECAPhysicsAssetLocal::FindConstraintByPair(PA, B1, B2) != INDEX_NONE)
		return FECACommandResult::Error(FString::Printf(TEXT("Constraint already exists between '%s' and '%s'"), *B1Str, *B2Str));

	PA->Modify();
	UPhysicsConstraintTemplate* CT = NewObject<UPhysicsConstraintTemplate>(PA, NAME_None, RF_Transactional);
	CT->DefaultInstance.JointName = B1;
	CT->DefaultInstance.ConstraintBone1 = B1;
	CT->DefaultInstance.ConstraintBone2 = B2;
	PA->ConstraintSetup.Add(CT);
	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone1_name"), B1Str);
	Result->SetStringField(TEXT("bone2_name"), B2Str);
	return FECACommandResult::Success(Result);
}

// === Task 5: set_constraint_limits (partial update) ===
FECACommandResult FECACommand_SetConstraintLimits::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, B1Str, B2Str;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	if (!GetStringParam(Params, TEXT("bone1_name"), B1Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone1_name"));
	if (!GetStringParam(Params, TEXT("bone2_name"), B2Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone2_name"));

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA) return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	const FName B1(*B1Str), B2(*B2Str);
	const int32 Idx = ECAPhysicsAssetLocal::FindConstraintByPair(PA, B1, B2);
	if (Idx == INDEX_NONE)
		return FECACommandResult::Error(FString::Printf(TEXT("No constraint between '%s' and '%s'"), *B1Str, *B2Str));
	UPhysicsConstraintTemplate* CT = PA->ConstraintSetup[Idx];
	FConstraintInstance& CI = CT->DefaultInstance;

	PA->Modify();
	FString MStr; double Deg = 0.0; EAngularConstraintMotion M;

	if (GetStringParam(Params, TEXT("swing1_motion"), MStr, false))
	{
		if (!ECAPhysicsAssetLocal::StringToMotion(MStr, M))
			return FECACommandResult::ValidationError(this, FString::Printf(TEXT("Invalid swing1_motion '%s'"), *MStr));
		CI.SetAngularSwing1Motion(M);
	}
	if (GetFloatParam(Params, TEXT("swing1_limit_degrees"), Deg, false))
		CI.SetAngularSwing1Limit(CI.GetAngularSwing1Motion(), static_cast<float>(Deg));
	if (GetStringParam(Params, TEXT("swing2_motion"), MStr, false))
	{
		if (!ECAPhysicsAssetLocal::StringToMotion(MStr, M))
			return FECACommandResult::ValidationError(this, FString::Printf(TEXT("Invalid swing2_motion '%s'"), *MStr));
		CI.SetAngularSwing2Motion(M);
	}
	if (GetFloatParam(Params, TEXT("swing2_limit_degrees"), Deg, false))
		CI.SetAngularSwing2Limit(CI.GetAngularSwing2Motion(), static_cast<float>(Deg));
	if (GetStringParam(Params, TEXT("twist_motion"), MStr, false))
	{
		if (!ECAPhysicsAssetLocal::StringToMotion(MStr, M))
			return FECACommandResult::ValidationError(this, FString::Printf(TEXT("Invalid twist_motion '%s'"), *MStr));
		CI.SetAngularTwistMotion(M);
	}
	if (GetFloatParam(Params, TEXT("twist_limit_degrees"), Deg, false))
		CI.SetAngularTwistLimit(CI.GetAngularTwistMotion(), static_cast<float>(Deg));

	ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	ECAPhysicsAssetLocal::FillConstraintJson(CI, B1, B2, Result);
	return FECACommandResult::Success(Result);
}

// === Task 5: remove_constraint ===
FECACommandResult FECACommand_RemoveConstraint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PAPath, B1Str, B2Str;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), PAPath))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	if (!GetStringParam(Params, TEXT("bone1_name"), B1Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone1_name"));
	if (!GetStringParam(Params, TEXT("bone2_name"), B2Str))
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: bone2_name"));

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(PAPath);
	if (!PA) return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *PAPath));
	const int32 Idx = ECAPhysicsAssetLocal::FindConstraintByPair(PA, FName(*B1Str), FName(*B2Str));
	bool bRemoved = false;
	if (Idx != INDEX_NONE)
	{
		PA->Modify();
		PA->ConstraintSetup.RemoveAt(Idx);
		bRemoved = true;
		ECAPhysicsAssetLocal::MarkAssetDirtyAndRefresh(PA);
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("bone1_name"), B1Str);
	Result->SetStringField(TEXT("bone2_name"), B2Str);
	Result->SetBoolField(TEXT("removed"), bRemoved);
	return FECACommandResult::Success(Result);
}
