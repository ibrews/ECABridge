// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECALightingCommands.h"

#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture.h"

#include "Components/LightComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"

#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/PackageName.h"

// ─── Helpers ─────────────────────────────────────────────────

namespace LightingCommandHelpers
{
	/** Get the ULightComponent from an actor, or nullptr if not a light */
	static ULightComponent* GetLightComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		// Try the ALight accessor first
		ALight* LightActor = Cast<ALight>(Actor);
		if (LightActor)
		{
			return LightActor->GetLightComponent();
		}

		// Fall back to component search
		return Actor->FindComponentByClass<ULightComponent>();
	}

	/** Determine the light type as a human-readable string */
	static FString GetLightTypeName(const ULightComponent* LightComp)
	{
		if (!LightComp)
		{
			return TEXT("unknown");
		}

		if (LightComp->IsA<USpotLightComponent>())
		{
			return TEXT("spot");
		}
		if (LightComp->IsA<UPointLightComponent>())
		{
			return TEXT("point");
		}
		if (LightComp->IsA<UDirectionalLightComponent>())
		{
			return TEXT("directional");
		}
		return TEXT("unknown");
	}

	/** Mobility enum to string */
	static FString MobilityToString(EComponentMobility::Type Mobility)
	{
		switch (Mobility)
		{
		case EComponentMobility::Static:     return TEXT("static");
		case EComponentMobility::Stationary: return TEXT("stationary");
		case EComponentMobility::Movable:    return TEXT("movable");
		default:                             return TEXT("unknown");
		}
	}

	/** Find the first APostProcessVolume in the editor world */
	static APostProcessVolume* FindFirstPostProcessVolume(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

// ─── REGISTER ────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_SetLightProperties);
REGISTER_ECA_COMMAND(FECACommand_GetLightProperties);
REGISTER_ECA_COMMAND(FECACommand_CreateLightRig);
REGISTER_ECA_COMMAND(FECACommand_SetPostProcessSettings);

// ─── set_light_properties ────────────────────────────────────

FECACommandResult FECACommand_SetLightProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));
	}

	ULightComponent* LightComp = LightingCommandHelpers::GetLightComponent(Actor);

	// SkyLight uses USkyLightComponent (inherits ULightComponentBase, NOT ULightComponent)
	// Handle it as a special case before the normal path.
	USkyLightComponent* SkyComp = Actor->FindComponentByClass<USkyLightComponent>();
	if (!LightComp && SkyComp)
	{
		int32 PropertiesSet = 0;
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("actor_name"), ActorName);
		Result->SetStringField(TEXT("light_type"), TEXT("SkyLight"));

		double Intensity;
		if (GetFloatParam(Params, TEXT("intensity"), Intensity, /*bRequired=*/false))
		{
			SkyComp->SetIntensity(static_cast<float>(Intensity));
			Result->SetNumberField(TEXT("intensity"), Intensity);
			++PropertiesSet;
		}

		const TSharedPtr<FJsonObject>* ColorObj = nullptr;
		if (GetObjectParam(Params, TEXT("color"), ColorObj, /*bRequired=*/false))
		{
			double R = (*ColorObj)->GetNumberField(TEXT("r"));
			double G = (*ColorObj)->GetNumberField(TEXT("g"));
			double B = (*ColorObj)->GetNumberField(TEXT("b"));
			SkyComp->SetLightColor(FLinearColor(
				static_cast<float>(R / 255.0),
				static_cast<float>(G / 255.0),
				static_cast<float>(B / 255.0), 1.0f));
			++PropertiesSet;
		}

		if (PropertiesSet == 0)
			return FECACommandResult::Error(TEXT("No valid properties provided for SkyLight. Supported: intensity, color."));

		SkyComp->MarkRenderStateDirty();
		Actor->MarkPackageDirty();
		Result->SetNumberField(TEXT("properties_set"), PropertiesSet);
		return FECACommandResult::Success(Result);
	}

	if (!LightComp)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no light component"), *ActorName));
	}

	int32 PropertiesSet = 0;
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("light_type"), LightingCommandHelpers::GetLightTypeName(LightComp));

	// Intensity
	double Intensity;
	if (GetFloatParam(Params, TEXT("intensity"), Intensity, /*bRequired=*/false))
	{
		LightComp->SetIntensity(static_cast<float>(Intensity));
		Result->SetNumberField(TEXT("intensity"), Intensity);
		++PropertiesSet;
	}

	// Color
	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (GetObjectParam(Params, TEXT("color"), ColorObj, /*bRequired=*/false))
	{
		double R = (*ColorObj)->GetNumberField(TEXT("r"));
		double G = (*ColorObj)->GetNumberField(TEXT("g"));
		double B = (*ColorObj)->GetNumberField(TEXT("b"));
		FLinearColor NewColor(
			static_cast<float>(R / 255.0),
			static_cast<float>(G / 255.0),
			static_cast<float>(B / 255.0),
			1.0f
		);
		LightComp->SetLightColor(NewColor);

		TSharedPtr<FJsonObject> ColorResult = MakeShared<FJsonObject>();
		ColorResult->SetNumberField(TEXT("r"), R);
		ColorResult->SetNumberField(TEXT("g"), G);
		ColorResult->SetNumberField(TEXT("b"), B);
		Result->SetObjectField(TEXT("color"), ColorResult);
		++PropertiesSet;
	}

	// Attenuation radius (only for local lights: point, spot)
	double AttenuationRadius;
	if (GetFloatParam(Params, TEXT("attenuation_radius"), AttenuationRadius, /*bRequired=*/false))
	{
		ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(LightComp);
		if (LocalLight)
		{
			LocalLight->SetAttenuationRadius(static_cast<float>(AttenuationRadius));
			Result->SetNumberField(TEXT("attenuation_radius"), AttenuationRadius);
			++PropertiesSet;
		}
		else
		{
			Result->SetStringField(TEXT("attenuation_radius_warning"),
				TEXT("Attenuation radius is not applicable to directional lights"));
		}
	}

	// Cast shadows
	bool bCastShadows;
	if (GetBoolParam(Params, TEXT("cast_shadows"), bCastShadows, /*bRequired=*/false))
	{
		LightComp->SetCastShadows(bCastShadows);
		Result->SetBoolField(TEXT("cast_shadows"), bCastShadows);
		++PropertiesSet;
	}

	// Temperature
	double Temperature;
	if (GetFloatParam(Params, TEXT("temperature"), Temperature, /*bRequired=*/false))
	{
		LightComp->SetTemperature(static_cast<float>(Temperature));
		Result->SetNumberField(TEXT("temperature"), Temperature);
		++PropertiesSet;
	}

	// Source radius (point/spot lights only)
	double SourceRadius;
	if (GetFloatParam(Params, TEXT("source_radius"), SourceRadius, /*bRequired=*/false))
	{
		UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
		if (PointLight)
		{
			PointLight->SetSourceRadius(static_cast<float>(SourceRadius));
			Result->SetNumberField(TEXT("source_radius"), SourceRadius);
			++PropertiesSet;
		}
		else
		{
			Result->SetStringField(TEXT("source_radius_warning"),
				TEXT("Source radius is only applicable to point and spot lights"));
		}
	}

	// Soft source radius (point/spot lights only)
	double SoftSourceRadius;
	if (GetFloatParam(Params, TEXT("soft_source_radius"), SoftSourceRadius, /*bRequired=*/false))
	{
		UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
		if (PointLight)
		{
			PointLight->SetSoftSourceRadius(static_cast<float>(SoftSourceRadius));
			Result->SetNumberField(TEXT("soft_source_radius"), SoftSourceRadius);
			++PropertiesSet;
		}
		else
		{
			Result->SetStringField(TEXT("soft_source_radius_warning"),
				TEXT("Soft source radius is only applicable to point and spot lights"));
		}
	}

	// Inner cone angle (spot lights only)
	double InnerConeAngle;
	if (GetFloatParam(Params, TEXT("inner_cone_angle"), InnerConeAngle, /*bRequired=*/false))
	{
		USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
		if (SpotLight)
		{
			SpotLight->SetInnerConeAngle(static_cast<float>(InnerConeAngle));
			Result->SetNumberField(TEXT("inner_cone_angle"), InnerConeAngle);
			++PropertiesSet;
		}
		else
		{
			Result->SetStringField(TEXT("inner_cone_angle_warning"),
				TEXT("Inner cone angle is only applicable to spot lights"));
		}
	}

	// Outer cone angle (spot lights only)
	double OuterConeAngle;
	if (GetFloatParam(Params, TEXT("outer_cone_angle"), OuterConeAngle, /*bRequired=*/false))
	{
		USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
		if (SpotLight)
		{
			SpotLight->SetOuterConeAngle(static_cast<float>(OuterConeAngle));
			Result->SetNumberField(TEXT("outer_cone_angle"), OuterConeAngle);
			++PropertiesSet;
		}
		else
		{
			Result->SetStringField(TEXT("outer_cone_angle_warning"),
				TEXT("Outer cone angle is only applicable to spot lights"));
		}
	}

	if (PropertiesSet == 0)
	{
		return FECACommandResult::Error(
			TEXT("No valid properties were provided. Specify at least one of: intensity, color, "
			     "attenuation_radius, cast_shadows, temperature, source_radius, soft_source_radius, "
			     "inner_cone_angle, outer_cone_angle"));
	}

	Result->SetNumberField(TEXT("properties_set"), PropertiesSet);

	// Refresh the viewport so the changes are visible immediately
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	return FECACommandResult::Success(Result);
}

