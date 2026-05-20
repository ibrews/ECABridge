// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * create_dmx_library — create a new UDMXLibrary asset on disk.
 *
 * Caller passes package_path (e.g. /Game/Lighting/MyLib). The command creates
 * the package, instantiates a fresh UDMXLibrary inside it, notifies the asset
 * registry, and saves the package to .uasset. Returns the resulting asset path.
 *
 * Gated by WITH_ECA_DMX.
 */
class FECACommand_CreateDmxLibrary : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_dmx_library"); }
	virtual FString GetDescription() const override { return TEXT("Create a new UDMXLibrary asset at the given package path (e.g. /Game/Lighting/MyLib). Returns the asset path. Errors if the package already exists unless overwrite=true."); }
	virtual FString GetCategory() const override { return TEXT("DMX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("package_path"), TEXT("string"), TEXT("Full asset package path, e.g. /Game/Lighting/MyLib."), true, TEXT("") },
			{ TEXT("overwrite"), TEXT("boolean"), TEXT("If true, overwrites an existing asset at the same path. Default false."), false, TEXT("false") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * add_dmx_fixture — add a UDMXEntityFixturePatch to an existing UDMXLibrary.
 *
 * Caller passes library_path plus optional universe_id, starting_channel,
 * active_mode, fixture_name, and an optional fixture_type_path. When no
 * fixture_type_path is supplied a minimal default UDMXEntityFixtureType is
 * created inside the same library. The library is saved when bSaveLibrary
 * (default true) is set.
 */
class FECACommand_AddDmxFixture : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_dmx_fixture"); }
	virtual FString GetDescription() const override { return TEXT("Add a UDMXEntityFixturePatch to a UDMXLibrary. Provide library_path, optional universe_id (default 1), starting_channel (default 1), active_mode (default 0), fixture_name, and fixture_type_path (creates a default fixture type if absent). Saves the library when save=true."); }
	virtual FString GetCategory() const override { return TEXT("DMX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("library_path"), TEXT("string"), TEXT("Asset path to an existing UDMXLibrary."), true, TEXT("") },
			{ TEXT("universe_id"), TEXT("integer"), TEXT("Local DMX universe ID for the patch (default 1)."), false, TEXT("1") },
			{ TEXT("starting_channel"), TEXT("integer"), TEXT("Starting channel within the universe (1-512, default 1)."), false, TEXT("1") },
			{ TEXT("active_mode"), TEXT("integer"), TEXT("Index of the active mode on the fixture type (default 0)."), false, TEXT("0") },
			{ TEXT("fixture_name"), TEXT("string"), TEXT("Optional display name for the new fixture patch."), false, TEXT("") },
			{ TEXT("fixture_type_path"), TEXT("string"), TEXT("Optional asset path to an existing UDMXEntityFixtureType. If empty, a minimal default fixture type is created inline in the library."), false, TEXT("") },
			{ TEXT("save"), TEXT("boolean"), TEXT("Whether to save the library after adding the patch (default true)."), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
