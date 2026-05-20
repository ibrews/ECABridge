// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAudioCommands.h"
#include "Commands/ECACommand.h"

#include "Sound/SoundCue.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"

#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_DumpSoundCue)
REGISTER_ECA_COMMAND(FECACommand_DumpSoundClass)
REGISTER_ECA_COMMAND(FECACommand_DumpSoundAttenuation)
REGISTER_ECA_COMMAND(FECACommand_ListSoundAssets)

namespace ECAAudioHelpers
{
	template<typename T>
	static T* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj) return Obj;

		// Tolerate /Game/Foo/MyAsset (no .MyAsset suffix)
		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Obj = LoadObject<T>(nullptr, *FullPath);
		}
		return Obj;
	}

	static TSharedPtr<FJsonObject> SoundNodeToJson(const USoundNode* Node)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Node)
		{
			Obj->SetStringField(TEXT("class"), TEXT("None"));
			return Obj;
		}
		Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
		Obj->SetNumberField(TEXT("child_count"), Node->ChildNodes.Num());

		// If this is a wave-player leaf, surface the wave path + duration
		if (const USoundNodeWavePlayer* WavePlayer = Cast<USoundNodeWavePlayer>(Node))
		{
			if (USoundWave* Wave = WavePlayer->GetSoundWave())
			{
				Obj->SetStringField(TEXT("wave_path"), Wave->GetPathName());
				Obj->SetNumberField(TEXT("wave_duration"), Wave->GetDuration());
			}
		}
		return Obj;
	}
}

//==============================================================================
// dump_sound_cue
//==============================================================================
FECACommandResult FECACommand_DumpSoundCue::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CuePath;
	if (!GetStringParam(Params, TEXT("sound_cue_path"), CuePath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: sound_cue_path"));
	}

	USoundCue* Cue = ECAAudioHelpers::LoadAssetTolerant<USoundCue>(CuePath);
	if (!Cue)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundCue at: %s"), *CuePath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), Cue->GetPathName());
	Result->SetStringField(TEXT("name"), Cue->GetName());
	Result->SetNumberField(TEXT("duration"), Cue->GetDuration());
	Result->SetNumberField(TEXT("max_distance"), Cue->GetMaxDistance());
	Result->SetNumberField(TEXT("volume_multiplier"), Cue->VolumeMultiplier);
	Result->SetNumberField(TEXT("pitch_multiplier"), Cue->PitchMultiplier);
	Result->SetStringField(TEXT("sound_class"), Cue->GetSoundClass() ? Cue->GetSoundClass()->GetPathName() : TEXT(""));
	Result->SetStringField(TEXT("attenuation"), Cue->AttenuationSettings ? Cue->AttenuationSettings->GetPathName() : TEXT(""));
	Result->SetBoolField(TEXT("attenuation_enabled"), Cue->bOverrideAttenuation);

	TArray<TSharedPtr<FJsonValue>> NodesJson;
	for (USoundNode* Node : Cue->AllNodes)
	{
		NodesJson.Add(MakeShared<FJsonValueObject>(ECAAudioHelpers::SoundNodeToJson(Node)));
	}
	Result->SetArrayField(TEXT("nodes"), NodesJson);
	Result->SetNumberField(TEXT("node_count"), Cue->AllNodes.Num());

	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_sound_class
//==============================================================================
FECACommandResult FECACommand_DumpSoundClass::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("sound_class_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: sound_class_path"));
	}

	USoundClass* SoundClass = ECAAudioHelpers::LoadAssetTolerant<USoundClass>(Path);
	if (!SoundClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundClass at: %s"), *Path));
	}

	const FSoundClassProperties& Props = SoundClass->Properties;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), SoundClass->GetPathName());
	Result->SetStringField(TEXT("name"), SoundClass->GetName());
	Result->SetNumberField(TEXT("volume"), Props.Volume);
	Result->SetNumberField(TEXT("pitch"), Props.Pitch);
	Result->SetNumberField(TEXT("lowpass_filter_freq"), Props.LowPassFilterFrequency);
	Result->SetBoolField(TEXT("apply_effects"), Props.bApplyEffects);
	Result->SetBoolField(TEXT("always_play"), Props.bAlwaysPlay);
	Result->SetBoolField(TEXT("is_ui"), Props.bIsUISound);
	Result->SetBoolField(TEXT("is_music"), Props.bIsMusic);
	Result->SetBoolField(TEXT("reverb"), Props.bReverb);
	Result->SetBoolField(TEXT("center_channel_only"), Props.bCenterChannelOnly);

	if (const UEnum* OutputTargetEnum = StaticEnum<EAudioOutputTarget::Type>())
	{
		Result->SetStringField(TEXT("output_target"), OutputTargetEnum->GetNameStringByValue(static_cast<int64>(Props.OutputTarget)));
	}
	else
	{
		Result->SetNumberField(TEXT("output_target"), static_cast<int32>(Props.OutputTarget));
	}

	TArray<TSharedPtr<FJsonValue>> ChildArray;
	for (USoundClass* Child : SoundClass->ChildClasses)
	{
		if (Child)
		{
			ChildArray.Add(MakeShared<FJsonValueString>(Child->GetPathName()));
		}
	}
	Result->SetArrayField(TEXT("child_classes"), ChildArray);

	if (SoundClass->ParentClass)
	{
		Result->SetStringField(TEXT("parent_class"), SoundClass->ParentClass->GetPathName());
	}

	if (SoundClass->PassiveSoundMixModifiers.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> MixArray;
		for (const FPassiveSoundMixModifier& Mix : SoundClass->PassiveSoundMixModifiers)
		{
			TSharedPtr<FJsonObject> MixObj = MakeShared<FJsonObject>();
			MixObj->SetStringField(TEXT("mix"), Mix.SoundMix ? Mix.SoundMix->GetPathName() : TEXT(""));
			MixObj->SetNumberField(TEXT("min_volume_threshold"), Mix.MinVolumeThreshold);
			MixObj->SetNumberField(TEXT("max_volume_threshold"), Mix.MaxVolumeThreshold);
			MixArray.Add(MakeShared<FJsonValueObject>(MixObj));
		}
		Result->SetArrayField(TEXT("passive_sound_mix_modifiers"), MixArray);
	}

	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_sound_attenuation
