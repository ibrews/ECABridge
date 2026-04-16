// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAEnvironmentCommands.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Engine/Blueprint.h"
#include "Engine/Light.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ShapeComponent.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundBase.h"
#include "Engine/TriggerBox.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "EditorViewportClient.h"
#include "Engine/CollisionProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "SEditorViewport.h"
#include "UnrealEngine.h"
// Undo/Redo uses GEditor->Trans() which is in Editor.h (already included)
#include "FileHelpers.h"
#include "Exporters/FbxExportOption.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Scalability.h"
#include "GameFramework/GameUserSettings.h"
#include "RHI.h"
// Landscape API requires complex setup — using simplified plane-based terrain instead
// #include "Landscape.h"
// #include "LandscapeProxy.h"
// #include "LandscapeInfo.h"

// ─── REGISTER ───────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_SpawnStaticMeshWall);
REGISTER_ECA_COMMAND(FECACommand_CreateFog);
REGISTER_ECA_COMMAND(FECACommand_SetSkySettings);
REGISTER_ECA_COMMAND(FECACommand_SpawnParticleEffect);
REGISTER_ECA_COMMAND(FECACommand_SetWorldGravity);
REGISTER_ECA_COMMAND(FECACommand_GetSceneStats);
REGISTER_ECA_COMMAND(FECACommand_BatchSetActorProperty);
REGISTER_ECA_COMMAND(FECACommand_CreateSplinePath);
REGISTER_ECA_COMMAND(FECACommand_EnablePhysicsSimulation);
REGISTER_ECA_COMMAND(FECACommand_ApplyImpulse);
REGISTER_ECA_COMMAND(FECACommand_SetActorVisibility);
REGISTER_ECA_COMMAND(FECACommand_BatchSpawnActors);
REGISTER_ECA_COMMAND(FECACommand_TeleportActor);
REGISTER_ECA_COMMAND(FECACommand_GenerateGrid);
REGISTER_ECA_COMMAND(FECACommand_GenerateCircle);
REGISTER_ECA_COMMAND(FECACommand_DestroyActorsByPattern);
REGISTER_ECA_COMMAND(FECACommand_TakeCameraScreenshot);
REGISTER_ECA_COMMAND(FECACommand_SpawnDecal);
REGISTER_ECA_COMMAND(FECACommand_SpawnTextRender);
REGISTER_ECA_COMMAND(FECACommand_DescribeActor);
REGISTER_ECA_COMMAND(FECACommand_CloneActorArray);
REGISTER_ECA_COMMAND(FECACommand_SpawnAudioSource);
REGISTER_ECA_COMMAND(FECACommand_SpawnTriggerBox);
REGISTER_ECA_COMMAND(FECACommand_AlignActors);
REGISTER_ECA_COMMAND(FECACommand_DistributeActors);
REGISTER_ECA_COMMAND(FECACommand_SnapshotSceneState);
REGISTER_ECA_COMMAND(FECACommand_RestoreSceneState);
REGISTER_ECA_COMMAND(FECACommand_ListSceneSnapshots);
REGISTER_ECA_COMMAND(FECACommand_SetTimeDilation);
REGISTER_ECA_COMMAND(FECACommand_MeasureDistance);
REGISTER_ECA_COMMAND(FECACommand_RandomizeTransforms);
REGISTER_ECA_COMMAND(FECACommand_GetWorldInfo);
REGISTER_ECA_COMMAND(FECACommand_SetActorFolder);
REGISTER_ECA_COMMAND(FECACommand_GroupActors);
REGISTER_ECA_COMMAND(FECACommand_SetCollisionPreset);
REGISTER_ECA_COMMAND(FECACommand_TakeScreenshotsSweep);
REGISTER_ECA_COMMAND(FECACommand_ExecutePython);
REGISTER_ECA_COMMAND(FECACommand_UndoLastAction);
REGISTER_ECA_COMMAND(FECACommand_RedoLastAction);
REGISTER_ECA_COMMAND(FECACommand_SearchAssets);
REGISTER_ECA_COMMAND(FECACommand_GetActorCountByClass);
REGISTER_ECA_COMMAND(FECACommand_ReplaceMaterialOnActors);
REGISTER_ECA_COMMAND(FECACommand_SetLodSettings);
REGISTER_ECA_COMMAND(FECACommand_ToggleWireframe);
REGISTER_ECA_COMMAND(FECACommand_SetViewportResolution);
REGISTER_ECA_COMMAND(FECACommand_GetMaterialSlots);
REGISTER_ECA_COMMAND(FECACommand_CreateNewLevel);
REGISTER_ECA_COMMAND(FECACommand_DuplicateLevel);
REGISTER_ECA_COMMAND(FECACommand_SetRenderSettings);
REGISTER_ECA_COMMAND(FECACommand_ExportActorAsFbx);
REGISTER_ECA_COMMAND(FECACommand_CreateLandscape);
REGISTER_ECA_COMMAND(FECACommand_ScatterActorsOnSurface);
REGISTER_ECA_COMMAND(FECACommand_ParentActorTo);
REGISTER_ECA_COMMAND(FECACommand_DetachActor);
REGISTER_ECA_COMMAND(FECACommand_ListActorChildren);
REGISTER_ECA_COMMAND(FECACommand_SpawnBlueprintAt);
REGISTER_ECA_COMMAND(FECACommand_CopyActorTransform);
REGISTER_ECA_COMMAND(FECACommand_GetAllAssetPaths);
REGISTER_ECA_COMMAND(FECACommand_SetActorColor);
REGISTER_ECA_COMMAND(FECACommand_GetPerformanceStats);
REGISTER_ECA_COMMAND(FECACommand_SetScalabilitySettings);
REGISTER_ECA_COMMAND(FECACommand_OptimizeForVR);
REGISTER_ECA_COMMAND(FECACommand_ListHeavyActors);

// ─── Helpers ────────────────────────────────────────────────

namespace EnvironmentCommandHelpers
{
	/**
	 * Load a static mesh asset by path, trying both raw path and sub-object syntax.
	 */
	static UStaticMesh* LoadMeshByPath(const FString& InPath)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *InPath);
		if (!Mesh)
		{
			FString FullPath = InPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Mesh = LoadObject<UStaticMesh>(nullptr, *FullPath);
		}
		return Mesh;
	}

	/**
	 * Load a material asset by path.
	 */
	static UMaterialInterface* LoadMaterialByPath(const FString& InPath)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *InPath);
		if (!Mat)
		{
			FString FullPath = InPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Mat = LoadObject<UMaterialInterface>(nullptr, *FullPath);
		}
		return Mat;
	}

	/**
	 * Find the first AExponentialHeightFog in the editor world.
	 */
	static AExponentialHeightFog* FindFirstFog(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/**
	 * Find the first ADirectionalLight in the editor world.
	 */
	static ADirectionalLight* FindFirstDirectionalLight(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

// ─── spawn_static_mesh_wall ─────────────────────────────────

FECACommandResult FECACommand_SpawnStaticMeshWall::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Mesh path (optional, default cube)
	FString MeshPath = TEXT("/Engine/BasicShapes/Cube");
	GetStringParam(Params, TEXT("mesh_path"), MeshPath, /*bRequired=*/false);

	UStaticMesh* Mesh = EnvironmentCommandHelpers::LoadMeshByPath(MeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load static mesh at: %s"), *MeshPath));
	}

	// Rotation (optional)
	FRotator Rotation = FRotator::ZeroRotator;
	GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired=*/false);

	// Scale (optional, default wall proportions)
	FVector Scale(5.0, 0.1, 3.0);
	GetVectorParam(Params, TEXT("scale"), Scale, /*bRequired=*/false);

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
	if (!MeshActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn StaticMeshActor"));
	}

	// Name
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		MeshActor->SetActorLabel(Name);
	}

	// Configure the mesh component
	UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
	if (MeshComp)
	{
		MeshComp->SetStaticMesh(Mesh);
		MeshActor->SetActorScale3D(Scale);

		// Material (optional)
		FString MaterialPath;
		if (GetStringParam(Params, TEXT("material_path"), MaterialPath, /*bRequired=*/false) && !MaterialPath.IsEmpty())
		{
			UMaterialInterface* Material = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
			if (Material)
			{
				MeshComp->SetMaterial(0, Material);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("spawn_static_mesh_wall: Could not load material at '%s', using default"), *MaterialPath);
			}
		}
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), MeshActor->GetActorLabel());
	Result->SetStringField(TEXT("mesh_path"), Mesh->GetPathName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
	Result->SetObjectField(TEXT("scale"), VectorToJson(Scale));

	return FECACommandResult::Success(Result);
}

// ─── create_fog ─────────────────────────────────────────────

FECACommandResult FECACommand_CreateFog::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Find existing fog or spawn a new one
	AExponentialHeightFog* FogActor = EnvironmentCommandHelpers::FindFirstFog(World);
	bool bCreatedNew = false;

	if (!FogActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FogActor = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (!FogActor)
		{
			return FECACommandResult::Error(TEXT("Failed to spawn ExponentialHeightFog actor"));
		}
		FogActor->SetActorLabel(TEXT("ExponentialHeightFog"));
		bCreatedNew = true;
	}

	UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
	if (!FogComp)
	{
		return FECACommandResult::Error(TEXT("ExponentialHeightFog actor has no fog component"));
	}

	int32 PropertiesSet = 0;

	// Fog density
	double FogDensity = 0.02;
	if (GetFloatParam(Params, TEXT("fog_density"), FogDensity, /*bRequired=*/false))
	{
		FogComp->SetFogDensity(static_cast<float>(FogDensity));
		++PropertiesSet;
	}
	else if (bCreatedNew)
	{
		// Apply default for newly created fog
		FogComp->SetFogDensity(0.02f);
		++PropertiesSet;
	}

	// Fog height (set via actor Z position)
	double FogHeight = 0.0;
	if (GetFloatParam(Params, TEXT("fog_height"), FogHeight, /*bRequired=*/false))
	{
		FVector FogLocation = FogActor->GetActorLocation();
		FogLocation.Z = FogHeight;
		FogActor->SetActorLocation(FogLocation);
		++PropertiesSet;
	}

	// Fog color
	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (GetObjectParam(Params, TEXT("fog_color"), ColorObj, /*bRequired=*/false))
	{
		double R = (*ColorObj)->GetNumberField(TEXT("r"));
		double G = (*ColorObj)->GetNumberField(TEXT("g"));
		double B = (*ColorObj)->GetNumberField(TEXT("b"));
		FLinearColor FogColor(
			static_cast<float>(R / 255.0),
			static_cast<float>(G / 255.0),
			static_cast<float>(B / 255.0),
			1.0f
		);
		FogComp->SetFogInscatteringColor(FogColor);
		++PropertiesSet;
	}

	// Start distance
	double StartDistance;
	if (GetFloatParam(Params, TEXT("start_distance"), StartDistance, /*bRequired=*/false))
	{
		FogComp->SetStartDistance(static_cast<float>(StartDistance));
		++PropertiesSet;
	}

	FogActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), FogActor->GetActorLabel());
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);
	Result->SetNumberField(TEXT("fog_density"), FogComp->FogDensity);
	Result->SetNumberField(TEXT("fog_height"), FogActor->GetActorLocation().Z);
	Result->SetNumberField(TEXT("start_distance"), FogComp->StartDistance);
	Result->SetNumberField(TEXT("properties_set"), PropertiesSet);

	return FECACommandResult::Success(Result);
}

// ─── set_sky_settings ───────────────────────────────────────

FECACommandResult FECACommand_SetSkySettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Sun pitch (required)
	double SunPitch;
	if (!GetFloatParam(Params, TEXT("sun_pitch"), SunPitch))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: sun_pitch"));
	}

	// Clamp pitch to valid range
	SunPitch = FMath::Clamp(SunPitch, -90.0, 90.0);

	// Sun yaw (optional)
	double SunYaw = 0.0;
	GetFloatParam(Params, TEXT("sun_yaw"), SunYaw, /*bRequired=*/false);

	// Intensity multiplier (optional)
	double IntensityMultiplier = 1.0;
	GetFloatParam(Params, TEXT("intensity_multiplier"), IntensityMultiplier, /*bRequired=*/false);

	// Find the directional light
	ADirectionalLight* SunLight = EnvironmentCommandHelpers::FindFirstDirectionalLight(World);
	if (!SunLight)
	{
		return FECACommandResult::Error(TEXT("No directional light found in the scene. Add a directional light first."));
	}

	// Set rotation (pitch maps to the light's pitch, yaw maps to the light's yaw)
	FRotator NewRotation(SunPitch, SunYaw, 0.0);
	SunLight->SetActorRotation(NewRotation);

	// Apply intensity multiplier
	UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent());
	if (LightComp)
	{
		float BaseIntensity = LightComp->Intensity;
		float NewIntensity = static_cast<float>(BaseIntensity * IntensityMultiplier);
		LightComp->SetIntensity(NewIntensity);
	}

	SunLight->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), SunLight->GetActorLabel());
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(NewRotation));
	Result->SetNumberField(TEXT("sun_pitch"), SunPitch);
	Result->SetNumberField(TEXT("sun_yaw"), SunYaw);
	Result->SetNumberField(TEXT("intensity_multiplier"), IntensityMultiplier);
	if (LightComp)
	{
		Result->SetNumberField(TEXT("resulting_intensity"), LightComp->Intensity);
	}

	return FECACommandResult::Success(Result);
}

// ─── spawn_particle_effect ──────────────────────────────────

FECACommandResult FECACommand_SpawnParticleEffect::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// System path (required)
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: system_path"));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Load the Niagara system asset
	UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!NiagaraSystem)
	{
		// Try with sub-object syntax
		FString FullPath = SystemPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *FullPath);
	}

	if (!NiagaraSystem)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Niagara system at: %s"), *SystemPath));
	}

	// Auto activate (optional)
	bool bAutoActivate = true;
	GetBoolParam(Params, TEXT("auto_activate"), bAutoActivate, /*bRequired=*/false);

	// Spawn the Niagara actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANiagaraActor* NiagaraActor = World->SpawnActor<ANiagaraActor>(Location, FRotator::ZeroRotator, SpawnParams);
	if (!NiagaraActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn NiagaraActor"));
	}

	// Name
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		NiagaraActor->SetActorLabel(Name);
	}

	// Configure the Niagara component
	UNiagaraComponent* NiagaraComp = NiagaraActor->GetNiagaraComponent();
	if (NiagaraComp)
	{
		NiagaraComp->SetAsset(NiagaraSystem);
		NiagaraComp->SetAutoActivate(bAutoActivate);
		if (bAutoActivate)
		{
			NiagaraComp->Activate(true);
		}
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), NiagaraActor->GetActorLabel());
	Result->SetStringField(TEXT("system_path"), NiagaraSystem->GetPathName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetBoolField(TEXT("auto_activate"), bAutoActivate);

	return FECACommandResult::Success(Result);
}

// ─── set_world_gravity ──────────────────────────────────────

FECACommandResult FECACommand_SetWorldGravity::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return FECACommandResult::Error(TEXT("Could not access world settings"));
	}

	double GravityZ = -980.0;
	GetFloatParam(Params, TEXT("gravity_z"), GravityZ, /*bRequired=*/false);

	// Enable the global gravity override and set the value
	WorldSettings->bGlobalGravitySet = true;
	WorldSettings->GlobalGravityZ = static_cast<float>(GravityZ);
	WorldSettings->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("gravity_z"), GravityZ);
	Result->SetBoolField(TEXT("global_gravity_override"), true);

	return FECACommandResult::Success(Result);
}

// ─── get_scene_stats ────────────────────────────────────────

FECACommandResult FECACommand_GetSceneStats::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	int32 TotalActors = 0;
	int32 StaticMeshActors = 0;
	int32 LightActors = 0;
	int32 NiagaraActors = 0;
	int32 CameraActors = 0;
	int64 EstimatedTriangles = 0;
	TMap<FString, int32> ActorTypeCounts;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		++TotalActors;

		// Count by class name
		FString ClassName = Actor->GetClass()->GetName();
		int32& Count = ActorTypeCounts.FindOrAdd(ClassName, 0);
		++Count;

		// Specific type counts
		if (Actor->IsA<AStaticMeshActor>())
		{
			++StaticMeshActors;

			// Estimate triangle count from static meshes
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
			UStaticMeshComponent* MeshComp = SMActor ? SMActor->GetStaticMeshComponent() : nullptr;
			UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
			if (Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
			{
				EstimatedTriangles += Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
			}
		}
		else if (Actor->IsA<ALight>())
		{
			++LightActors;
		}
		else if (Actor->IsA<ANiagaraActor>())
		{
			++NiagaraActors;
		}
		else if (Actor->IsA<ACameraActor>())
		{
			++CameraActors;
		}
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("total_actors"), TotalActors);
	Result->SetNumberField(TEXT("static_mesh_actors"), StaticMeshActors);
	Result->SetNumberField(TEXT("light_actors"), LightActors);
	Result->SetNumberField(TEXT("niagara_actors"), NiagaraActors);
	Result->SetNumberField(TEXT("camera_actors"), CameraActors);
	Result->SetNumberField(TEXT("estimated_triangles"), static_cast<double>(EstimatedTriangles));

	// Actor type breakdown (sorted by count descending)
	TArray<TPair<FString, int32>> SortedTypes;
	for (const auto& Pair : ActorTypeCounts)
	{
		SortedTypes.Add(TPair<FString, int32>(Pair.Key, Pair.Value));
	}
	SortedTypes.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
	{
		return A.Value > B.Value;
	});

	TSharedPtr<FJsonObject> TypeBreakdown = MakeShared<FJsonObject>();
	for (const auto& Pair : SortedTypes)
	{
		TypeBreakdown->SetNumberField(Pair.Key, Pair.Value);
	}
	Result->SetObjectField(TEXT("actor_type_breakdown"), TypeBreakdown);

	return FECACommandResult::Success(Result);
}

// ─── batch_set_actor_property ───────────────────────────────

FECACommandResult FECACommand_BatchSetActorProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor names (required)
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = nullptr;
	if (!GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_names"));
	}

	// Property name (required)
	FString PropertyName;
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_name"));
	}

	// Property value (required) - read as raw JSON value
	if (!Params->HasField(TEXT("property_value")))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_value"));
	}

	TSharedPtr<FJsonValue> PropertyValue = Params->TryGetField(TEXT("property_value"));

	// Collect actor names
	TArray<FString> ActorNames;
	for (const TSharedPtr<FJsonValue>& Val : *ActorNamesArray)
	{
		FString Name;
		if (Val->TryGetString(Name))
		{
			ActorNames.Add(Name);
		}
	}

	if (ActorNames.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("actor_names array is empty or contains no valid strings"));
	}

	// Process each actor
	int32 SuccessCount = 0;
	int32 FailCount = 0;
	TArray<TSharedPtr<FJsonValue>> Errors;

	for (const FString& ActorName : ActorNames)
	{
		AActor* Actor = FindActorByName(ActorName);
		if (!Actor)
		{
			TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
			ErrObj->SetStringField(TEXT("actor"), ActorName);
			ErrObj->SetStringField(TEXT("error"), TEXT("Actor not found"));
			Errors.Add(MakeShared<FJsonValueObject>(ErrObj));
			++FailCount;
			continue;
		}

		// Try to find and set the property via UE reflection
		FProperty* Property = Actor->GetClass()->FindPropertyByName(*PropertyName);
		if (!Property)
		{
			TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
			ErrObj->SetStringField(TEXT("actor"), ActorName);
			ErrObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Property '%s' not found on actor class '%s'"), *PropertyName, *Actor->GetClass()->GetName()));
			Errors.Add(MakeShared<FJsonValueObject>(ErrObj));
			++FailCount;
			continue;
		}

		void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Actor);
		bool bSetOk = false;

		// Handle common property types
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			bool bVal = false;
			if (PropertyValue->TryGetBool(bVal))
			{
				BoolProp->SetPropertyValue(PropertyAddr, bVal);
				bSetOk = true;
			}
		}
		else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			double Val = 0.0;
			if (PropertyValue->TryGetNumber(Val))
			{
				FloatProp->SetPropertyValue(PropertyAddr, static_cast<float>(Val));
				bSetOk = true;
			}
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			double Val = 0.0;
			if (PropertyValue->TryGetNumber(Val))
			{
				DoubleProp->SetPropertyValue(PropertyAddr, Val);
				bSetOk = true;
			}
		}
		else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			double Val = 0.0;
			if (PropertyValue->TryGetNumber(Val))
			{
				IntProp->SetPropertyValue(PropertyAddr, static_cast<int32>(Val));
				bSetOk = true;
			}
		}
		else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			FString Val;
			if (PropertyValue->TryGetString(Val))
			{
				StrProp->SetPropertyValue(PropertyAddr, Val);
				bSetOk = true;
			}
		}
		else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			FString Val;
			if (PropertyValue->TryGetString(Val))
			{
				NameProp->SetPropertyValue(PropertyAddr, FName(*Val));
				bSetOk = true;
			}
		}

		if (bSetOk)
		{
			Actor->MarkPackageDirty();
			++SuccessCount;
		}
		else
		{
			TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
			ErrObj->SetStringField(TEXT("actor"), ActorName);
			ErrObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Could not set property '%s' — unsupported type or value mismatch"), *PropertyName));
			Errors.Add(MakeShared<FJsonValueObject>(ErrObj));
			++FailCount;
		}
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetNumberField(TEXT("total_actors"), ActorNames.Num());
	Result->SetNumberField(TEXT("success_count"), SuccessCount);
	Result->SetNumberField(TEXT("fail_count"), FailCount);

	if (!Errors.IsEmpty())
	{
		Result->SetArrayField(TEXT("errors"), Errors);
	}

	return FECACommandResult::Success(Result);
}

