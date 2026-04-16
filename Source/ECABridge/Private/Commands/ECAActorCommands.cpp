// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAActorCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "EditorActorFolders.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Components/PrimitiveComponent.h"

//------------------------------------------------------------------------------
// Spatial Verification Helpers
//------------------------------------------------------------------------------

namespace SpatialHelpers
{
	// Convert FBox to JSON
	TSharedPtr<FJsonObject> BoxToJson(const FBox& Box)
	{
		TSharedPtr<FJsonObject> BoxJson = MakeShared<FJsonObject>();
		
		TSharedPtr<FJsonObject> MinJson = MakeShared<FJsonObject>();
		MinJson->SetNumberField(TEXT("x"), Box.Min.X);
		MinJson->SetNumberField(TEXT("y"), Box.Min.Y);
		MinJson->SetNumberField(TEXT("z"), Box.Min.Z);
		BoxJson->SetObjectField(TEXT("min"), MinJson);
		
		TSharedPtr<FJsonObject> MaxJson = MakeShared<FJsonObject>();
		MaxJson->SetNumberField(TEXT("x"), Box.Max.X);
		MaxJson->SetNumberField(TEXT("y"), Box.Max.Y);
		MaxJson->SetNumberField(TEXT("z"), Box.Max.Z);
		BoxJson->SetObjectField(TEXT("max"), MaxJson);
		
		// Add size for convenience
		FVector Size = Box.GetSize();
		TSharedPtr<FJsonObject> SizeJson = MakeShared<FJsonObject>();
		SizeJson->SetNumberField(TEXT("x"), Size.X);
		SizeJson->SetNumberField(TEXT("y"), Size.Y);
		SizeJson->SetNumberField(TEXT("z"), Size.Z);
		BoxJson->SetObjectField(TEXT("size"), SizeJson);
		
		// Add center
		FVector Center = Box.GetCenter();
		TSharedPtr<FJsonObject> CenterJson = MakeShared<FJsonObject>();
		CenterJson->SetNumberField(TEXT("x"), Center.X);
		CenterJson->SetNumberField(TEXT("y"), Center.Y);
		CenterJson->SetNumberField(TEXT("z"), Center.Z);
		BoxJson->SetObjectField(TEXT("center"), CenterJson);
		
		return BoxJson;
	}
	
	// Get spatial verification data for an actor
	TSharedPtr<FJsonObject> GetSpatialData(AActor* Actor, UWorld* World)
	{
		if (!Actor || !World)
		{
			return nullptr;
		}
		
		TSharedPtr<FJsonObject> SpatialJson = MakeShared<FJsonObject>();
		
		// World bounds
		FBox WorldBounds = Actor->GetComponentsBoundingBox(true);
		if (WorldBounds.IsValid)
		{
			SpatialJson->SetObjectField(TEXT("world_bounds"), BoxToJson(WorldBounds));
		}
		
		// Distance to ground (trace down from actor origin)
		FVector ActorLocation = Actor->GetActorLocation();
		FHitResult HitResult;
		FVector TraceStart = ActorLocation;
		FVector TraceEnd = ActorLocation - FVector(0, 0, 100000); // Trace 1km down
		
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Actor);
		
		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
		{
			double DistanceToGround = FVector::Dist(ActorLocation, HitResult.Location);
			SpatialJson->SetNumberField(TEXT("distance_to_ground"), DistanceToGround);
			
			// Ground hit info
			TSharedPtr<FJsonObject> GroundJson = MakeShared<FJsonObject>();
			GroundJson->SetNumberField(TEXT("x"), HitResult.Location.X);
			GroundJson->SetNumberField(TEXT("y"), HitResult.Location.Y);
			GroundJson->SetNumberField(TEXT("z"), HitResult.Location.Z);
			if (HitResult.GetActor())
			{
				GroundJson->SetStringField(TEXT("hit_actor"), HitResult.GetActor()->GetActorLabel());
			}
			SpatialJson->SetObjectField(TEXT("ground_point"), GroundJson);
		}
		else
		{
			SpatialJson->SetNumberField(TEXT("distance_to_ground"), -1); // No ground found
		}
		
