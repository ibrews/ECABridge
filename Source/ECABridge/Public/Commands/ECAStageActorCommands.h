// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * spawn_light_card — drop an ADisplayClusterLightCardActor into the active
 * editor world at a given location. Light cards are the primary ICVFX
 * "virtual practical" — flat-card emissive proxies on the LED volume used
 * to stand in for off-camera light sources.
 *
 * The command spawns a stock light card with default mesh, optionally parents
 * it under a target ADisplayClusterRootActor if `attach_to_root` is provided,
 * and returns the new actor's path + name. Color, mask, and gradient tuning
 * is left to follow-up commands (search "spawn_light_card" in the docs for the
 * full lifecycle).
 *
 * Gated by WITH_ECA_NDISPLAY — the light-card class lives in DisplayCluster.
 */
class FECACommand_SpawnLightCard : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_light_card"); }
	virtual FString GetDescription() const override { return TEXT("Spawn an ADisplayClusterLightCardActor at the given world location. Pass location (object {x,y,z}, required). Optional rotation (object {pitch,yaw,roll}, default zero), label (default 'LightCard'), and attach_to_root_actor (actor path string to parent the card under). Returns actor_path of the spawned card."); }
	virtual FString GetCategory() const override { return TEXT("NDisplay"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("location"), TEXT("object"), TEXT("World-space location {x,y,z}"), true, TEXT("{\"x\":0,\"y\":0,\"z\":0}") },
			{ TEXT("rotation"), TEXT("object"), TEXT("Rotation {pitch,yaw,roll} (default zero)"), false, TEXT("{\"pitch\":0,\"yaw\":0,\"roll\":0}") },
			{ TEXT("label"), TEXT("string"), TEXT("Label for the spawned actor (default 'LightCard')"), false, TEXT("LightCard") },
			{ TEXT("attach_to_root_actor"), TEXT("string"), TEXT("Optional path to an ADisplayClusterRootActor to parent under"), false, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