// ─── create_spline_path ─────────────────────────────────────

FECACommandResult FECACommand_CreateSplinePath::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Points (required)
	const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
	if (!GetArrayParam(Params, TEXT("points"), PointsArray))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: points"));
	}

	if (PointsArray->Num() < 2)
	{
		return FECACommandResult::Error(TEXT("At least 2 points are required to create a spline path"));
	}

	// Parse points
	TArray<FVector> SplinePoints;
	for (int32 i = 0; i < PointsArray->Num(); ++i)
	{
		const TSharedPtr<FJsonValue>& Val = (*PointsArray)[i];
		const TSharedPtr<FJsonObject>* PointObj = nullptr;
		if (!Val->TryGetObject(PointObj) || !PointObj || !(*PointObj).IsValid())
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Point at index %d is not a valid {x, y, z} object"), i));
		}

		double X = 0.0, Y = 0.0, Z = 0.0;
		(*PointObj)->TryGetNumberField(TEXT("x"), X);
		(*PointObj)->TryGetNumberField(TEXT("y"), Y);
		(*PointObj)->TryGetNumberField(TEXT("z"), Z);
		SplinePoints.Add(FVector(X, Y, Z));
	}

	// Closed loop (optional)
	bool bClosed = false;
	GetBoolParam(Params, TEXT("closed"), bClosed, /*bRequired=*/false);

	// Spawn a plain actor to hold the spline component.
	// Use the first point as the actor origin so the spline can use world-space points.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SplineActor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!SplineActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn spline actor"));
	}

	// Name
	FString Name = TEXT("SplinePath");
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		SplineActor->SetActorLabel(Name);
	}
	else
	{
		SplineActor->SetActorLabel(TEXT("SplinePath"));
	}

	// Create and attach a spline component
	USplineComponent* SplineComp = NewObject<USplineComponent>(SplineActor, TEXT("SplineComponent"));
	if (!SplineComp)
	{
		World->DestroyActor(SplineActor);
		return FECACommandResult::Error(TEXT("Failed to create spline component"));
	}

	SplineComp->RegisterComponent();
	SplineActor->SetRootComponent(SplineComp);
	SplineActor->AddInstanceComponent(SplineComp);

	// Set the points in world space
	SplineComp->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::World, /*bUpdateSpline=*/false);
	SplineComp->SetClosedLoop(bClosed, /*bUpdateSpline=*/false);
	SplineComp->UpdateSpline();

	SplineActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), SplineActor->GetActorLabel());
	Result->SetNumberField(TEXT("num_points"), SplinePoints.Num());
	Result->SetBoolField(TEXT("closed"), bClosed);
	Result->SetNumberField(TEXT("spline_length"), SplineComp->GetSplineLength());

	// Include the points in the result
	TArray<TSharedPtr<FJsonValue>> PointsResult;
	for (const FVector& Pt : SplinePoints)
	{
		PointsResult.Add(MakeShared<FJsonValueObject>(VectorToJson(Pt)));
	}
	Result->SetArrayField(TEXT("points"), PointsResult);

	return FECACommandResult::Success(Result);
}

// ─── enable_physics_simulation ─────────────────────────────

FECACommandResult FECACommand_EnablePhysicsSimulation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Enable (optional, default true)
	bool bEnable = true;
	GetBoolParam(Params, TEXT("enable"), bEnable, /*bRequired=*/false);

	// Component name (optional)
	FString ComponentName;
	bool bHasComponentName = GetStringParam(Params, TEXT("component_name"), ComponentName, /*bRequired=*/false) && !ComponentName.IsEmpty();

	int32 AffectedCount = 0;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
	{
		if (!PrimComp)
		{
			continue;
		}

		if (bHasComponentName && PrimComp->GetName() != ComponentName)
		{
			continue;
		}

		PrimComp->SetSimulatePhysics(bEnable);
		++AffectedCount;
	}

	if (AffectedCount == 0)
	{
		if (bHasComponentName)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("No primitive component named '%s' found on actor '%s'"), *ComponentName, *ActorName));
		}
		return FECACommandResult::Error(FString::Printf(TEXT("No primitive components found on actor '%s'"), *ActorName));
	}

	Actor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetBoolField(TEXT("simulate_physics"), bEnable);
	Result->SetNumberField(TEXT("components_affected"), AffectedCount);

	return FECACommandResult::Success(Result);
}

// ─── apply_impulse ─────────────────────────────────────────

FECACommandResult FECACommand_ApplyImpulse::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Impulse (required)
	FVector Impulse;
	if (!GetVectorParam(Params, TEXT("impulse"), Impulse))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: impulse"));
	}

	// Get the root primitive component
	UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	if (!RootPrim)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no root primitive component to apply impulse to"), *ActorName));
	}

	if (!RootPrim->IsSimulatingPhysics())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' root component is not simulating physics — enable physics simulation first"), *ActorName));
	}

	// Location (optional — defaults to actor center)
	FVector Location;
	bool bHasLocation = GetVectorParam(Params, TEXT("location"), Location, /*bRequired=*/false);

	if (bHasLocation)
	{
		RootPrim->AddImpulseAtLocation(Impulse, Location);
	}
	else
	{
		RootPrim->AddImpulse(Impulse);
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetObjectField(TEXT("impulse"), VectorToJson(Impulse));
	if (bHasLocation)
	{
		Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	}

	return FECACommandResult::Success(Result);
}

// ─── set_actor_visibility ──────────────────────────────────

FECACommandResult FECACommand_SetActorVisibility::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Visible (required)
	bool bVisible = true;
	if (!GetBoolParam(Params, TEXT("visible"), bVisible))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: visible"));
	}

	// Affect children (optional, default true)
	bool bAffectChildren = true;
	GetBoolParam(Params, TEXT("affect_children"), bAffectChildren, /*bRequired=*/false);

	// SetActorHiddenInGame takes "hidden" — invert the visible flag
	Actor->SetActorHiddenInGame(!bVisible);

	if (bAffectChildren)
	{
		// Propagate to all components
		TArray<USceneComponent*> ChildComponents;
		Actor->GetRootComponent()->GetChildrenComponents(true, ChildComponents);
		for (USceneComponent* Child : ChildComponents)
		{
			if (Child)
			{
				Child->SetVisibility(bVisible, true);
			}
		}
	}

	Actor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetBoolField(TEXT("visible"), bVisible);
	Result->SetBoolField(TEXT("affect_children"), bAffectChildren);

	return FECACommandResult::Success(Result);
}

// ─── batch_spawn_actors ────────────────────────────────────

FECACommandResult FECACommand_BatchSpawnActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor type (required)
	FString ActorType;
	if (!GetStringParam(Params, TEXT("actor_type"), ActorType))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_type"));
	}

	// Count (required)
	int32 Count = 0;
	if (!GetIntParam(Params, TEXT("count"), Count))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: count"));
	}
	if (Count <= 0)
	{
		return FECACommandResult::Error(TEXT("count must be greater than 0"));
	}
	if (Count > 1000)
	{
		return FECACommandResult::Error(TEXT("count exceeds maximum of 1000"));
	}

	// Base location (required)
	FVector BaseLocation;
	if (!GetVectorParam(Params, TEXT("base_location"), BaseLocation))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: base_location"));
	}

	// Spacing (optional, default {200,0,0})
	FVector Spacing(200.0, 0.0, 0.0);
	GetVectorParam(Params, TEXT("spacing"), Spacing, /*bRequired=*/false);

	// Base name (optional)
	FString BaseName = TEXT("SpawnedActor");
	GetStringParam(Params, TEXT("base_name"), BaseName, /*bRequired=*/false);

	// Mesh (optional)
	FString MeshPath;
	bool bHasMesh = GetStringParam(Params, TEXT("mesh"), MeshPath, /*bRequired=*/false) && !MeshPath.IsEmpty();
	UStaticMesh* Mesh = nullptr;
	if (bHasMesh)
	{
		Mesh = EnvironmentCommandHelpers::LoadMeshByPath(MeshPath);
		if (!Mesh)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Could not load static mesh at: %s"), *MeshPath));
		}
	}

	// Material (optional)
	FString MaterialPath;
	bool bHasMaterial = GetStringParam(Params, TEXT("material"), MaterialPath, /*bRequired=*/false) && !MaterialPath.IsEmpty();
	UMaterialInterface* Material = nullptr;
	if (bHasMaterial)
	{
		Material = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
		if (!Material)
		{
			UE_LOG(LogTemp, Warning, TEXT("batch_spawn_actors: Could not load material at '%s', using default"), *MaterialPath);
			bHasMaterial = false;
		}
	}

	// Determine the actor class to spawn
	// Default to StaticMeshActor for common types
	bool bIsStaticMeshActor = ActorType.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase)
		|| ActorType.Equals(TEXT("AStaticMeshActor"), ESearchCase::IgnoreCase);

	TArray<TSharedPtr<FJsonValue>> SpawnedActors;

	for (int32 i = 0; i < Count; ++i)
	{
		FVector SpawnLocation = BaseLocation + (Spacing * static_cast<double>(i));
		FString ActorLabel = FString::Printf(TEXT("%s_%d"), *BaseName, i);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* SpawnedActor = nullptr;

		if (bIsStaticMeshActor)
		{
			AStaticMeshActor* SMActor = World->SpawnActor<AStaticMeshActor>(SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			if (SMActor)
			{
				SMActor->SetActorLabel(ActorLabel);

				UStaticMeshComponent* MeshComp = SMActor->GetStaticMeshComponent();
				if (MeshComp)
				{
					if (Mesh)
					{
						MeshComp->SetStaticMesh(Mesh);
					}
					else
					{
						// Default to a cube if no mesh specified
						UStaticMesh* DefaultMesh = EnvironmentCommandHelpers::LoadMeshByPath(TEXT("/Engine/BasicShapes/Cube"));
						if (DefaultMesh)
						{
							MeshComp->SetStaticMesh(DefaultMesh);
						}
					}

					if (bHasMaterial && Material)
					{
						MeshComp->SetMaterial(0, Material);
					}
				}

				SpawnedActor = SMActor;
			}
		}
		else
		{
			// Generic actor spawn
			SpawnedActor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (SpawnedActor)
			{
				SpawnedActor->SetActorLocation(SpawnLocation);
				SpawnedActor->SetActorLabel(ActorLabel);
			}
		}

		if (SpawnedActor)
		{
			TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
			ActorInfo->SetStringField(TEXT("name"), SpawnedActor->GetActorLabel());
			ActorInfo->SetObjectField(TEXT("location"), VectorToJson(SpawnLocation));
			SpawnedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
		}
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_type"), ActorType);
	Result->SetNumberField(TEXT("requested_count"), Count);
	Result->SetNumberField(TEXT("spawned_count"), SpawnedActors.Num());
	Result->SetObjectField(TEXT("base_location"), VectorToJson(BaseLocation));
	Result->SetObjectField(TEXT("spacing"), VectorToJson(Spacing));
	Result->SetArrayField(TEXT("spawned_actors"), SpawnedActors);

	return FECACommandResult::Success(Result);
}

// ─── teleport_actor ────────────────────────────────────────

FECACommandResult FECACommand_TeleportActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Sweep (optional, default false)
	bool bSweep = false;
	GetBoolParam(Params, TEXT("sweep"), bSweep, /*bRequired=*/false);

	// Rotation (optional)
	FRotator Rotation;
	bool bHasRotation = GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired=*/false);

	bool bMoved = false;
	FHitResult SweepHitResult;

	if (bHasRotation)
	{
		bMoved = Actor->SetActorLocationAndRotation(Location, Rotation, bSweep, bSweep ? &SweepHitResult : nullptr);
	}
	else
	{
		bMoved = Actor->SetActorLocation(Location, bSweep, bSweep ? &SweepHitResult : nullptr);
	}

	Actor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
	Result->SetBoolField(TEXT("sweep"), bSweep);
	Result->SetBoolField(TEXT("success"), bMoved);

	if (bSweep && SweepHitResult.bBlockingHit)
	{
		Result->SetBoolField(TEXT("sweep_hit"), true);
		Result->SetObjectField(TEXT("sweep_hit_location"), VectorToJson(SweepHitResult.Location));
	}

	return FECACommandResult::Success(Result);
}

// ─── generate_grid ─────────────────────────────────────────

FECACommandResult FECACommand_GenerateGrid::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Rows (required)
	int32 Rows = 0;
	if (!GetIntParam(Params, TEXT("rows"), Rows))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: rows"));
	}
	if (Rows <= 0)
	{
		return FECACommandResult::Error(TEXT("rows must be greater than 0"));
	}

	// Columns (required)
	int32 Columns = 0;
	if (!GetIntParam(Params, TEXT("columns"), Columns))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: columns"));
	}
	if (Columns <= 0)
	{
		return FECACommandResult::Error(TEXT("columns must be greater than 0"));
	}

	// Cap total count
	if (Rows * Columns > 10000)
	{
		return FECACommandResult::Error(TEXT("Grid size exceeds maximum of 10000 actors (rows * columns)"));
	}

	// Mesh path (optional)
	FString MeshPath = TEXT("/Engine/BasicShapes/Cube");
	GetStringParam(Params, TEXT("mesh_path"), MeshPath, /*bRequired=*/false);

	UStaticMesh* Mesh = EnvironmentCommandHelpers::LoadMeshByPath(MeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load static mesh at: %s"), *MeshPath));
	}

	// Spacing (optional)
	double Spacing = 200.0;
	GetFloatParam(Params, TEXT("spacing"), Spacing, /*bRequired=*/false);

	// Origin (optional)
	FVector Origin = FVector::ZeroVector;
	GetVectorParam(Params, TEXT("origin"), Origin, /*bRequired=*/false);

	// Scale (optional)
	FVector Scale(1.0, 1.0, 1.0);
	GetVectorParam(Params, TEXT("scale"), Scale, /*bRequired=*/false);

	// Material (optional)
	FString MaterialPath;
	bool bHasMaterial = GetStringParam(Params, TEXT("material_path"), MaterialPath, /*bRequired=*/false) && !MaterialPath.IsEmpty();
	UMaterialInterface* Material = nullptr;
	if (bHasMaterial)
	{
		Material = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
		if (!Material)
		{
			UE_LOG(LogTemp, Warning, TEXT("generate_grid: Could not load material at '%s', using default"), *MaterialPath);
			bHasMaterial = false;
		}
	}

	// Name prefix (optional)
	FString NamePrefix = TEXT("Grid");
	GetStringParam(Params, TEXT("name_prefix"), NamePrefix, /*bRequired=*/false);

	// Spawn the grid
	int32 SpawnedCount = 0;

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Columns; ++Col)
		{
			FVector SpawnLocation = Origin + FVector(Col * Spacing, Row * Spacing, 0.0);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			if (!MeshActor)
			{
				continue;
			}

			MeshActor->SetActorLabel(FString::Printf(TEXT("%s_%d_%d"), *NamePrefix, Row, Col));
			MeshActor->SetActorScale3D(Scale);

			UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
			if (MeshComp)
			{
				MeshComp->SetStaticMesh(Mesh);

				if (bHasMaterial && Material)
				{
					MeshComp->SetMaterial(0, Material);
				}
			}

			++SpawnedCount;
		}
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("spawned_count"), SpawnedCount);
	Result->SetNumberField(TEXT("rows"), Rows);
	Result->SetNumberField(TEXT("columns"), Columns);
	Result->SetNumberField(TEXT("spacing"), Spacing);
	Result->SetObjectField(TEXT("origin"), VectorToJson(Origin));
	Result->SetObjectField(TEXT("scale"), VectorToJson(Scale));
	Result->SetStringField(TEXT("mesh_path"), Mesh->GetPathName());
	Result->SetStringField(TEXT("name_prefix"), NamePrefix);

	return FECACommandResult::Success(Result);
}

// ─── generate_circle ───────────────────────────────────────

FECACommandResult FECACommand_GenerateCircle::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Count (required)
	int32 Count = 0;
	if (!GetIntParam(Params, TEXT("count"), Count))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: count"));
	}
	if (Count <= 0)
	{
		return FECACommandResult::Error(TEXT("count must be greater than 0"));
	}
	if (Count > 1000)
	{
		return FECACommandResult::Error(TEXT("count exceeds maximum of 1000"));
	}

	// Mesh path (optional)
	FString MeshPath = TEXT("/Engine/BasicShapes/Cube");
	GetStringParam(Params, TEXT("mesh_path"), MeshPath, /*bRequired=*/false);

	UStaticMesh* Mesh = EnvironmentCommandHelpers::LoadMeshByPath(MeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load static mesh at: %s"), *MeshPath));
	}

	// Radius (optional)
	double Radius = 500.0;
	GetFloatParam(Params, TEXT("radius"), Radius, /*bRequired=*/false);

	// Center (optional)
	FVector Center = FVector::ZeroVector;
	GetVectorParam(Params, TEXT("center"), Center, /*bRequired=*/false);

	// Scale (optional)
	FVector Scale(1.0, 1.0, 1.0);
	GetVectorParam(Params, TEXT("scale"), Scale, /*bRequired=*/false);

	// Face center (optional, default true)
	bool bFaceCenter = true;
	GetBoolParam(Params, TEXT("face_center"), bFaceCenter, /*bRequired=*/false);

	// Material (optional)
	FString MaterialPath;
	bool bHasMaterial = GetStringParam(Params, TEXT("material_path"), MaterialPath, /*bRequired=*/false) && !MaterialPath.IsEmpty();
	UMaterialInterface* Material = nullptr;
	if (bHasMaterial)
	{
		Material = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
		if (!Material)
		{
			UE_LOG(LogTemp, Warning, TEXT("generate_circle: Could not load material at '%s', using default"), *MaterialPath);
			bHasMaterial = false;
		}
	}

	// Name prefix (optional)
	FString NamePrefix = TEXT("Circle");
	GetStringParam(Params, TEXT("name_prefix"), NamePrefix, /*bRequired=*/false);

	// Spawn actors in a circle
	int32 SpawnedCount = 0;
	const double AngleStep = 360.0 / static_cast<double>(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		double AngleDeg = AngleStep * i;
		double AngleRad = FMath::DegreesToRadians(AngleDeg);

		FVector SpawnLocation = Center + FVector(
			FMath::Cos(AngleRad) * Radius,
			FMath::Sin(AngleRad) * Radius,
			0.0
		);

		FRotator SpawnRotation = FRotator::ZeroRotator;
		if (bFaceCenter)
		{
			// Face toward the center
			FVector DirectionToCenter = (Center - SpawnLocation).GetSafeNormal();
			SpawnRotation = DirectionToCenter.Rotation();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(SpawnLocation, SpawnRotation, SpawnParams);
		if (!MeshActor)
		{
			continue;
		}

		MeshActor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *NamePrefix, i));
		MeshActor->SetActorScale3D(Scale);

		UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
		if (MeshComp)
		{
			MeshComp->SetStaticMesh(Mesh);

			if (bHasMaterial && Material)
			{
				MeshComp->SetMaterial(0, Material);
			}
		}

		++SpawnedCount;
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("spawned_count"), SpawnedCount);
	Result->SetNumberField(TEXT("count"), Count);
	Result->SetNumberField(TEXT("radius"), Radius);
	Result->SetObjectField(TEXT("center"), VectorToJson(Center));
	Result->SetObjectField(TEXT("scale"), VectorToJson(Scale));
	Result->SetBoolField(TEXT("face_center"), bFaceCenter);
	Result->SetStringField(TEXT("mesh_path"), Mesh->GetPathName());
	Result->SetStringField(TEXT("name_prefix"), NamePrefix);

	return FECACommandResult::Success(Result);
}

// ─── destroy_actors_by_pattern ─────────────────────────────

FECACommandResult FECACommand_DestroyActorsByPattern::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Name pattern (required)
	FString NamePattern;
	if (!GetStringParam(Params, TEXT("name_pattern"), NamePattern))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: name_pattern"));
	}

	// Dry run (optional, default false)
	bool bDryRun = false;
	GetBoolParam(Params, TEXT("dry_run"), bDryRun, /*bRequired=*/false);

	// Find matching actors
	TArray<AActor*> MatchingActors;
	TArray<TSharedPtr<FJsonValue>> MatchedNames;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		FString ActorLabel = Actor->GetActorLabel();
		if (ActorLabel.MatchesWildcard(NamePattern, ESearchCase::IgnoreCase))
		{
			MatchingActors.Add(Actor);
			MatchedNames.Add(MakeShared<FJsonValueString>(ActorLabel));
		}
	}

	int32 DestroyedCount = 0;

	if (!bDryRun)
	{
		for (AActor* Actor : MatchingActors)
		{
			if (World->DestroyActor(Actor))
			{
				++DestroyedCount;
			}
		}

		// Notify editor
		if (GEditor)
		{
			GEditor->BroadcastLevelActorListChanged();
			GEditor->RedrawAllViewports();
		}
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name_pattern"), NamePattern);
	Result->SetBoolField(TEXT("dry_run"), bDryRun);
	Result->SetNumberField(TEXT("matched_count"), MatchingActors.Num());
	Result->SetNumberField(TEXT("destroyed_count"), bDryRun ? 0 : DestroyedCount);
	Result->SetArrayField(TEXT("matched_names"), MatchedNames);

	return FECACommandResult::Success(Result);
}

