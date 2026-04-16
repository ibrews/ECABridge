// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANavigationCommands.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AIController.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"

// ─── REGISTER ──────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_BuildNavigation);
REGISTER_ECA_COMMAND(FECACommand_FindPath);
REGISTER_ECA_COMMAND(FECACommand_MoveActorTo);
REGISTER_ECA_COMMAND(FECACommand_GetNavMeshInfo);
REGISTER_ECA_COMMAND(FECACommand_AddActorTag);
REGISTER_ECA_COMMAND(FECACommand_RemoveActorTag);
REGISTER_ECA_COMMAND(FECACommand_FindActorsByTag);
REGISTER_ECA_COMMAND(FECACommand_GetActorHierarchy);
REGISTER_ECA_COMMAND(FECACommand_SetActorMobility);

// ─── build_navigation ─────────────────────────────────────────

FECACommandResult FECACommand_BuildNavigation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
		return FECACommandResult::Error(TEXT("No NavigationSystemV1 found. Make sure the level has a NavMeshBoundsVolume."));

	// Optionally update agent properties before building
	double AgentRadius = 0.0, AgentHeight = 0.0;
	bool bHasAgentRadius = GetFloatParam(Params, TEXT("agent_radius"), AgentRadius, false);
	bool bHasAgentHeight = GetFloatParam(Params, TEXT("agent_height"), AgentHeight, false);

	if (bHasAgentRadius || bHasAgentHeight)
	{
		ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavData);
		if (RecastNavMesh)
		{
			if (bHasAgentRadius)
			{
				RecastNavMesh->AgentRadius = static_cast<float>(AgentRadius);
			}
			if (bHasAgentHeight)
			{
				RecastNavMesh->AgentHeight = static_cast<float>(AgentHeight);
			}
		}
	}

	// Build the navigation mesh
	NavSys->Build();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("built"), true);

	// Report nav data info after build
	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (NavData)
	{
		Result->SetStringField(TEXT("nav_data_class"), NavData->GetClass()->GetName());

		ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavData);
		if (RecastNavMesh)
		{
			Result->SetNumberField(TEXT("agent_radius"), RecastNavMesh->AgentRadius);
			Result->SetNumberField(TEXT("agent_height"), RecastNavMesh->AgentHeight);
		}
	}

	// Check if nav is actually built
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	bool bIsBuilt = NavSys->IsNavigationBuilt(WorldSettings);
	Result->SetBoolField(TEXT("is_navigation_built"), bIsBuilt);

	return FECACommandResult::Success(Result);
}

// ─── find_path ────────────────────────────────────────────────

FECACommandResult FECACommand_FindPath::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FVector StartLoc, EndLoc;
	if (!GetVectorParam(Params, TEXT("start"), StartLoc))
		return FECACommandResult::Error(TEXT("Missing required parameter: start"));
	if (!GetVectorParam(Params, TEXT("end"), EndLoc))
		return FECACommandResult::Error(TEXT("Missing required parameter: end"));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
		return FECACommandResult::Error(TEXT("No NavigationSystemV1 found. Ensure a NavMeshBoundsVolume exists and navigation is built."));

	// Use the synchronous path-finding API
	UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, StartLoc, EndLoc);
	if (!NavPath)
		return FECACommandResult::Error(TEXT("Path finding failed — returned null"));

	if (!NavPath->IsValid() || NavPath->PathPoints.Num() == 0)
		return FECACommandResult::Error(FString::Printf(
			TEXT("No valid path found from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f). Check that both points are within the NavMesh."),
			StartLoc.X, StartLoc.Y, StartLoc.Z, EndLoc.X, EndLoc.Y, EndLoc.Z));

	TSharedPtr<FJsonObject> Result = MakeResult();

	// Path points
	TArray<TSharedPtr<FJsonValue>> PointsArray;
	for (const FVector& Point : NavPath->PathPoints)
	{
		PointsArray.Add(MakeShared<FJsonValueObject>(VectorToJson(Point)));
	}
	Result->SetArrayField(TEXT("path_points"), PointsArray);
	Result->SetNumberField(TEXT("point_count"), NavPath->PathPoints.Num());
	Result->SetNumberField(TEXT("path_length"), NavPath->GetPathLength());
	Result->SetNumberField(TEXT("path_cost"), NavPath->GetPathCost());
	Result->SetBoolField(TEXT("is_partial"), NavPath->IsPartial());

	return FECACommandResult::Success(Result);
}

