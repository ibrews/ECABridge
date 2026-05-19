// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/** Save a named profile of CVar name/value pairs to Saved/CVarProfiles/<name>.json. */
class FECACommand_SaveCVarProfile : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("save_cvar_profile"); }
	virtual FString GetDescription() const override { return TEXT("Save the current values of a CVar list to a named profile under Saved/CVarProfiles/<name>.json."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"),      TEXT("string"), TEXT("Profile name (becomes the json filename)"), true },
			{ TEXT("cvar_list"), TEXT("array"),  TEXT("Array of CVar names whose current values to snapshot"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Load and apply a previously-saved CVar profile. */
class FECACommand_LoadCVarProfile : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("load_cvar_profile"); }
	virtual FString GetDescription() const override { return TEXT("Load a profile from Saved/CVarProfiles/<name>.json and apply the stored CVar values."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Profile name to load"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Enumerate available CVar profiles in Saved/CVarProfiles/. */
class FECACommand_ListCVarProfiles : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_cvar_profiles"); }
	virtual FString GetDescription() const override { return TEXT("List the CVar profiles available under Saved/CVarProfiles/."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