//==============================================================================
FECACommandResult FECACommand_DumpSoundAttenuation::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!GetStringParam(Params, TEXT("attenuation_path"), Path))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: attenuation_path"));
	}

	USoundAttenuation* Att = ECAAudioHelpers::LoadAssetTolerant<USoundAttenuation>(Path);
	if (!Att)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load USoundAttenuation at: %s"), *Path));
	}

	const FSoundAttenuationSettings& S = Att->Attenuation;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), Att->GetPathName());
	Result->SetStringField(TEXT("name"), Att->GetName());

	// Top-level toggles
	Result->SetBoolField(TEXT("attenuate"), S.bAttenuate);
	Result->SetBoolField(TEXT("spatialize"), S.bSpatialize);
	Result->SetBoolField(TEXT("attenuate_with_lpf"), S.bAttenuateWithLPF);
	Result->SetBoolField(TEXT("enable_listener_focus"), S.bEnableListenerFocus);
	Result->SetBoolField(TEXT("enable_occlusion"), S.bEnableOcclusion);
	Result->SetBoolField(TEXT("enable_reverb_send"), S.bEnableReverbSend);
	Result->SetBoolField(TEXT("enable_priority_attenuation"), S.bEnablePriorityAttenuation);
	Result->SetBoolField(TEXT("enable_submix_sends"), S.bEnableSubmixSends);

	// Distance/falloff
	Result->SetNumberField(TEXT("falloff_distance"), S.FalloffDistance);
	Result->SetNumberField(TEXT("attenuation_shape_radius"), S.AttenuationShapeExtents.X);
	Result->SetNumberField(TEXT("attenuation_shape_y"),      S.AttenuationShapeExtents.Y);
	Result->SetNumberField(TEXT("attenuation_shape_z"),      S.AttenuationShapeExtents.Z);
	Result->SetNumberField(TEXT("cone_offset"),              S.ConeOffset);
	Result->SetNumberField(TEXT("non_spatialized_radius_start"), S.NonSpatializedRadiusStart);
	Result->SetNumberField(TEXT("non_spatialized_radius_end"),   S.NonSpatializedRadiusEnd);

	// Air absorption
	Result->SetNumberField(TEXT("lpf_radius_min"), S.LPFRadiusMin);
	Result->SetNumberField(TEXT("lpf_radius_max"), S.LPFRadiusMax);
	Result->SetNumberField(TEXT("lpf_frequency_at_min"), S.LPFFrequencyAtMin);
	Result->SetNumberField(TEXT("lpf_frequency_at_max"), S.LPFFrequencyAtMax);

	// Occlusion
	Result->SetNumberField(TEXT("occlusion_low_pass_filter_frequency"), S.OcclusionLowPassFilterFrequency);
	Result->SetNumberField(TEXT("occlusion_volume_attenuation"),        S.OcclusionVolumeAttenuation);
	Result->SetNumberField(TEXT("occlusion_interpolation_time"),        S.OcclusionInterpolationTime);

	// Reverb send
	Result->SetNumberField(TEXT("reverb_distance_min"),    S.ReverbDistanceMin);
	Result->SetNumberField(TEXT("reverb_distance_max"),    S.ReverbDistanceMax);
	Result->SetNumberField(TEXT("reverb_wet_level_min"),   S.ReverbWetLevelMin);
	Result->SetNumberField(TEXT("reverb_wet_level_max"),   S.ReverbWetLevelMax);

	// Enum names where useful
	if (const UEnum* AlgEnum = StaticEnum<EAttenuationDistanceModel>())
	{
		Result->SetStringField(TEXT("distance_algorithm"), AlgEnum->GetNameStringByValue(static_cast<int64>(S.DistanceAlgorithm)));
	}
	if (const UEnum* ShapeEnum = StaticEnum<EAttenuationShape::Type>())
	{
		Result->SetStringField(TEXT("attenuation_shape"), ShapeEnum->GetNameStringByValue(static_cast<int64>(S.AttenuationShape)));
	}

	return FECACommandResult::Success(Result);
}

