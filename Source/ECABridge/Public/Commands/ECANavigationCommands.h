// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// ─── build_navigation ─────────────────────────────────────────
// Build (or rebuild) the NavMesh for the current level.
class FECACommand_BuildNavigation : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("build_navigation"); }
	virtual FString GetDescription() const override { return TEXT("Build or rebuild the navigation mesh for the current level"); }
	virtual FString GetCategory() const override { return TEXT("Navigation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("agent_radius"), TEXT("number"), TEXT("Navigation agent radius in cm (uses project default if omitted)"), false },
			{ TEXT("agent_height"), TEXT("number"), TEXT("Navigation agent height in cm (uses project default if omitted)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── find_path ────────────────────────────────────────────────
// Find a navigation path between two world-space points.
class FECACommand_FindPath : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("find_path"); }
	virtual FString GetDescription() const override { return TEXT("Find a navigation path between two points, returning the path points or an error if no path exists"); }
	virtual FString GetCategory() const override { return TEXT("Navigation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("start"), TEXT("object"), TEXT("Start location as {x, y, z}"), true },
			{ TEXT("end"), TEXT("object"), TEXT("End/goal location as {x, y, z}"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── move_actor_to ────────────────────────────────────────────
// Move an actor along a nav path to a destination using simple AI movement.
class FECACommand_MoveActorTo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("move_actor_to"); }
	virtual FString GetDescription() const override { return TEXT("Move an actor along a navigation path to a destination using simple AI movement"); }
	virtual FString GetCategory() const override { return TEXT("Navigation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name/label of the actor to move"), true },
			{ TEXT("destination"), TEXT("object"), TEXT("Target location as {x, y, z}"), true },
			{ TEXT("speed"), TEXT("number"), TEXT("Movement speed in cm/s (default: 300)"), false, TEXT("300") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// ─── get_nav_mesh_info ────────────────────────────────────────
// Get information about the navigation mesh: bounds, build status, area count.
class FECACommand_GetNavMeshInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_nav_mesh_info"); }
	virtual FString GetDescription() const override { return TEXT("Get information about the navigation mesh: bounds, build status, and nav data details"); }
	virtual FString GetCategory() const override { return TEXT("Navigation"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
