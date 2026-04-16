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
#include "Camera/CameraActor.h"
#include "Engine/Light.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/PackageName.h"

// ─── REGISTER ───────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_SpawnStaticMeshWall);
REGISTER_ECA_COMMAND(FECACommand_CreateFog);
REGISTER_ECA_COMMAND(FECACommand_SetSkySettings);
REGISTER_ECA_COMMAND(FECACommand_SpawnParticleEffect);
REGISTER_ECA_COMMAND(FECACommand_SetWorldGravity);
REGISTER_ECA_COMMAND(FECACommand_GetSceneStats);
REGISTER_ECA_COMMAND(FECACommand_BatchSetActorProperty);
REGISTER_ECA_COMMAND(FECACommand_CreateSplinePath);

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