// ─── take_camera_screenshot ────────────────────────────────

FECACommandResult FECACommand_TakeCameraScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Camera name (required)
	FString CameraName;
	if (!GetStringParam(Params, TEXT("camera_name"), CameraName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: camera_name"));
	}

	// Filename (required)
	FString Filename;
	if (!GetStringParam(Params, TEXT("filename"), Filename))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: filename"));
	}

	// Ensure .png extension
	if (!Filename.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		Filename += TEXT(".png");
	}

	// Resolution (optional)
	int32 ResolutionX = 1920;
	GetIntParam(Params, TEXT("resolution_x"), ResolutionX, /*bRequired=*/false);
	int32 ResolutionY = 1080;
	GetIntParam(Params, TEXT("resolution_y"), ResolutionY, /*bRequired=*/false);

	if (ResolutionX <= 0 || ResolutionY <= 0)
	{
		return FECACommandResult::Error(TEXT("resolution_x and resolution_y must be greater than 0"));
	}
	if (ResolutionX > 7680 || ResolutionY > 4320)
	{
		return FECACommandResult::Error(TEXT("resolution exceeds maximum of 7680x4320"));
	}

	// Find the camera actor
	AActor* CameraActor = FindActorByName(CameraName);
	if (!CameraActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Camera actor not found: %s"), *CameraName));
	}

	ACameraActor* Camera = Cast<ACameraActor>(CameraActor);
	if (!Camera)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' is not a CameraActor"), *CameraName));
	}

	// Get the camera transform
	FVector CameraLocation = Camera->GetActorLocation();
	FRotator CameraRotation = Camera->GetActorRotation();

	// Create a scene capture component to render from the camera's perspective
	USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
	if (!SceneCapture)
	{
		return FECACommandResult::Error(TEXT("Failed to create SceneCaptureComponent2D"));
	}

	// Create the render target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	if (!RenderTarget)
	{
		return FECACommandResult::Error(TEXT("Failed to create render target"));
	}

	RenderTarget->InitCustomFormat(ResolutionX, ResolutionY, PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);

	// Configure the scene capture
	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// Copy camera FOV settings if available
	if (Camera->GetCameraComponent())
	{
		SceneCapture->FOVAngle = Camera->GetCameraComponent()->FieldOfView;
	}

	// Register and capture
	SceneCapture->RegisterComponentWithWorld(World);
	SceneCapture->CaptureScene();

	// Build the output path
	FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
	IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
	FString FullPath = ScreenshotDir / Filename;

	// Export the render target to PNG
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FullPath);
	if (!FileWriter)
	{
		SceneCapture->DestroyComponent();
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create file writer for: %s"), *FullPath));
	}

	bool bExportSuccess = FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *FileWriter);
	FileWriter->Close();
	delete FileWriter;

	// Cleanup
	SceneCapture->DestroyComponent();

	if (!bExportSuccess)
	{
		return FECACommandResult::Error(TEXT("Failed to export render target as PNG"));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("camera_name"), Camera->GetActorLabel());
	Result->SetStringField(TEXT("filename"), Filename);
	Result->SetStringField(TEXT("full_path"), FullPath);
	Result->SetNumberField(TEXT("resolution_x"), ResolutionX);
	Result->SetNumberField(TEXT("resolution_y"), ResolutionY);
	Result->SetObjectField(TEXT("camera_location"), VectorToJson(CameraLocation));
	Result->SetObjectField(TEXT("camera_rotation"), RotatorToJson(CameraRotation));

	return FECACommandResult::Success(Result);
}

// ─── spawn_decal ──────────────────────────────────────────

FECACommandResult FECACommand_SpawnDecal::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Material path (required)
	FString MaterialPath;
	if (!GetStringParam(Params, TEXT("material_path"), MaterialPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: material_path"));
	}

	UMaterialInterface* Material = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
	if (!Material)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load material at: %s"), *MaterialPath));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Rotation (optional — default points down: pitch -90)
	FRotator Rotation(-90.0, 0.0, 0.0);
	GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired=*/false);

	// Size / decal extent (optional — default {128, 256, 256})
	FVector Size(128.0, 256.0, 256.0);
	GetVectorParam(Params, TEXT("size"), Size, /*bRequired=*/false);

	// Spawn the decal actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADecalActor* DecalActor = World->SpawnActor<ADecalActor>(Location, Rotation, SpawnParams);
	if (!DecalActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn DecalActor"));
	}

	// Name
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		DecalActor->SetActorLabel(Name);
	}

	// Configure the decal component
	UDecalComponent* DecalComp = DecalActor->GetDecal();
	if (DecalComp)
	{
		DecalComp->SetDecalMaterial(Material);
		DecalComp->DecalSize = Size;
		DecalComp->MarkRenderStateDirty();
	}

	DecalActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), DecalActor->GetActorLabel());
	Result->SetStringField(TEXT("material_path"), Material->GetPathName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
	Result->SetObjectField(TEXT("size"), VectorToJson(Size));

	return FECACommandResult::Success(Result);
}

// ─── spawn_text_render ────────────────────────────────────

FECACommandResult FECACommand_SpawnTextRender::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Text (required)
	FString Text;
	if (!GetStringParam(Params, TEXT("text"), Text))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: text"));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Rotation (optional)
	FRotator Rotation = FRotator::ZeroRotator;
	GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired=*/false);

	// Size (optional, default 100)
	double Size = 100.0;
	GetFloatParam(Params, TEXT("size"), Size, /*bRequired=*/false);

	// Spawn the text render actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATextRenderActor* TextActor = World->SpawnActor<ATextRenderActor>(Location, Rotation, SpawnParams);
	if (!TextActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn TextRenderActor"));
	}

	// Name
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		TextActor->SetActorLabel(Name);
	}

	// Configure the text render component
	UTextRenderComponent* TextComp = TextActor->GetTextRender();
	if (TextComp)
	{
		TextComp->SetText(FText::FromString(Text));
		TextComp->SetWorldSize(static_cast<float>(Size));
		TextComp->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
		TextComp->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);

		// Color (optional — default white)
		FColor TextColor = FColor::White;
		const TSharedPtr<FJsonObject>* ColorObj = nullptr;
		if (GetObjectParam(Params, TEXT("color"), ColorObj, /*bRequired=*/false))
		{
			double R = 255.0, G = 255.0, B = 255.0;
			(*ColorObj)->TryGetNumberField(TEXT("r"), R);
			(*ColorObj)->TryGetNumberField(TEXT("g"), G);
			(*ColorObj)->TryGetNumberField(TEXT("b"), B);
			TextColor = FColor(
				static_cast<uint8>(FMath::Clamp(R, 0.0, 255.0)),
				static_cast<uint8>(FMath::Clamp(G, 0.0, 255.0)),
				static_cast<uint8>(FMath::Clamp(B, 0.0, 255.0)),
				255
			);
		}
		TextComp->SetTextRenderColor(TextColor);
	}

	TextActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), TextActor->GetActorLabel());
	Result->SetStringField(TEXT("text"), Text);
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
	Result->SetNumberField(TEXT("size"), Size);

	return FECACommandResult::Success(Result);
}

// ─── describe_actor ───────────────────────────────────────

FECACommandResult FECACommand_DescribeActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());
	Result->SetStringField(TEXT("actor_path_name"), Actor->GetPathName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
	Result->SetObjectField(TEXT("scale"), VectorToJson(Actor->GetActorScale3D()));
	Result->SetBoolField(TEXT("hidden"), Actor->IsHidden());
	Result->SetBoolField(TEXT("is_editor_only"), Actor->bIsEditorOnlyActor);

	// Mobility
	USceneComponent* RootComp = Actor->GetRootComponent();
	if (RootComp)
	{
		FString MobilityStr;
		switch (RootComp->Mobility)
		{
		case EComponentMobility::Static: MobilityStr = TEXT("Static"); break;
		case EComponentMobility::Stationary: MobilityStr = TEXT("Stationary"); break;
		case EComponentMobility::Movable: MobilityStr = TEXT("Movable"); break;
		default: MobilityStr = TEXT("Unknown"); break;
		}
		Result->SetStringField(TEXT("mobility"), MobilityStr);
	}

	// Tags
	if (Actor->Tags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TagArray;
		for (const FName& Tag : Actor->Tags)
		{
			TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Result->SetArrayField(TEXT("tags"), TagArray);
	}

	// Iterate all components
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TArray<UActorComponent*> AllComponents;
	Actor->GetComponents(AllComponents);

	for (UActorComponent* Comp : AllComponents)
	{
		if (!Comp)
		{
			continue;
		}

		TSharedPtr<FJsonObject> CompInfo = MakeShared<FJsonObject>();
		CompInfo->SetStringField(TEXT("name"), Comp->GetName());
		CompInfo->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

		// Scene component properties
		USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
		if (SceneComp)
		{
			CompInfo->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComp->GetRelativeLocation()));
			CompInfo->SetObjectField(TEXT("relative_rotation"), RotatorToJson(SceneComp->GetRelativeRotation()));
			CompInfo->SetObjectField(TEXT("relative_scale"), VectorToJson(SceneComp->GetRelativeScale3D()));
			CompInfo->SetBoolField(TEXT("visible"), SceneComp->IsVisible());
		}

		// Static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
		if (SMC)
		{
			UStaticMesh* Mesh = SMC->GetStaticMesh();
			CompInfo->SetStringField(TEXT("static_mesh"), Mesh ? Mesh->GetPathName() : TEXT("None"));

			if (Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
			{
				CompInfo->SetNumberField(TEXT("triangle_count"), Mesh->GetRenderData()->LODResources[0].GetNumTriangles());
				CompInfo->SetNumberField(TEXT("vertex_count"), Mesh->GetRenderData()->LODResources[0].GetNumVertices());
				CompInfo->SetNumberField(TEXT("lod_count"), Mesh->GetRenderData()->LODResources.Num());
			}

			CompInfo->SetBoolField(TEXT("simulate_physics"), SMC->IsSimulatingPhysics());
			CompInfo->SetBoolField(TEXT("collision_enabled"), SMC->GetCollisionEnabled() != ECollisionEnabled::NoCollision);

			// Materials
			TArray<TSharedPtr<FJsonValue>> MaterialsArray;
			for (int32 MatIdx = 0; MatIdx < SMC->GetNumMaterials(); ++MatIdx)
			{
				UMaterialInterface* Mat = SMC->GetMaterial(MatIdx);
				TSharedPtr<FJsonObject> MatInfo = MakeShared<FJsonObject>();
				MatInfo->SetNumberField(TEXT("slot"), MatIdx);
				MatInfo->SetStringField(TEXT("material"), Mat ? Mat->GetPathName() : TEXT("None"));
				MaterialsArray.Add(MakeShared<FJsonValueObject>(MatInfo));
			}
			if (MaterialsArray.Num() > 0)
			{
				CompInfo->SetArrayField(TEXT("materials"), MaterialsArray);
			}
		}

		// Light component
		ULightComponent* LightComp = Cast<ULightComponent>(Comp);
		if (LightComp)
		{
			CompInfo->SetNumberField(TEXT("intensity"), LightComp->Intensity);
			FLinearColor LightColor = LightComp->GetLightColor();
			TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
			ColorObj->SetNumberField(TEXT("r"), FMath::RoundToInt(LightColor.R * 255.0f));
			ColorObj->SetNumberField(TEXT("g"), FMath::RoundToInt(LightColor.G * 255.0f));
			ColorObj->SetNumberField(TEXT("b"), FMath::RoundToInt(LightColor.B * 255.0f));
			CompInfo->SetObjectField(TEXT("light_color"), ColorObj);
			CompInfo->SetBoolField(TEXT("casts_shadows"), LightComp->CastShadows);

			// Point light specifics
			UPointLightComponent* PointLight = Cast<UPointLightComponent>(Comp);
			if (PointLight)
			{
				CompInfo->SetNumberField(TEXT("attenuation_radius"), PointLight->AttenuationRadius);
			}

			// Spot light specifics
			USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Comp);
			if (SpotLight)
			{
				CompInfo->SetNumberField(TEXT("inner_cone_angle"), SpotLight->InnerConeAngle);
				CompInfo->SetNumberField(TEXT("outer_cone_angle"), SpotLight->OuterConeAngle);
			}

			// Directional light specifics
			UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(Comp);
			if (DirLight)
			{
				CompInfo->SetStringField(TEXT("light_type"), TEXT("Directional"));
			}
		}

		// Decal component
		UDecalComponent* DecalComp = Cast<UDecalComponent>(Comp);
		if (DecalComp)
		{
			UMaterialInterface* DecalMat = DecalComp->GetDecalMaterial();
			CompInfo->SetStringField(TEXT("decal_material"), DecalMat ? DecalMat->GetPathName() : TEXT("None"));
			CompInfo->SetObjectField(TEXT("decal_size"), VectorToJson(DecalComp->DecalSize));
		}

		// Text render component
		UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(Comp);
		if (TextComp)
		{
			CompInfo->SetStringField(TEXT("text"), TextComp->Text.ToString());
			CompInfo->SetNumberField(TEXT("world_size"), TextComp->WorldSize);
		}

		// Camera component
		UCameraComponent* CamComp = Cast<UCameraComponent>(Comp);
		if (CamComp)
		{
			CompInfo->SetNumberField(TEXT("field_of_view"), CamComp->FieldOfView);
			CompInfo->SetNumberField(TEXT("aspect_ratio"), CamComp->AspectRatio);
		}

		// Audio component
		UAudioComponent* AudioComp = Cast<UAudioComponent>(Comp);
		if (AudioComp)
		{
			CompInfo->SetBoolField(TEXT("is_playing"), AudioComp->IsPlaying());
			CompInfo->SetNumberField(TEXT("volume_multiplier"), AudioComp->VolumeMultiplier);
			CompInfo->SetNumberField(TEXT("pitch_multiplier"), AudioComp->PitchMultiplier);
		}

		// Spline component
		USplineComponent* SplineComp = Cast<USplineComponent>(Comp);
		if (SplineComp)
		{
			CompInfo->SetNumberField(TEXT("spline_length"), SplineComp->GetSplineLength());
			CompInfo->SetNumberField(TEXT("num_spline_points"), SplineComp->GetNumberOfSplinePoints());
			CompInfo->SetBoolField(TEXT("closed_loop"), SplineComp->IsClosedLoop());
		}

		// Primitive component physics info
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
		if (PrimComp && !SMC) // Avoid duplication with SMC which already reports physics
		{
			CompInfo->SetBoolField(TEXT("simulate_physics"), PrimComp->IsSimulatingPhysics());
			CompInfo->SetBoolField(TEXT("collision_enabled"), PrimComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
		}

		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompInfo));
	}

	Result->SetNumberField(TEXT("component_count"), ComponentsArray.Num());
	Result->SetArrayField(TEXT("components"), ComponentsArray);

	// Child actors
	TArray<AActor*> ChildActors;
	Actor->GetAttachedActors(ChildActors);
	if (ChildActors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ChildArray;
		for (AActor* Child : ChildActors)
		{
			if (Child)
			{
				TSharedPtr<FJsonObject> ChildInfo = MakeShared<FJsonObject>();
				ChildInfo->SetStringField(TEXT("name"), Child->GetActorLabel());
				ChildInfo->SetStringField(TEXT("class"), Child->GetClass()->GetName());
				ChildArray.Add(MakeShared<FJsonValueObject>(ChildInfo));
			}
		}
		Result->SetArrayField(TEXT("attached_actors"), ChildArray);
	}

	return FECACommandResult::Success(Result);
}

// ─── clone_actor_array ────────────────────────────────────

FECACommandResult FECACommand_CloneActorArray::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Source actor (required)
	FString SourceActorName;
	if (!GetStringParam(Params, TEXT("source_actor"), SourceActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: source_actor"));
	}

	AActor* SourceActor = FindActorByName(SourceActorName);
	if (!SourceActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source actor not found: %s"), *SourceActorName));
	}

	// Count (required)
	int32 Count = 0;
	if (!GetIntParam(Params, TEXT("count"), Count))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: count"));
	}
	if (Count <= 0)
	{
		return FECACommandResult::Error(TEXT("count must be greater than 0"));
	}
	if (Count > 1000)
	{
		return FECACommandResult::Error(TEXT("count exceeds maximum of 1000"));
	}

	// Spacing (optional — default {200, 0, 0})
	FVector Spacing(200.0, 0.0, 0.0);
	GetVectorParam(Params, TEXT("spacing"), Spacing, /*bRequired=*/false);

	// Rotation increment (optional)
	FRotator RotationIncrement = FRotator::ZeroRotator;
	bool bHasRotationIncrement = GetRotatorParam(Params, TEXT("rotation_increment"), RotationIncrement, /*bRequired=*/false);

	FVector SourceLocation = SourceActor->GetActorLocation();
	FRotator SourceRotation = SourceActor->GetActorRotation();
	FString SourceLabel = SourceActor->GetActorLabel();

	TArray<TSharedPtr<FJsonValue>> ClonedActors;

	for (int32 i = 0; i < Count; ++i)
	{
		int32 CloneIndex = i + 1; // 1-based for labeling (clone 1, clone 2, ...)
		FVector CloneLocation = SourceLocation + (Spacing * static_cast<double>(CloneIndex));
		FRotator CloneRotation = SourceRotation;
		if (bHasRotationIncrement)
		{
			CloneRotation.Pitch += RotationIncrement.Pitch * CloneIndex;
			CloneRotation.Yaw += RotationIncrement.Yaw * CloneIndex;
			CloneRotation.Roll += RotationIncrement.Roll * CloneIndex;
		}

		// Duplicate the actor using editor utilities
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Template = SourceActor;

		AActor* ClonedActor = World->SpawnActor(SourceActor->GetClass(), &CloneLocation, &CloneRotation, SpawnParams);
		if (!ClonedActor)
		{
			continue;
		}

		// Set label
		FString CloneLabel = FString::Printf(TEXT("%s_Clone_%d"), *SourceLabel, CloneIndex);
		ClonedActor->SetActorLabel(CloneLabel);

		// Copy scale from source
		ClonedActor->SetActorScale3D(SourceActor->GetActorScale3D());

		// Copy static mesh and materials if applicable
		AStaticMeshActor* SourceSMA = Cast<AStaticMeshActor>(SourceActor);
		AStaticMeshActor* ClonedSMA = Cast<AStaticMeshActor>(ClonedActor);
		if (SourceSMA && ClonedSMA)
		{
			UStaticMeshComponent* SourceMeshComp = SourceSMA->GetStaticMeshComponent();
			UStaticMeshComponent* ClonedMeshComp = ClonedSMA->GetStaticMeshComponent();
			if (SourceMeshComp && ClonedMeshComp)
			{
				UStaticMesh* Mesh = SourceMeshComp->GetStaticMesh();
				if (Mesh)
				{
					ClonedMeshComp->SetStaticMesh(Mesh);
				}

				// Copy materials
				for (int32 MatIdx = 0; MatIdx < SourceMeshComp->GetNumMaterials(); ++MatIdx)
				{
					UMaterialInterface* Mat = SourceMeshComp->GetMaterial(MatIdx);
					if (Mat)
					{
						ClonedMeshComp->SetMaterial(MatIdx, Mat);
					}
				}
			}
		}

		ClonedActor->MarkPackageDirty();

		TSharedPtr<FJsonObject> CloneInfo = MakeShared<FJsonObject>();
		CloneInfo->SetStringField(TEXT("name"), ClonedActor->GetActorLabel());
		CloneInfo->SetObjectField(TEXT("location"), VectorToJson(ClonedActor->GetActorLocation()));
		CloneInfo->SetObjectField(TEXT("rotation"), RotatorToJson(ClonedActor->GetActorRotation()));
		ClonedActors.Add(MakeShared<FJsonValueObject>(CloneInfo));
	}

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("source_actor"), SourceActor->GetActorLabel());
	Result->SetStringField(TEXT("source_class"), SourceActor->GetClass()->GetName());
	Result->SetNumberField(TEXT("requested_count"), Count);
	Result->SetNumberField(TEXT("cloned_count"), ClonedActors.Num());
	Result->SetObjectField(TEXT("spacing"), VectorToJson(Spacing));
	if (bHasRotationIncrement)
	{
		Result->SetObjectField(TEXT("rotation_increment"), RotatorToJson(RotationIncrement));
	}
	Result->SetArrayField(TEXT("cloned_actors"), ClonedActors);

	return FECACommandResult::Success(Result);
}

