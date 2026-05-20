// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_EnableStatGroup : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("enable_stat_group"); }
	virtual FString GetDescription() const override { return TEXT("Enable a stat group on the active viewport (toggles 'stat <name>'). Examples: unit, fps, gpu, scenerendering, memory."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return { { TEXT("name"), TEXT("string"), TEXT("Stat group name (without 'stat ' prefix)"), true } };
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DisableStatGroup : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("disable_stat_group"); }
	virtual FString GetDescription() const override { return TEXT("Disable a stat group. If name is 'all' or 'none', clears all displayed stats."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return { { TEXT("name"), TEXT("string"), TEXT("Stat group name, or 'all'/'none' to clear all"), true } };
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DumpStatValues : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_stat_values"); }
	virtual FString GetDescription() const override { return TEXT("Report the current frame-time / FPS averages plus engine memory headline numbers. Detailed per-group stat values must be read from the editor log after enabling the group."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual bool IsMutating() const override { return false; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override;
};
