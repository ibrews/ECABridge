// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAXRCommands.h"

#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

namespace
{
	const TCHAR* TrackedDeviceTypeToString(EXRTrackedDeviceType Type)
	{
		switch (Type)
		{
			case EXRTrackedDeviceType::HeadMountedDisplay: return TEXT("hmd");
			case EXRTrackedDeviceType::Controller:         return TEXT("controller");
			case EXRTrackedDeviceType::TrackingReference:  return TEXT("tracking_reference");
			case EXRTrackedDeviceType::Other:              return TEXT("other");
			case EXRTrackedDeviceType::Any:                return TEXT("any");
			case EXRTrackedDeviceType::Invalid:            default: return TEXT("invalid");
		}
	}

	void AddVectorFields(TSharedPtr<FJsonObject> Obj, const TCHAR* Key, const FVector& V)
	{
		TSharedPtr<FJsonObject> Inner = MakeShared<FJsonObject>();
		Inner->SetNumberField(TEXT("x"), V.X);
		Inner->SetNumberField(TEXT("y"), V.Y);
		Inner->SetNumberField(TEXT("z"), V.Z);
		Obj->SetObjectField(Key, Inner);
	}

	void AddQuatFields(TSharedPtr<FJsonObject> Obj, const TCHAR* Key, const FQuat& Q)
	{
		TSharedPtr<FJsonObject> Inner = MakeShared<FJsonObject>();
		Inner->SetNumberField(TEXT("x"), Q.X);
		Inner->SetNumberField(TEXT("y"), Q.Y);
		Inner->SetNumberField(TEXT("z"), Q.Z);
		Inner->SetNumberField(TEXT("w"), Q.W);
		Obj->SetObjectField(Key, Inner);
	}
}

FECACommandResult FECACommand_GetXRRuntimeInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		Result->SetBoolField(TEXT("xr_active"), false);
		Result->SetStringField(TEXT("message"), TEXT("No XR system is currently bound to GEngine. Either no XR plugin is loaded (check OpenXR/SteamVR/Oculus plugin enablement) or no headset is connected."));
		return FECACommandResult::Success(Result);
	}

	IXRTrackingSystem* XR = GEngine->XRSystem.Get();
	Result->SetBoolField(TEXT("xr_active"), true);
	Result->SetStringField(TEXT("system_name"), XR->GetSystemName().ToString());
	Result->SetStringField(TEXT("version"), XR->GetVersionString());
	Result->SetNumberField(TEXT("flags"), XR->GetXRSystemFlags());
	Result->SetBoolField(TEXT("supports_positional_tracking"), XR->DoesSupportPositionalTracking());
	Result->SetBoolField(TEXT("has_valid_tracking_position"), XR->HasValidTrackingPosition());
	Result->SetBoolField(TEXT("head_tracking_allowed"), XR->IsHeadTrackingAllowed());

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_GetXRRuntimeInfo);

FECACommandResult FECACommand_DumpXRInputState::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		Result->SetBoolField(TEXT("xr_active"), false);
		Result->SetStringField(TEXT("message"), TEXT("No XR system is currently bound to GEngine."));
		Result->SetArrayField(TEXT("devices"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	IXRTrackingSystem* XR = GEngine->XRSystem.Get();

	TArray<int32> DeviceIds;
	XR->EnumerateTrackedDevices(DeviceIds, EXRTrackedDeviceType::Any);

	TArray<TSharedPtr<FJsonValue>> Devices;
	Devices.Reserve(DeviceIds.Num());

	for (int32 Id : DeviceIds)
	{
		TSharedPtr<FJsonObject> D = MakeShared<FJsonObject>();
		D->SetNumberField(TEXT("device_id"), Id);

		// Probe per-type by membership test; the tracking system has no public
		// "what type is device N" call, so we enumerate per-type and tag.
		FString TypeStr = TEXT("unknown");
		const EXRTrackedDeviceType Types[] = {
			EXRTrackedDeviceType::HeadMountedDisplay,
			EXRTrackedDeviceType::Controller,
			EXRTrackedDeviceType::TrackingReference,
			EXRTrackedDeviceType::Other,
		};
		for (EXRTrackedDeviceType T : Types)
		{
			TArray<int32> Typed;
			XR->EnumerateTrackedDevices(Typed, T);
			if (Typed.Contains(Id))
			{
				TypeStr = TrackedDeviceTypeToString(T);
				break;
			}
		}
		D->SetStringField(TEXT("device_type"), TypeStr);

		const bool bTracking = XR->IsTracking(Id);
		D->SetBoolField(TEXT("is_tracking"), bTracking);

		FQuat Orient = FQuat::Identity;
		FVector Pos = FVector::ZeroVector;
		if (XR->GetCurrentPose(Id, Orient, Pos))
		{
			AddQuatFields(D, TEXT("orientation"), Orient);
			AddVectorFields(D, TEXT("position"), Pos);
			D->SetBoolField(TEXT("pose_valid"), true);
		}
		else
		{
			D->SetBoolField(TEXT("pose_valid"), false);
		}

		Devices.Add(MakeShared<FJsonValueObject>(D));
	}

	Result->SetBoolField(TEXT("xr_active"), true);
	Result->SetStringField(TEXT("system_name"), XR->GetSystemName().ToString());
	Result->SetNumberField(TEXT("device_count"), Devices.Num());
	Result->SetArrayField(TEXT("devices"), Devices);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DumpXRInputState);
