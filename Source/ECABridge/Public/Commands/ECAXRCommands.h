// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * get_xr_runtime_info — return information about the active XR runtime / HMD.
 *
 * Reports whether GEngine->XRSystem is non-null (i.e. an XR plugin is loaded
 * and a runtime is responding), the system name (e.g. "OpenXR", "OculusHMD"),
 * the runtime version string, supported-feature flags, and head-tracking
 * permission. If no XR system is bound the response sets xr_active=false with
 * an explanation — never errors so callers can probe-then-branch cleanly.
 */
class FECACommand_GetXRRuntimeInfo : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_xr_runtime_info"); }
	virtual FString GetDescription() const override { return TEXT("Get information about the active XR / OpenXR runtime: system name, version, positional-tracking support, and head-tracking permission. Returns xr_active=false with a message if no XR system is currently bound."); }
	virtual FString GetCategory() const override { return TEXT("XR"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * dump_xr_input_state — enumerate currently tracked XR devices (HMD, controllers,
 * trackers) and report each device's tracking status and current pose.
 *
 * Useful for verifying that hand controllers are tracked before recording an
 * XR-driven sequence, or for debugging which Vive trackers are bound to which
 * device IDs. Returns an empty devices array when no XR system is active.
 */
class FECACommand_DumpXRInputState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_xr_input_state"); }
	virtual FString GetDescription() const override { return TEXT("Dump the current tracking state of every XR device known to the active runtime — HMD, controllers, trackers, sensors. Each entry includes device id, tracking flag, and (if tracked) current orientation + position."); }
	virtual FString GetCategory() const override { return TEXT("XR"); }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
