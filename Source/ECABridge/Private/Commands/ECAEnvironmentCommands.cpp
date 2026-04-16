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
