// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAViewCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "EditorViewportClient.h"
#include "Modules/ModuleManager.h"
#include "ConvexVolume.h"

// Register all view commands
REGISTER_ECA_COMMAND(FECACommand_GetActorsInView)
REGISTER_ECA_COMMAND(FECACommand_GetViewportCamera)
REGISTER_ECA_COMMAND(FECACommand_DescribeView)

//------------------------------------------------------------------------------
// Helper structures and functions
//------------------------------------------------------------------------------

namespace ViewHelpers
{
	/** Information about an actor visible in the view */
	struct FVisibleActorInfo
	{
		AActor* Actor;
		float Distance;
		FVector RelativeLocation;  // Location relative to camera (Forward, Right, Up)
		FBox WorldBounds;
	};

	/** Get the active editor viewport client using the same method as other ECA commands */
	FEditorViewportClient* GetActiveEditorViewportClient()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
		
		if (ActiveViewport.IsValid() && ActiveViewport->GetActiveViewport())
		{
			return static_cast<FEditorViewportClient*>(ActiveViewport->GetActiveViewport()->GetClient());
		}
		
		return nullptr;
	}

	/** Get camera transform from a named camera actor or the editor viewport */
	bool GetCameraTransform(const FString& CameraName, UWorld* World, FVector& OutLocation, FRotator& OutRotation, float& OutFOV, FString& OutSourceName)
	{
		// If camera name specified, find that camera actor
		if (!CameraName.IsEmpty() && World)
		{
			for (TActorIterator<ACameraActor> It(World); It; ++It)
			{
				ACameraActor* CameraActor = *It;
				if (CameraActor && (CameraActor->GetActorLabel().Contains(CameraName) || CameraActor->GetName().Contains(CameraName)))
				{
					OutLocation = CameraActor->GetActorLocation();
					OutRotation = CameraActor->GetActorRotation();
					OutSourceName = CameraActor->GetActorLabel();
					
					if (UCameraComponent* CameraComp = CameraActor->GetCameraComponent())
					{
						OutFOV = CameraComp->FieldOfView;
					}
					else
					{
						OutFOV = 90.0f;
					}
					return true;
				}
			}
			return false; // Camera not found
		}
		
		// Use editor viewport camera
		FEditorViewportClient* ViewportClient = GetActiveEditorViewportClient();
		if (ViewportClient)
		{
			OutLocation = ViewportClient->GetViewLocation();
			OutRotation = ViewportClient->GetViewRotation();
			OutFOV = ViewportClient->ViewFOV;
			OutSourceName = TEXT("EditorViewport");
			return true;
		}
		
		return false;
	}

	/** 
	 * Check if an actor is potentially visible from the camera.
	 * Uses a generous check - we'd rather include actors that might be slightly out of view
	 * than miss actors that are clearly visible.
	 */
	bool IsInViewFrustum(const FVector& CameraLocation, const FVector& CameraForward, const FVector& CameraRight, const FVector& CameraUp,
						 float HalfFOVRadians, float AspectRatio, float MaxDistance, 
						 const FVector& TestPoint, const FBox* OptionalBounds = nullptr)
	{
		// If we have bounds, use the closest point on bounds to camera for the primary test
		FVector PointToTest = TestPoint;
		if (OptionalBounds && OptionalBounds->IsValid)
		{
			// Use bounds center, but also we'll test if ANY part of bounds is in front
			PointToTest = OptionalBounds->GetCenter();
		}
		
		FVector ToPoint = PointToTest - CameraLocation;
		float Distance = ToPoint.Size();
		
		// Distance check
		if (Distance > MaxDistance)
		{
			return false;
		}
		
		// Very close objects are always "in view" (within 2 meters)
		if (Distance < 200.0f)
		{
			return true;
		}
		
		// Check if the point (or any corner of bounds) is in front of camera
		// "In front" means the dot product with forward is positive
		float ForwardDot = FVector::DotProduct(ToPoint.GetSafeNormal(), CameraForward);
		
		// If center is behind camera, check bounds corners
		if (ForwardDot < -0.1f)
		{
			if (OptionalBounds && OptionalBounds->IsValid)
			{
				// Check if any corner is in front
				FVector Corners[8] = {
					FVector(OptionalBounds->Min.X, OptionalBounds->Min.Y, OptionalBounds->Min.Z),
					FVector(OptionalBounds->Max.X, OptionalBounds->Min.Y, OptionalBounds->Min.Z),
					FVector(OptionalBounds->Min.X, OptionalBounds->Max.Y, OptionalBounds->Min.Z),
					FVector(OptionalBounds->Max.X, OptionalBounds->Max.Y, OptionalBounds->Min.Z),
					FVector(OptionalBounds->Min.X, OptionalBounds->Min.Y, OptionalBounds->Max.Z),
					FVector(OptionalBounds->Max.X, OptionalBounds->Min.Y, OptionalBounds->Max.Z),
					FVector(OptionalBounds->Min.X, OptionalBounds->Max.Y, OptionalBounds->Max.Z),
					FVector(OptionalBounds->Max.X, OptionalBounds->Max.Y, OptionalBounds->Max.Z)
				};
				
				bool bAnyInFront = false;
				for (const FVector& Corner : Corners)
				{
					FVector ToCorner = Corner - CameraLocation;
					if (FVector::DotProduct(ToCorner.GetSafeNormal(), CameraForward) > -0.1f)
					{
						bAnyInFront = true;
						break;
					}
				}
				
				if (!bAnyInFront)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		
		// For the FOV check, be generous - use a wide angle that accounts for:
		// - Horizontal FOV (wider than vertical due to aspect ratio)
		// - Large objects that extend beyond their center point
		// - Some margin for objects at screen edges
		float HorizontalHalfFOV = FMath::Atan(FMath::Tan(HalfFOVRadians) * AspectRatio);
		float MaxHalfAngle = FMath::Max(HalfFOVRadians, HorizontalHalfFOV);
		
		// Add generous margin (50%) to catch objects at edges and large objects
		MaxHalfAngle *= 1.5f;
		
		// Also add extra angle for bounds size
		if (OptionalBounds && OptionalBounds->IsValid && Distance > 1.0f)
		{
			float BoundsRadius = OptionalBounds->GetExtent().Size();
			float BoundsAngle = FMath::Atan(BoundsRadius / Distance);
			MaxHalfAngle += BoundsAngle;
		}
		
		// Calculate angle from camera forward to the point
		float AngleToPoint = FMath::Acos(FMath::Clamp(ForwardDot, -1.0f, 1.0f));
		
		return AngleToPoint <= MaxHalfAngle;
	}

	/** Get actors visible from a camera position/orientation */
	TArray<FVisibleActorInfo> GetVisibleActors(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		float MinDistance,
		float MaxDistance,
		bool bIncludeHidden,
		const FString& ClassFilter,
		int32 MaxResults)
	{
		TArray<FVisibleActorInfo> VisibleActors;
		
		if (!World)
		{
			return VisibleActors;
		}
		
		// Calculate camera vectors
		FVector CameraForward = CameraRotation.Vector();
		FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
		FVector CameraUp = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);
		
		float HalfFOVRadians = FMath::DegreesToRadians(FOV * 0.5f);
		float AspectRatio = 16.0f / 9.0f;
		
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || Actor->IsPendingKillPending())
			{
				continue;
			}
			
			// Skip hidden actors unless requested
			if (!bIncludeHidden && Actor->IsHiddenEd())
			{
				continue;
			}
			
			// Skip transient actors
			if (Actor->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			
			// Apply class filter
			if (!ClassFilter.IsEmpty())
			{
				FString ActorClassName = Actor->GetClass()->GetName();
				if (!ActorClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
				{
					continue;
				}
			}
			
			// Get actor location and bounds
			FVector ActorLocation = Actor->GetActorLocation();
			FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
			
			// Check distance
			float Distance = FVector::Dist(CameraLocation, ActorLocation);
			if (Distance < MinDistance || Distance > MaxDistance)
			{
				continue;
			}
			
			// Check if in view frustum
			if (!IsInViewFrustum(CameraLocation, CameraForward, CameraRight, CameraUp,
								 HalfFOVRadians, AspectRatio, MaxDistance,
								 ActorLocation, ActorBounds.IsValid ? &ActorBounds : nullptr))
			{
				continue;
			}
			
			// Calculate relative position
			FVector ToActor = ActorLocation - CameraLocation;
			FVector RelativeLocation(
				FVector::DotProduct(ToActor, CameraForward),
				FVector::DotProduct(ToActor, CameraRight),
				FVector::DotProduct(ToActor, CameraUp)
			);
			
			FVisibleActorInfo Info;
			Info.Actor = Actor;
			Info.Distance = Distance;
			Info.RelativeLocation = RelativeLocation;
			Info.WorldBounds = ActorBounds;
			
			VisibleActors.Add(Info);
		}
		
		// Sort by distance (nearest first)
		VisibleActors.Sort([](const FVisibleActorInfo& A, const FVisibleActorInfo& B)
		{
			return A.Distance < B.Distance;
		});
		
		// Limit results
		if (MaxResults > 0 && VisibleActors.Num() > MaxResults)
		{
			VisibleActors.SetNum(MaxResults);
		}
		
		return VisibleActors;
	}

	/** Get a descriptive category for an actor */
	FString GetActorCategory(AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("Unknown");
		}
		
		FString ClassName = Actor->GetClass()->GetName();
		
		if (ClassName.Contains(TEXT("Light"))) return TEXT("Lighting");
		if (ClassName.Contains(TEXT("Camera"))) return TEXT("Cameras");
		if (ClassName.Contains(TEXT("StaticMesh"))) return TEXT("StaticMeshes");
		if (ClassName.Contains(TEXT("SkeletalMesh")) || ClassName.Contains(TEXT("Character"))) return TEXT("Characters");
		if (ClassName.Contains(TEXT("Trigger")) || ClassName.Contains(TEXT("Volume"))) return TEXT("Volumes");
		if (ClassName.Contains(TEXT("Audio")) || ClassName.Contains(TEXT("Sound"))) return TEXT("Audio");
		if (ClassName.Contains(TEXT("Emitter")) || ClassName.Contains(TEXT("Particle")) || ClassName.Contains(TEXT("Niagara"))) return TEXT("Effects");
		if (ClassName.Contains(TEXT("Landscape")) || ClassName.Contains(TEXT("Terrain"))) return TEXT("Landscape");
		if (ClassName.Contains(TEXT("Foliage"))) return TEXT("Foliage");
		if (ClassName.Contains(TEXT("Decal"))) return TEXT("Decals");
		if (ClassName.Contains(TEXT("Reflection")) || ClassName.Contains(TEXT("PostProcess"))) return TEXT("PostProcess");
		if (ClassName.Contains(TEXT("BP_")) || Actor->GetClass()->ClassGeneratedBy != nullptr) return TEXT("Blueprints");
		
		return TEXT("Other");
	}

	/** Get screen position description */
	FString GetScreenPosition(const FVector& RelativeLocation)
	{
		FString Position;
		
		// Horizontal position
		if (FMath::Abs(RelativeLocation.Y) < 100.0f)
		{
			Position = TEXT("center");
		}
		else
		{
			Position = RelativeLocation.Y > 0 ? TEXT("right") : TEXT("left");
		}
		
		// Vertical position
		if (FMath::Abs(RelativeLocation.Z) > 100.0f)
		{
			Position += RelativeLocation.Z > 0 ? TEXT("-above") : TEXT("-below");
		}
		
		return Position;
	}
}