		// Check for overlapping actors
		TArray<TSharedPtr<FJsonValue>> OverlapsArray;
		TArray<AActor*> OverlappingActors;
		Actor->GetOverlappingActors(OverlappingActors);
		
		for (AActor* OtherActor : OverlappingActors)
		{
			if (OtherActor && OtherActor != Actor)
			{
				TSharedPtr<FJsonObject> OverlapJson = MakeShared<FJsonObject>();
				OverlapJson->SetStringField(TEXT("actor"), OtherActor->GetActorLabel());
				OverlapJson->SetStringField(TEXT("class"), OtherActor->GetClass()->GetName());
				OverlapsArray.Add(MakeShared<FJsonValueObject>(OverlapJson));
			}
		}
		SpatialJson->SetArrayField(TEXT("overlapping_actors"), OverlapsArray);
		
		// Find nearby actors within a radius
		TArray<TSharedPtr<FJsonValue>> NearbyArray;
		const float NearbyRadius = 500.0f; // 5 meters
		
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* OtherActor = *It;
			if (OtherActor && OtherActor != Actor && !OtherActor->IsHiddenEd())
			{
				float Distance = FVector::Dist(ActorLocation, OtherActor->GetActorLocation());
				if (Distance <= NearbyRadius)
				{
					TSharedPtr<FJsonObject> NearbyJson = MakeShared<FJsonObject>();
					NearbyJson->SetStringField(TEXT("actor"), OtherActor->GetActorLabel());
					NearbyJson->SetNumberField(TEXT("distance"), Distance);
					NearbyArray.Add(MakeShared<FJsonValueObject>(NearbyJson));
				}
			}
		}
		SpatialJson->SetArrayField(TEXT("nearby_actors"), NearbyArray);
		
		return SpatialJson;
	}
}

// Register all actor commands
REGISTER_ECA_COMMAND(FECACommand_GetActorsInLevel)
REGISTER_ECA_COMMAND(FECACommand_CreateActor)
REGISTER_ECA_COMMAND(FECACommand_DeleteActor)
REGISTER_ECA_COMMAND(FECACommand_SetActorTransform)
REGISTER_ECA_COMMAND(FECACommand_GetActorProperties)
REGISTER_ECA_COMMAND(FECACommand_SetActorProperty)
REGISTER_ECA_COMMAND(FECACommand_FindActors)
REGISTER_ECA_COMMAND(FECACommand_DuplicateActor)