// ─── move_actor_to ────────────────────────────────────────────

FECACommandResult FECACommand_MoveActorTo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	FVector Destination;
	double Speed = 300.0;

	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetVectorParam(Params, TEXT("destination"), Destination))
		return FECACommandResult::Error(TEXT("Missing required parameter: destination"));
	GetFloatParam(Params, TEXT("speed"), Speed, false);

	if (Speed <= 0.0)
		return FECACommandResult::Error(TEXT("speed must be a positive number"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
		return FECACommandResult::Error(TEXT("No NavigationSystemV1 found. Ensure a NavMeshBoundsVolume exists and navigation is built."));

	// The actor should be a Pawn for AI movement
	APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn)
		return FECACommandResult::Error(FString::Printf(
			TEXT("Actor '%s' (class: %s) is not a Pawn. move_actor_to requires a Pawn with a controller. Consider using find_path instead and moving the actor manually."),
			*ActorName, *Actor->GetClass()->GetName()));

	// Set movement speed on known movement components
	UCharacterMovementComponent* CharMoveComp = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if (CharMoveComp)
	{
		CharMoveComp->MaxWalkSpeed = static_cast<float>(Speed);
	}
	else
	{
		UFloatingPawnMovement* FloatMoveComp = Pawn->FindComponentByClass<UFloatingPawnMovement>();
		if (FloatMoveComp)
		{
			FloatMoveComp->MaxSpeed = static_cast<float>(Speed);
		}
	}

	// Ensure the pawn has a controller
	AController* Controller = Pawn->GetController();
	if (!Controller)
	{
		// Spawn a default AI controller for this pawn
		Pawn->SpawnDefaultController();
		Controller = Pawn->GetController();
	}

	if (!Controller)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not obtain or create a controller for Pawn '%s'"), *ActorName));

	// Issue the move command via the AI helper
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, Destination);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetObjectField(TEXT("destination"), VectorToJson(Destination));
	Result->SetNumberField(TEXT("speed"), Speed);
	Result->SetObjectField(TEXT("start_location"), VectorToJson(Actor->GetActorLocation()));
	Result->SetStringField(TEXT("status"), TEXT("move_requested"));

	// Also compute the path for reference
	UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Actor->GetActorLocation(), Destination);
	if (NavPath && NavPath->IsValid() && NavPath->PathPoints.Num() > 0)
	{
		Result->SetNumberField(TEXT("path_length"), NavPath->GetPathLength());
		Result->SetNumberField(TEXT("path_point_count"), NavPath->PathPoints.Num());
	}
	else
	{
		Result->SetStringField(TEXT("warning"), TEXT("No valid nav path found to destination — movement may fail at runtime"));
	}

	return FECACommandResult::Success(Result);
}

// ─── get_nav_mesh_info ────────────────────────────────────────

FECACommandResult FECACommand_GetNavMeshInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
		return FECACommandResult::Error(TEXT("No NavigationSystemV1 found in the current world"));

	TSharedPtr<FJsonObject> Result = MakeResult();

	// Build status
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	bool bIsBuilt = NavSys->IsNavigationBuilt(WorldSettings);
	Result->SetBoolField(TEXT("is_built"), bIsBuilt);

	// Navigation bounds
	const TSet<FNavigationBounds>& NavBounds = NavSys->GetNavigationBounds();
	Result->SetNumberField(TEXT("nav_bounds_count"), NavBounds.Num());

	TArray<TSharedPtr<FJsonValue>> BoundsArray;
	for (const FNavigationBounds& Bounds : NavBounds)
	{
		TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		BoundsObj->SetObjectField(TEXT("center"), VectorToJson(Bounds.AreaBox.GetCenter()));
		BoundsObj->SetObjectField(TEXT("extent"), VectorToJson(Bounds.AreaBox.GetExtent()));
		BoundsObj->SetObjectField(TEXT("min"), VectorToJson(Bounds.AreaBox.Min));
		BoundsObj->SetObjectField(TEXT("max"), VectorToJson(Bounds.AreaBox.Max));
		BoundsArray.Add(MakeShared<FJsonValueObject>(BoundsObj));
	}
	Result->SetArrayField(TEXT("navigation_bounds"), BoundsArray);

	// Default nav data info
	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (NavData)
	{
		Result->SetStringField(TEXT("nav_data_class"), NavData->GetClass()->GetName());
		Result->SetStringField(TEXT("nav_data_name"), NavData->GetName());

		ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavData);
		if (RecastNavMesh)
		{
			Result->SetNumberField(TEXT("agent_radius"), RecastNavMesh->AgentRadius);
			Result->SetNumberField(TEXT("agent_height"), RecastNavMesh->AgentHeight);
			Result->SetNumberField(TEXT("cell_size"), RecastNavMesh->CellSize);
			Result->SetNumberField(TEXT("cell_height"), RecastNavMesh->CellHeight);
			Result->SetNumberField(TEXT("tile_size_uu"), RecastNavMesh->TileSizeUU);
			Result->SetNumberField(TEXT("max_agent_radius"), RecastNavMesh->AgentMaxSlope);
		}
	}
	else
	{
		Result->SetStringField(TEXT("nav_data_class"), TEXT("None"));
	}

	// Count NavMeshBoundsVolumes in the level
	int32 BoundsVolumeCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("NavMeshBoundsVolume")))
		{
			BoundsVolumeCount++;
		}
	}
	Result->SetNumberField(TEXT("nav_mesh_bounds_volume_count"), BoundsVolumeCount);

	return FECACommandResult::Success(Result);
}

