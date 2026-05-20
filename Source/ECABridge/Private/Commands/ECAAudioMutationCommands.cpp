// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAudioMutationCommands.h"
#include "Commands/ECACommand.h"

#include "Sound/SoundCue.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_SetSoundAttenuation)
REGISTER_ECA_COMMAND(FECACommand_SetSoundClassProperties)
REGISTER_ECA_COMMAND(FECACommand_SetSoundCueProperties)

namespace ECAAudioMutationHelpers
{
	template<typename T>
	static T* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj) return Obj;
		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Obj = LoadObject<T>(nullptr, *FullPath);
		}
		return Obj;
	}

	static bool TryGetDouble(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, double& Out)
	{
		return Params.IsValid() && Params->TryGetNumberField(Name, Out);
	}
	static bool TryGetBool(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, bool& Out)
	{
		return Params.IsValid() && Params->TryGetBoolField(Name, Out);
	}
	static bool TryGetString(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FString& Out)
	{
		return Params.IsValid() && Params->TryGetStringField(Name, Out);
	}
}

//==============================================================================
// set_sound_attenuation
//==============================================================================
FECACommandResult FECACommand_SetSoundAttenuation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAudioMutationHelpers;

	FString Path;
	if (!GetStringParam(Params, TEXT("attenuation_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: attenuation_path"));
	}

	USoundAttenuation* Att = LoadAssetTolerant<USoundAttenuation>(Path);
	if (!Att)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundAttenuation at: %s"), *Path));
	}

	FSoundAttenuationSettings& S = Att->Attenuation;
	TArray<FString> Updated;

	double D = 0; bool B = false;

	if (TryGetDouble(Params, TEXT("falloff_distance"), D))                  { S.FalloffDistance = D;                  Updated.Add(TEXT("falloff_distance")); }
	if (TryGetBool  (Params, TEXT("attenuate"), B))                         { S.bAttenuate = B;                       Updated.Add(TEXT("attenuate")); }
	if (TryGetBool  (Params, TEXT("spatialize"), B))                        { S.bSpatialize = B;                      Updated.Add(TEXT("spatialize")); }
	if (TryGetBool  (Params, TEXT("attenuate_with_lpf"), B))                { S.bAttenuateWithLPF = B;                Updated.Add(TEXT("attenuate_with_lpf")); }
	if (TryGetBool  (Params, TEXT("enable_listener_focus"), B))             { S.bEnableListenerFocus = B;             Updated.Add(TEXT("enable_listener_focus")); }
	if (TryGetBool  (Params, TEXT("enable_occlusion"), B))                  { S.bEnableOcclusion = B;                 Updated.Add(TEXT("enable_occlusion")); }
	if (TryGetBool  (Params, TEXT("enable_reverb_send"), B))                { S.bEnableReverbSend = B;                Updated.Add(TEXT("enable_reverb_send")); }
	if (TryGetDouble(Params, TEXT("lpf_radius_min"), D))                    { S.LPFRadiusMin = D;                     Updated.Add(TEXT("lpf_radius_min")); }
	if (TryGetDouble(Params, TEXT("lpf_radius_max"), D))                    { S.LPFRadiusMax = D;                     Updated.Add(TEXT("lpf_radius_max")); }
	if (TryGetDouble(Params, TEXT("occlusion_volume_attenuation"), D))      { S.OcclusionVolumeAttenuation = D;       Updated.Add(TEXT("occlusion_volume_attenuation")); }
	if (TryGetDouble(Params, TEXT("reverb_distance_min"), D))               { S.ReverbDistanceMin = D;                Updated.Add(TEXT("reverb_distance_min")); }
	if (TryGetDouble(Params, TEXT("reverb_distance_max"), D))               { S.ReverbDistanceMax = D;                Updated.Add(TEXT("reverb_distance_max")); }
	if (TryGetDouble(Params, TEXT("reverb_wet_level_min"), D))              { S.ReverbWetLevelMin = D;                Updated.Add(TEXT("reverb_wet_level_min")); }
	if (TryGetDouble(Params, TEXT("reverb_wet_level_max"), D))              { S.ReverbWetLevelMax = D;                Updated.Add(TEXT("reverb_wet_level_max")); }

	if (Updated.Num() > 0)
	{
		Att->Modify();
		Att->PostEditChange();
		Att->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), Att->GetPathName());
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& F : Updated) Arr.Add(MakeShared<FJsonValueString>(F));
	Result->SetArrayField(TEXT("fields_updated"), Arr);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_sound_class_properties