// ─── get_light_properties ────────────────────────────────────

FECACommandResult FECACommand_GetLightProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));
	}

	ULightComponent* LightComp = LightingCommandHelpers::GetLightComponent(Actor);
	if (!LightComp)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no light component"), *ActorName));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);

	// Light type
	FString LightType = LightingCommandHelpers::GetLightTypeName(LightComp);
	Result->SetStringField(TEXT("light_type"), LightType);

	// Mobility
	Result->SetStringField(TEXT("mobility"), LightingCommandHelpers::MobilityToString(LightComp->Mobility));

	// Intensity
	Result->SetNumberField(TEXT("intensity"), LightComp->Intensity);

	// Color
	FLinearColor LightColor = LightComp->GetLightColor();
	TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
	ColorObj->SetNumberField(TEXT("r"), FMath::RoundToInt(LightColor.R * 255.0f));
	ColorObj->SetNumberField(TEXT("g"), FMath::RoundToInt(LightColor.G * 255.0f));
	ColorObj->SetNumberField(TEXT("b"), FMath::RoundToInt(LightColor.B * 255.0f));
	Result->SetObjectField(TEXT("color"), ColorObj);

	// Temperature
	Result->SetBoolField(TEXT("use_temperature"), LightComp->bUseTemperature);
	Result->SetNumberField(TEXT("temperature"), LightComp->Temperature);

	// Shadow settings
	Result->SetBoolField(TEXT("cast_shadows"), LightComp->CastShadows);

	// Attenuation radius (local lights only)
	ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(LightComp);
	if (LocalLight)
	{
		Result->SetNumberField(TEXT("attenuation_radius"), LocalLight->AttenuationRadius);
	}

	// Point/Spot-specific properties
	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
	if (PointLight)
	{
		Result->SetNumberField(TEXT("source_radius"), PointLight->SourceRadius);
		Result->SetNumberField(TEXT("soft_source_radius"), PointLight->SoftSourceRadius);
		Result->SetNumberField(TEXT("source_length"), PointLight->SourceLength);
	}

	// Spot-specific properties
	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	if (SpotLight)
	{
		Result->SetNumberField(TEXT("inner_cone_angle"), SpotLight->InnerConeAngle);
		Result->SetNumberField(TEXT("outer_cone_angle"), SpotLight->OuterConeAngle);
	}

	// Actor transform
	Result->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));

	return FECACommandResult::Success(Result);
}