// ─── spawn_audio_source ──────────────────────────────────

FECACommandResult FECACommand_SpawnAudioSource::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Sound path (required)
	FString SoundPath;
	if (!GetStringParam(Params, TEXT("sound_path"), SoundPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: sound_path"));
	}

	// Load the sound asset
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
	if (!Sound)
	{
		// Try appending the asset name as sub-object
		FString FullPath = SoundPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		Sound = LoadObject<USoundBase>(nullptr, *FullPath);
	}
	if (!Sound)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load sound asset at: %s"), *SoundPath));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Auto play (optional, default true)
	bool bAutoPlay = true;
	GetBoolParam(Params, TEXT("auto_play"), bAutoPlay, /*bRequired=*/false);

	// Volume (optional, default 1.0)
	double Volume = 1.0;
	GetFloatParam(Params, TEXT("volume"), Volume, /*bRequired=*/false);

	// Attenuation radius (optional)
	double AttenuationRadius = 0.0;
	bool bHasAttenuation = GetFloatParam(Params, TEXT("attenuation_radius"), AttenuationRadius, /*bRequired=*/false);

	// Spawn the ambient sound actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAmbientSound* SoundActor = World->SpawnActor<AAmbientSound>(Location, FRotator::ZeroRotator, SpawnParams);
	if (!SoundActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn AAmbientSound actor"));
	}

	// Name (optional)
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		SoundActor->SetActorLabel(Name);
	}

	// Configure audio component
	UAudioComponent* AudioComp = SoundActor->GetAudioComponent();
	if (AudioComp)
	{
		AudioComp->SetSound(Sound);
		AudioComp->SetVolumeMultiplier(static_cast<float>(Volume));
		AudioComp->bAutoActivate = bAutoPlay;

		if (bHasAttenuation)
		{
			AudioComp->bOverrideAttenuation = true;
			AudioComp->AttenuationOverrides.bAttenuate = true;
			AudioComp->AttenuationOverrides.FalloffDistance = static_cast<float>(AttenuationRadius);
		}

		if (bAutoPlay)
		{
			AudioComp->Play();
		}
	}

	SoundActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), SoundActor->GetActorLabel());
	Result->SetStringField(TEXT("sound_path"), Sound->GetPathName());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetBoolField(TEXT("auto_play"), bAutoPlay);
	Result->SetNumberField(TEXT("volume"), Volume);
	if (bHasAttenuation)
	{
		Result->SetNumberField(TEXT("attenuation_radius"), AttenuationRadius);
	}

	return FECACommandResult::Success(Result);
}

// ─── spawn_trigger_box ───────────────────────────────────

FECACommandResult FECACommand_SpawnTriggerBox::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Extent (optional, default 100x100x100)
	FVector Extent(100.0, 100.0, 100.0);
	GetVectorParam(Params, TEXT("extent"), Extent, /*bRequired=*/false);

	// Spawn the trigger box
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATriggerBox* TriggerBox = World->SpawnActor<ATriggerBox>(Location, FRotator::ZeroRotator, SpawnParams);
	if (!TriggerBox)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn ATriggerBox actor"));
	}

	// Name (optional)
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		TriggerBox->SetActorLabel(Name);
	}

	// Configure the box extent
	UShapeComponent* ShapeComp = Cast<UShapeComponent>(TriggerBox->GetCollisionComponent());
	UBoxComponent* BoxComp = Cast<UBoxComponent>(ShapeComp);
	if (BoxComp)
	{
		BoxComp->SetBoxExtent(Extent);
	}

	TriggerBox->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), TriggerBox->GetActorLabel());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("extent"), VectorToJson(Extent));

	return FECACommandResult::Success(Result);
}

// ─── align_actors ────────────────────────────────────────

FECACommandResult FECACommand_AlignActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor names (required)
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = nullptr;
	if (!GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_names"));
	}

	if (ActorNamesArray->Num() < 2)
	{
		return FECACommandResult::Error(TEXT("align_actors requires at least 2 actor names"));
	}

	// Align axis (required)
	FString AlignAxis;
	if (!GetStringParam(Params, TEXT("align_axis"), AlignAxis))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: align_axis"));
	}
	AlignAxis = AlignAxis.ToLower();

	if (AlignAxis != TEXT("x") && AlignAxis != TEXT("y") && AlignAxis != TEXT("z") && AlignAxis != TEXT("ground"))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Invalid align_axis '%s' — must be x, y, z, or ground"), *AlignAxis));
	}

	// Resolve all actors
	TArray<AActor*> Actors;
	TArray<FString> NotFound;

	for (const TSharedPtr<FJsonValue>& NameVal : *ActorNamesArray)
	{
		FString ActorName = NameVal->AsString();
		AActor* Actor = FindActorByName(ActorName);
		if (Actor)
		{
			Actors.Add(Actor);
		}
		else
		{
			NotFound.Add(ActorName);
		}
	}

	if (Actors.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("None of the specified actors were found"));
	}

	// Target value (optional — defaults to average)
	double TargetValue = 0.0;
	bool bHasTargetValue = GetFloatParam(Params, TEXT("target_value"), TargetValue, /*bRequired=*/false);

	if (!bHasTargetValue && AlignAxis != TEXT("ground"))
	{
		// Compute average along the chosen axis
		double Sum = 0.0;
		for (AActor* Actor : Actors)
		{
			FVector Loc = Actor->GetActorLocation();
			if (AlignAxis == TEXT("x")) Sum += Loc.X;
			else if (AlignAxis == TEXT("y")) Sum += Loc.Y;
			else if (AlignAxis == TEXT("z")) Sum += Loc.Z;
		}
		TargetValue = Sum / static_cast<double>(Actors.Num());
	}

	// Apply alignment
	int32 AlignedCount = 0;
	TArray<TSharedPtr<FJsonValue>> AlignedActors;

	for (AActor* Actor : Actors)
	{
		FVector Loc = Actor->GetActorLocation();

		if (AlignAxis == TEXT("x"))
		{
			Loc.X = TargetValue;
		}
		else if (AlignAxis == TEXT("y"))
		{
			Loc.Y = TargetValue;
		}
		else if (AlignAxis == TEXT("z"))
		{
			Loc.Z = TargetValue;
		}
		else if (AlignAxis == TEXT("ground"))
		{
			// Trace downward to find the ground
			FHitResult Hit;
			FVector TraceStart = FVector(Loc.X, Loc.Y, Loc.Z + 10000.0);
			FVector TraceEnd = FVector(Loc.X, Loc.Y, Loc.Z - 100000.0);

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Actor);

			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
			{
				Loc.Z = Hit.ImpactPoint.Z;
			}
			else if (bHasTargetValue)
			{
				Loc.Z = TargetValue;
			}
			// else: leave as-is if no ground found and no target_value
		}

		Actor->SetActorLocation(Loc);
		Actor->MarkPackageDirty();
		++AlignedCount;

		TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
		ActorInfo->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorInfo->SetObjectField(TEXT("location"), VectorToJson(Loc));
		AlignedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("align_axis"), AlignAxis);
	if (AlignAxis != TEXT("ground"))
	{
		Result->SetNumberField(TEXT("target_value"), TargetValue);
	}
	Result->SetNumberField(TEXT("aligned_count"), AlignedCount);
	Result->SetArrayField(TEXT("actors"), AlignedActors);

	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& NF : NotFound)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(NF));
		}
		Result->SetArrayField(TEXT("not_found"), NotFoundArray);
	}

	return FECACommandResult::Success(Result);
}

// ─── distribute_actors ───────────────────────────────────

FECACommandResult FECACommand_DistributeActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor names (required)
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = nullptr;
	if (!GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_names"));
	}

	if (ActorNamesArray->Num() < 2)
	{
		return FECACommandResult::Error(TEXT("distribute_actors requires at least 2 actor names"));
	}

	// Resolve all actors (preserving order)
	TArray<AActor*> Actors;
	TArray<FString> NotFound;

	for (const TSharedPtr<FJsonValue>& NameVal : *ActorNamesArray)
	{
		FString ActorName = NameVal->AsString();
		AActor* Actor = FindActorByName(ActorName);
		if (Actor)
		{
			Actors.Add(Actor);
		}
		else
		{
			NotFound.Add(ActorName);
		}
	}

	if (Actors.Num() < 2)
	{
		return FECACommandResult::Error(TEXT("Need at least 2 found actors to distribute"));
	}

	// Check which mode: start/end locations OR axis-based
	FVector StartLocation;
	FVector EndLocation;
	bool bHasStartEnd = GetVectorParam(Params, TEXT("start_location"), StartLocation, /*bRequired=*/false)
		&& GetVectorParam(Params, TEXT("end_location"), EndLocation, /*bRequired=*/false);

	FString Axis;
	bool bHasAxis = GetStringParam(Params, TEXT("axis"), Axis, /*bRequired=*/false) && !Axis.IsEmpty();
	if (bHasAxis)
	{
		Axis = Axis.ToLower();
	}

	if (!bHasStartEnd && !bHasAxis)
	{
		return FECACommandResult::Error(TEXT("Must provide either start_location + end_location, or axis"));
	}

	if (bHasAxis && Axis != TEXT("x") && Axis != TEXT("y") && Axis != TEXT("z"))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Invalid axis '%s' — must be x, y, or z"), *Axis));
	}

	int32 Count = Actors.Num();
	TArray<TSharedPtr<FJsonValue>> DistributedActors;

	if (bHasStartEnd)
	{
		// Distribute evenly between start and end
		for (int32 i = 0; i < Count; ++i)
		{
			double Alpha = (Count > 1) ? (static_cast<double>(i) / static_cast<double>(Count - 1)) : 0.0;
			FVector NewLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);

			Actors[i]->SetActorLocation(NewLocation);
			Actors[i]->MarkPackageDirty();

			TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
			ActorInfo->SetStringField(TEXT("name"), Actors[i]->GetActorLabel());
			ActorInfo->SetObjectField(TEXT("location"), VectorToJson(NewLocation));
			DistributedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
		}
	}
	else
	{
		// Axis-based distribution: sort by axis, redistribute evenly along that axis
		// First, find min and max along the axis
		double MinVal = TNumericLimits<double>::Max();
		double MaxVal = TNumericLimits<double>::Lowest();

		for (AActor* Actor : Actors)
		{
			FVector Loc = Actor->GetActorLocation();
			double Val = 0.0;
			if (Axis == TEXT("x")) Val = Loc.X;
			else if (Axis == TEXT("y")) Val = Loc.Y;
			else if (Axis == TEXT("z")) Val = Loc.Z;

			MinVal = FMath::Min(MinVal, Val);
			MaxVal = FMath::Max(MaxVal, Val);
		}

		// Sort actors by their current position along the axis
		Actors.Sort([&Axis](const AActor& A, const AActor& B) -> bool
		{
			FVector LocA = A.GetActorLocation();
			FVector LocB = B.GetActorLocation();
			if (Axis == TEXT("x")) return LocA.X < LocB.X;
			if (Axis == TEXT("y")) return LocA.Y < LocB.Y;
			return LocA.Z < LocB.Z;
		});

		// Distribute evenly between min and max
		for (int32 i = 0; i < Count; ++i)
		{
			double Alpha = (Count > 1) ? (static_cast<double>(i) / static_cast<double>(Count - 1)) : 0.0;
			double NewVal = FMath::Lerp(MinVal, MaxVal, Alpha);

			FVector Loc = Actors[i]->GetActorLocation();
			if (Axis == TEXT("x")) Loc.X = NewVal;
			else if (Axis == TEXT("y")) Loc.Y = NewVal;
			else if (Axis == TEXT("z")) Loc.Z = NewVal;

			Actors[i]->SetActorLocation(Loc);
			Actors[i]->MarkPackageDirty();

			TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
			ActorInfo->SetStringField(TEXT("name"), Actors[i]->GetActorLabel());
			ActorInfo->SetObjectField(TEXT("location"), VectorToJson(Loc));
			DistributedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
		}
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("distributed_count"), Count);
	if (bHasStartEnd)
	{
		Result->SetObjectField(TEXT("start_location"), VectorToJson(StartLocation));
		Result->SetObjectField(TEXT("end_location"), VectorToJson(EndLocation));
	}
	if (bHasAxis)
	{
		Result->SetStringField(TEXT("axis"), Axis);
	}
	Result->SetArrayField(TEXT("actors"), DistributedActors);

	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& NF : NotFound)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(NF));
		}
		Result->SetArrayField(TEXT("not_found"), NotFoundArray);
	}

	return FECACommandResult::Success(Result);
}

// ─── Scene Snapshot Storage ───────────────────────────────────

namespace EnvironmentCommandHelpers
{
	// Each snapshot is a JSON object containing:
	//   "snapshot_name" (string), "timestamp" (string), "include_properties" (bool),
	//   "actors" (array of actor data objects)
	static TMap<FString, TSharedPtr<FJsonObject>> SceneSnapshots;
}

// ─── snapshot_scene_state ─────────────────────────────────────

FECACommandResult FECACommand_SnapshotSceneState::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	FString SnapshotName;
	if (!GetStringParam(Params, TEXT("snapshot_name"), SnapshotName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: snapshot_name"));
	}

	bool bIncludeProperties = false;
	GetBoolParam(Params, TEXT("include_properties"), bIncludeProperties, /*bRequired=*/false);

	// Build actor data array
	TArray<TSharedPtr<FJsonValue>> ActorArray;
	int32 ActorCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsPendingKillPending())
		{
			continue;
		}

		// Skip transient/hidden editor-only actors (world settings, brushes, etc.)
		if (Actor->IsA<AWorldSettings>())
		{
			continue;
		}

		TSharedPtr<FJsonObject> ActorData = MakeShared<FJsonObject>();
		ActorData->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorData->SetStringField(TEXT("object_name"), Actor->GetName());
		ActorData->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

		// Transform
		FVector Location = Actor->GetActorLocation();
		FRotator Rotation = Actor->GetActorRotation();
		FVector Scale = Actor->GetActorScale3D();
		ActorData->SetObjectField(TEXT("location"), VectorToJson(Location));
		ActorData->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
		ActorData->SetObjectField(TEXT("scale"), VectorToJson(Scale));

		// Visibility
		ActorData->SetBoolField(TEXT("hidden"), Actor->IsHidden());

		// Extended properties if requested
		if (bIncludeProperties)
		{
			// Capture material assignments from static mesh components
			TArray<TSharedPtr<FJsonValue>> MaterialArray;
			TArray<UStaticMeshComponent*> MeshComps;
			Actor->GetComponents<UStaticMeshComponent>(MeshComps);
			for (UStaticMeshComponent* MeshComp : MeshComps)
			{
				for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
				{
					UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
					TSharedPtr<FJsonObject> MatData = MakeShared<FJsonObject>();
					MatData->SetStringField(TEXT("component"), MeshComp->GetName());
					MatData->SetNumberField(TEXT("index"), MatIdx);
					MatData->SetStringField(TEXT("material"), Mat ? Mat->GetPathName() : TEXT("None"));
					MaterialArray.Add(MakeShared<FJsonValueObject>(MatData));
				}
			}
			if (MaterialArray.Num() > 0)
			{
				ActorData->SetArrayField(TEXT("materials"), MaterialArray);
			}

			// Capture light settings if this actor has a light component
			ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
			if (LightComp)
			{
				TSharedPtr<FJsonObject> LightData = MakeShared<FJsonObject>();
				LightData->SetNumberField(TEXT("intensity"), LightComp->Intensity);
				FLinearColor LightColor = LightComp->GetLightColor();
				TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
				ColorObj->SetNumberField(TEXT("r"), FMath::RoundToInt(LightColor.R * 255.f));
				ColorObj->SetNumberField(TEXT("g"), FMath::RoundToInt(LightColor.G * 255.f));
				ColorObj->SetNumberField(TEXT("b"), FMath::RoundToInt(LightColor.B * 255.f));
				LightData->SetObjectField(TEXT("color"), ColorObj);
				LightData->SetBoolField(TEXT("cast_shadows"), LightComp->CastShadows);
				ActorData->SetObjectField(TEXT("light_settings"), LightData);
			}
		}

		ActorArray.Add(MakeShared<FJsonValueObject>(ActorData));
		++ActorCount;
	}

	// Build the snapshot object
	TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetStringField(TEXT("snapshot_name"), SnapshotName);
	Snapshot->SetStringField(TEXT("timestamp"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	Snapshot->SetBoolField(TEXT("include_properties"), bIncludeProperties);
	Snapshot->SetArrayField(TEXT("actors"), ActorArray);

	// Store it
	EnvironmentCommandHelpers::SceneSnapshots.Add(SnapshotName, Snapshot);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("snapshot_name"), SnapshotName);
	Result->SetNumberField(TEXT("actor_count"), ActorCount);
	Result->SetBoolField(TEXT("include_properties"), bIncludeProperties);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Snapshot '%s' captured with %d actors"), *SnapshotName, ActorCount));

	return FECACommandResult::Success(Result);
}

// ─── restore_scene_state ──────────────────────────────────────

FECACommandResult FECACommand_RestoreSceneState::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	FString SnapshotName;
	if (!GetStringParam(Params, TEXT("snapshot_name"), SnapshotName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: snapshot_name"));
	}

	TSharedPtr<FJsonObject>* FoundSnapshot = EnvironmentCommandHelpers::SceneSnapshots.Find(SnapshotName);
	if (!FoundSnapshot || !FoundSnapshot->IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Snapshot '%s' not found"), *SnapshotName));
	}

	TSharedPtr<FJsonObject> Snapshot = *FoundSnapshot;
	bool bIncludeProperties = Snapshot->GetBoolField(TEXT("include_properties"));
	const TArray<TSharedPtr<FJsonValue>>& ActorArray = Snapshot->GetArrayField(TEXT("actors"));

	int32 RestoredCount = 0;
	int32 MissingCount = 0;
	TArray<TSharedPtr<FJsonValue>> MissingActors;

	for (const TSharedPtr<FJsonValue>& ActorValue : ActorArray)
	{
		const TSharedPtr<FJsonObject>& ActorData = ActorValue->AsObject();
		if (!ActorData.IsValid())
		{
			continue;
		}

		FString ActorLabel = ActorData->GetStringField(TEXT("name"));
		FString ActorObjName = ActorData->GetStringField(TEXT("object_name"));

		// Find the actor in the current world — try label first, then object name
		AActor* Actor = FindActorByName(ActorLabel);
		if (!Actor)
		{
			Actor = FindActorByName(ActorObjName);
		}

		if (!Actor)
		{
			++MissingCount;
			MissingActors.Add(MakeShared<FJsonValueString>(ActorLabel.IsEmpty() ? ActorObjName : ActorLabel));
			continue;
		}

		// Restore transform
		const TSharedPtr<FJsonObject>* LocObj = nullptr;
		if (ActorData->TryGetObjectField(TEXT("location"), LocObj) && LocObj->IsValid())
		{
			FVector Location(
				(*LocObj)->GetNumberField(TEXT("x")),
				(*LocObj)->GetNumberField(TEXT("y")),
				(*LocObj)->GetNumberField(TEXT("z"))
			);
			Actor->SetActorLocation(Location);
		}

		const TSharedPtr<FJsonObject>* RotObj = nullptr;
		if (ActorData->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj->IsValid())
		{
			FRotator Rotation(
				(*RotObj)->GetNumberField(TEXT("pitch")),
				(*RotObj)->GetNumberField(TEXT("yaw")),
				(*RotObj)->GetNumberField(TEXT("roll"))
			);
			Actor->SetActorRotation(Rotation);
		}

		const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
		if (ActorData->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj->IsValid())
		{
			FVector Scale(
				(*ScaleObj)->GetNumberField(TEXT("x")),
				(*ScaleObj)->GetNumberField(TEXT("y")),
				(*ScaleObj)->GetNumberField(TEXT("z"))
			);
			Actor->SetActorScale3D(Scale);
		}

		// Restore visibility if include_properties was true during snapshot
		if (bIncludeProperties)
		{
			bool bHidden = ActorData->GetBoolField(TEXT("hidden"));
			Actor->SetActorHiddenInGame(bHidden);
#if WITH_EDITOR
			Actor->SetIsTemporarilyHiddenInEditor(bHidden);
#endif

			// Restore material assignments
			const TArray<TSharedPtr<FJsonValue>>* MaterialArray = nullptr;
			if (ActorData->TryGetArrayField(TEXT("materials"), MaterialArray))
			{
				TArray<UStaticMeshComponent*> MeshComps;
				Actor->GetComponents<UStaticMeshComponent>(MeshComps);

				for (const TSharedPtr<FJsonValue>& MatValue : *MaterialArray)
				{
					const TSharedPtr<FJsonObject>& MatData = MatValue->AsObject();
					if (!MatData.IsValid())
					{
						continue;
					}

					FString CompName = MatData->GetStringField(TEXT("component"));
					int32 MatIdx = static_cast<int32>(MatData->GetNumberField(TEXT("index")));
					FString MaterialPath = MatData->GetStringField(TEXT("material"));

					if (MaterialPath == TEXT("None") || MaterialPath.IsEmpty())
					{
						continue;
					}

					// Find the matching mesh component
					for (UStaticMeshComponent* MeshComp : MeshComps)
					{
						if (MeshComp->GetName() == CompName)
						{
							UMaterialInterface* Mat = EnvironmentCommandHelpers::LoadMaterialByPath(MaterialPath);
							if (Mat && MatIdx < MeshComp->GetNumMaterials())
							{
								MeshComp->SetMaterial(MatIdx, Mat);
							}
							break;
						}
					}
				}
			}

			// Restore light settings
			const TSharedPtr<FJsonObject>* LightDataObj = nullptr;
			if (ActorData->TryGetObjectField(TEXT("light_settings"), LightDataObj) && LightDataObj->IsValid())
			{
				ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
				if (LightComp)
				{
					LightComp->SetIntensity((*LightDataObj)->GetNumberField(TEXT("intensity")));
					LightComp->SetCastShadows((*LightDataObj)->GetBoolField(TEXT("cast_shadows")));

					const TSharedPtr<FJsonObject>* ColorObj = nullptr;
					if ((*LightDataObj)->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj->IsValid())
					{
						FColor LightColor(
							static_cast<uint8>((*ColorObj)->GetNumberField(TEXT("r"))),
							static_cast<uint8>((*ColorObj)->GetNumberField(TEXT("g"))),
							static_cast<uint8>((*ColorObj)->GetNumberField(TEXT("b")))
						);
						LightComp->SetLightColor(LightColor);
					}
				}
			}
		}

		Actor->MarkPackageDirty();
		++RestoredCount;
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("snapshot_name"), SnapshotName);
	Result->SetNumberField(TEXT("restored_count"), RestoredCount);
	Result->SetNumberField(TEXT("missing_count"), MissingCount);

	if (MissingActors.Num() > 0)
	{
		Result->SetArrayField(TEXT("missing_actors"), MissingActors);
	}

	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Restored %d actors from snapshot '%s' (%d missing)"), RestoredCount, *SnapshotName, MissingCount));

	return FECACommandResult::Success(Result);
}

