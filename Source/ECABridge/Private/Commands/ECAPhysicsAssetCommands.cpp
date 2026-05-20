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
// Tasks 3-5 declared in header; implementations land in subsequent commits.
// Stubs below satisfy linker for the not-yet-registered classes.
// =============================================================================
#define ECA_TASK_STUB(ClassName) \
	FECACommandResult ClassName::Execute(const TSharedPtr<FJsonObject>&) { \
		return FECACommandResult::Error(TEXT(#ClassName " not implemented yet")); }

ECA_TASK_STUB(FECACommand_SetBodySphere)
ECA_TASK_STUB(FECACommand_SetBodyCapsule)
ECA_TASK_STUB(FECACommand_SetBodyBox)
ECA_TASK_STUB(FECACommand_RemoveBodyShape)
ECA_TASK_STUB(FECACommand_GetBodyPhysicsMode)
ECA_TASK_STUB(FECACommand_SetBodyPhysicsMode)
ECA_TASK_STUB(FECACommand_GetBodyMassScale)
ECA_TASK_STUB(FECACommand_SetBodyMassScale)
ECA_TASK_STUB(FECACommand_GetConstraints)
ECA_TASK_STUB(FECACommand_AddConstraint)
ECA_TASK_STUB(FECACommand_SetConstraintLimits)
ECA_TASK_STUB(FECACommand_RemoveConstraint)
#undef ECA_TASK_STUB
