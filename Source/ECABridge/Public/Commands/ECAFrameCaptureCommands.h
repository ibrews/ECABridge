// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FECACommand_CaptureFrame : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("capture_frame"); }
	virtual FString GetDescription() const override { return TEXT("Capture a single frame for profiling. method=renderdoc invokes RenderDoc CaptureFrame (requires the RenderDoc plugin enabled); method=screenshot or default writes a high-resolution PNG to Saved/Profiling/Captures/."); }
	virtual FString GetCategory() const override { return TEXT("Observability"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("method"), TEXT("string"), TEXT("Capture method: 'renderdoc' or 'screenshot' (default)"), false, TEXT("screenshot") },
			{ TEXT("label"), TEXT("string"), TEXT("Optional label appended to the artifact filename"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