//------------------------------------------------------------------------------
// GetActorsInLevel
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetActorsInLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	FString ClassFilter;
	bool bIncludeHidden = false;
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, false);
	GetBoolParam(Params, TEXT("include_hidden"), bIncludeHidden, false);
	
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		// Skip hidden actors unless requested
		if (!bIncludeHidden && Actor->IsHiddenEd())
		{
			continue;
		}
		
		// Apply class filter
		if (!ClassFilter.IsEmpty())
		{
			FString ActorClassName = Actor->GetClass()->GetName();
			if (!ActorClassName.Contains(ClassFilter))
			{
				continue;
			}
		}
		
		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorJson->SetStringField(TEXT("internal_name"), Actor->GetName());
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorJson->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
		ActorJson->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
		ActorJson->SetObjectField(TEXT("scale"), VectorToJson(Actor->GetActorScale3D()));
		ActorJson->SetBoolField(TEXT("is_hidden"), Actor->IsHiddenEd());
		
		// Include folder path
		FName FolderPath = Actor->GetFolderPath();
		if (!FolderPath.IsNone())
		{
			ActorJson->SetStringField(TEXT("folder"), FolderPath.ToString());
		}
		
		// Include tags
		if (Actor->Tags.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TagsArray;
			for (const FName& Tag : Actor->Tags)
			{
				TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ActorJson->SetArrayField(TEXT("tags"), TagsArray);
		}
		
		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// CreateActor
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorType;
	if (!GetStringParam(Params, TEXT("actor_type"), ActorType))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_type"));
	}
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	// Parse transform parameters
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	
	GetVectorParam(Params, TEXT("location"), Location, false);
	GetRotatorParam(Params, TEXT("rotation"), Rotation, false);
	GetVectorParam(Params, TEXT("scale"), Scale, false);
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AActor* NewActor = nullptr;
	
	// Handle built-in actor types
	if (ActorType.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase))
	{
		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
		if (MeshActor)
		{
			FString MeshPath;
			if (GetStringParam(Params, TEXT("mesh"), MeshPath, false))
			{
				UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
				if (Mesh && MeshActor->GetStaticMeshComponent())
				{
					MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
				}
			}
			else
			{
				// Default cube
				UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				if (CubeMesh && MeshActor->GetStaticMeshComponent())
				{
					MeshActor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
				}
			}
		}
		NewActor = MeshActor;
	}
	else if (ActorType.Equals(TEXT("PointLight"), ESearchCase::IgnoreCase))
	{
		NewActor = World->SpawnActor<APointLight>(Location, Rotation, SpawnParams);
	}
	else if (ActorType.Equals(TEXT("SpotLight"), ESearchCase::IgnoreCase))
	{
		NewActor = World->SpawnActor<ASpotLight>(Location, Rotation, SpawnParams);
	}
	else if (ActorType.Equals(TEXT("DirectionalLight"), ESearchCase::IgnoreCase))
	{
		NewActor = World->SpawnActor<ADirectionalLight>(Location, Rotation, SpawnParams);
	}
	else if (ActorType.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase))
	{
		NewActor = World->SpawnActor<ACameraActor>(Location, Rotation, SpawnParams);
	}
	else
	{
		// Try to find class by path
		UClass* ActorClass = LoadObject<UClass>(nullptr, *ActorType);
		if (!ActorClass)
		{
			// Try as a short name
			ActorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ActorType));
		}
		
		if (ActorClass && ActorClass->IsChildOf(AActor::StaticClass()))
		{
			NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
		}
	}
	
	if (!NewActor)
	{
		return FECACommandResult::Error(TEXT("Failed to create actor"));
	}
	
	// Apply scale
	NewActor->SetActorScale3D(Scale);
	
	// Set name if specified
	FString ActorName;
	if (GetStringParam(Params, TEXT("name"), ActorName, false))
	{
		NewActor->SetActorLabel(*ActorName);
	}
	
	// Put in folder if specified
	FString FolderPath;
	if (GetStringParam(Params, TEXT("folder"), FolderPath, false))
	{
		NewActor->SetFolderPath(*FolderPath);
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), NewActor->GetActorLabel());
	Result->SetStringField(TEXT("internal_name"), NewActor->GetName());
	Result->SetStringField(TEXT("class"), NewActor->GetClass()->GetName());
	Result->SetObjectField(TEXT("transform"), TransformToJson(NewActor->GetTransform()));
	
	// Add spatial verification data
	TSharedPtr<FJsonObject> SpatialData = SpatialHelpers::GetSpatialData(NewActor, World);
	if (SpatialData.IsValid())
	{
		Result->SetObjectField(TEXT("spatial"), SpatialData);
	}
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// DeleteActor
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DeleteActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}
	
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}
	
	FString DeletedName = Actor->GetActorLabel();
	Actor->Destroy();
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("deleted_actor"), DeletedName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetActorTransform
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetActorTransform::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}
	
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}
	
	// Check if actor has a root component
	USceneComponent* RootComp = Actor->GetRootComponent();
	if (!RootComp)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no root component"), *ActorName));
	}
	
	// Get current transform as base
	FVector NewLocation = Actor->GetActorLocation();
	FRotator NewRotation = Actor->GetActorRotation();
	FVector NewScale = Actor->GetActorScale3D();
	
	bool bLocationProvided = false;
	bool bRotationProvided = false;
	bool bScaleProvided = false;
	
	// Get new values if provided
	FVector Location;
	if (GetVectorParam(Params, TEXT("location"), Location, false))
	{
		NewLocation = Location;
		bLocationProvided = true;
	}
	
	FRotator Rotation;
	if (GetRotatorParam(Params, TEXT("rotation"), Rotation, false))
	{
		NewRotation = Rotation;
		bRotationProvided = true;
	}
	
	FVector Scale;
	if (GetVectorParam(Params, TEXT("scale"), Scale, false))
	{
		NewScale = Scale;
		bScaleProvided = true;
	}
	
	// For editor operations, we need to use Modify() to properly record undo
	// and notify the editor of changes - this is how the viewport transform tools work
	RootComp->Modify();
	Actor->Modify();
	
	// Set transform values directly on the root component
	// Using SetRelativeLocation_Direct etc. bypasses mobility checks
	// This is the same approach used by the editor's transform gizmo
	if (bLocationProvided)
	{
		RootComp->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
	}
	
	if (bRotationProvided)
	{
		RootComp->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::ResetPhysics);
	}
	
	if (bScaleProvided)
	{
		RootComp->SetWorldScale3D(NewScale);
	}
	
	// Force update transform - crucial for static meshes
	RootComp->UpdateComponentToWorld();
	
	// Notify that the actor has been moved (triggers editor updates)
	GEditor->BroadcastLevelActorListChanged();
	Actor->PostEditMove(true);
	
	// Mark package dirty so changes can be saved
	Actor->MarkPackageDirty();
	
	// Refresh all viewports to show the change
	GEditor->RedrawAllViewports();
	
	// Verify the transform was actually applied
	FVector FinalLocation = Actor->GetActorLocation();
	FRotator FinalRotation = Actor->GetActorRotation();
	FVector FinalScale = Actor->GetActorScale3D();
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetTransform()));
	Result->SetBoolField(TEXT("success"), true);
	
	// Report what was changed
	TArray<FString> ChangedFields;
	if (bLocationProvided) ChangedFields.Add(TEXT("location"));
	if (bRotationProvided) ChangedFields.Add(TEXT("rotation"));
	if (bScaleProvided) ChangedFields.Add(TEXT("scale"));
	Result->SetStringField(TEXT("changed"), FString::Join(ChangedFields, TEXT(", ")));
	
	// Verify transform was applied (compare requested vs actual)
	bool bLocationMatch = !bLocationProvided || FinalLocation.Equals(NewLocation, 0.01f);
	bool bRotationMatch = !bRotationProvided || FinalRotation.Equals(NewRotation, 0.01f);
	bool bScaleMatch = !bScaleProvided || FinalScale.Equals(NewScale, 0.01f);
	
	if (!bLocationMatch || !bRotationMatch || !bScaleMatch)
	{
		Result->SetStringField(TEXT("warning"), TEXT("Transform may not have been fully applied - check mobility or constraints"));
	}
	
	// Add spatial verification data
	UWorld* World = GetEditorWorld();
	TSharedPtr<FJsonObject> SpatialData = SpatialHelpers::GetSpatialData(Actor, World);
	if (SpatialData.IsValid())
	{
		Result->SetObjectField(TEXT("spatial"), SpatialData);
	}
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetActorProperties
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetActorProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}
	
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("internal_name"), Actor->GetName());
	Result->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Result->SetStringField(TEXT("class_path"), Actor->GetClass()->GetPathName());
	Result->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetTransform()));
	Result->SetBoolField(TEXT("is_hidden"), Actor->IsHiddenEd());
	
	// Mobility
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (RootComponent)
	{
		FString Mobility;
		switch (RootComponent->Mobility)
		{
			case EComponentMobility::Static: Mobility = TEXT("Static"); break;
			case EComponentMobility::Stationary: Mobility = TEXT("Stationary"); break;
			case EComponentMobility::Movable: Mobility = TEXT("Movable"); break;
		}
		Result->SetStringField(TEXT("mobility"), Mobility);
	}
	
	// Folder
	FName FolderPath = Actor->GetFolderPath();
	if (!FolderPath.IsNone())
	{
		Result->SetStringField(TEXT("folder"), FolderPath.ToString());
	}
	
	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Actor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Result->SetArrayField(TEXT("tags"), TagsArray);
	
	// Components
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Component->GetName());
		CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}
	Result->SetArrayField(TEXT("components"), ComponentsArray);
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetActorProperty
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetActorProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Support both actor_name (legacy) and actor_path (new)
	FString ActorName;
	FString ActorPath;
	AActor* Actor = nullptr;
	
	if (GetStringParam(Params, TEXT("actor_path"), ActorPath, false))
	{
		Actor = FindObject<AActor>(nullptr, *ActorPath);
		if (!Actor)
		{
			Actor = LoadObject<AActor>(nullptr, *ActorPath);
		}
	}
	else if (GetStringParam(Params, TEXT("actor_name"), ActorName, false))
	{
		Actor = FindActorByName(ActorName);
	}
	else
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_path or actor_name"));
	}
	
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), ActorPath.IsEmpty() ? *ActorName : *ActorPath));
	}
	
	FString PropertyName;
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_name"));
	}
	
	// Optional: target a specific component
	FString ComponentName;
	UObject* TargetObject = Actor;
	if (GetStringParam(Params, TEXT("component_name"), ComponentName, false))
	{
		UActorComponent* Component = Actor->FindComponentByClass<UActorComponent>();
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
				Comp->GetClass()->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				TargetObject = Comp;
				break;
			}
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("property_name"), PropertyName);
	
	// Handle special properties first
	if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
	{
		FString Value;
		if (GetStringParam(Params, TEXT("property_value"), Value))
		{
			USceneComponent* RootComponent = Actor->GetRootComponent();
			if (RootComponent)
			{
				if (Value.Equals(TEXT("Static"), ESearchCase::IgnoreCase))
				{
					RootComponent->SetMobility(EComponentMobility::Static);
				}
				else if (Value.Equals(TEXT("Stationary"), ESearchCase::IgnoreCase))
				{
					RootComponent->SetMobility(EComponentMobility::Stationary);
				}
				else if (Value.Equals(TEXT("Movable"), ESearchCase::IgnoreCase))
				{
					RootComponent->SetMobility(EComponentMobility::Movable);
				}
				Result->SetStringField(TEXT("new_value"), Value);
			}
		}
		return FECACommandResult::Success(Result);
	}
	
	if (PropertyName.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
	{
		bool bHidden;
		if (GetBoolParam(Params, TEXT("property_value"), bHidden))
		{
			Actor->SetIsTemporarilyHiddenInEditor(bHidden);
			Result->SetBoolField(TEXT("new_value"), bHidden);
		}
		return FECACommandResult::Success(Result);
	}
	
	if (PropertyName.Equals(TEXT("Folder"), ESearchCase::IgnoreCase))
	{
		FString FolderPath;
		if (GetStringParam(Params, TEXT("property_value"), FolderPath))
		{
			Actor->SetFolderPath(*FolderPath);
			Result->SetStringField(TEXT("new_value"), FolderPath);
		}
		return FECACommandResult::Success(Result);
	}
	
	// Handle StaticMesh property specially (common case)
	if (PropertyName.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase))
	{
		FString MeshPath;
		if (!GetStringParam(Params, TEXT("property_value"), MeshPath))
		{
			return FECACommandResult::Error(TEXT("Missing property_value for StaticMesh"));
		}
		
		UStaticMesh* NewMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
		if (!NewMesh)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath));
		}
		
		UStaticMeshComponent* MeshComp = nullptr;
		if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor))
		{
			MeshComp = SMActor->GetStaticMeshComponent();
		}
		else
		{
			MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		}
		
		if (!MeshComp)
		{
			return FECACommandResult::Error(TEXT("Actor has no StaticMeshComponent"));
		}
		
		FString OldPath = MeshComp->GetStaticMesh() ? MeshComp->GetStaticMesh()->GetPathName() : TEXT("None");
		MeshComp->SetStaticMesh(NewMesh);
		
		Result->SetStringField(TEXT("old_value"), OldPath);
		Result->SetStringField(TEXT("new_value"), NewMesh->GetPathName());
		return FECACommandResult::Success(Result);
	}
	
	// Handle Material property
	if (PropertyName.Equals(TEXT("Material"), ESearchCase::IgnoreCase) || PropertyName.StartsWith(TEXT("Material[")))
	{
		FString MaterialPath;
		if (!GetStringParam(Params, TEXT("property_value"), MaterialPath))
		{
			return FECACommandResult::Error(TEXT("Missing property_value for Material"));
		}
		
		int32 SlotIndex = 0;
		if (PropertyName.StartsWith(TEXT("Material[")))
		{
			FString IndexStr = PropertyName.Mid(9);
			IndexStr = IndexStr.LeftChop(1);  // Remove trailing ]
			SlotIndex = FCString::Atoi(*IndexStr);
		}
		
		UMaterialInterface* NewMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!NewMaterial)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
		}
		
		UPrimitiveComponent* PrimComp = Actor->FindComponentByClass<UPrimitiveComponent>();
		if (!PrimComp)
		{
			return FECACommandResult::Error(TEXT("Actor has no PrimitiveComponent"));
		}
		
		UMaterialInterface* OldMaterial = PrimComp->GetMaterial(SlotIndex);
		FString OldPath = OldMaterial ? OldMaterial->GetPathName() : TEXT("None");
		PrimComp->SetMaterial(SlotIndex, NewMaterial);
		
		Result->SetStringField(TEXT("old_value"), OldPath);
		Result->SetStringField(TEXT("new_value"), NewMaterial->GetPathName());
		Result->SetNumberField(TEXT("slot_index"), SlotIndex);
		return FECACommandResult::Success(Result);
	}
	
	// Generic property setting via UE reflection
	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// Try case-insensitive search
		for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				Property = *It;
				break;
			}
		}
	}
	
	if (!Property)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}
	
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
	
	// Handle different property types
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		double Value;
		if (GetFloatParam(Params, TEXT("property_value"), Value))
		{
			if (NumericProp->IsInteger())
			{
				NumericProp->SetIntPropertyValue(ValuePtr, (int64)Value);
			}
			else
			{
				NumericProp->SetFloatingPointPropertyValue(ValuePtr, Value);
			}
			Result->SetNumberField(TEXT("new_value"), Value);
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value;
		if (GetBoolParam(Params, TEXT("property_value"), Value))
		{
			BoolProp->SetPropertyValue(ValuePtr, Value);
			Result->SetBoolField(TEXT("new_value"), Value);
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString Value;
		if (GetStringParam(Params, TEXT("property_value"), Value))
		{
			StrProp->SetPropertyValue(ValuePtr, Value);
			Result->SetStringField(TEXT("new_value"), Value);
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString Value;
		if (GetStringParam(Params, TEXT("property_value"), Value))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*Value));
			Result->SetStringField(TEXT("new_value"), Value);
		}
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		FString ObjectPath;
		if (GetStringParam(Params, TEXT("property_value"), ObjectPath))
		{
			UObject* NewObject = LoadObject<UObject>(nullptr, *ObjectPath);
			if (NewObject)
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, NewObject);
				Result->SetStringField(TEXT("new_value"), NewObject->GetPathName());
			}
			else
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Object not found: %s"), *ObjectPath));
			}
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// Handle common struct types
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			FVector Value;
			if (GetVectorParam(Params, TEXT("property_value"), Value))
			{
				*StructProp->ContainerPtrToValuePtr<FVector>(TargetObject) = Value;
				Result->SetObjectField(TEXT("new_value"), VectorToJson(Value));
			}
		}
		else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			FRotator Value;
			if (GetRotatorParam(Params, TEXT("property_value"), Value))
			{
				*StructProp->ContainerPtrToValuePtr<FRotator>(TargetObject) = Value;
				Result->SetObjectField(TEXT("new_value"), RotatorToJson(Value));
			}
		}
		else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			FVector Value;
			if (GetVectorParam(Params, TEXT("property_value"), Value))
			{
				FLinearColor Color(Value.X, Value.Y, Value.Z, 1.0f);
				*StructProp->ContainerPtrToValuePtr<FLinearColor>(TargetObject) = Color;
			}
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Struct type not supported: %s"), *StructProp->Struct->GetName()));
		}
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property type not supported: %s"), *Property->GetClass()->GetName()));
	}
	
	// Notify that property changed
	TargetObject->PostEditChange();
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// FindActors
//------------------------------------------------------------------------------

