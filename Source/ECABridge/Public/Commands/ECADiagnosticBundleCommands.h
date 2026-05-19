// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_CaptureDiagnosticBundle : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("capture_diagnostic_bundle"); }
	virtual FString GetDescription() const override { return TEXT("Capture an everything-bundle for a perf or support report: a screenshot, the recent editor log tail, the current CVar list, scene stats, and top-N actors-by-cost. Writes to Saved/Profiling/Bundles/<label>/ and returns the manifest."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("label"), TEXT("string"), TEXT("Bundle label used as the output subdirectory"), true },
			{ TEXT("log_tail_lines"), TEXT("number"), TEXT("How many trailing lines of the editor log to include"), false, TEXT("500") },
			{ TEXT("top_n_actors"), TEXT("number"), TEXT("How many actors-by-component-count to include"), false, TEXT("20") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
