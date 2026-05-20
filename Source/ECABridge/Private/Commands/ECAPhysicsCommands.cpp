// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPhysicsCommands.h"
#include "Commands/ECACommand.h"

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_DumpPhysicsAsset)
REGISTER_ECA_COMMAND(FECACommand_DumpPhysicalMaterial)
REGISTER_ECA_COMMAND(FECACommand_CreatePhysicalMaterial)
REGISTER_ECA_COMMAND(FECACommand_SpawnPhysicsConstraint)

namespace ECAPhysicsHelpers
{
	template<typename T>
	static T* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj) return Obj;

		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Obj = LoadObject<T>(nullptr, *FullPath);
		}
		return Obj;
	}

	template<typename TEnum>
	static FString EnumToName(int64 Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(Value);
		}
		return FString::Printf(TEXT("%lld"), Value);
	}
}

//==============================================================================
// dump_physics_asset
//==============================================================================
FECACommandResult FECACommand_DumpPhysicsAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("physics_asset_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physics_asset_path"));
	}

	UPhysicsAsset* PA = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicsAsset>(Path);
	if (!PA)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicsAsset at: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), PA->GetPathName());
	Result->SetStringField(TEXT("name"), PA->GetName());
	Result->SetNumberField(TEXT("body_count"), PA->SkeletalBodySetups.Num());
	Result->SetNumberField(TEXT("constraint_count"), PA->ConstraintSetup.Num());

	// Bodies
	TArray<TSharedPtr<FJsonValue>> Bodies;
	for (USkeletalBodySetup* Body : PA->SkeletalBodySetups)
	{
		if (!Body) continue;
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("bone_name"), Body->BoneName.ToString());

		TSharedPtr<FJsonObject> Shapes = MakeShared<FJsonObject>();
		Shapes->SetNumberField(TEXT("sphere"), Body->AggGeom.SphereElems.Num());
		Shapes->SetNumberField(TEXT("box"),    Body->AggGeom.BoxElems.Num());
		Shapes->SetNumberField(TEXT("sphyl"),  Body->AggGeom.SphylElems.Num());
		Shapes->SetNumberField(TEXT("convex"), Body->AggGeom.ConvexElems.Num());
		Shapes->SetNumberField(TEXT("tapered_capsule"), Body->AggGeom.TaperedCapsuleElems.Num());
		Shapes->SetNumberField(TEXT("total"),
			Body->AggGeom.SphereElems.Num() + Body->AggGeom.BoxElems.Num() +
			Body->AggGeom.SphylElems.Num()  + Body->AggGeom.ConvexElems.Num() +
			Body->AggGeom.TaperedCapsuleElems.Num());
		Obj->SetObjectField(TEXT("shape_summary"), Shapes);

		if (Body->PhysMaterial)
		{
			Obj->SetStringField(TEXT("phys_material"), Body->PhysMaterial->GetPathName());
		}
		Bodies.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("bodies"), Bodies);

	// Constraints
	TArray<TSharedPtr<FJsonValue>> Constraints;
	for (UPhysicsConstraintTemplate* CT : PA->ConstraintSetup)
	{
		if (!CT) continue;
		const FConstraintInstance& CI = CT->DefaultInstance;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("joint_name"), CI.JointName.ToString());
		Obj->SetStringField(TEXT("child_bone"),  CI.GetChildBoneName().ToString());
		Obj->SetStringField(TEXT("parent_bone"), CI.GetParentBoneName().ToString());
		Obj->SetNumberField(TEXT("linear_limit"), CI.GetLinearLimit());

		TSharedPtr<FJsonObject> Motions = MakeShared<FJsonObject>();
		Motions->SetStringField(TEXT("linear_x"), ECAPhysicsHelpers::EnumToName<ELinearConstraintMotion>(static_cast<int64>(CI.GetLinearXMotion())));
		Motions->SetStringField(TEXT("linear_y"), ECAPhysicsHelpers::EnumToName<ELinearConstraintMotion>(static_cast<int64>(CI.GetLinearYMotion())));
		Motions->SetStringField(TEXT("linear_z"), ECAPhysicsHelpers::EnumToName<ELinearConstraintMotion>(static_cast<int64>(CI.GetLinearZMotion())));
		Obj->SetObjectField(TEXT("motions"), Motions);

		Obj->SetNumberField(TEXT("linear_break_threshold"), CI.GetLinearBreakThreshold());

		Constraints.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("constraints"), Constraints);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_physical_material