FECACommandResult FECACommand_FindActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	FString NamePattern;
	FString ClassName;
	FString Tag;
	FVector NearLocation;
	double Radius = 1000.0;
	
	bool bHasNamePattern = GetStringParam(Params, TEXT("name_pattern"), NamePattern, false);
	bool bHasClassName = GetStringParam(Params, TEXT("class_name"), ClassName, false);
	bool bHasTag = GetStringParam(Params, TEXT("tag"), Tag, false);
	bool bHasLocation = GetVectorParam(Params, TEXT("near_location"), NearLocation, false);
	GetFloatParam(Params, TEXT("radius"), Radius, false);
	
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		bool bMatch = true;
		
		// Name pattern filter - check both label and object name
		if (bHasNamePattern && bMatch)
		{
			FString ActorLabel = Actor->GetActorLabel();
			FString ActorObjName = Actor->GetName();
			bMatch = ActorLabel.MatchesWildcard(NamePattern, ESearchCase::IgnoreCase) || 
			         ActorObjName.MatchesWildcard(NamePattern, ESearchCase::IgnoreCase);
		}
		
		// Class filter
		if (bHasClassName && bMatch)
		{
			FString ActorClassName = Actor->GetClass()->GetName();
			bMatch = ActorClassName.Contains(ClassName);
		}
		
		// Tag filter
		if (bHasTag && bMatch)
		{
			bMatch = Actor->Tags.Contains(FName(*Tag));
		}
		
		// Location filter
		if (bHasLocation && bMatch)
		{
			double Distance = FVector::Dist(Actor->GetActorLocation(), NearLocation);
			bMatch = Distance <= Radius;
		}
		
		if (bMatch)
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorJson->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// DuplicateActor
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DuplicateActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}
	
	AActor* SourceActor = FindActorByName(ActorName);
	if (!SourceActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	// Get offset
	FVector Offset = FVector(100, 0, 0); // Default offset
	GetVectorParam(Params, TEXT("offset"), Offset, false);
	
	// Duplicate using editor subsystem
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return FECACommandResult::Error(TEXT("EditorActorSubsystem not available"));
	}
	
	AActor* NewActor = EditorActorSubsystem->DuplicateActor(SourceActor, World);
	if (!NewActor)
	{
		return FECACommandResult::Error(TEXT("Failed to duplicate actor"));
	}
	
	// Apply offset
	NewActor->SetActorLocation(SourceActor->GetActorLocation() + Offset);
	
	// Set new name if specified
	FString NewName;
	if (GetStringParam(Params, TEXT("new_name"), NewName, false))
	{
		NewActor->SetActorLabel(*NewName);
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), NewActor->GetActorLabel());
	Result->SetStringField(TEXT("internal_name"), NewActor->GetName());
	Result->SetObjectField(TEXT("transform"), TransformToJson(NewActor->GetTransform()));
	return FECACommandResult::Success(Result);
}