//==============================================================================
// list_sound_assets
//==============================================================================
FECACommandResult FECACommand_ListSoundAssets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	GetStringParam(Params, TEXT("path_filter"), PathFilter, false);

	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	FString ClassFilter;
	GetStringParam(Params, TEXT("class_filter"), ClassFilter, false);

	int32 MaxResults = 200;
	GetIntParam(Params, TEXT("max_results"), MaxResults, false);
	MaxResults = FMath::Clamp(MaxResults, 1, 5000);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*PathFilter));
	Filter.bRecursivePaths = true;

	UClass* BaseClass = USoundBase::StaticClass();
	if (!ClassFilter.IsEmpty())
	{
		UClass* Resolved = nullptr;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName().Equals(ClassFilter, ESearchCase::IgnoreCase))
			{
				Resolved = *It;
				break;
			}
		}
		if (Resolved && (Resolved->IsChildOf(USoundBase::StaticClass()) ||
		                 Resolved->IsChildOf(USoundClass::StaticClass()) ||
		                 Resolved->IsChildOf(USoundAttenuation::StaticClass())))
		{
			BaseClass = Resolved;
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("class_filter '%s' did not resolve to a USoundBase/USoundClass/USoundAttenuation subclass"), *ClassFilter));
		}
	}
	Filter.ClassPaths.Add(BaseClass->GetClassPathName());
	Filter.bRecursiveClasses = true;

	// When no class filter is given, also pull in USoundClass and USoundAttenuation
	// so a generic "list sound assets" call sees the full audio surface.
	if (ClassFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(USoundAttenuation::StaticClass()->GetClassPathName());
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	if (!NameFilter.IsEmpty())
	{
		AssetDataList.RemoveAll([&NameFilter](const FAssetData& Data)
		{
			return !Data.AssetName.ToString().MatchesWildcard(NameFilter);
		});
	}

	const int32 TotalFound = AssetDataList.Num();
	const int32 Count = FMath::Min(TotalFound, MaxResults);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (int32 i = 0; i < Count; ++i)
	{
		const FAssetData& Data = AssetDataList[i];
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Data.GetObjectPathString());
		Obj->SetStringField(TEXT("name"), Data.AssetName.ToString());
		Obj->SetStringField(TEXT("class"), Data.AssetClassPath.GetAssetName().ToString());
		AssetsArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("total_found"), TotalFound);
	Result->SetNumberField(TEXT("returned"), Count);
	Result->SetArrayField(TEXT("assets"), AssetsArray);
	return FECACommandResult::Success(Result);
}