//==============================================================================
FECACommandResult FECACommand_SetSoundClassProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAudioMutationHelpers;

	FString Path;
	if (!GetStringParam(Params, TEXT("sound_class_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: sound_class_path"));
	}

	USoundClass* SC = LoadAssetTolerant<USoundClass>(Path);
	if (!SC)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundClass at: %s"), *Path));
	}

	FSoundClassProperties& P = SC->Properties;
	TArray<FString> Updated;
	double D = 0; bool B = false;

	if (TryGetDouble(Params, TEXT("volume"), D))              { P.Volume                 = D; Updated.Add(TEXT("volume")); }
	if (TryGetDouble(Params, TEXT("pitch"), D))               { P.Pitch                  = D; Updated.Add(TEXT("pitch")); }
	if (TryGetDouble(Params, TEXT("lowpass_filter_freq"), D)) { P.LowPassFilterFrequency = D; Updated.Add(TEXT("lowpass_filter_freq")); }
	if (TryGetBool  (Params, TEXT("apply_effects"), B))       { P.bApplyEffects          = B; Updated.Add(TEXT("apply_effects")); }
	if (TryGetBool  (Params, TEXT("always_play"), B))         { P.bAlwaysPlay            = B; Updated.Add(TEXT("always_play")); }
	if (TryGetBool  (Params, TEXT("is_ui"), B))               { P.bIsUISound             = B; Updated.Add(TEXT("is_ui")); }
	if (TryGetBool  (Params, TEXT("is_music"), B))            { P.bIsMusic               = B; Updated.Add(TEXT("is_music")); }
	if (TryGetBool  (Params, TEXT("reverb"), B))              { P.bReverb                = B; Updated.Add(TEXT("reverb")); }
	if (TryGetBool  (Params, TEXT("center_channel_only"), B)) { P.bCenterChannelOnly     = B; Updated.Add(TEXT("center_channel_only")); }

	if (Updated.Num() > 0)
	{
		SC->Modify();
		SC->PostEditChange();
		SC->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), SC->GetPathName());
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& F : Updated) Arr.Add(MakeShared<FJsonValueString>(F));
	Result->SetArrayField(TEXT("fields_updated"), Arr);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_sound_cue_properties
//==============================================================================
FECACommandResult FECACommand_SetSoundCueProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAudioMutationHelpers;

	FString Path;
	if (!GetStringParam(Params, TEXT("sound_cue_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: sound_cue_path"));
	}

	USoundCue* Cue = LoadAssetTolerant<USoundCue>(Path);
	if (!Cue)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundCue at: %s"), *Path));
	}

	TArray<FString> Updated;
	double D = 0; bool B = false; FString StrVal;

	if (TryGetDouble(Params, TEXT("volume_multiplier"), D))    { Cue->VolumeMultiplier  = D; Updated.Add(TEXT("volume_multiplier")); }
	if (TryGetDouble(Params, TEXT("pitch_multiplier"), D))     { Cue->PitchMultiplier   = D; Updated.Add(TEXT("pitch_multiplier")); }
	if (TryGetBool  (Params, TEXT("override_attenuation"), B)) { Cue->bOverrideAttenuation = B; Updated.Add(TEXT("override_attenuation")); }

	if (TryGetString(Params, TEXT("sound_class"), StrVal))
	{
		if (StrVal.IsEmpty())
		{
			Cue->SoundClassObject = nullptr;
		}
		else
		{
			USoundClass* SC = LoadAssetTolerant<USoundClass>(StrVal);
			if (!SC)
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundClass at: %s"), *StrVal));
			}
			Cue->SoundClassObject = SC;
		}
		Updated.Add(TEXT("sound_class"));
	}

	if (TryGetString(Params, TEXT("attenuation_settings"), StrVal))
	{
		if (StrVal.IsEmpty())
		{
			Cue->AttenuationSettings = nullptr;
		}
		else
		{
			USoundAttenuation* Att = LoadAssetTolerant<USoundAttenuation>(StrVal);
			if (!Att)
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundAttenuation at: %s"), *StrVal));
			}
			Cue->AttenuationSettings = Att;
		}
		Updated.Add(TEXT("attenuation_settings"));
	}

	if (Updated.Num() > 0)
	{
		Cue->Modify();
		Cue->PostEditChange();
		Cue->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), Cue->GetPathName());
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& F : Updated) Arr.Add(MakeShared<FJsonValueString>(F));
	Result->SetArrayField(TEXT("fields_updated"), Arr);
	return FECACommandResult::Success(Result);
}
