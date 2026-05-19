// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASkyAtmosphereCommands.h"

#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"

#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/World.h"

REGISTER_ECA_COMMAND(FECACommand_SetSunPosition);
REGISTER_ECA_COMMAND(FECACommand_SetSkyAtmosphereSettings);
REGISTER_ECA_COMMAND(FECACommand_SetVolumetricCloudSettings);

namespace SkyHelpers
{
	template<typename TActor>
	static TActor* FindActorByNameOrFirst(UWorld* World, const FString& Name)
	{
		if (!World) return nullptr;
		if (Name.IsEmpty())
		{
			for (TActorIterator<TActor> It(World); It; ++It) return *It;
			return nullptr;
		}
		for (TActorIterator<TActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() == Name || It->GetName() == Name) return *It;
		}
		return nullptr;
	}

	// Convert (elevation, azimuth in degrees, from-north-clockwise) to a sun-light
	// rotator. Sun direction (toward the sun) = (cos(elev)*sin(az), cos(elev)*cos(az), sin(elev))
	// in a Z-up, X=north, Y=east frame. The DirectionalLight's forward axis points
	// in the light's travel direction (i.e. away from the sun), so we rotate to face
	// the OPPOSITE of the sun direction.
	static FRotator ElevationAzimuthToLightRotator(double ElevationDeg, double AzimuthDeg)
	{
		const double ElevRad = FMath::DegreesToRadians(ElevationDeg);
		const double AzRad = FMath::DegreesToRadians(AzimuthDeg);
		const FVector SunDir(
			FMath::Cos(ElevRad) * FMath::Cos(AzRad),
			FMath::Cos(ElevRad) * FMath::Sin(AzRad),
			FMath::Sin(ElevRad)
		);
		const FVector LightForward = -SunDir;
		return LightForward.Rotation();
	}
}

// ─── set_sun_position ────────────────────────────────────────

FECACommandResult FECACommand_SetSunPosition::Execute(const TSharedPtr<FJsonObject>& Params)
{
	double Elev;
	if (!GetFloatParam(Params, TEXT("elevation_deg"), Elev))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: elevation_deg"));
	}
	double Az;
	if (!GetFloatParam(Params, TEXT("azimuth_deg"), Az))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: azimuth_deg"));
	}

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);

	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	ADirectionalLight* Sun = SkyHelpers::FindActorByNameOrFirst<ADirectionalLight>(World, ActorName);
	if (!Sun) return FECACommandResult::Error(TEXT("No DirectionalLight actor found"));

	const FRotator NewRot = SkyHelpers::ElevationAzimuthToLightRotator(Elev, Az);
	Sun->SetActorRotation(NewRot);
	Sun->MarkPackageDirty();

	// If a SkyAtmosphere actor exists, also override its atmosphere-light direction
	// so the sky responds without waiting for the light's listener callback.
	bool bOverroteAtmosphere = false;
	if (ASkyAtmosphere* Sky = SkyHelpers::FindActorByNameOrFirst<ASkyAtmosphere>(World, FString()))
	{
		if (USkyAtmosphereComponent* Comp = Sky->GetComponent())
		{
			const double ElevRad = FMath::DegreesToRadians(Elev);
			const double AzRad = FMath::DegreesToRadians(Az);
			const FVector SunDir(
				FMath::Cos(ElevRad) * FMath::Cos(AzRad),
				FMath::Cos(ElevRad) * FMath::Sin(AzRad),
				FMath::Sin(ElevRad));
			Comp->OverrideAtmosphereLightDirection(0, -SunDir);
			bOverroteAtmosphere = true;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), Sun->GetActorLabel());
	Result->SetNumberField(TEXT("elevation_deg"), Elev);
	Result->SetNumberField(TEXT("azimuth_deg"), Az);
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(NewRot));
	Result->SetBoolField(TEXT("sky_atmosphere_updated"), bOverroteAtmosphere);
	return FECACommandResult::Success(Result);
}

// ─── set_sky_atmosphere_settings ─────────────────────────────

