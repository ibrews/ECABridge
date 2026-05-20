// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAXRMutationCommands.h"

#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "StereoRendering.h"

namespace
{
	void FillXRState(TSharedPtr<FJsonObject> Out)
	{
		IHeadMountedDisplay* HMD = nullptr;
		if (GEngine && GEngine->XRSystem.IsValid())
		{
			IXRTrackingSystem* XR = GEngine->XRSystem.Get();
			HMD = XR->GetHMDDevice();
			Out->SetBoolField(TEXT("xr_active"), true);
			Out->SetStringField(TEXT("system_name"), XR->GetSystemName().ToString());
			Out->SetStringField(TEXT("version"), XR->GetVersionString());
			Out->SetBoolField(TEXT("head_tracking_allowed"), XR->IsHeadTrackingAllowed());
		}
		else
		{
			Out->SetBoolField(TEXT("xr_active"), false);
		}

		if (HMD)
		{
			Out->SetBoolField(TEXT("hmd_connected"), HMD->IsHMDConnected());
			Out->SetBoolField(TEXT("hmd_enabled"), HMD->IsHMDEnabled());
		}
		else
		{
			Out->SetBoolField(TEXT("hmd_connected"), false);
			Out->SetBoolField(TEXT("hmd_enabled"), false);
		}

		if (GEngine && GEngine->StereoRenderingDevice.IsValid())
		{
			Out->SetBoolField(TEXT("stereo_enabled"), GEngine->StereoRenderingDevice->IsStereoEnabled());
		}
		else
		{
			Out->SetBoolField(TEXT("stereo_enabled"), false);
		}
	}
}

FECACommandResult FECACommand_StartXRSession::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		Result->SetBoolField(TEXT("xr_active"), false);
		Result->SetStringField(TEXT("message"), TEXT("No XR system is currently bound to GEngine. Enable an XR plugin (OpenXR / SteamVR / Oculus) and ensure a runtime is responding before calling start_xr_session."));
		return FECACommandResult::Success(Result);
	}

	bool bEnableStereo = true;
	GetBoolParam(Params, TEXT("enable_stereo"), bEnableStereo, false);

	if (IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice())
	{
		HMD->EnableHMD(true);
	}

	if (bEnableStereo && GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->EnableStereo(true);
	}

	FillXRState(Result);
	Result->SetStringField(TEXT("action"), TEXT("started"));
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_StartXRSession);

FECACommandResult FECACommand_StopXRSession::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		Result->SetBoolField(TEXT("xr_active"), false);
		Result->SetStringField(TEXT("message"), TEXT("No XR system is currently bound to GEngine; nothing to stop."));
		return FECACommandResult::Success(Result);
	}

	if (GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled())
	{
		GEngine->StereoRenderingDevice->EnableStereo(false);
	}

	if (IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice())
	{
		HMD->EnableHMD(false);
	}

	FillXRState(Result);
	Result->SetStringField(TEXT("action"), TEXT("stopped"));
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_StopXRSession);