//==============================================================================
FECACommandResult FECACommand_DumpPhysicalMaterial::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("physical_material_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: physical_material_path"));
	}

	UPhysicalMaterial* PM = ECAPhysicsHelpers::LoadAssetTolerant<UPhysicalMaterial>(Path);
	if (!PM)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UPhysicalMaterial at: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), PM->GetPathName());
	Result->SetStringField(TEXT("name"), PM->GetName());
	Result->SetNumberField(TEXT("friction"),            PM->Friction);
	Result->SetNumberField(TEXT("static_friction"),     PM->StaticFriction);
	Result->SetNumberField(TEXT("restitution"),         PM->Restitution);
	Result->SetNumberField(TEXT("density"),             PM->Density);
	Result->SetNumberField(TEXT("raise_mass_to_power"), PM->RaiseMassToPower);
	Result->SetBoolField  (TEXT("override_friction_combine_mode"),    PM->bOverrideFrictionCombineMode);
	Result->SetBoolField  (TEXT("override_restitution_combine_mode"), PM->bOverrideRestitutionCombineMode);
	Result->SetStringField(TEXT("friction_combine_mode"),    ECAPhysicsHelpers::EnumToName<EFrictionCombineMode::Type>(static_cast<int64>(PM->FrictionCombineMode)));
	Result->SetStringField(TEXT("restitution_combine_mode"), ECAPhysicsHelpers::EnumToName<EFrictionCombineMode::Type>(static_cast<int64>(PM->RestitutionCombineMode)));
	Result->SetStringField(TEXT("surface_type"),             ECAPhysicsHelpers::EnumToName<EPhysicalSurface>(static_cast<int64>(PM->SurfaceType.GetValue())));

	return FECACommandResult::Success(Result);
}

//==============================================================================
// create_physical_material
//==============================================================================
FECACommandResult FECACommand_CreatePhysicalMaterial::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	FString Name;
	if (!GetStringParam(Params, TEXT("path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: path"));
	}
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}

	double Friction = 0.7;
	double Restitution = 0.3;
	double Density = 1.0;
	bool bOverwrite = false;
	GetFloatParam(Params, TEXT("friction"),    Friction,    false);
	GetFloatParam(Params, TEXT("restitution"), Restitution, false);
	GetFloatParam(Params, TEXT("density"),     Density,     false);
	GetBoolParam (Params, TEXT("overwrite"),   bOverwrite,  false);

	if (!Path.EndsWith(TEXT("/"))) { Path += TEXT("/"); }
	const FString PackagePath = Path + Name;

	UPhysicalMaterial* ExistingPM = nullptr;
	if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *PackagePath))
	{
		ExistingPM = Cast<UPhysicalMaterial>(Existing);
		if (!ExistingPM)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Asset exists at '%s' but is %s, not UPhysicalMaterial"), *PackagePath, *Existing->GetClass()->GetName()));
		}
		if (!bOverwrite)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("UPhysicalMaterial already exists at '%s'. Pass overwrite=true to update."), *PackagePath));
		}
	}

	UPhysicalMaterial* PM = ExistingPM;
	if (!PM)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return FECACommandResult::Error(TEXT("Failed to create package"));
		}
		PM = NewObject<UPhysicalMaterial>(Package, *Name, RF_Public | RF_Standalone);
		if (!PM)
		{
			return FECACommandResult::Error(TEXT("Failed to create UPhysicalMaterial"));
		}
		FAssetRegistryModule::AssetCreated(PM);
	}

	PM->Friction        = static_cast<float>(Friction);
	PM->StaticFriction  = static_cast<float>(Friction);
	PM->Restitution     = static_cast<float>(Restitution);
	PM->Density         = static_cast<float>(Density);
	PM->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), PM->GetPathName());
	Result->SetBoolField  (TEXT("created"), ExistingPM == nullptr);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// spawn_physics_constraint
//==============================================================================
FECACommandResult FECACommand_SpawnPhysicsConstraint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	FString Actor1Name, Actor2Name;
	if (!GetStringParam(Params, TEXT("actor1_name"), Actor1Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: actor1_name"));
	}
	if (!GetStringParam(Params, TEXT("actor2_name"), Actor2Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: actor2_name"));
	}

	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: location"));
	}

	AActor* A1 = FindActorByName(Actor1Name);
	AActor* A2 = FindActorByName(Actor2Name);
	if (!A1) return FECACommandResult::Error(FString::Printf(TEXT("actor1 '%s' not found"), *Actor1Name));
	if (!A2) return FECACommandResult::Error(FString::Printf(TEXT("actor2 '%s' not found"), *Actor2Name));

	FString Label;
	GetStringParam(Params, TEXT("label"), Label, false);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APhysicsConstraintActor* Constraint = World->SpawnActor<APhysicsConstraintActor>(APhysicsConstraintActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	if (!Constraint)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn APhysicsConstraintActor"));
	}

	if (!Label.IsEmpty())
	{
		Constraint->SetActorLabel(Label);
	}

	if (UPhysicsConstraintComponent* CC = Constraint->GetConstraintComp())
	{
		CC->ConstraintActor1 = A1;
		CC->ConstraintActor2 = A2;
		CC->SetConstrainedComponents(
			A1->GetRootComponent() ? Cast<UPrimitiveComponent>(A1->GetRootComponent()) : nullptr,
			NAME_None,
			A2->GetRootComponent() ? Cast<UPrimitiveComponent>(A2->GetRootComponent()) : nullptr,
			NAME_None);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("constraint_actor"), Constraint->GetPathName());
	Result->SetStringField(TEXT("actor1"), A1->GetPathName());
	Result->SetStringField(TEXT("actor2"), A2->GetPathName());
	return FECACommandResult::Success(Result);
}
