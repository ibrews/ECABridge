// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * list_dmx_libraries — enumerate every UDMXLibrary asset known to the AssetRegistry.
 *
 * Returns the package path, asset name, and patch count for each library. Patch
 * count is read by loading the asset (so the registry doesn't need to have it
 * cached), but soft references aren't followed beyond that.
 *
 * Gated by WITH_ECA_DMX so projects without the DMX plugins build cleanly.
 */
class FECACommand_ListDmxLibraries : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_dmx_libraries"); }
	virtual FString GetDescription() const override { return TEXT("List every UDMXLibrary asset in the project. Returns package_path, name, and patch_count for each library."); }
	virtual FString GetCategory() const override { return TEXT("DMX"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * dump_dmx_patch — for a given UDMXLibrary asset, return every fixture patch
 * with its universe id, starting channel, fixture type name, and active mode.
 *
 * Caller passes library_path (e.g. "/Game/Lighting/MyLibrary"). The command
 * loads the asset synchronously, walks its UDMXEntityFixturePatch entities,
 * and serializes the patch table. Useful for confirming a DMX patching state
 * before driving an nDisplay stage shoot.
 */
class FECACommand_DumpDmxPatch : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_dmx_patch"); }
	virtual FString GetDescription() const override { return TEXT("Dump the fixture-patch table for a UDMXLibrary asset. Pass library_path (e.g. /Game/Lighting/MyLib). Returns an array of patches with universe_id, starting_channel, fixture_type, mode, and entity_id."); }
	virtual FString GetCategory() const override { return TEXT("DMX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("library_path"), TEXT("string"), TEXT("Asset path to a UDMXLibrary (e.g. /Game/Lighting/MyLib)"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