FECACommandResult FECACommand_SetSkyAtmosphereSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);

	ASkyAtmosphere* Sky = SkyHelpers::FindActorByNameOrFirst<ASkyAtmosphere>(World, ActorName);
	if (!Sky) return FECACommandResult::Error(TEXT("No SkyAtmosphere actor found"));
	USkyAtmosphereComponent* Comp = Sky->GetComponent();
	if (!Comp) return FECACommandResult::Error(TEXT("SkyAtmosphere has no component"));

	TSharedPtr<FJsonObject> Result = MakeResult();
	int32 Applied = 0;
	double V;

	if (GetFloatParam(Params, TEXT("rayleigh_scattering_scale"), V, /*bRequired=*/false))
	{
		Comp->SetRayleighScatteringScale((float)V); Result->SetNumberField(TEXT("rayleigh_scattering_scale"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("mie_scattering_scale"), V, /*bRequired=*/false))
	{
		Comp->SetMieScatteringScale((float)V); Result->SetNumberField(TEXT("mie_scattering_scale"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("mie_absorption_scale"), V, /*bRequired=*/false))
	{
		Comp->SetMieAbsorptionScale((float)V); Result->SetNumberField(TEXT("mie_absorption_scale"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("mie_anisotropy"), V, /*bRequired=*/false))
	{
		Comp->SetMieAnisotropy((float)V); Result->SetNumberField(TEXT("mie_anisotropy"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("multi_scattering_factor"), V, /*bRequired=*/false))
	{
		Comp->SetMultiScatteringFactor((float)V); Result->SetNumberField(TEXT("multi_scattering_factor"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("atmosphere_height_km"), V, /*bRequired=*/false))
	{
		Comp->SetAtmosphereHeight((float)V); Result->SetNumberField(TEXT("atmosphere_height_km"), V); ++Applied;
	}

	if (Applied == 0)
	{
		return FECACommandResult::Error(TEXT("No valid settings provided"));
	}

	Sky->MarkPackageDirty();
	Result->SetStringField(TEXT("actor_name"), Sky->GetActorLabel());
	Result->SetNumberField(TEXT("settings_applied"), Applied);
	return FECACommandResult::Success(Result);
}

// ─── set_volumetric_cloud_settings ──────────────────────────

FECACommandResult FECACommand_SetVolumetricCloudSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);

	// AVolumetricCloud lives in Engine; find by class iteration via base AActor + component check
	AActor* CloudActor = nullptr;
	UVolumetricCloudComponent* Comp = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		UVolumetricCloudComponent* C = A->FindComponentByClass<UVolumetricCloudComponent>();
		if (!C) continue;
		if (ActorName.IsEmpty() || A->GetActorLabel() == ActorName || A->GetName() == ActorName)
		{
			CloudActor = A;
			Comp = C;
			break;
		}
	}
	if (!Comp) return FECACommandResult::Error(TEXT("No VolumetricCloud actor found"));

	TSharedPtr<FJsonObject> Result = MakeResult();
	int32 Applied = 0;
	double V;

	if (GetFloatParam(Params, TEXT("layer_bottom_altitude_km"), V, /*bRequired=*/false))
	{
		Comp->SetLayerBottomAltitude((float)V); Result->SetNumberField(TEXT("layer_bottom_altitude_km"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("layer_height_km"), V, /*bRequired=*/false))
	{
		Comp->SetLayerHeight((float)V); Result->SetNumberField(TEXT("layer_height_km"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("tracing_start_max_distance_km"), V, /*bRequired=*/false))
	{
		Comp->SetTracingStartMaxDistance((float)V); Result->SetNumberField(TEXT("tracing_start_max_distance_km"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("tracing_max_distance_km"), V, /*bRequired=*/false))
	{
		Comp->SetTracingMaxDistance((float)V); Result->SetNumberField(TEXT("tracing_max_distance_km"), V); ++Applied;
	}
	if (GetFloatParam(Params, TEXT("view_sample_count_scale"), V, /*bRequired=*/false))
	{
		Comp->SetViewSampleCountScale((float)V); Result->SetNumberField(TEXT("view_sample_count_scale"), V); ++Applied;
	}

	if (Applied == 0) return FECACommandResult::Error(TEXT("No valid settings provided"));

	CloudActor->MarkPackageDirty();
	Result->SetStringField(TEXT("actor_name"), CloudActor->GetActorLabel());
	Result->SetNumberField(TEXT("settings_applied"), Applied);
	return FECACommandResult::Success(Result);
}