// ─── add_actor_tag ───────────────────────────────────────────

FECACommandResult FECACommand_AddActorTag::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, Tag;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("tag"), Tag))
		return FECACommandResult::Error(TEXT("Missing required parameter: tag"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	FName TagName(*Tag);
	bool bAlreadyHad = Actor->Tags.Contains(TagName);
	Actor->Tags.AddUnique(TagName);
	Actor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetBoolField(TEXT("already_had_tag"), bAlreadyHad);

	// Return current tags list
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& ActorTag : Actor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(ActorTag.ToString()));
	}
	Result->SetArrayField(TEXT("current_tags"), TagsArray);

	return FECACommandResult::Success(Result);
}

// ─── remove_actor_tag ────────────────────────────────────────

FECACommandResult FECACommand_RemoveActorTag::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, Tag;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("tag"), Tag))
		return FECACommandResult::Error(TEXT("Missing required parameter: tag"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	FName TagName(*Tag);
	int32 Removed = Actor->Tags.Remove(TagName);

	if (Removed > 0)
	{
		Actor->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetBoolField(TEXT("was_removed"), Removed > 0);

	// Return current tags list
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& ActorTag : Actor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(ActorTag.ToString()));
	}
	Result->SetArrayField(TEXT("current_tags"), TagsArray);

	return FECACommandResult::Success(Result);
}

// ─── find_actors_by_tag ──────────────────────────────────────

FECACommandResult FECACommand_FindActorsByTag::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Tag;
	if (!GetStringParam(Params, TEXT("tag"), Tag))
		return FECACommandResult::Error(TEXT("Missing required parameter: tag"));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	FName TagName(*Tag);

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->Tags.Contains(TagName))
		{
			TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
			ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel().IsEmpty() ? Actor->GetName() : Actor->GetActorLabel());
			ActorObj->SetStringField(TEXT("internal_name"), Actor->GetName());
			ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorObj->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));

			// Include all tags on this actor
			TArray<TSharedPtr<FJsonValue>> TagsArray;
			for (const FName& ActorTag : Actor->Tags)
			{
				TagsArray.Add(MakeShared<FJsonValueString>(ActorTag.ToString()));
			}
			ActorObj->SetArrayField(TEXT("tags"), TagsArray);

			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	Result->SetArrayField(TEXT("actors"), ActorsArray);

	return FECACommandResult::Success(Result);
}

// ─── get_actor_hierarchy ─────────────────────────────────────

