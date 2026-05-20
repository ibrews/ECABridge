// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_StartInsightsTrace : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("start_insights_trace"); }
	virtual FString GetDescription() const override { return TEXT("Start an Unreal Insights trace to a .utrace file. Channels default to cpu,frame,bookmark."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("file"), TEXT("string"), TEXT("Output .utrace path. Defaults to Project/Saved/Profiling/Traces/<timestamp>.utrace"), false },
			{ TEXT("channels"), TEXT("string"), TEXT("Comma-separated channel list (cpu,gpu,frame,bookmark,log,...)"), false, TEXT("cpu,frame,bookmark") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_StopInsightsTrace : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_insights_trace"); }
	virtual FString GetDescription() const override { return TEXT("Stop the current Unreal Insights trace. Returns the most recently started trace file path if known."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DumpInsightsSummary : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_insights_summary"); }
	virtual FString GetDescription() const override { return TEXT("Summarize an Unreal Insights .utrace file (size, mtime, channels). Returns a small JSON description, not the full trace contents."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual bool IsMutating() const override { return false; }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("trace_file"), TEXT("string"), TEXT("Path to a .utrace file. Omit to summarize the most recently started trace."), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