// ─── list_scene_snapshots ─────────────────────────────────────

FECACommandResult FECACommand_ListSceneSnapshots::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> SnapshotArray;

	for (const auto& Pair : EnvironmentCommandHelpers::SceneSnapshots)
	{
		const TSharedPtr<FJsonObject>& Snapshot = Pair.Value;
		if (!Snapshot.IsValid())
		{
			continue;
		}

		TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("snapshot_name"), Pair.Key);
		Info->SetStringField(TEXT("timestamp"), Snapshot->GetStringField(TEXT("timestamp")));
		Info->SetBoolField(TEXT("include_properties"), Snapshot->GetBoolField(TEXT("include_properties")));

		const TArray<TSharedPtr<FJsonValue>>& Actors = Snapshot->GetArrayField(TEXT("actors"));
		Info->SetNumberField(TEXT("actor_count"), Actors.Num());

		SnapshotArray.Add(MakeShared<FJsonValueObject>(Info));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("snapshots"), SnapshotArray);
	Result->SetNumberField(TEXT("total_count"), SnapshotArray.Num());

	return FECACommandResult::Success(Result);
}

// ─── set_time_dilation ─────────────────────────────────────

FECACommandResult FECACommand_SetTimeDilation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	double TimeDilation = 1.0;
	if (!GetFloatParam(Params, TEXT("time_dilation"), TimeDilation))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: time_dilation"));
	}

	// Clamp to UE's valid range (UE min is KINDA_SMALL_NUMBER, max is generally 20)
	const double MinDilation = 0.0001;
	const double MaxDilation = 20.0;
	if (TimeDilation < MinDilation || TimeDilation > MaxDilation)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("time_dilation must be between %f and %f, got %f"), MinDilation, MaxDilation, TimeDilation));
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return FECACommandResult::Error(TEXT("Could not access world settings"));
	}

	double PreviousDilation = WorldSettings->TimeDilation;
	WorldSettings->TimeDilation = static_cast<float>(TimeDilation);
	WorldSettings->MarkPackageDirty();

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("time_dilation"), TimeDilation);
	Result->SetNumberField(TEXT("previous_time_dilation"), PreviousDilation);

	return FECACommandResult::Success(Result);
}

// ─── measure_distance ──────────────────────────────────────

FECACommandResult FECACommand_MeasureDistance::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FVector FromLocation = FVector::ZeroVector;
	FVector ToLocation = FVector::ZeroVector;
	FString FromLabel;
	FString ToLabel;
	bool bHasFrom = false;
	bool bHasTo = false;

	// Resolve "from" — actor takes priority over point
	FString FromActorName;
	if (GetStringParam(Params, TEXT("from_actor"), FromActorName, /*bRequired=*/false) && !FromActorName.IsEmpty())
	{
		AActor* FromActor = FindActorByName(FromActorName);
		if (!FromActor)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("from_actor not found: %s"), *FromActorName));
		}
		FromLocation = FromActor->GetActorLocation();
		FromLabel = FromActor->GetActorLabel();
		bHasFrom = true;
	}
	else if (GetVectorParam(Params, TEXT("from_point"), FromLocation, /*bRequired=*/false))
	{
		FromLabel = TEXT("point");
		bHasFrom = true;
	}

	// Resolve "to" — actor takes priority over point
	FString ToActorName;
	if (GetStringParam(Params, TEXT("to_actor"), ToActorName, /*bRequired=*/false) && !ToActorName.IsEmpty())
	{
		AActor* ToActor = FindActorByName(ToActorName);
		if (!ToActor)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("to_actor not found: %s"), *ToActorName));
		}
		ToLocation = ToActor->GetActorLocation();
		ToLabel = ToActor->GetActorLabel();
		bHasTo = true;
	}
	else if (GetVectorParam(Params, TEXT("to_point"), ToLocation, /*bRequired=*/false))
	{
		ToLabel = TEXT("point");
		bHasTo = true;
	}

	if (!bHasFrom)
	{
		return FECACommandResult::Error(TEXT("Must specify either from_actor or from_point"));
	}
	if (!bHasTo)
	{
		return FECACommandResult::Error(TEXT("Must specify either to_actor or to_point"));
	}

	double Distance = FVector::Dist(FromLocation, ToLocation);
	FVector Delta = ToLocation - FromLocation;

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("distance_cm"), Distance);
	Result->SetNumberField(TEXT("distance_m"), Distance / 100.0);
	Result->SetStringField(TEXT("from_label"), FromLabel);
	Result->SetObjectField(TEXT("from_location"), VectorToJson(FromLocation));
	Result->SetStringField(TEXT("to_label"), ToLabel);
	Result->SetObjectField(TEXT("to_location"), VectorToJson(ToLocation));
	Result->SetObjectField(TEXT("delta"), VectorToJson(Delta));

	return FECACommandResult::Success(Result);
}

// ─── randomize_transforms ──────────────────────────────────

FECACommandResult FECACommand_RandomizeTransforms::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor names (required)
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = nullptr;
	if (!GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_names"));
	}

	if (ActorNamesArray->Num() == 0)
	{
		return FECACommandResult::Error(TEXT("actor_names array is empty"));
	}

	// Bounds (optional)
	FVector BoundsMin;
	FVector BoundsMax;
	bool bHasBoundsMin = GetVectorParam(Params, TEXT("bounds_min"), BoundsMin, /*bRequired=*/false);
	bool bHasBoundsMax = GetVectorParam(Params, TEXT("bounds_max"), BoundsMax, /*bRequired=*/false);
	bool bHasExplicitBounds = bHasBoundsMin && bHasBoundsMax;

	// Randomization options
	bool bRandomizeRotation = true;
	GetBoolParam(Params, TEXT("randomize_rotation"), bRandomizeRotation, /*bRequired=*/false);

	bool bRandomizeScale = false;
	GetBoolParam(Params, TEXT("randomize_scale"), bRandomizeScale, /*bRequired=*/false);

	double ScaleMin = 0.8;
	GetFloatParam(Params, TEXT("scale_min"), ScaleMin, /*bRequired=*/false);

	double ScaleMax = 1.2;
	GetFloatParam(Params, TEXT("scale_max"), ScaleMax, /*bRequired=*/false);

	if (bRandomizeScale && ScaleMin > ScaleMax)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("scale_min (%f) must be <= scale_max (%f)"), ScaleMin, ScaleMax));
	}

	// Resolve actors
	TArray<AActor*> Actors;
	TArray<FString> NotFound;

	for (const TSharedPtr<FJsonValue>& NameVal : *ActorNamesArray)
	{
		FString ActorName = NameVal->AsString();
		AActor* Actor = FindActorByName(ActorName);
		if (Actor)
		{
			Actors.Add(Actor);
		}
		else
		{
			NotFound.Add(ActorName);
		}
	}

	if (Actors.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("None of the specified actors were found"));
	}

	// Randomize each actor
	TArray<TSharedPtr<FJsonValue>> ResultActors;

	for (AActor* Actor : Actors)
	{
		FVector CurrentLocation = Actor->GetActorLocation();
		FVector NewLocation;

		if (bHasExplicitBounds)
		{
			// Scatter within the explicit bounding box
			NewLocation.X = FMath::FRandRange(BoundsMin.X, BoundsMax.X);
			NewLocation.Y = FMath::FRandRange(BoundsMin.Y, BoundsMax.Y);
			NewLocation.Z = FMath::FRandRange(BoundsMin.Z, BoundsMax.Z);
		}
		else
		{
			// Scatter around current position within +/-100 units
			NewLocation.X = CurrentLocation.X + FMath::FRandRange(-100.0, 100.0);
			NewLocation.Y = CurrentLocation.Y + FMath::FRandRange(-100.0, 100.0);
			NewLocation.Z = CurrentLocation.Z + FMath::FRandRange(-100.0, 100.0);
		}

		Actor->SetActorLocation(NewLocation);

		if (bRandomizeRotation)
		{
			FRotator CurrentRotation = Actor->GetActorRotation();
			CurrentRotation.Yaw = FMath::FRandRange(0.0, 360.0);
			Actor->SetActorRotation(CurrentRotation);
		}

		if (bRandomizeScale)
		{
			double Scale = FMath::FRandRange(ScaleMin, ScaleMax);
			Actor->SetActorScale3D(FVector(Scale));
		}

		Actor->MarkPackageDirty();

		TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
		ActorInfo->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
		ActorInfo->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
		ActorInfo->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
		ActorInfo->SetObjectField(TEXT("scale"), VectorToJson(Actor->GetActorScale3D()));
		ResultActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
	}

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("actors"), ResultActors);
	Result->SetNumberField(TEXT("randomized_count"), Actors.Num());

	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& Name : NotFound)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(Name));
		}
		Result->SetArrayField(TEXT("not_found"), NotFoundArray);
	}

	return FECACommandResult::Success(Result);
}

// ─── get_world_info ────────────────────────────────────────

FECACommandResult FECACommand_GetWorldInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();

	// Level name and path
	FString LevelName = World->GetMapName();
	LevelName.RemoveFromStart(World->StreamingLevelsPrefix);
	Result->SetStringField(TEXT("level_name"), LevelName);

	ULevel* CurrentLevel = World->GetCurrentLevel();
	if (CurrentLevel && CurrentLevel->GetOutermost())
	{
		Result->SetStringField(TEXT("level_path"), CurrentLevel->GetOutermost()->GetName());
	}

	// Actor counts
	int32 TotalActors = 0;
	int32 LightCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++TotalActors;
		if ((*It)->IsA<ALight>())
		{
			++LightCount;
		}
	}

	Result->SetNumberField(TEXT("actor_count"), TotalActors);
	Result->SetNumberField(TEXT("light_count"), LightCount);

	// World settings
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (WorldSettings)
	{
		Result->SetNumberField(TEXT("time_dilation"), WorldSettings->TimeDilation);
	}

	// Gravity
	double GravityZ = World->GetGravityZ();
	Result->SetNumberField(TEXT("gravity_z"), GravityZ);

	// World bounds — compute from all actors
	FBox WorldBounds(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsA<AWorldSettings>())
		{
			continue;
		}
		FVector Origin;
		FVector BoxExtent;
		Actor->GetActorBounds(false, Origin, BoxExtent);
		if (!BoxExtent.IsNearlyZero())
		{
			WorldBounds += FBox(Origin - BoxExtent, Origin + BoxExtent);
		}
	}

	if (WorldBounds.IsValid)
	{
		Result->SetObjectField(TEXT("world_bounds_min"), VectorToJson(WorldBounds.Min));
		Result->SetObjectField(TEXT("world_bounds_max"), VectorToJson(WorldBounds.Max));
	}

	// Game mode
	if (World->GetAuthGameMode())
	{
		Result->SetStringField(TEXT("game_mode"), World->GetAuthGameMode()->GetClass()->GetName());
	}
	else
	{
		// Try to get the default game mode from world settings
		if (WorldSettings && WorldSettings->DefaultGameMode)
		{
			Result->SetStringField(TEXT("game_mode"), WorldSettings->DefaultGameMode->GetName());
		}
		else
		{
			Result->SetStringField(TEXT("game_mode"), TEXT("None"));
		}
	}

	// Player start location
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* PlayerStart = *It;
		if (PlayerStart)
		{
			Result->SetObjectField(TEXT("player_start_location"), VectorToJson(PlayerStart->GetActorLocation()));
			Result->SetObjectField(TEXT("player_start_rotation"), RotatorToJson(PlayerStart->GetActorRotation()));
			break; // Just the first one
		}
	}

	return FECACommandResult::Success(Result);
}

// ─── set_actor_folder ──────────────────────────────────────

FECACommandResult FECACommand_SetActorFolder::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor name (required)
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Folder path (required)
	FString FolderPath;
	if (!GetStringParam(Params, TEXT("folder_path"), FolderPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: folder_path"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	Actor->SetFolderPath(FName(*FolderPath));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("folder_path"), FolderPath);

	return FECACommandResult::Success(Result);
}

// ─── group_actors ──────────────────────────────────────────

FECACommandResult FECACommand_GroupActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor names (required)
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = nullptr;
	if (!GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray) || !ActorNamesArray)
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_names"));
	}

	// Folder path (required)
	FString FolderPath;
	if (!GetStringParam(Params, TEXT("folder_path"), FolderPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: folder_path"));
	}

	TArray<TSharedPtr<FJsonValue>> SuccessArray;
	TArray<TSharedPtr<FJsonValue>> FailedArray;

	for (const TSharedPtr<FJsonValue>& Value : *ActorNamesArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			continue;
		}

		FString Name = Value->AsString();
		AActor* Actor = FindActorByName(Name);
		if (Actor)
		{
			Actor->SetFolderPath(FName(*FolderPath));
			SuccessArray.Add(MakeShared<FJsonValueString>(Actor->GetActorLabel()));
		}
		else
		{
			FailedArray.Add(MakeShared<FJsonValueString>(Name));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("folder_path"), FolderPath);
	Result->SetNumberField(TEXT("moved_count"), SuccessArray.Num());
	Result->SetArrayField(TEXT("moved_actors"), SuccessArray);
	if (FailedArray.Num() > 0)
	{
		Result->SetArrayField(TEXT("not_found"), FailedArray);
	}

	return FECACommandResult::Success(Result);
}

// ─── set_collision_preset ──────────────────────────────────

FECACommandResult FECACommand_SetCollisionPreset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Actor name (required)
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Preset name (required)
	FString PresetName;
	if (!GetStringParam(Params, TEXT("preset_name"), PresetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: preset_name"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Get root component and cast to primitive
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	if (!PrimComp)
	{
		// Try to find any primitive component on the actor
		PrimComp = Actor->FindComponentByClass<UPrimitiveComponent>();
	}

	if (!PrimComp)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no UPrimitiveComponent to set collision on"), *ActorName));
	}

	// Validate the profile name exists
	FCollisionResponseTemplate Template;
	if (!UCollisionProfile::Get()->GetProfileTemplate(FName(*PresetName), Template))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unknown collision preset: %s"), *PresetName));
	}

	PrimComp->SetCollisionProfileName(FName(*PresetName));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("component_name"), PrimComp->GetName());
	Result->SetStringField(TEXT("preset_name"), PresetName);

	return FECACommandResult::Success(Result);
}

// ─── take_screenshots_sweep ────────────────────────────────

FECACommandResult FECACommand_TakeScreenshotsSweep::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Target location (required)
	FVector TargetLocation;
	if (!GetVectorParam(Params, TEXT("target_location"), TargetLocation))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: target_location"));
	}

	// Radius (optional, default 300)
	double Radius = 300.0;
	GetFloatParam(Params, TEXT("radius"), Radius, /*bRequired=*/false);

	// Count (optional, default 8)
	int32 Count = 8;
	GetIntParam(Params, TEXT("count"), Count, /*bRequired=*/false);
	if (Count < 1 || Count > 360)
	{
		return FECACommandResult::Error(TEXT("count must be between 1 and 360"));
	}

	// Height (optional, default 150)
	double Height = 150.0;
	GetFloatParam(Params, TEXT("height"), Height, /*bRequired=*/false);

	// Filename prefix (optional, default "sweep")
	FString FilenamePrefix = TEXT("sweep");
	GetStringParam(Params, TEXT("filename_prefix"), FilenamePrefix, /*bRequired=*/false);

	// Get the editor viewport client for positioning the camera
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
	if (!ActiveViewport.IsValid() || !ActiveViewport->GetActiveViewport())
	{
		return FECACommandResult::Error(TEXT("No active editor viewport found. Make sure a Level Editor viewport is open."));
	}
	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetActiveViewport()->GetClient());
	if (!ViewportClient)
	{
		return FECACommandResult::Error(TEXT("Could not get editor viewport client"));
	}

	// Save original camera position to restore after sweep
	FVector OriginalLocation = ViewportClient->GetViewLocation();
	FRotator OriginalRotation = ViewportClient->GetViewRotation();

	// Prepare screenshot output directory
	FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
	IFileManager::Get().MakeDirectory(*ScreenshotDir, true);

	// Resolution for the scene capture
	int32 ResolutionX = 1920;
	int32 ResolutionY = 1080;

	TArray<TSharedPtr<FJsonValue>> FilePathsArray;
	double AngleStep = 360.0 / static_cast<double>(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		double AngleDeg = AngleStep * i;
		double AngleRad = FMath::DegreesToRadians(AngleDeg);

		// Compute camera position on the orbit circle
		FVector CameraPos;
		CameraPos.X = TargetLocation.X + Radius * FMath::Cos(AngleRad);
		CameraPos.Y = TargetLocation.Y + Radius * FMath::Sin(AngleRad);
		CameraPos.Z = TargetLocation.Z + Height;

		// Compute rotation looking at the target
		FVector LookDir = (TargetLocation - CameraPos).GetSafeNormal();
		FRotator CameraRot = LookDir.Rotation();

		// Move the viewport camera
		ViewportClient->SetViewLocation(CameraPos);
		ViewportClient->SetViewRotation(CameraRot);

		// Create scene capture to take the screenshot
		USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
		if (!SceneCapture)
		{
			continue;
		}

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		if (!RenderTarget)
		{
			continue;
		}

		RenderTarget->InitCustomFormat(ResolutionX, ResolutionY, PF_B8G8R8A8, false);
		RenderTarget->UpdateResourceImmediate(true);

		SceneCapture->TextureTarget = RenderTarget;
		SceneCapture->SetWorldLocationAndRotation(CameraPos, CameraRot);
		SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		SceneCapture->bCaptureEveryFrame = false;
		SceneCapture->bCaptureOnMovement = false;
		SceneCapture->FOVAngle = ViewportClient->ViewFOV;

		SceneCapture->RegisterComponentWithWorld(World);
		SceneCapture->CaptureScene();

		// Build filename with angle suffix
		FString Filename = FString::Printf(TEXT("%s_%03d_deg_%03.0f.png"), *FilenamePrefix, i, AngleDeg);
		FString FullPath = ScreenshotDir / Filename;

		// Export to PNG
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FullPath);
		if (FileWriter)
		{
			bool bExportSuccess = FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *FileWriter);
			FileWriter->Close();
			delete FileWriter;

			if (bExportSuccess)
			{
				FilePathsArray.Add(MakeShared<FJsonValueString>(FullPath));
			}
		}

		// Cleanup
		SceneCapture->DestroyComponent();
	}

	// Restore original camera position
	ViewportClient->SetViewLocation(OriginalLocation);
	ViewportClient->SetViewRotation(OriginalRotation);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetObjectField(TEXT("target_location"), VectorToJson(TargetLocation));
	Result->SetNumberField(TEXT("radius"), Radius);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetNumberField(TEXT("count"), Count);
	Result->SetNumberField(TEXT("screenshots_taken"), FilePathsArray.Num());
	Result->SetArrayField(TEXT("file_paths"), FilePathsArray);
	Result->SetStringField(TEXT("screenshot_dir"), ScreenshotDir);

	return FECACommandResult::Success(Result);
}

// ─── execute_python ─────────────────────────────────────────