// ─── create_light_rig ────────────────────────────────────────

FECACommandResult FECACommand_CreateLightRig::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Determine target location
	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;

	FString TargetName;
	GetStringParam(Params, TEXT("target_name"), TargetName, /*bRequired=*/false);

	// Try target_location first
	FVector ParamLocation;
	if (GetVectorParam(Params, TEXT("target_location"), ParamLocation, /*bRequired=*/false))
	{
		TargetLocation = ParamLocation;
		bHasTarget = true;
	}
	// Fall back to target_name
	else if (!TargetName.IsEmpty())
	{
		AActor* TargetActor = FindActorByName(TargetName);
		if (!TargetActor)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Target actor '%s' not found in the level"), *TargetName));
		}
		TargetLocation = TargetActor->GetActorLocation();
		bHasTarget = true;
	}

	if (!bHasTarget)
	{
		return FECACommandResult::Error(
			TEXT("Either 'target_location' or 'target_name' must be provided"));
	}

	// Get optional parameters with defaults
	double KeyIntensity = 5000.0;
	GetFloatParam(Params, TEXT("key_intensity"), KeyIntensity, /*bRequired=*/false);

	double FillRatio = 0.5;
	GetFloatParam(Params, TEXT("fill_ratio"), FillRatio, /*bRequired=*/false);

	double RimIntensity = 3000.0;
	GetFloatParam(Params, TEXT("rim_intensity"), RimIntensity, /*bRequired=*/false);

	double Radius = 300.0;
	GetFloatParam(Params, TEXT("radius"), Radius, /*bRequired=*/false);

	double FillIntensity = KeyIntensity * FillRatio;

	// Calculate light positions relative to target
	// Key light: front-left, elevated 45 degrees
	// Using a coordinate system where X is forward, Y is right, Z is up
	FVector KeyOffset(
		-Radius * 0.707,   // Forward-left X component
		-Radius * 0.707,   // Forward-left Y component
		Radius * 0.707     // Elevated ~45 degrees
	);

	// Fill light: front-right, slightly elevated, softer
	FVector FillOffset(
		-Radius * 0.707,   // Forward-right X component
		Radius * 0.707,    // Forward-right Y component
		Radius * 0.5       // Slightly lower than key
	);

	// Rim/back light: behind and above the target
	FVector RimOffset(
		Radius * 0.866,    // Behind the target
		0.0,               // Centered
		Radius * 0.5       // Elevated
	);

	FVector KeyPosition = TargetLocation + KeyOffset;
	FVector FillPosition = TargetLocation + FillOffset;
	FVector RimPosition = TargetLocation + RimOffset;

	// Calculate rotations to face the target
	FRotator KeyRotation = (TargetLocation - KeyPosition).Rotation();
	FRotator FillRotation = (TargetLocation - FillPosition).Rotation();
	FRotator RimRotation = (TargetLocation - RimPosition).Rotation();

	// Spawn the three lights
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Key light - warm point light
	APointLight* KeyLight = World->SpawnActor<APointLight>(KeyPosition, KeyRotation, SpawnParams);
	if (!KeyLight)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn key light"));
	}
	KeyLight->SetActorLabel(TEXT("LightRig_Key"));
	UPointLightComponent* KeyComp = Cast<UPointLightComponent>(KeyLight->GetLightComponent());
	if (KeyComp)
	{
		KeyComp->SetIntensity(static_cast<float>(KeyIntensity));
		KeyComp->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f)); // Warm white
		KeyComp->SetAttenuationRadius(static_cast<float>(Radius * 5.0));
		KeyComp->SetCastShadows(true);
		KeyComp->SetSourceRadius(20.0f);
	}

	// Fill light - cool, softer point light
	APointLight* FillLight = World->SpawnActor<APointLight>(FillPosition, FillRotation, SpawnParams);
	if (!FillLight)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn fill light"));
	}
	FillLight->SetActorLabel(TEXT("LightRig_Fill"));
	UPointLightComponent* FillComp = Cast<UPointLightComponent>(FillLight->GetLightComponent());
	if (FillComp)
	{
		FillComp->SetIntensity(static_cast<float>(FillIntensity));
		FillComp->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f)); // Cool white
		FillComp->SetAttenuationRadius(static_cast<float>(Radius * 5.0));
		FillComp->SetCastShadows(false); // Fill usually doesn't cast shadows
		FillComp->SetSourceRadius(40.0f); // Larger source = softer light
	}

	// Rim/back light - strong point light behind
	APointLight* RimLight = World->SpawnActor<APointLight>(RimPosition, RimRotation, SpawnParams);
	if (!RimLight)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn rim light"));
	}
	RimLight->SetActorLabel(TEXT("LightRig_Rim"));
	UPointLightComponent* RimComp = Cast<UPointLightComponent>(RimLight->GetLightComponent());
	if (RimComp)
	{
		RimComp->SetIntensity(static_cast<float>(RimIntensity));
		RimComp->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f)); // Neutral white
		RimComp->SetAttenuationRadius(static_cast<float>(Radius * 5.0));
		RimComp->SetCastShadows(true);
		RimComp->SetSourceRadius(10.0f);
	}

	// Notify editor of new actors
	if (GEditor)
	{
		GEditor->BroadcastLevelActorListChanged();
		GEditor->RedrawAllViewports();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetObjectField(TEXT("target_location"), VectorToJson(TargetLocation));
	if (!TargetName.IsEmpty())
	{
		Result->SetStringField(TEXT("target_name"), TargetName);
	}
	Result->SetNumberField(TEXT("radius"), Radius);

	// Key light info
	TSharedPtr<FJsonObject> KeyInfo = MakeShared<FJsonObject>();
	KeyInfo->SetStringField(TEXT("name"), TEXT("LightRig_Key"));
	KeyInfo->SetNumberField(TEXT("intensity"), KeyIntensity);
	KeyInfo->SetObjectField(TEXT("location"), VectorToJson(KeyPosition));
	KeyInfo->SetStringField(TEXT("color_description"), TEXT("warm white"));
	Result->SetObjectField(TEXT("key_light"), KeyInfo);

	// Fill light info
	TSharedPtr<FJsonObject> FillInfo = MakeShared<FJsonObject>();
	FillInfo->SetStringField(TEXT("name"), TEXT("LightRig_Fill"));
	FillInfo->SetNumberField(TEXT("intensity"), FillIntensity);
	FillInfo->SetObjectField(TEXT("location"), VectorToJson(FillPosition));
	FillInfo->SetStringField(TEXT("color_description"), TEXT("cool white"));
	Result->SetObjectField(TEXT("fill_light"), FillInfo);

	// Rim light info
	TSharedPtr<FJsonObject> RimInfo = MakeShared<FJsonObject>();
	RimInfo->SetStringField(TEXT("name"), TEXT("LightRig_Rim"));
	RimInfo->SetNumberField(TEXT("intensity"), RimIntensity);
	RimInfo->SetObjectField(TEXT("location"), VectorToJson(RimPosition));
	RimInfo->SetStringField(TEXT("color_description"), TEXT("neutral white"));
	Result->SetObjectField(TEXT("rim_light"), RimInfo);

	return FECACommandResult::Success(Result);
}