//------------------------------------------------------------------------------
// GetActorsInView Implementation
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetActorsInView::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	// Get parameters
	FString CameraName;
	double MaxDistance = 10000.0;  // 100 meters default
	double MinDistance = 0.0;
	bool bIncludeHidden = false;
	FString ClassFilter;
	bool bIncludeComponents = false;
	int32 MaxResults = 100;
	double FOVScale = 1.0;
	
	GetStringParam(Params, TEXT("camera_name"), CameraName, false);
	GetFloatParam(Params, TEXT("max_distance"), MaxDistance, false);
	GetFloatParam(Params, TEXT("min_distance"), MinDistance, false);
	GetBoolParam(Params, TEXT("include_hidden"), bIncludeHidden, false);
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, false);
	GetBoolParam(Params, TEXT("include_components"), bIncludeComponents, false);
	GetIntParam(Params, TEXT("max_results"), MaxResults, false);
	GetFloatParam(Params, TEXT("fov_scale"), FOVScale, false);
	
	// Get camera transform
	FVector CameraLocation;
	FRotator CameraRotation;
	float CameraFOV;
	FString SourceName;
	
	if (!ViewHelpers::GetCameraTransform(CameraName, World, CameraLocation, CameraRotation, CameraFOV, SourceName))
	{
		if (!CameraName.IsEmpty())
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Camera not found: %s"), *CameraName));
		}
		return FECACommandResult::Error(TEXT("No editor viewport available. Make sure a Level Editor viewport is open."));
	}
	
	// Apply FOV scale
	float EffectiveFOV = FMath::Clamp(CameraFOV * (float)FOVScale, 10.0f, 170.0f);
	
	// Get visible actors
	TArray<ViewHelpers::FVisibleActorInfo> VisibleActors = ViewHelpers::GetVisibleActors(
		World, CameraLocation, CameraRotation, EffectiveFOV,
		(float)MinDistance, (float)MaxDistance, bIncludeHidden, ClassFilter, MaxResults
	);
	
	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	// Camera info
	TSharedPtr<FJsonObject> CameraJson = MakeShared<FJsonObject>();
	CameraJson->SetStringField(TEXT("source"), SourceName);
	CameraJson->SetObjectField(TEXT("location"), VectorToJson(CameraLocation));
	CameraJson->SetObjectField(TEXT("rotation"), RotatorToJson(CameraRotation));
	CameraJson->SetNumberField(TEXT("fov"), CameraFOV);
	CameraJson->SetNumberField(TEXT("effective_fov"), EffectiveFOV);
	Result->SetObjectField(TEXT("camera"), CameraJson);
	
	// Actors array
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (const ViewHelpers::FVisibleActorInfo& Info : VisibleActors)
	{
		AActor* Actor = Info.Actor;
		
		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorJson->SetStringField(TEXT("internal_name"), Actor->GetName());
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorJson->SetStringField(TEXT("category"), ViewHelpers::GetActorCategory(Actor));
		ActorJson->SetNumberField(TEXT("distance"), Info.Distance);
		ActorJson->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
		ActorJson->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
		
		// Relative position to camera
		TSharedPtr<FJsonObject> RelativeJson = MakeShared<FJsonObject>();
		RelativeJson->SetNumberField(TEXT("forward"), Info.RelativeLocation.X);
		RelativeJson->SetNumberField(TEXT("right"), Info.RelativeLocation.Y);
		RelativeJson->SetNumberField(TEXT("up"), Info.RelativeLocation.Z);
		ActorJson->SetObjectField(TEXT("relative_to_camera"), RelativeJson);
		
		// Screen position hint
		ActorJson->SetStringField(TEXT("screen_position"), ViewHelpers::GetScreenPosition(Info.RelativeLocation));
		
		// Bounds size
		if (Info.WorldBounds.IsValid)
		{
			FVector Size = Info.WorldBounds.GetSize();
			TSharedPtr<FJsonObject> SizeJson = MakeShared<FJsonObject>();
			SizeJson->SetNumberField(TEXT("x"), Size.X);
			SizeJson->SetNumberField(TEXT("y"), Size.Y);
			SizeJson->SetNumberField(TEXT("z"), Size.Z);
			ActorJson->SetObjectField(TEXT("bounds_size"), SizeJson);
		}
		
		// Tags
		if (Actor->Tags.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TagsArray;
			for (const FName& Tag : Actor->Tags)
			{
				TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ActorJson->SetArrayField(TEXT("tags"), TagsArray);
		}
		
		// Folder
		FName FolderPath = Actor->GetFolderPath();
		if (!FolderPath.IsNone())
		{
			ActorJson->SetStringField(TEXT("folder"), FolderPath.ToString());
		}
		
		// Components if requested
		if (bIncludeComponents)
		{
			TArray<TSharedPtr<FJsonValue>> ComponentsArray;
			TArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			
			for (UActorComponent* Component : Components)
			{
				if (Component)
				{
					TSharedPtr<FJsonObject> CompJson = MakeShared<FJsonObject>();
					CompJson->SetStringField(TEXT("name"), Component->GetName());
					CompJson->SetStringField(TEXT("class"), Component->GetClass()->GetName());
					ComponentsArray.Add(MakeShared<FJsonValueObject>(CompJson));
				}
			}
			ActorJson->SetArrayField(TEXT("components"), ComponentsArray);
		}
		
		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
	}
	
	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	Result->SetNumberField(TEXT("max_distance"), MaxDistance);
	Result->SetNumberField(TEXT("min_distance"), MinDistance);
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetViewportCamera Implementation
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetViewportCamera::Execute(const TSharedPtr<FJsonObject>& Params)
{
	int32 ViewportIndex = 0;
	GetIntParam(Params, TEXT("viewport_index"), ViewportIndex, false);
	
	FEditorViewportClient* ViewportClient = ViewHelpers::GetActiveEditorViewportClient();
	if (!ViewportClient)
	{
		return FECACommandResult::Error(TEXT("No active editor viewport found. Make sure a Level Editor viewport is open."));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	// Location
	FVector Location = ViewportClient->GetViewLocation();
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	
	// Rotation
	FRotator Rotation = ViewportClient->GetViewRotation();
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
	
	// Direction vectors
	FVector Forward = Rotation.Vector();
	FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
	FVector Up = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
	
	TSharedPtr<FJsonObject> DirectionsJson = MakeShared<FJsonObject>();
	DirectionsJson->SetObjectField(TEXT("forward"), VectorToJson(Forward));
	DirectionsJson->SetObjectField(TEXT("right"), VectorToJson(Right));
	DirectionsJson->SetObjectField(TEXT("up"), VectorToJson(Up));
	Result->SetObjectField(TEXT("directions"), DirectionsJson);
	
	// Camera properties
	Result->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);
	Result->SetBoolField(TEXT("is_perspective"), ViewportClient->IsPerspective());
	Result->SetBoolField(TEXT("is_realtime"), ViewportClient->IsRealtime());
	
	// View mode
	FString ViewModeStr;
	EViewModeIndex ViewMode = ViewportClient->GetViewMode();
	switch (ViewMode)
	{
		case VMI_Lit: ViewModeStr = TEXT("Lit"); break;
		case VMI_Unlit: ViewModeStr = TEXT("Unlit"); break;
		case VMI_Wireframe: ViewModeStr = TEXT("Wireframe"); break;
		case VMI_BrushWireframe: ViewModeStr = TEXT("BrushWireframe"); break;
		case VMI_LightingOnly: ViewModeStr = TEXT("LightingOnly"); break;
		default: ViewModeStr = TEXT("Other"); break;
	}
	Result->SetStringField(TEXT("view_mode"), ViewModeStr);
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// DescribeView Implementation
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DescribeView::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	// Get parameters
	FString CameraName;
	double MaxDistance = 50000.0;  // 500 meters
	double NearThreshold = 500.0;  // 5 meters
	double MidThreshold = 2000.0;  // 20 meters
	bool bIncludeHidden = false;
	
	GetStringParam(Params, TEXT("camera_name"), CameraName, false);
	GetFloatParam(Params, TEXT("max_distance"), MaxDistance, false);
	GetFloatParam(Params, TEXT("near_threshold"), NearThreshold, false);
	GetFloatParam(Params, TEXT("mid_threshold"), MidThreshold, false);
	GetBoolParam(Params, TEXT("include_hidden"), bIncludeHidden, false);
	
	// Get camera transform
	FVector CameraLocation;
	FRotator CameraRotation;
	float CameraFOV;
	FString SourceName;
	
	if (!ViewHelpers::GetCameraTransform(CameraName, World, CameraLocation, CameraRotation, CameraFOV, SourceName))
	{
		if (!CameraName.IsEmpty())
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Camera not found: %s"), *CameraName));
		}
		return FECACommandResult::Error(TEXT("No editor viewport available. Make sure a Level Editor viewport is open."));
	}
	
	// Get all visible actors (no max limit)
	TArray<ViewHelpers::FVisibleActorInfo> VisibleActors = ViewHelpers::GetVisibleActors(
		World, CameraLocation, CameraRotation, CameraFOV,
		0.0f, (float)MaxDistance, bIncludeHidden, TEXT(""), 0
	);
	
	// Categorize by distance zones
	TMap<FString, TArray<TPair<FString, float>>> NearByCategory;  // Category -> [(ActorName, Distance)]
	TMap<FString, TArray<TPair<FString, float>>> MidByCategory;
	TMap<FString, TArray<TPair<FString, float>>> FarByCategory;
	
	int32 NearCount = 0, MidCount = 0, FarCount = 0;
	
	for (const ViewHelpers::FVisibleActorInfo& Info : VisibleActors)
	{
		FString Category = ViewHelpers::GetActorCategory(Info.Actor);
		FString ActorName = Info.Actor->GetActorLabel();
		
		if (Info.Distance <= NearThreshold)
		{
			NearByCategory.FindOrAdd(Category).Add(TPair<FString, float>(ActorName, Info.Distance));
			NearCount++;
		}
		else if (Info.Distance <= MidThreshold)
		{
			MidByCategory.FindOrAdd(Category).Add(TPair<FString, float>(ActorName, Info.Distance));
			MidCount++;
		}
		else
		{
			FarByCategory.FindOrAdd(Category).Add(TPair<FString, float>(ActorName, Info.Distance));
			FarCount++;
		}
	}
	
	// Helper to build zone JSON
	auto BuildZoneJson = [](const TMap<FString, TArray<TPair<FString, float>>>& ByCategory, int32 TotalCount) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> ZoneJson = MakeShared<FJsonObject>();
		ZoneJson->SetNumberField(TEXT("total_count"), TotalCount);
		
		TArray<TSharedPtr<FJsonValue>> CategoriesArray;
		for (const auto& Pair : ByCategory)
		{
			TSharedPtr<FJsonObject> CatJson = MakeShared<FJsonObject>();
			CatJson->SetStringField(TEXT("category"), Pair.Key);
			CatJson->SetNumberField(TEXT("count"), Pair.Value.Num());
			
			// List actors (limit to 10 per category)
			TArray<TSharedPtr<FJsonValue>> ActorsList;
			int32 Count = 0;
			for (const auto& ActorPair : Pair.Value)
			{
				if (Count >= 10) break;
				TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
				ActorJson->SetStringField(TEXT("name"), ActorPair.Key);
				ActorJson->SetNumberField(TEXT("distance"), FMath::RoundToInt(ActorPair.Value));
				ActorsList.Add(MakeShared<FJsonValueObject>(ActorJson));
				Count++;
			}
			CatJson->SetArrayField(TEXT("actors"), ActorsList);
			
			CategoriesArray.Add(MakeShared<FJsonValueObject>(CatJson));
		}
		ZoneJson->SetArrayField(TEXT("categories"), CategoriesArray);
		
		return ZoneJson;
	};
	
	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	// Camera info
	TSharedPtr<FJsonObject> CameraJson = MakeShared<FJsonObject>();
	CameraJson->SetStringField(TEXT("source"), SourceName);
	CameraJson->SetObjectField(TEXT("location"), VectorToJson(CameraLocation));
	CameraJson->SetObjectField(TEXT("rotation"), RotatorToJson(CameraRotation));
	CameraJson->SetNumberField(TEXT("fov"), CameraFOV);
	
	// Looking direction description
	FVector Forward = CameraRotation.Vector();
	FString LookingDir;
	if (FMath::Abs(Forward.Z) > 0.7f)
	{
		LookingDir = Forward.Z > 0 ? TEXT("up") : TEXT("down");
	}
	else
	{
		float Yaw = CameraRotation.Yaw;
		while (Yaw < 0) Yaw += 360.f;
		while (Yaw >= 360) Yaw -= 360.f;
		
		if (Yaw < 45.f || Yaw >= 315.f) LookingDir = TEXT("North (+X)");
		else if (Yaw < 135.f) LookingDir = TEXT("East (+Y)");
		else if (Yaw < 225.f) LookingDir = TEXT("South (-X)");
		else LookingDir = TEXT("West (-Y)");
	}
	CameraJson->SetStringField(TEXT("looking_direction"), LookingDir);
	Result->SetObjectField(TEXT("camera"), CameraJson);
	
	// Zones
	TSharedPtr<FJsonObject> NearJson = BuildZoneJson(NearByCategory, NearCount);
	NearJson->SetStringField(TEXT("description"), FString::Printf(TEXT("Within %.1f meters"), NearThreshold / 100.0));
	Result->SetObjectField(TEXT("near"), NearJson);
	
	TSharedPtr<FJsonObject> MidJson = BuildZoneJson(MidByCategory, MidCount);
	MidJson->SetStringField(TEXT("description"), FString::Printf(TEXT("%.1f to %.1f meters"), NearThreshold / 100.0, MidThreshold / 100.0));
	Result->SetObjectField(TEXT("mid"), MidJson);
	
	TSharedPtr<FJsonObject> FarJson = BuildZoneJson(FarByCategory, FarCount);
	FarJson->SetStringField(TEXT("description"), FString::Printf(TEXT("%.1f to %.1f meters"), MidThreshold / 100.0, MaxDistance / 100.0));
	Result->SetObjectField(TEXT("far"), FarJson);
	
	// Summary
	TSharedPtr<FJsonObject> SummaryJson = MakeShared<FJsonObject>();
	SummaryJson->SetNumberField(TEXT("total_visible"), VisibleActors.Num());
	SummaryJson->SetNumberField(TEXT("near_count"), NearCount);
	SummaryJson->SetNumberField(TEXT("mid_count"), MidCount);
	SummaryJson->SetNumberField(TEXT("far_count"), FarCount);
	Result->SetObjectField(TEXT("summary"), SummaryJson);
	
	return FECACommandResult::Success(Result);
}