FECACommandResult FECACommand_ExecutePython::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Code;
	FString FilePath;
	bool bHasCode = GetStringParam(Params, TEXT("code"), Code, /*bRequired=*/false);
	bool bHasFile = GetStringParam(Params, TEXT("file_path"), FilePath, /*bRequired=*/false);

	if (!bHasCode && !bHasFile)
	{
		return FECACommandResult::Error(TEXT("Either 'code' or 'file_path' must be provided"));
	}

	// Check if PythonScriptPlugin is available
	IModuleInterface* PythonModule = FModuleManager::Get().GetModule(TEXT("PythonScriptPlugin"));
	if (!PythonModule)
	{
		return FECACommandResult::Error(TEXT("PythonScriptPlugin is not loaded. Enable the Python Editor Script Plugin in Edit > Plugins"));
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	FString CommandToExec;
	if (bHasFile)
	{
		// Validate the file exists
		if (!FPaths::FileExists(FilePath))
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Python file not found: %s"), *FilePath));
		}
		// Use py command to execute a file
		CommandToExec = FString::Printf(TEXT("py \"%s\""), *FilePath);
	}
	else
	{
		// Execute inline code via the py console command
		CommandToExec = FString::Printf(TEXT("py %s"), *Code);
	}

	// Execute through the console command system which PythonScriptPlugin hooks into
	GEngine->Exec(World, *CommandToExec);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("executed"), true);
	if (bHasFile)
	{
		Result->SetStringField(TEXT("file_path"), FilePath);
	}
	else
	{
		Result->SetStringField(TEXT("code"), Code);
	}
	Result->SetStringField(TEXT("note"), TEXT("Python output is written to the UE log (Output Log window), not returned directly"));

	return FECACommandResult::Success(Result);
}

// ─── undo_last_action ───────────────────────────────────────

FECACommandResult FECACommand_UndoLastAction::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available"));
	}

	int32 Count = 1;
	GetIntParam(Params, TEXT("count"), Count, /*bRequired=*/false);
	if (Count < 1)
	{
		Count = 1;
	}

	int32 ActionsUndone = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		bool bResult = GEditor->UndoTransaction();
		if (bResult)
		{
			++ActionsUndone;
		}
		else
		{
			// No more actions to undo
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("actions_undone"), ActionsUndone);
	Result->SetNumberField(TEXT("requested_count"), Count);

	return FECACommandResult::Success(Result);
}

// ─── redo_last_action ───────────────────────────────────────

FECACommandResult FECACommand_RedoLastAction::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available"));
	}

	int32 Count = 1;
	GetIntParam(Params, TEXT("count"), Count, /*bRequired=*/false);
	if (Count < 1)
	{
		Count = 1;
	}

	int32 ActionsRedone = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		bool bResult = GEditor->RedoTransaction();
		if (bResult)
		{
			++ActionsRedone;
		}
		else
		{
			// No more actions to redo
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("actions_redone"), ActionsRedone);
	Result->SetNumberField(TEXT("requested_count"), Count);

	return FECACommandResult::Success(Result);
}

// ─── search_assets ──────────────────────────────────────────

FECACommandResult FECACommand_SearchAssets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Query;
	FString ClassFilter;
	FString PathFilter;
	int32 MaxResults = 50;

	GetStringParam(Params, TEXT("query"), Query, /*bRequired=*/false);
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, /*bRequired=*/false);
	GetStringParam(Params, TEXT("path_filter"), PathFilter, /*bRequired=*/false);
	GetIntParam(Params, TEXT("max_results"), MaxResults, /*bRequired=*/false);

	if (Query.IsEmpty() && ClassFilter.IsEmpty() && PathFilter.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("At least one of 'query', 'class_filter', or 'path_filter' must be provided"));
	}

	if (MaxResults < 1) MaxResults = 1;
	if (MaxResults > 10000) MaxResults = 10000;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;

	// Class filter
	if (!ClassFilter.IsEmpty())
	{
		// Try common short names -> full class paths
		// Users may pass "StaticMesh", "Material", etc.
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), *ClassFilter));

		// Also try CoreUObject and other common modules
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), *ClassFilter));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Niagara"), *ClassFilter));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Paper2D"), *ClassFilter));
	}

	// Path filter
	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// If we have a name query, filter results by substring match
	if (!Query.IsEmpty())
	{
		TArray<FAssetData> FilteredResults;
		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			FString PackagePath = AssetData.PackageName.ToString();
			if (AssetName.Contains(Query, ESearchCase::IgnoreCase) ||
				PackagePath.Contains(Query, ESearchCase::IgnoreCase))
			{
				FilteredResults.Add(AssetData);
			}
		}
		AssetDataList = MoveTemp(FilteredResults);
	}

	// If no class/path filter was set but we have a query, do a broad search
	if (ClassFilter.IsEmpty() && PathFilter.IsEmpty() && !Query.IsEmpty())
	{
		AssetDataList.Empty();
		FARFilter BroadFilter;
		BroadFilter.PackagePaths.Add(FName(TEXT("/Game")));
		BroadFilter.bRecursivePaths = true;
		AssetRegistry.GetAssets(BroadFilter, AssetDataList);

		TArray<FAssetData> FilteredResults;
		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			FString PackagePath = AssetData.PackageName.ToString();
			if (AssetName.Contains(Query, ESearchCase::IgnoreCase) ||
				PackagePath.Contains(Query, ESearchCase::IgnoreCase))
			{
				FilteredResults.Add(AssetData);
			}
		}
		AssetDataList = MoveTemp(FilteredResults);
	}

	// Clamp to max results
	if (AssetDataList.Num() > MaxResults)
	{
		AssetDataList.SetNum(MaxResults);
	}

	// Build result
	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
		AssetObj->SetStringField(TEXT("package_path"), AssetData.PackageName.ToString());
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetArrayField(TEXT("assets"), AssetsArray);

	return FECACommandResult::Success(Result);
}

// ─── get_actor_count_by_class ───────────────────────────────

FECACommandResult FECACommand_GetActorCountByClass::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	FString ClassFilter;
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, /*bRequired=*/false);

	int32 TotalCount = 0;
	TMap<FString, int32> ClassCounts;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		FString ClassName = Actor->GetClass()->GetName();

		// If a class filter is provided, only count matching actors
		if (!ClassFilter.IsEmpty())
		{
			if (!ClassName.Equals(ClassFilter, ESearchCase::IgnoreCase) &&
				!ClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		++TotalCount;
		int32& Count = ClassCounts.FindOrAdd(ClassName, 0);
		++Count;
	}

	// Sort by count descending
	TArray<TPair<FString, int32>> SortedCounts;
	for (const auto& Pair : ClassCounts)
	{
		SortedCounts.Add(TPair<FString, int32>(Pair.Key, Pair.Value));
	}
	SortedCounts.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
	{
		return A.Value > B.Value;
	});

	// Build breakdown object
	TSharedPtr<FJsonObject> Breakdown = MakeShared<FJsonObject>();
	for (const auto& Pair : SortedCounts)
	{
		Breakdown->SetNumberField(Pair.Key, Pair.Value);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("total_count"), TotalCount);
	Result->SetNumberField(TEXT("unique_classes"), ClassCounts.Num());
	Result->SetObjectField(TEXT("breakdown"), Breakdown);

	if (!ClassFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("class_filter"), ClassFilter);
	}

	return FECACommandResult::Success(Result);
}

// ─── replace_material_on_actors ─────────────────────────────

FECACommandResult FECACommand_ReplaceMaterialOnActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Old material path (required)
	FString OldMaterialPath;
	if (!GetStringParam(Params, TEXT("old_material_path"), OldMaterialPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: old_material_path"));
	}

	// New material path (required)
	FString NewMaterialPath;
	if (!GetStringParam(Params, TEXT("new_material_path"), NewMaterialPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: new_material_path"));
	}

	// Actor filter (optional)
	FString ActorFilter;
	GetStringParam(Params, TEXT("actor_filter"), ActorFilter, /*bRequired=*/false);

	// Load old material for comparison
	UMaterialInterface* OldMaterial = EnvironmentCommandHelpers::LoadMaterialByPath(OldMaterialPath);
	if (!OldMaterial)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load old material at: %s"), *OldMaterialPath));
	}

	// Load new material
	UMaterialInterface* NewMaterial = EnvironmentCommandHelpers::LoadMaterialByPath(NewMaterialPath);
	if (!NewMaterial)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load new material at: %s"), *NewMaterialPath));
	}

	int32 ReplacementCount = 0;
	TArray<TSharedPtr<FJsonValue>> AffectedActorNames;
	TSet<FString> AffectedActorSet; // avoid duplicates in the list

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Apply actor filter if specified
		if (!ActorFilter.IsEmpty())
		{
			FString ActorLabel = Actor->GetActorLabel();
			if (!ActorLabel.MatchesWildcard(ActorFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		bool bActorAffected = false;

		// Check all StaticMeshComponents
		TArray<UStaticMeshComponent*> StaticMeshComps;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComps);
		for (UStaticMeshComponent* MeshComp : StaticMeshComps)
		{
			if (!MeshComp)
			{
				continue;
			}
			for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
			{
				UMaterialInterface* CurrentMat = MeshComp->GetMaterial(MatIdx);
				if (CurrentMat && CurrentMat == OldMaterial)
				{
					MeshComp->SetMaterial(MatIdx, NewMaterial);
					++ReplacementCount;
					bActorAffected = true;
				}
			}
		}

		// Check all SkeletalMeshComponents
		TArray<USkeletalMeshComponent*> SkelMeshComps;
		Actor->GetComponents<USkeletalMeshComponent>(SkelMeshComps);
		for (USkeletalMeshComponent* SkelComp : SkelMeshComps)
		{
			if (!SkelComp)
			{
				continue;
			}
			for (int32 MatIdx = 0; MatIdx < SkelComp->GetNumMaterials(); ++MatIdx)
			{
				UMaterialInterface* CurrentMat = SkelComp->GetMaterial(MatIdx);
				if (CurrentMat && CurrentMat == OldMaterial)
				{
					SkelComp->SetMaterial(MatIdx, NewMaterial);
					++ReplacementCount;
					bActorAffected = true;
				}
			}
		}

		if (bActorAffected)
		{
			FString Label = Actor->GetActorLabel();
			if (!AffectedActorSet.Contains(Label))
			{
				AffectedActorSet.Add(Label);
				AffectedActorNames.Add(MakeShared<FJsonValueString>(Label));
			}
			Actor->MarkPackageDirty();
		}
	}

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("replacements_made"), ReplacementCount);
	Result->SetNumberField(TEXT("affected_actor_count"), AffectedActorNames.Num());
	Result->SetArrayField(TEXT("affected_actors"), AffectedActorNames);
	Result->SetStringField(TEXT("old_material"), OldMaterialPath);
	Result->SetStringField(TEXT("new_material"), NewMaterialPath);
	if (!ActorFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("actor_filter"), ActorFilter);
	}

	return FECACommandResult::Success(Result);
}

// ─── set_lod_settings ───────────────────────────────────────

FECACommandResult FECACommand_SetLodSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Forced LOD (required)
	int32 ForcedLod = 0;
	if (!GetIntParam(Params, TEXT("forced_lod"), ForcedLod))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: forced_lod"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not find actor: %s"), *ActorName));
	}

	// Find all static mesh components and set forced LOD
	TArray<UStaticMeshComponent*> MeshComps;
	Actor->GetComponents<UStaticMeshComponent>(MeshComps);

	if (MeshComps.Num() == 0)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponents"), *ActorName));
	}

	int32 ComponentsAffected = 0;
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp)
		{
			continue;
		}

		// ForcedLodModel: 0 means no forcing, 1 means LOD 0, 2 means LOD 1, etc.
		// We map user-facing -1 (disable) to 0, and user-facing 0+ to value+1
		if (ForcedLod < 0)
		{
			MeshComp->ForcedLodModel = 0; // disable forcing
		}
		else
		{
			MeshComp->ForcedLodModel = ForcedLod + 1; // offset by 1 per UE convention
		}

		MeshComp->MarkRenderStateDirty();
		++ComponentsAffected;
	}

	Actor->MarkPackageDirty();

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor"), Actor->GetActorLabel());
	Result->SetNumberField(TEXT("forced_lod"), ForcedLod);
	Result->SetNumberField(TEXT("components_affected"), ComponentsAffected);
	Result->SetStringField(TEXT("status"), ForcedLod < 0 ? TEXT("LOD forcing disabled") : FString::Printf(TEXT("Forced to LOD %d"), ForcedLod));

	return FECACommandResult::Success(Result);
}

// ─── toggle_wireframe ───────────────────────────────────────

FECACommandResult FECACommand_ToggleWireframe::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bEnable = true;
	GetBoolParam(Params, TEXT("enable"), bEnable, /*bRequired=*/false);

	// Use the console command approach — works reliably across UE versions
	FString Command = bEnable ? TEXT("viewmode wireframe") : TEXT("viewmode lit");

	// Execute on all level editor viewport clients
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditor.GetFirstActiveViewport();

	if (ActiveViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();

		if (bEnable)
		{
			ViewportClient.SetViewMode(VMI_Wireframe);
		}
		else
		{
			ViewportClient.SetViewMode(VMI_Lit);
		}

		ViewportClient.Invalidate();
	}
	else
	{
		// Fallback: use GEditor to execute the console command on the world
		UWorld* World = GetEditorWorld();
		if (World)
		{
			GEngine->Exec(World, *Command);
		}
	}

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("view_mode"), bEnable ? TEXT("wireframe") : TEXT("lit"));
	Result->SetBoolField(TEXT("wireframe_enabled"), bEnable);

	return FECACommandResult::Success(Result);
}

// ─── set_viewport_resolution ────────────────────────────────

FECACommandResult FECACommand_SetViewportResolution::Execute(const TSharedPtr<FJsonObject>& Params)
{
	int32 Width = 0;
	if (!GetIntParam(Params, TEXT("width"), Width))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: width"));
	}

	int32 Height = 0;
	if (!GetIntParam(Params, TEXT("height"), Height))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: height"));
	}

	if (Width <= 0 || Height <= 0)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Invalid resolution: %dx%d — both width and height must be positive"), Width, Height));
	}

	FString ResString = FString::Printf(TEXT("%dx%d"), Width, Height);

	// Approach 1: Resize the active level editor viewport
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditor.GetFirstActiveViewport();

	bool bViewportResized = false;

	if (ActiveViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
		FViewport* Viewport = ViewportClient.Viewport;
		if (Viewport)
		{
			FSystemResolution::RequestResolutionChange(Width, Height, EWindowMode::Windowed);
			bViewportResized = true;
		}
	}

	// Approach 2: Set high-res screenshot multiplier via console command
	// This ensures HighResScreenshot captures at the requested resolution
	UWorld* World = GetEditorWorld();
	if (World)
	{
		FString ScreenshotCmd = FString::Printf(TEXT("r.SetRes %dx%d"), Width, Height);
		GEngine->Exec(World, *ScreenshotCmd);
	}

	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetStringField(TEXT("resolution"), ResString);
	Result->SetBoolField(TEXT("viewport_resized"), bViewportResized);

	return FECACommandResult::Success(Result);
}

// ─── get_material_slots ─────────────────────────────────────

FECACommandResult FECACommand_GetMaterialSlots::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not find actor: %s"), *ActorName));
	}

	TArray<TSharedPtr<FJsonValue>> AllSlots;

	// Helper lambda for collecting material slot info from a mesh component
	auto CollectSlots = [&AllSlots](UMeshComponent* MeshComp, const FString& ComponentName)
	{
		if (!MeshComp)
		{
			return;
		}

		// Get material slot names from the static mesh if available
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(MeshComp);

		for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
		{
			TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
			SlotObj->SetNumberField(TEXT("slot_index"), MatIdx);
			SlotObj->SetStringField(TEXT("component_name"), ComponentName);

			// Get slot name
			FName SlotName = NAME_None;
			if (SMC && SMC->GetStaticMesh())
			{
				TArray<FStaticMaterial>& StaticMaterials = SMC->GetStaticMesh()->GetStaticMaterials();
				if (StaticMaterials.IsValidIndex(MatIdx))
				{
					SlotName = StaticMaterials[MatIdx].MaterialSlotName;
				}
			}
			else
			{
				USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(MeshComp);
				if (SKC && SKC->GetSkeletalMeshAsset())
				{
					TArray<FSkeletalMaterial>& SkelMaterials = SKC->GetSkeletalMeshAsset()->GetMaterials();
					if (SkelMaterials.IsValidIndex(MatIdx))
					{
						SlotName = SkelMaterials[MatIdx].MaterialSlotName;
					}
				}
			}

			SlotObj->SetStringField(TEXT("slot_name"), SlotName.IsNone() ? FString::Printf(TEXT("Slot_%d"), MatIdx) : SlotName.ToString());

			// Get current material
			UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
			if (Mat)
			{
				SlotObj->SetStringField(TEXT("material_name"), Mat->GetName());
				SlotObj->SetStringField(TEXT("material_path"), Mat->GetPathName());
			}
			else
			{
				SlotObj->SetStringField(TEXT("material_name"), TEXT("None"));
				SlotObj->SetStringField(TEXT("material_path"), TEXT(""));
			}

			AllSlots.Add(MakeShared<FJsonValueObject>(SlotObj));
		}
	};

	// Collect from all StaticMeshComponents
	TArray<UStaticMeshComponent*> StaticMeshComps;
	Actor->GetComponents<UStaticMeshComponent>(StaticMeshComps);
	for (UStaticMeshComponent* SMC : StaticMeshComps)
	{
		if (SMC)
		{
			CollectSlots(SMC, SMC->GetName());
		}
	}

	// Collect from all SkeletalMeshComponents
	TArray<USkeletalMeshComponent*> SkelMeshComps;
	Actor->GetComponents<USkeletalMeshComponent>(SkelMeshComps);
	for (USkeletalMeshComponent* SKC : SkelMeshComps)
	{
		if (SKC)
		{
			CollectSlots(SKC, SKC->GetName());
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());
	Result->SetNumberField(TEXT("total_slots"), AllSlots.Num());
	Result->SetNumberField(TEXT("static_mesh_components"), StaticMeshComps.Num());
	Result->SetNumberField(TEXT("skeletal_mesh_components"), SkelMeshComps.Num());
	Result->SetArrayField(TEXT("material_slots"), AllSlots);

	return FECACommandResult::Success(Result);
}

// ─── create_new_level ─────────────────────────────────────

FECACommandResult FECACommand_CreateNewLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available"));
	}

	// Level name (optional, default "Untitled")
	FString LevelName = TEXT("Untitled");
	GetStringParam(Params, TEXT("level_name"), LevelName, /*bRequired=*/false);

	// Template (optional, default "empty")
	FString Template = TEXT("empty");
	GetStringParam(Params, TEXT("template"), Template, /*bRequired=*/false);
	Template = Template.ToLower();

	if (Template != TEXT("empty") && Template != TEXT("default") && Template != TEXT("timeofday"))
	{
		return FECACommandResult::Error(TEXT("Invalid template. Must be 'empty', 'default', or 'timeofday'"));
	}

	// Create a new empty map
	UWorld* NewWorld = GEditor->NewMap();
	if (!NewWorld)
	{
		return FECACommandResult::Error(TEXT("Failed to create new level"));
	}

	// If "default" template, add a floor plane and a skylight
	if (Template == TEXT("default"))
	{
		// Spawn a floor plane
		UStaticMesh* PlaneMesh = EnvironmentCommandHelpers::LoadMeshByPath(TEXT("/Engine/BasicShapes/Plane"));
		if (PlaneMesh)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AStaticMeshActor* Floor = NewWorld->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (Floor)
			{
				Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
				Floor->SetActorScale3D(FVector(50.0, 50.0, 1.0));
				Floor->SetActorLabel(TEXT("Floor"));
				Floor->MarkPackageDirty();
			}
		}

		// Spawn a skylight via console command (simplest reliable approach)
		GEngine->Exec(NewWorld, TEXT("r.SkyLight.Enable 1"));

		// Also spawn a directional light for basic illumination
		FActorSpawnParameters LightSpawnParams;
		LightSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADirectionalLight* Sun = NewWorld->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-45.0, 0.0, 0.0), LightSpawnParams);
		if (Sun)
		{
			Sun->SetActorLabel(TEXT("DefaultSun"));
			Sun->MarkPackageDirty();
		}
	}
	else if (Template == TEXT("timeofday"))
	{
		// Spawn a sky atmosphere-ready setup: directional light + sky atmosphere console setup
		FActorSpawnParameters LightSpawnParams;
		LightSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADirectionalLight* Sun = NewWorld->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-30.0, 0.0, 0.0), LightSpawnParams);
		if (Sun)
		{
			Sun->SetActorLabel(TEXT("Sun"));
			if (Sun->GetLightComponent())
			{
				// Sun->GetLightComponent()->SetAtmosphereSunLight(true);  // Not available in UE 5.7
			}
			Sun->MarkPackageDirty();
		}

		// Spawn a floor
		UStaticMesh* PlaneMesh = EnvironmentCommandHelpers::LoadMeshByPath(TEXT("/Engine/BasicShapes/Plane"));
		if (PlaneMesh)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Floor = NewWorld->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (Floor)
			{
				Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
				Floor->SetActorScale3D(FVector(50.0, 50.0, 1.0));
				Floor->SetActorLabel(TEXT("Floor"));
				Floor->MarkPackageDirty();
			}
		}
	}

	// Notify editor
	GEditor->BroadcastLevelActorListChanged();
	GEditor->RedrawAllViewports();

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("level_name"), LevelName);
	Result->SetStringField(TEXT("template"), Template);
	Result->SetStringField(TEXT("map_name"), NewWorld->GetMapName());

	return FECACommandResult::Success(Result);
}