FECACommandResult FECACommand_GetActorHierarchy::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel().IsEmpty() ? Actor->GetName() : Actor->GetActorLabel());
	Result->SetStringField(TEXT("internal_name"), Actor->GetName());
	Result->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));

	// Parent (attachment parent)
	AActor* ParentActor = Actor->GetAttachParentActor();
	if (ParentActor)
	{
		TSharedPtr<FJsonObject> ParentObj = MakeShared<FJsonObject>();
		ParentObj->SetStringField(TEXT("name"), ParentActor->GetActorLabel().IsEmpty() ? ParentActor->GetName() : ParentActor->GetActorLabel());
		ParentObj->SetStringField(TEXT("internal_name"), ParentActor->GetName());
		ParentObj->SetStringField(TEXT("class"), ParentActor->GetClass()->GetName());
		ParentObj->SetObjectField(TEXT("location"), VectorToJson(ParentActor->GetActorLocation()));

		USceneComponent* ParentSocket = Actor->GetRootComponent() ? Actor->GetRootComponent()->GetAttachParent() : nullptr;
		if (ParentSocket)
		{
			ParentObj->SetStringField(TEXT("attach_socket"), ParentSocket->GetAttachSocketName().ToString());
			ParentObj->SetStringField(TEXT("attach_component"), ParentSocket->GetName());
		}
		Result->SetObjectField(TEXT("parent"), ParentObj);
	}
	else
	{
		Result->SetField(TEXT("parent"), MakeShared<FJsonValueNull>());
	}

	// Children (attached child actors)
	TArray<AActor*> ChildActors;
	Actor->GetAttachedActors(ChildActors);

	TArray<TSharedPtr<FJsonValue>> ChildrenArray;
	for (AActor* Child : ChildActors)
	{
		if (!Child) continue;
		TSharedPtr<FJsonObject> ChildObj = MakeShared<FJsonObject>();
		ChildObj->SetStringField(TEXT("name"), Child->GetActorLabel().IsEmpty() ? Child->GetName() : Child->GetActorLabel());
		ChildObj->SetStringField(TEXT("internal_name"), Child->GetName());
		ChildObj->SetStringField(TEXT("class"), Child->GetClass()->GetName());
		ChildObj->SetObjectField(TEXT("location"), VectorToJson(Child->GetActorLocation()));
		ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildObj));
	}
	Result->SetNumberField(TEXT("children_count"), ChildrenArray.Num());
	Result->SetArrayField(TEXT("children"), ChildrenArray);

	// Components
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp) continue;
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Comp->GetName());
		CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

		USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
		if (SceneComp)
		{
			CompObj->SetBoolField(TEXT("is_scene_component"), true);
			CompObj->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComp->GetRelativeLocation()));
			CompObj->SetStringField(TEXT("mobility"),
				SceneComp->Mobility == EComponentMobility::Static ? TEXT("Static") :
				SceneComp->Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Movable"));

			// Parent component within this actor
			USceneComponent* ParentComp = SceneComp->GetAttachParent();
			if (ParentComp && ParentComp->GetOwner() == Actor)
			{
				CompObj->SetStringField(TEXT("parent_component"), ParentComp->GetName());
			}
		}
		else
		{
			CompObj->SetBoolField(TEXT("is_scene_component"), false);
		}

		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}
	Result->SetNumberField(TEXT("component_count"), ComponentsArray.Num());
	Result->SetArrayField(TEXT("components"), ComponentsArray);

	return FECACommandResult::Success(Result);
}

// ─── set_actor_mobility ──────────────────────────────────────

FECACommandResult FECACommand_SetActorMobility::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, MobilityStr;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("mobility"), MobilityStr))
		return FECACommandResult::Error(TEXT("Missing required parameter: mobility"));

	// Parse mobility enum
	EComponentMobility::Type Mobility;
	MobilityStr = MobilityStr.ToLower();
	if (MobilityStr == TEXT("static"))
	{
		Mobility = EComponentMobility::Static;
	}
	else if (MobilityStr == TEXT("stationary"))
	{
		Mobility = EComponentMobility::Stationary;
	}
	else if (MobilityStr == TEXT("movable"))
	{
		Mobility = EComponentMobility::Movable;
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Invalid mobility '%s'. Must be 'static', 'stationary', or 'movable'."), *MobilityStr));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	USceneComponent* RootComp = Actor->GetRootComponent();
	if (!RootComp)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no root component to set mobility on"), *ActorName));

	FString PreviousMobility =
		RootComp->Mobility == EComponentMobility::Static ? TEXT("Static") :
		RootComp->Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Movable");

	RootComp->SetMobility(Mobility);
	Actor->MarkPackageDirty();

	FString NewMobility =
		Mobility == EComponentMobility::Static ? TEXT("Static") :
		Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Movable");

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("previous_mobility"), PreviousMobility);
	Result->SetStringField(TEXT("new_mobility"), NewMobility);
	Result->SetStringField(TEXT("root_component"), RootComp->GetName());
	Result->SetStringField(TEXT("root_component_class"), RootComp->GetClass()->GetName());

	return FECACommandResult::Success(Result);
}