// ─── set_post_process_settings ───────────────────────────────

FECACommandResult FECACommand_SetPostProcessSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Find the post-process volume
	APostProcessVolume* PPV = nullptr;

	FString ActorName;
	if (GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false) && !ActorName.IsEmpty())
	{
		AActor* Actor = FindActorByName(ActorName);
		if (!Actor)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Actor '%s' not found in the level"), *ActorName));
		}
		PPV = Cast<APostProcessVolume>(Actor);
		if (!PPV)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Actor '%s' is not a PostProcessVolume"), *ActorName));
		}
	}
	else
	{
		PPV = LightingCommandHelpers::FindFirstPostProcessVolume(World);
		if (!PPV)
		{
			return FECACommandResult::Error(
				TEXT("No PostProcessVolume found in the level. Specify an actor_name or create a PostProcessVolume first."));
		}
	}

	FPostProcessSettings& Settings = PPV->Settings;
	int32 PropertiesSet = 0;
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());

	// Bloom intensity
	double BloomIntensity;
	if (GetFloatParam(Params, TEXT("bloom_intensity"), BloomIntensity, /*bRequired=*/false))
	{
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = static_cast<float>(BloomIntensity);
		Result->SetNumberField(TEXT("bloom_intensity"), BloomIntensity);
		++PropertiesSet;
	}

	// Exposure compensation (maps to AutoExposureBias)
	double ExposureCompensation;
	if (GetFloatParam(Params, TEXT("exposure_compensation"), ExposureCompensation, /*bRequired=*/false))
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = static_cast<float>(ExposureCompensation);
		Result->SetNumberField(TEXT("exposure_compensation"), ExposureCompensation);
		++PropertiesSet;
	}

	// Ambient occlusion intensity
	double AOIntensity;
	if (GetFloatParam(Params, TEXT("ambient_occlusion_intensity"), AOIntensity, /*bRequired=*/false))
	{
		Settings.bOverride_AmbientOcclusionIntensity = true;
		Settings.AmbientOcclusionIntensity = static_cast<float>(AOIntensity);
		Result->SetNumberField(TEXT("ambient_occlusion_intensity"), AOIntensity);
		++PropertiesSet;
	}

	// Auto exposure toggle
	bool bAutoExposure;
	if (GetBoolParam(Params, TEXT("auto_exposure"), bAutoExposure, /*bRequired=*/false))
	{
		Settings.bOverride_AutoExposureMethod = true;
		if (bAutoExposure)
		{
			Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		}
		else
		{
			Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		}
		Result->SetBoolField(TEXT("auto_exposure"), bAutoExposure);
		++PropertiesSet;
	}

	// Vignette intensity
	double VignetteIntensity;
	if (GetFloatParam(Params, TEXT("vignette_intensity"), VignetteIntensity, /*bRequired=*/false))
	{
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = static_cast<float>(VignetteIntensity);
		Result->SetNumberField(TEXT("vignette_intensity"), VignetteIntensity);
		++PropertiesSet;
	}

	// Color grading LUT
	FString LUTPath;
	if (GetStringParam(Params, TEXT("color_grading_lut"), LUTPath, /*bRequired=*/false) && !LUTPath.IsEmpty())
	{
		UTexture* LUTTexture = LoadObject<UTexture>(nullptr, *LUTPath);
		if (!LUTTexture)
		{
			// Try with sub-object syntax
			FString FullPath = LUTPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			LUTTexture = LoadObject<UTexture>(nullptr, *FullPath);
		}

		if (!LUTTexture)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Could not load color grading LUT texture at: %s"), *LUTPath));
		}

		Settings.bOverride_ColorGradingLUT = true;
		Settings.ColorGradingLUT = LUTTexture;
		Result->SetStringField(TEXT("color_grading_lut"), LUTTexture->GetPathName());
		++PropertiesSet;
	}

	if (PropertiesSet == 0)
	{
		return FECACommandResult::Error(
			TEXT("No valid properties were provided. Specify at least one of: bloom_intensity, "
			     "exposure_compensation, ambient_occlusion_intensity, auto_exposure, "
			     "vignette_intensity, color_grading_lut"));
	}

	Result->SetNumberField(TEXT("properties_set"), PropertiesSet);

	// Mark the PPV as needing an update
	PPV->MarkPackageDirty();

	// Refresh viewports
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	return FECACommandResult::Success(Result);
}