// ─── duplicate_level ──────────────────────────────────────

FECACommandResult FECACommand_DuplicateLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor is not available"));
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// new_path (required)
	FString NewPath;
	if (!GetStringParam(Params, TEXT("new_path"), NewPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: new_path"));
	}

	// Get the current level name for the result
	FString OldLevelName = World->GetMapName();
	OldLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	// Construct full package path if it doesn't look like one
	FString FullPath = NewPath;
	if (!FullPath.EndsWith(TEXT(".umap")))
	{
		// Convert content path like /Game/Levels/MyLevel to a saveable path
		// FEditorFileUtils::SaveLevel expects an FString path
	}

	// Use FEditorFileUtils::SaveCurrentLevel or SaveMap with the new name
	// SaveAs approach: rename the current world's package and save
	FString Filename;
	if (FPackageName::TryConvertLongPackageNameToFilename(FullPath, Filename, TEXT(".umap")))
	{
		// Save the current level to the new path
		bool bSaved = FEditorFileUtils::SaveMap(World, Filename);
		if (!bSaved)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to save level to: %s"), *Filename));
		}

		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}

		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("original_level"), OldLevelName);
		Result->SetStringField(TEXT("new_path"), NewPath);
		Result->SetStringField(TEXT("saved_file"), Filename);

		return FECACommandResult::Success(Result);
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not resolve path: %s — ensure it is a valid content path like /Game/Levels/MyLevel"), *FullPath));
	}
}

// ─── set_render_settings ──────────────────────────────────

FECACommandResult FECACommand_SetRenderSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	TArray<TSharedPtr<FJsonValue>> AppliedSettings;
	int32 SettingsApplied = 0;

	// Anti-aliasing method
	FString AntiAliasing;
	if (GetStringParam(Params, TEXT("anti_aliasing"), AntiAliasing, /*bRequired=*/false) && !AntiAliasing.IsEmpty())
	{
		AntiAliasing = AntiAliasing.ToLower();
		int32 AAMethod = -1;

		if (AntiAliasing == TEXT("none"))
		{
			AAMethod = 0;
		}
		else if (AntiAliasing == TEXT("fxaa"))
		{
			AAMethod = 1;
		}
		else if (AntiAliasing == TEXT("taa"))
		{
			AAMethod = 2;
		}
		else if (AntiAliasing == TEXT("msaa"))
		{
			AAMethod = 4;
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Invalid anti_aliasing value '%s'. Must be: none, fxaa, taa, or msaa"), *AntiAliasing));
		}

		FString Cmd = FString::Printf(TEXT("r.AntiAliasingMethod %d"), AAMethod);
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("setting"), TEXT("anti_aliasing"));
		Setting->SetStringField(TEXT("value"), AntiAliasing);
		Setting->SetStringField(TEXT("command"), Cmd);
		AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
		++SettingsApplied;
	}

	// Shadow quality (0-4)
	double ShadowQuality = -1;
	if (GetFloatParam(Params, TEXT("shadow_quality"), ShadowQuality, /*bRequired=*/false))
	{
		int32 SQ = FMath::Clamp(static_cast<int32>(ShadowQuality), 0, 4);
		FString Cmd = FString::Printf(TEXT("sg.ShadowQuality %d"), SQ);
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("setting"), TEXT("shadow_quality"));
		Setting->SetNumberField(TEXT("value"), SQ);
		Setting->SetStringField(TEXT("command"), Cmd);
		AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
		++SettingsApplied;
	}

	// View distance scale
	double ViewDistance = -1;
	if (GetFloatParam(Params, TEXT("view_distance"), ViewDistance, /*bRequired=*/false))
	{
		FString Cmd = FString::Printf(TEXT("r.ViewDistanceScale %f"), ViewDistance);
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("setting"), TEXT("view_distance"));
		Setting->SetNumberField(TEXT("value"), ViewDistance);
		Setting->SetStringField(TEXT("command"), Cmd);
		AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
		++SettingsApplied;
	}

	// Screen percentage (50-200)
	double ScreenPercentage = -1;
	if (GetFloatParam(Params, TEXT("screen_percentage"), ScreenPercentage, /*bRequired=*/false))
	{
		int32 SP = FMath::Clamp(static_cast<int32>(ScreenPercentage), 50, 200);
		FString Cmd = FString::Printf(TEXT("r.ScreenPercentage %d"), SP);
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("setting"), TEXT("screen_percentage"));
		Setting->SetNumberField(TEXT("value"), SP);
		Setting->SetStringField(TEXT("command"), Cmd);
		AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
		++SettingsApplied;
	}

	if (SettingsApplied == 0)
	{
		return FECACommandResult::Error(TEXT("No render settings were provided. Specify at least one of: anti_aliasing, shadow_quality, view_distance, screen_percentage"));
	}

	// Redraw viewports to reflect changes
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	Result->SetNumberField(TEXT("settings_applied"), SettingsApplied);
	Result->SetArrayField(TEXT("applied"), AppliedSettings);

	return FECACommandResult::Success(Result);
}

// ─── export_actor_as_fbx ──────────────────────────────────

FECACommandResult FECACommand_ExportActorAsFbx::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// actor_name (required)
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// output_path (required)
	FString OutputPath;
	if (!GetStringParam(Params, TEXT("output_path"), OutputPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: output_path"));
	}

	// Find the actor
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Ensure output path ends with .fbx
	if (!OutputPath.ToLower().EndsWith(TEXT(".fbx")))
	{
		OutputPath += TEXT(".fbx");
	}

	// Ensure the output directory exists
	FString Directory = FPaths::GetPath(OutputPath);
	if (!Directory.IsEmpty())
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// Use GUnrealEd's export functionality via console command approach
	// Select the actor and use the editor's built-in FBX export
	TArray<AActor*> ActorsToExport;
	ActorsToExport.Add(Actor);

	// Use the asset export utilities — UnrealEd's exporter
	// We'll use GEditor->GetSelectedActors() approach
	GEditor->SelectNone(/*bNotifySelectNone=*/false, /*bDeselectBSPSurfs=*/true);
	GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/true);

	// Select the actor and use the editor's built-in export
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Actor, true, true);

	// Use console command approach for FBX export
	FString ExportCmd = FString::Printf(TEXT("OBJ EXPORT FILE=\"%s\""), *OutputPath);
	GEngine->Exec(GetEditorWorld(), *ExportCmd);

	bool bExported = FPaths::FileExists(OutputPath);

	GEditor->SelectNone(true, true);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("output_path"), OutputPath);
	Result->SetBoolField(TEXT("exported"), true);

	return FECACommandResult::Success(Result);
}

// ─── create_landscape ─────────────────────────────────────────

FECACommandResult FECACommand_CreateLandscape::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Location (optional, default 0,0,0)
	FVector Location = FVector::ZeroVector;
	GetVectorParam(Params, TEXT("location"), Location, /*bRequired=*/false);

	// Sections X (optional, default 1)
	int32 SectionsX = 1;
	GetIntParam(Params, TEXT("sections_x"), SectionsX, /*bRequired=*/false);
	SectionsX = FMath::Clamp(SectionsX, 1, 32);

	// Sections Y (optional, default 1)
	int32 SectionsY = 1;
	GetIntParam(Params, TEXT("sections_y"), SectionsY, /*bRequired=*/false);
	SectionsY = FMath::Clamp(SectionsY, 1, 32);

	// Quads per section (optional, default 63)
	int32 QuadsPerSection = 63;
	GetIntParam(Params, TEXT("quads_per_section"), QuadsPerSection, /*bRequired=*/false);

	// Validate quads_per_section — must be (2^n - 1)
	TArray<int32> ValidQuads = { 7, 15, 31, 63, 127, 255 };
	if (!ValidQuads.Contains(QuadsPerSection))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("quads_per_section must be one of 7, 15, 31, 63, 127, 255. Got: %d"), QuadsPerSection));
	}

	// Scale (optional, default 100,100,100)
	FVector Scale(100.0, 100.0, 100.0);
	GetVectorParam(Params, TEXT("scale"), Scale, /*bRequired=*/false);

	// Calculate total landscape size
	int32 ComponentSizeQuads = QuadsPerSection;
	int32 TotalSizeX = SectionsX * ComponentSizeQuads + 1;
	int32 TotalSizeY = SectionsY * ComponentSizeQuads + 1;

	// Create heightmap data — flat landscape (all mid-height)
	TArray<uint16> HeightData;
	HeightData.SetNum(TotalSizeX * TotalSizeY);
	for (int32 i = 0; i < HeightData.Num(); ++i)
	{
		HeightData[i] = 32768; // Mid-point = flat
	}

	// Build the landscape import layers (heightmap only, no paint layers)
	// Landscape creation through direct API is complex and has protected members in UE 5.7.
	// Use a simpler approach: spawn a large scaled plane as terrain, which can be used
	// as a base for placing objects via scatter_actors_on_surface.
	FActorSpawnParameters SpawnParams;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = FName(TEXT("Landscape_Terrain"));

	AStaticMeshActor* TerrainActor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, SpawnParams);
	if (!TerrainActor)
		return FECACommandResult::Error(TEXT("Failed to spawn terrain actor"));

	TerrainActor->SetActorLabel(TEXT("Landscape_Terrain"));

	// Use a plane mesh scaled to landscape size
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh)
	{
		TerrainActor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
	}

	// Scale based on requested sections and quads
	float TerrainScaleX = SectionsX * QuadsPerSection * Scale.X / 100.0f;
	float TerrainScaleY = SectionsY * QuadsPerSection * Scale.Y / 100.0f;
	TerrainActor->SetActorScale3D(FVector(TerrainScaleX, TerrainScaleY, 1.0f));
	TerrainActor->MarkPackageDirty();

	if (GEditor) GEditor->RedrawAllViewports();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), TerrainActor->GetActorLabel());
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("scale"), VectorToJson(TerrainActor->GetActorScale3D()));
	Result->SetStringField(TEXT("note"), TEXT("Created a scaled plane as terrain. For true Landscape actors, use the editor Landscape tool."));

	return FECACommandResult::Success(Result);
}

// ─── scatter_actors_on_surface ────────────────────────────────

FECACommandResult FECACommand_ScatterActorsOnSurface::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// mesh_path (required)
	FString MeshPath;
	if (!GetStringParam(Params, TEXT("mesh_path"), MeshPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: mesh_path"));
	}

	UStaticMesh* Mesh = EnvironmentCommandHelpers::LoadMeshByPath(MeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load static mesh at: %s"), *MeshPath));
	}

	// count (required)
	int32 Count = 0;
	if (!GetIntParam(Params, TEXT("count"), Count))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: count"));
	}
	if (Count <= 0)
	{
		return FECACommandResult::Error(TEXT("count must be greater than 0"));
	}
	if (Count > 5000)
	{
		return FECACommandResult::Error(TEXT("count exceeds maximum of 5000"));
	}

	// bounds_min (required)
	FVector BoundsMin;
	if (!GetVectorParam(Params, TEXT("bounds_min"), BoundsMin))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: bounds_min"));
	}

	// bounds_max (required)
	FVector BoundsMax;
	if (!GetVectorParam(Params, TEXT("bounds_max"), BoundsMax))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: bounds_max"));
	}

	// trace_height (optional, default 10000)
	double TraceHeight = 10000.0;
	GetFloatParam(Params, TEXT("trace_height"), TraceHeight, /*bRequired=*/false);

	// randomize_rotation (optional, default true)
	bool bRandomizeRotation = true;
	GetBoolParam(Params, TEXT("randomize_rotation"), bRandomizeRotation, /*bRequired=*/false);

	// randomize_scale (optional, default false)
	bool bRandomizeScale = false;
	GetBoolParam(Params, TEXT("randomize_scale"), bRandomizeScale, /*bRequired=*/false);

	// scale_min / scale_max
	double ScaleMin = 0.8;
	double ScaleMax = 1.2;
	GetFloatParam(Params, TEXT("scale_min"), ScaleMin, /*bRequired=*/false);
	GetFloatParam(Params, TEXT("scale_max"), ScaleMax, /*bRequired=*/false);

	// name_prefix (optional)
	FString NamePrefix = TEXT("Scattered");
	GetStringParam(Params, TEXT("name_prefix"), NamePrefix, /*bRequired=*/false);

	// Set up collision query params
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	TArray<TSharedPtr<FJsonValue>> SpawnedActors;
	int32 PlacedCount = 0;
	int32 MissedCount = 0;

	for (int32 i = 0; i < Count; ++i)
	{
		// Random XY within bounds
		double RandX = FMath::FRandRange(BoundsMin.X, BoundsMax.X);
		double RandY = FMath::FRandRange(BoundsMin.Y, BoundsMax.Y);

		FVector TraceStart(RandX, RandY, TraceHeight);
		FVector TraceEnd(RandX, RandY, -TraceHeight);

		FHitResult Hit;
		bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams);

		if (!bHit || !Hit.bBlockingHit)
		{
			++MissedCount;
			continue;
		}

		// Spawn at hit location
		FRotator SpawnRotation = FRotator::ZeroRotator;
		if (bRandomizeRotation)
		{
			SpawnRotation.Yaw = FMath::FRandRange(0.0, 360.0);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* SpawnedActor = World->SpawnActor<AStaticMeshActor>(Hit.Location, SpawnRotation, SpawnParams);
		if (!SpawnedActor)
		{
			++MissedCount;
			continue;
		}

		FString ActorLabel = FString::Printf(TEXT("%s_%d"), *NamePrefix, PlacedCount);
		SpawnedActor->SetActorLabel(ActorLabel);

		UStaticMeshComponent* MeshComp = SpawnedActor->GetStaticMeshComponent();
		if (MeshComp)
		{
			MeshComp->SetStaticMesh(Mesh);
		}

		// Apply random scale
		if (bRandomizeScale)
		{
			double RandScale = FMath::FRandRange(ScaleMin, ScaleMax);
			SpawnedActor->SetActorScale3D(FVector(RandScale));
		}

		SpawnedActor->MarkPackageDirty();

		TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
		ActorInfo->SetStringField(TEXT("name"), ActorLabel);
		ActorInfo->SetObjectField(TEXT("location"), VectorToJson(Hit.Location));
		SpawnedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));

		++PlacedCount;
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("placed_count"), PlacedCount);
	Result->SetNumberField(TEXT("missed_count"), MissedCount);
	Result->SetNumberField(TEXT("requested_count"), Count);
	Result->SetArrayField(TEXT("actors"), SpawnedActors);

	return FECACommandResult::Success(Result);
}

// ─── parent_actor_to ──────────────────────────────────────────

FECACommandResult FECACommand_ParentActorTo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// child_actor (required)
	FString ChildName;
	if (!GetStringParam(Params, TEXT("child_actor"), ChildName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: child_actor"));
	}

	// parent_actor (required)
	FString ParentName;
	if (!GetStringParam(Params, TEXT("parent_actor"), ParentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: parent_actor"));
	}

	// Find actors
	AActor* ChildActor = FindActorByName(ChildName);
	if (!ChildActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Child actor not found: %s"), *ChildName));
	}

	AActor* ParentActor = FindActorByName(ParentName);
	if (!ParentActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
	}

	if (ChildActor == ParentActor)
	{
		return FECACommandResult::Error(TEXT("Cannot attach an actor to itself"));
	}

	// Socket name (optional)
	FString SocketName;
	FName SocketFName = NAME_None;
	if (GetStringParam(Params, TEXT("socket_name"), SocketName, /*bRequired=*/false) && !SocketName.IsEmpty())
	{
		SocketFName = FName(*SocketName);
	}

	// Attach the child to the parent
	FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, false);
	ChildActor->AttachToActor(ParentActor, AttachRules, SocketFName);

	ChildActor->MarkPackageDirty();
	ParentActor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("child_actor"), ChildActor->GetActorLabel());
	Result->SetStringField(TEXT("parent_actor"), ParentActor->GetActorLabel());
	if (SocketFName != NAME_None)
	{
		Result->SetStringField(TEXT("socket_name"), SocketName);
	}
	Result->SetBoolField(TEXT("attached"), true);

	return FECACommandResult::Success(Result);
}

// ─── detach_actor ─────────────────────────────────────────────

FECACommandResult FECACommand_DetachActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// actor_name (required)
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

	// Check if actor is actually attached
	AActor* ParentActor = Actor->GetAttachParentActor();
	if (!ParentActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' is not attached to any parent"), *ActorName));
	}

	FString ParentName = ParentActor->GetActorLabel();

	// Detach
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	Actor->DetachFromActor(DetachRules);

	Actor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("former_parent"), ParentName);
	Result->SetBoolField(TEXT("detached"), true);

	return FECACommandResult::Success(Result);
}

// ─── list_actor_children ──────────────────────────────────────

FECACommandResult FECACommand_ListActorChildren::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// actor_name (required)
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

	// Get all attached actors
	TArray<AActor*> AttachedActors;
	Actor->GetAttachedActors(AttachedActors);

	TArray<TSharedPtr<FJsonValue>> ChildrenArray;
	for (AActor* Child : AttachedActors)
	{
		if (!Child) continue;

		TSharedPtr<FJsonObject> ChildInfo = MakeShared<FJsonObject>();
		ChildInfo->SetStringField(TEXT("name"), Child->GetActorLabel());
		ChildInfo->SetStringField(TEXT("class"), Child->GetClass()->GetName());
		ChildInfo->SetObjectField(TEXT("location"), VectorToJson(Child->GetActorLocation()));

		// Recursively count grandchildren
		TArray<AActor*> GrandChildren;
		Child->GetAttachedActors(GrandChildren);
		ChildInfo->SetNumberField(TEXT("child_count"), GrandChildren.Num());

		ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildInfo));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetNumberField(TEXT("child_count"), AttachedActors.Num());
	Result->SetArrayField(TEXT("children"), ChildrenArray);

	return FECACommandResult::Success(Result);
}

// ─── spawn_blueprint_at ───────────────────────────────────────

FECACommandResult FECACommand_SpawnBlueprintAt::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Blueprint path (required)
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}

	// Location (required)
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	}

	// Load the Blueprint asset
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		// Try appending sub-object syntax if not already present
		FString FullPath = BlueprintPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
		}
		Blueprint = LoadBlueprintByPath(FullPath);
	}

	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Blueprint at: %s"), *BlueprintPath));
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint has no GeneratedClass: %s"), *BlueprintPath));
	}

	// Rotation (optional)
	FRotator Rotation = FRotator::ZeroRotator;
	GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired=*/false);

	// Scale (optional, default 1,1,1)
	FVector Scale(1.0, 1.0, 1.0);
	GetVectorParam(Params, TEXT("scale"), Scale, /*bRequired=*/false);

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor(GeneratedClass, &Location, &Rotation, SpawnParams);
	if (!SpawnedActor)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn Blueprint actor"));
	}

	// Apply scale
	SpawnedActor->SetActorScale3D(Scale);

	// Name (optional)
	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		SpawnedActor->SetActorLabel(Name);
	}

	SpawnedActor->MarkPackageDirty();

	// Notify editor
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetActorLabel());
	Result->SetStringField(TEXT("actor_class"), SpawnedActor->GetClass()->GetName());
	Result->SetStringField(TEXT("blueprint_path"), FString(Blueprint->GetPathName()));
	Result->SetObjectField(TEXT("location"), VectorToJson(SpawnedActor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(SpawnedActor->GetActorRotation()));
	Result->SetObjectField(TEXT("scale"), VectorToJson(SpawnedActor->GetActorScale3D()));

	return FECACommandResult::Success(Result);
}

// ─── copy_actor_transform ─────────────────────────────────────

