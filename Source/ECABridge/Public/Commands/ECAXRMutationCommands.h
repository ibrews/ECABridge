// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * start_xr_session — enable the bound XR head-mounted display + stereo rendering.
 *
 * Wraps UHeadMountedDisplayFunctionLibrary::EnableHMD(true) and EnableStereo(true),
 * then reports the resulting connection / enable / stereo state plus the system name.
 * Idempotent: calling when the HMD is already enabled is a no-op. Returns
 * xr_active=false with a message if no XR system is currently bound.
 */
class FECACommand_StartXRSession : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("start_xr_session"); }
	virtual FString GetDescription() const override { return TEXT("Enable the bound XR HMD and stereo rendering (UHeadMountedDisplayFunctionLibrary::EnableHMD + EnableStereo). Idempotent. Returns the resulting connected/enabled/stereo state and the XR system name."); }
	virtual FString GetCategory() const override { return TEXT("XR"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("enable_stereo"), TEXT("boolean"), TEXT("Whether to also enable stereo rendering after enabling the HMD (default: true)."), false, TEXT("true") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * stop_xr_session — disable the bound XR HMD + stereo rendering.
 *
 * Wraps UHeadMountedDisplayFunctionLibrary::EnableHMD(false) (stereo is implicitly
 * disabled when the HMD is disabled). Idempotent. Returns xr_active=false with a
 * message if no XR system is currently bound.
 */
class FECACommand_StopXRSession : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("stop_xr_session"); }
	virtual FString GetDescription() const override { return TEXT("Disable the bound XR HMD (UHeadMountedDisplayFunctionLibrary::EnableHMD(false)) — stereo rendering is implicitly stopped. Idempotent."); }
	virtual FString GetCategory() const override { return TEXT("XR"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
