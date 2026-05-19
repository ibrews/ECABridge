// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// set_sun_position ─ rotate a DirectionalLight by elevation/azimuth (degrees)
class FECACommand_SetSunPosition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sun_position"); }
	virtual FString GetDescription() const override { return TEXT("Set sun position by rotating a Directional Light to a given elevation/azimuth in degrees"); }
	virtual FString GetCategory() const override { return TEXT("Sky"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("elevation_deg"), TEXT("number"), TEXT("Sun elevation above horizon (-90..90)"), true },
			{ TEXT("azimuth_deg"), TEXT("number"), TEXT("Sun azimuth from north, clockwise (0..360)"), true },
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the Directional Light actor. Defaults to the first DirectionalLight in the level."), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// set_sky_atmosphere_settings ─ tune SkyAtmosphere actor's component fields
class FECACommand_SetSkyAtmosphereSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_sky_atmosphere_settings"); }
	virtual FString GetDescription() const override { return TEXT("Tune a SkyAtmosphere actor (Rayleigh, Mie, multi-scatter, ground)"); }
	virtual FString GetCategory() const override { return TEXT("Sky"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the SkyAtmosphere actor. Defaults to first in level."), false },
			{ TEXT("rayleigh_scattering_scale"), TEXT("number"), TEXT("Rayleigh scattering scale"), false },
			{ TEXT("mie_scattering_scale"), TEXT("number"), TEXT("Mie scattering scale"), false },
			{ TEXT("mie_absorption_scale"), TEXT("number"), TEXT("Mie absorption scale"), false },
			{ TEXT("mie_anisotropy"), TEXT("number"), TEXT("Mie anisotropy (-1..1)"), false },
			{ TEXT("multi_scattering_factor"), TEXT("number"), TEXT("Multi-scattering factor"), false },
			{ TEXT("atmosphere_height_km"), TEXT("number"), TEXT("Atmosphere height in km"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// set_volumetric_cloud_settings ─ tune VolumetricCloud component
class FECACommand_SetVolumetricCloudSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_volumetric_cloud_settings"); }
	virtual FString GetDescription() const override { return TEXT("Tune a VolumetricCloud actor's settings (altitude, height, tracing distances)"); }
	virtual FString GetCategory() const override { return TEXT("Sky"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the VolumetricCloud actor. Defaults to first in level."), false },
			{ TEXT("layer_bottom_altitude_km"), TEXT("number"), TEXT("Layer bottom altitude (km)"), false },
			{ TEXT("layer_height_km"), TEXT("number"), TEXT("Layer height (km)"), false },
			{ TEXT("tracing_start_max_distance_km"), TEXT("number"), TEXT("Tracing start max distance (km)"), false },
			{ TEXT("tracing_max_distance_km"), TEXT("number"), TEXT("Tracing max distance (km)"), false },
			{ TEXT("view_sample_count_scale"), TEXT("number"), TEXT("View sample count scale"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