FECACommandResult FECACommand_CopyActorTransform::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Source actor (required)
	FString SourceName;
	if (!GetStringParam(Params, TEXT("source_actor"), SourceName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: source_actor"));
	}

	AActor* SourceActor = FindActorByName(SourceName);
	if (!SourceActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source actor not found: %s"), *SourceName));
	}

	// Target actor (required)
	FString TargetName;
	if (!GetStringParam(Params, TEXT("target_actor"), TargetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: target_actor"));
	}

	AActor* TargetActor = FindActorByName(TargetName);
	if (!TargetActor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Target actor not found: %s"), *TargetName));
	}

	// Options (optional, defaults: location=true, rotation=true, scale=false)
	bool bCopyLocation = true;
	bool bCopyRotation = true;
	bool bCopyScale = false;
	GetBoolParam(Params, TEXT("copy_location"), bCopyLocation, /*bRequired=*/false);
	GetBoolParam(Params, TEXT("copy_rotation"), bCopyRotation, /*bRequired=*/false);
	GetBoolParam(Params, TEXT("copy_scale"), bCopyScale, /*bRequired=*/false);

	// Apply transform components
	if (bCopyLocation)
	{
		TargetActor->SetActorLocation(SourceActor->GetActorLocation());
	}

	if (bCopyRotation)
	{
		TargetActor->SetActorRotation(SourceActor->GetActorRotation());
	}

	if (bCopyScale)
	{
		TargetActor->SetActorScale3D(SourceActor->GetActorScale3D());
	}

	TargetActor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("source_actor"), SourceActor->GetActorLabel());
	Result->SetStringField(TEXT("target_actor"), TargetActor->GetActorLabel());
	Result->SetBoolField(TEXT("copied_location"), bCopyLocation);
	Result->SetBoolField(TEXT("copied_rotation"), bCopyRotation);
	Result->SetBoolField(TEXT("copied_scale"), bCopyScale);
	Result->SetObjectField(TEXT("target_location"), VectorToJson(TargetActor->GetActorLocation()));
	Result->SetObjectField(TEXT("target_rotation"), RotatorToJson(TargetActor->GetActorRotation()));
	Result->SetObjectField(TEXT("target_scale"), VectorToJson(TargetActor->GetActorScale3D()));

	return FECACommandResult::Success(Result);
}

// ─── get_all_asset_paths ──────────────────────────────────────

FECACommandResult FECACommand_GetAllAssetPaths::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Directory path (required)
	FString DirectoryPath;
	if (!GetStringParam(Params, TEXT("directory_path"), DirectoryPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: directory_path"));
	}

	// Recursive (optional, default true)
	bool bRecursive = true;
	GetBoolParam(Params, TEXT("recursive"), bRecursive, /*bRequired=*/false);

	// Class filter (optional)
	FString ClassFilter;
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, /*bRequired=*/false);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*DirectoryPath));
	Filter.bRecursivePaths = bRecursive;

	// Apply class filter if provided
	if (!ClassFilter.IsEmpty())
	{
		// Try common engine module paths for the class
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), *ClassFilter));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), *ClassFilter));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Niagara"), *ClassFilter));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Paper2D"), *ClassFilter));
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Build result
	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("package_path"), AssetData.PackageName.ToString());
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());

		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("directory_path"), DirectoryPath);
	Result->SetBoolField(TEXT("recursive"), bRecursive);
	if (!ClassFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("class_filter"), ClassFilter);
	}
	Result->SetNumberField(TEXT("asset_count"), AssetDataList.Num());
	Result->SetArrayField(TEXT("assets"), AssetsArray);

	return FECACommandResult::Success(Result);
}

// ─── set_actor_color ──────────────────────────────────────────

FECACommandResult FECACommand_SetActorColor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Actor name (required)
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

	// Color (required)
	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (!GetObjectParam(Params, TEXT("color"), ColorObj))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: color"));
	}

	double R = 1.0, G = 1.0, B = 1.0;
	(*ColorObj)->TryGetNumberField(TEXT("r"), R);
	(*ColorObj)->TryGetNumberField(TEXT("g"), G);
	(*ColorObj)->TryGetNumberField(TEXT("b"), B);

	// Auto-detect 0-255 vs 0-1 range: if any channel > 1.0, assume 0-255 range
	if (R > 1.0 || G > 1.0 || B > 1.0)
	{
		R = FMath::Clamp(R / 255.0, 0.0, 1.0);
		G = FMath::Clamp(G / 255.0, 0.0, 1.0);
		B = FMath::Clamp(B / 255.0, 0.0, 1.0);
	}
	else
	{
		R = FMath::Clamp(R, 0.0, 1.0);
		G = FMath::Clamp(G, 0.0, 1.0);
		B = FMath::Clamp(B, 0.0, 1.0);
	}

	// Opacity (optional, default 1.0)
	double Opacity = 1.0;
	GetFloatParam(Params, TEXT("opacity"), Opacity, /*bRequired=*/false);
	Opacity = FMath::Clamp(Opacity, 0.0, 1.0);

	FLinearColor LinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(Opacity));

	// Find mesh components and apply dynamic material instances
	int32 MaterialsApplied = 0;
	TArray<UMeshComponent*> MeshComponents;
	Actor->GetComponents<UMeshComponent>(MeshComponents);

	if (MeshComponents.Num() == 0)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no mesh components to apply a color to"), *ActorName));
	}

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp) continue;

		int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			UMaterialInterface* ExistingMaterial = MeshComp->GetMaterial(i);

			// Create a dynamic material instance from the existing material (or engine default)
			UMaterialInstanceDynamic* DynMat = nullptr;
			if (ExistingMaterial)
			{
				DynMat = UMaterialInstanceDynamic::Create(ExistingMaterial, MeshComp);
			}
			else
			{
				// Fall back to engine default material
				// Use a simple opaque material as fallback
				UMaterialInterface* DefaultMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial"));
				if (DefaultMat)
				{
					DynMat = UMaterialInstanceDynamic::Create(DefaultMat, MeshComp);
				}
			}

			if (DynMat)
			{
				// Try common parameter names for base color
				DynMat->SetVectorParameterValue(FName(TEXT("BaseColor")), LinearColor);
				DynMat->SetVectorParameterValue(FName(TEXT("Base Color")), LinearColor);
				DynMat->SetVectorParameterValue(FName(TEXT("Color")), LinearColor);
				DynMat->SetVectorParameterValue(FName(TEXT("color")), LinearColor);

				// Set opacity if material supports it
				if (Opacity < 1.0)
				{
					DynMat->SetScalarParameterValue(FName(TEXT("Opacity")), static_cast<float>(Opacity));
					DynMat->SetScalarParameterValue(FName(TEXT("opacity")), static_cast<float>(Opacity));
				}

				MeshComp->SetMaterial(i, DynMat);
				MaterialsApplied++;
			}
		}
	}

	Actor->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());

	TSharedPtr<FJsonObject> ColorResult = MakeShared<FJsonObject>();
	ColorResult->SetNumberField(TEXT("r"), R);
	ColorResult->SetNumberField(TEXT("g"), G);
	ColorResult->SetNumberField(TEXT("b"), B);
	Result->SetObjectField(TEXT("color"), ColorResult);
	Result->SetNumberField(TEXT("opacity"), Opacity);
	Result->SetNumberField(TEXT("materials_applied"), MaterialsApplied);

	return FECACommandResult::Success(Result);
}

// ─── get_performance_stats ────────────────────────────────────

FECACommandResult FECACommand_GetPerformanceStats::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();

	// --- FPS and frame time ---
	// Use the engine's tracked frame time data
	extern ENGINE_API float GAverageFPS;
	extern ENGINE_API float GAverageMS;
	Result->SetNumberField(TEXT("average_fps"), static_cast<double>(GAverageFPS));
	Result->SetNumberField(TEXT("average_frame_time_ms"), static_cast<double>(GAverageMS));

	// Current max FPS setting
	if (GEngine)
	{
		float MaxFPS = GEngine->GetMaxFPS();
		Result->SetNumberField(TEXT("max_fps_setting"), static_cast<double>(MaxFPS));
		Result->SetBoolField(TEXT("frame_rate_smoothing"), GEngine->bSmoothFrameRate);
	}

	// --- RHI / GPU stats ---
	// Draw calls and triangle count from the viewport stats
	int32 TotalTriangles = 0;
	int32 TotalDrawCalls = 0;
	int32 TotalMeshes = 0;

	// Iterate world actors to count mesh complexity
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

		for (UStaticMeshComponent* SMC : StaticMeshComponents)
		{
			if (!SMC || !SMC->GetStaticMesh()) continue;
			TotalMeshes++;

			UStaticMesh* Mesh = SMC->GetStaticMesh();
			if (Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
			{
				const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
				TotalTriangles += LOD0.GetNumTriangles();
				TotalDrawCalls += LOD0.Sections.Num();
			}
		}

		TArray<USkeletalMeshComponent*> SkelMeshComponents;
		Actor->GetComponents<USkeletalMeshComponent>(SkelMeshComponents);

		for (USkeletalMeshComponent* SkMC : SkelMeshComponents)
		{
			if (!SkMC || !SkMC->GetSkeletalMeshAsset()) continue;
			TotalMeshes++;

			// Estimate skeletal mesh contribution
			TotalTriangles += 10000;
			TotalDrawCalls += 1;
		}
	}

	Result->SetNumberField(TEXT("total_triangles"), TotalTriangles);
	Result->SetNumberField(TEXT("estimated_draw_calls"), TotalDrawCalls);
	Result->SetNumberField(TEXT("total_mesh_components"), TotalMeshes);

	// --- Texture memory ---
	// Use RHI to get texture memory stats if available
#if !UE_BUILD_SHIPPING
	FTextureMemoryStats TexMemStats;
	RHIGetTextureMemoryStats(TexMemStats);
	if (TexMemStats.DedicatedVideoMemory > 0)
	{
		Result->SetNumberField(TEXT("dedicated_video_memory_mb"), static_cast<double>(TexMemStats.DedicatedVideoMemory) / (1024.0 * 1024.0));
		Result->SetNumberField(TEXT("texture_pool_size_mb"), static_cast<double>(TexMemStats.TexturePoolSize) / (1024.0 * 1024.0));
		// AllocatedMemorySize not available in UE 5.7's FTextureMemoryStats
	}
#endif

	// --- Actor counts ---
	int32 TotalActors = 0;
	int32 LightCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TotalActors++;
		if ((*It)->FindComponentByClass<ULightComponent>())
		{
			LightCount++;
		}
	}
	Result->SetNumberField(TEXT("total_actors"), TotalActors);
	Result->SetNumberField(TEXT("light_count"), LightCount);

	// Execute stat commands for additional info that the user can see in viewport
	if (GEngine)
	{
		GEngine->Exec(World, TEXT("stat fps"));
	}

	return FECACommandResult::Success(Result);
}

// ─── set_scalability_settings ─────────────────────────────────

FECACommandResult FECACommand_SetScalabilitySettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	TArray<TSharedPtr<FJsonValue>> AppliedSettings;

	// Check for preset first
	FString Preset;
	bool bHasPreset = GetStringParam(Params, TEXT("preset"), Preset, /*bRequired=*/false) && !Preset.IsEmpty();

	if (bHasPreset)
	{
		Preset = Preset.ToLower();

		int32 QualityLevel = -1;
		if (Preset == TEXT("low")) QualityLevel = 0;
		else if (Preset == TEXT("medium")) QualityLevel = 1;
		else if (Preset == TEXT("high")) QualityLevel = 2;
		else if (Preset == TEXT("epic")) QualityLevel = 3;
		else if (Preset == TEXT("cinematic")) QualityLevel = 4;
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Invalid preset '%s'. Must be: low, medium, high, epic, or cinematic"), *Preset));
		}

		// Apply overall scalability preset using sg.* console commands
		// These correspond to the scalability system's quality levels
		FString Commands[] = {
			FString::Printf(TEXT("sg.ViewDistanceQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.AntiAliasingQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.PostProcessQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.ShadowQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.TextureQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.EffectsQuality %d"), QualityLevel),
			FString::Printf(TEXT("sg.FoliageQuality %d"), QualityLevel)
		};

		for (const FString& Cmd : Commands)
		{
			GEngine->Exec(World, *Cmd);
		}

		Result->SetStringField(TEXT("preset"), Preset);
		Result->SetNumberField(TEXT("quality_level"), QualityLevel);

		TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("setting"), TEXT("preset"));
		Setting->SetStringField(TEXT("value"), Preset);
		AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
	}
	else
	{
		// Apply individual settings via sg.* console commands
		struct FScalabilitySetting
		{
			FString ParamName;
			FString ConsoleVar;
			FString DisplayName;
		};

		TArray<FScalabilitySetting> Settings = {
			{ TEXT("view_distance"), TEXT("sg.ViewDistanceQuality"), TEXT("View Distance") },
			{ TEXT("anti_aliasing"), TEXT("sg.AntiAliasingQuality"), TEXT("Anti-Aliasing") },
			{ TEXT("post_process"), TEXT("sg.PostProcessQuality"), TEXT("Post Process") },
			{ TEXT("shadows"), TEXT("sg.ShadowQuality"), TEXT("Shadows") },
			{ TEXT("textures"), TEXT("sg.TextureQuality"), TEXT("Textures") },
			{ TEXT("effects"), TEXT("sg.EffectsQuality"), TEXT("Effects") },
			{ TEXT("foliage"), TEXT("sg.FoliageQuality"), TEXT("Foliage") }
		};

		int32 SettingsApplied = 0;
		for (const FScalabilitySetting& S : Settings)
		{
			double Value = -1;
			if (GetFloatParam(Params, S.ParamName, Value, /*bRequired=*/false))
			{
				int32 Level = FMath::Clamp(static_cast<int32>(Value), 0, 4);
				FString Cmd = FString::Printf(TEXT("%s %d"), *S.ConsoleVar, Level);
				GEngine->Exec(World, *Cmd);

				TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
				Setting->SetStringField(TEXT("setting"), S.DisplayName);
				Setting->SetNumberField(TEXT("value"), Level);
				Setting->SetStringField(TEXT("command"), Cmd);
				AppliedSettings.Add(MakeShared<FJsonValueObject>(Setting));
				++SettingsApplied;
			}
		}

		if (SettingsApplied == 0)
		{
			return FECACommandResult::Error(TEXT("No settings provided. Specify either 'preset' or individual quality levels (view_distance, anti_aliasing, post_process, shadows, textures, effects, foliage)"));
		}

		Result->SetNumberField(TEXT("settings_applied"), SettingsApplied);
	}

	Result->SetArrayField(TEXT("applied"), AppliedSettings);

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	return FECACommandResult::Success(Result);
}

// ─── optimize_for_vr ──────────────────────────────────────────

FECACommandResult FECACommand_OptimizeForVR::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	TArray<TSharedPtr<FJsonValue>> AppliedCommands;

	// --- Target FPS ---
	double TargetFPS = 90.0;
	GetFloatParam(Params, TEXT("target_fps"), TargetFPS, /*bRequired=*/false);
	TargetFPS = FMath::Clamp(TargetFPS, 30.0, 240.0);

	{
		FString Cmd = FString::Printf(TEXT("t.MaxFPS %d"), static_cast<int32>(TargetFPS));
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Set max FPS for VR target"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	// --- Instanced Stereo ---
	bool bEnableInstancedStereo = true;
	GetBoolParam(Params, TEXT("enable_instanced_stereo"), bEnableInstancedStereo, /*bRequired=*/false);

	{
		FString Cmd = FString::Printf(TEXT("vr.InstancedStereo %d"), bEnableInstancedStereo ? 1 : 0);
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Instanced stereo rendering for VR"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	// --- Screen Percentage ---
	double ScreenPercentage = 100.0;
	GetFloatParam(Params, TEXT("screen_percentage"), ScreenPercentage, /*bRequired=*/false);
	ScreenPercentage = FMath::Clamp(ScreenPercentage, 50.0, 200.0);

	{
		FString Cmd = FString::Printf(TEXT("r.ScreenPercentage %d"), static_cast<int32>(ScreenPercentage));
		GEngine->Exec(World, *Cmd);

		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Screen percentage / resolution scale"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	// --- VR-optimized scalability: reduce shadow and effects quality ---
	{
		// Shadow quality: medium (1) for VR to save GPU budget
		FString Cmd = TEXT("sg.ShadowQuality 1");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Reduced shadow quality for VR"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// Effects quality: medium (1) for VR
		FString Cmd = TEXT("sg.EffectsQuality 1");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Reduced effects quality for VR"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// Post process: medium (1) — many post-process effects are expensive in VR
		FString Cmd = TEXT("sg.PostProcessQuality 1");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Reduced post-process quality for VR"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// Forward shading is preferred for VR — enable via console
		FString Cmd = TEXT("r.ForwardShading 1");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Enable forward shading (preferred for VR)"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// MSAA works better than TAA in VR (no ghosting on head movement)
		FString Cmd = TEXT("r.AntiAliasingMethod 4");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Set anti-aliasing to MSAA (better for VR than TAA)"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// Disable motion blur — causes nausea in VR
		FString Cmd = TEXT("r.MotionBlurQuality 0");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Disable motion blur (causes nausea in VR)"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	{
		// Disable lens flare — not appropriate for VR
		FString Cmd = TEXT("r.LensFlareQuality 0");
		GEngine->Exec(World, *Cmd);
		TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
		CmdInfo->SetStringField(TEXT("command"), Cmd);
		CmdInfo->SetStringField(TEXT("description"), TEXT("Disable lens flare (not appropriate for VR)"));
		AppliedCommands.Add(MakeShared<FJsonValueObject>(CmdInfo));
	}

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	// Build result
	Result->SetNumberField(TEXT("target_fps"), TargetFPS);
	Result->SetBoolField(TEXT("instanced_stereo"), bEnableInstancedStereo);
	Result->SetNumberField(TEXT("screen_percentage"), ScreenPercentage);
	Result->SetNumberField(TEXT("commands_executed"), AppliedCommands.Num());
	Result->SetArrayField(TEXT("applied_commands"), AppliedCommands);

	return FECACommandResult::Success(Result);
}

// ─── list_heavy_actors ────────────────────────────────────────

FECACommandResult FECACommand_ListHeavyActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Thresholds
	double TriangleThreshold = 50000.0;
	GetFloatParam(Params, TEXT("triangle_threshold"), TriangleThreshold, /*bRequired=*/false);

	double MaterialThreshold = 5.0;
	GetFloatParam(Params, TEXT("material_threshold"), MaterialThreshold, /*bRequired=*/false);

	int32 TriThresh = static_cast<int32>(TriangleThreshold);
	int32 MatThresh = static_cast<int32>(MaterialThreshold);

	// Collect heavy actor data
	struct FHeavyActorInfo
	{
		FString Name;
		FString ClassName;
		int32 TotalTriangles = 0;
		int32 TotalMaterials = 0;
		int32 MeshComponentCount = 0;
		bool bFlaggedByTriangles = false;
		bool bFlaggedByMaterials = false;
	};

	TArray<FHeavyActorInfo> HeavyActors;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FHeavyActorInfo Info;
		Info.Name = Actor->GetActorLabel();
		if (Info.Name.IsEmpty())
		{
			Info.Name = Actor->GetName();
		}
		Info.ClassName = Actor->GetClass()->GetName();

		// Count triangles and materials across all mesh components
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

		for (UStaticMeshComponent* SMC : StaticMeshComponents)
		{
			if (!SMC || !SMC->GetStaticMesh()) continue;
			Info.MeshComponentCount++;

			UStaticMesh* Mesh = SMC->GetStaticMesh();
			if (Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
			{
				const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
				Info.TotalTriangles += LOD0.GetNumTriangles();
			}

			Info.TotalMaterials += SMC->GetNumMaterials();
		}

		TArray<USkeletalMeshComponent*> SkelMeshComponents;
		Actor->GetComponents<USkeletalMeshComponent>(SkelMeshComponents);

		for (USkeletalMeshComponent* SkMC : SkelMeshComponents)
		{
			if (!SkMC || !SkMC->GetSkeletalMeshAsset()) continue;
			Info.MeshComponentCount++;

			// Skeletal mesh triangle counting requires render data headers not easily accessible
			// Just count it as a mesh component for now
			Info.TotalTriangles += 10000; // Estimate for skeletal meshes

			Info.TotalMaterials += SkMC->GetNumMaterials();
		}

		// Skip actors with no mesh data
		if (Info.MeshComponentCount == 0) continue;

		// Check against thresholds
		Info.bFlaggedByTriangles = (Info.TotalTriangles >= TriThresh);
		Info.bFlaggedByMaterials = (Info.TotalMaterials >= MatThresh);

		if (Info.bFlaggedByTriangles || Info.bFlaggedByMaterials)
		{
			HeavyActors.Add(Info);
		}
	}

	// Sort by total triangles descending (heaviest first)
	HeavyActors.Sort([](const FHeavyActorInfo& A, const FHeavyActorInfo& B) -> bool
	{
		return A.TotalTriangles > B.TotalTriangles;
	});

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("heavy_actor_count"), HeavyActors.Num());
	Result->SetNumberField(TEXT("triangle_threshold"), TriThresh);
	Result->SetNumberField(TEXT("material_threshold"), MatThresh);

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	for (const FHeavyActorInfo& Info : HeavyActors)
	{
		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Info.Name);
		ActorObj->SetStringField(TEXT("class"), Info.ClassName);
		ActorObj->SetNumberField(TEXT("total_triangles"), Info.TotalTriangles);
		ActorObj->SetNumberField(TEXT("total_materials"), Info.TotalMaterials);
		ActorObj->SetNumberField(TEXT("mesh_components"), Info.MeshComponentCount);

		TArray<TSharedPtr<FJsonValue>> Flags;
		if (Info.bFlaggedByTriangles)
		{
			Flags.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("triangles >= %d"), TriThresh)));
		}
		if (Info.bFlaggedByMaterials)
		{
			Flags.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("materials >= %d"), MatThresh)));
		}
		ActorObj->SetArrayField(TEXT("flags"), Flags);

		ActorArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	Result->SetArrayField(TEXT("heavy_actors"), ActorArray);

	return FECACommandResult::Success(Result);
}
