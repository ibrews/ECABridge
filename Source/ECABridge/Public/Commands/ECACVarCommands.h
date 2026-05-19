// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/** Read a single console variable's current value, type, default, and help text. */
class FECACommand_GetCVar : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_cvar"); }
	virtual FString GetDescription() const override { return TEXT("Get the current value of a console variable (CVar) by name. Returns value, type, default, and help text."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"), TEXT("string"), TEXT("Name of the console variable (e.g., r.ScreenPercentage)"), true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Set a console variable's value at runtime. */
class FECACommand_SetCVar : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_cvar"); }
	virtual FString GetDescription() const override { return TEXT("Set a console variable (CVar) to a new value. Accepts string, number, or boolean values."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name"),  TEXT("string"), TEXT("Name of the console variable (e.g., r.ScreenPercentage)"), true },
			{ TEXT("value"), TEXT("string"), TEXT("New value (string, number, or boolean accepted)"),         true }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/** Enumerate registered CVars, optionally filtered by substring. */
class FECACommand_ListCVars : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_cvars"); }
	virtual FString GetDescription() const override { return TEXT("List registered console variables. Optional substring filter (e.g., 'r.Shadow') applied to the name."); }
	virtual FString GetCategory() const override { return TEXT("EditorUX"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("filter"),    TEXT("string"),  TEXT("Substring filter applied to CVar names (case-insensitive). Empty = no filter."), false },
			{ TEXT("max_count"), TEXT("number"),  TEXT("Maximum number of CVars to return (default 500)"), false, TEXT("500") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
