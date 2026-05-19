// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAColorGradingCommands.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

REGISTER_ECA_COMMAND(FECACommand_LoadLutTexture);
REGISTER_ECA_COMMAND(FECACommand_ApplyLutToVolume);
REGISTER_ECA_COMMAND(FECACommand_DumpColorGrading);

namespace ColorGradingHelpers
{
	static APostProcessVolume* FindVolume(UWorld* World, const FString& Name)
	{
		if (!World) return nullptr;
		if (Name.IsEmpty())
		{
			for (TActorIterator<APostProcessVolume> It(World); It; ++It) return *It;
			return nullptr;
		}
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->GetActorLabel() == Name || It->GetName() == Name) return *It;
		}
		return nullptr;
	}

	static UTexture* LoadTextureFlexible(const FString& Path)
	{
		UTexture* T = LoadObject<UTexture>(nullptr, *Path);
		if (T) return T;
		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString Asset = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + Asset;
		}
		return LoadObject<UTexture>(nullptr, *FullPath);
	}

	static TSharedPtr<FJsonObject> Vec4ToJson(const FVector4& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		O->SetNumberField(TEXT("z"), V.Z);
		O->SetNumberField(TEXT("w"), V.W);
		return O;
	}
}

// ─── load_lut_texture ────────────────────────────────────────

FECACommandResult FECACommand_LoadLutTexture::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TexPath;
	if (!GetStringParam(Params, TEXT("texture_path"), TexPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: texture_path"));
	}

	UTexture* Tex = ColorGradingHelpers::LoadTextureFlexible(TexPath);
	if (!Tex)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Texture not found at path: %s"), *TexPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("texture_path"), Tex->GetPathName());
	Result->SetStringField(TEXT("texture_class"), Tex->GetClass()->GetName());

	if (UTexture2D* T2D = Cast<UTexture2D>(Tex))
	{
		const int32 W = T2D->GetSizeX();
		const int32 H = T2D->GetSizeY();
		Result->SetNumberField(TEXT("size_x"), W);
		Result->SetNumberField(TEXT("size_y"), H);
		// A typical unrolled LUT is W = H * H (e.g. 256x16 for 16-slice). Flag plausibility.
		const bool bPlausibleUnrolledLUT = (W > 0 && H > 0 && W == H * H);
		Result->SetBoolField(TEXT("plausible_unrolled_lut"), bPlausibleUnrolledLUT);
	}
	return FECACommandResult::Success(Result);
}

// ─── apply_lut_to_volume ─────────────────────────────────────

FECACommandResult FECACommand_ApplyLutToVolume::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);
	APostProcessVolume* PPV = ColorGradingHelpers::FindVolume(World, ActorName);
	if (!PPV) return FECACommandResult::Error(TEXT("No PostProcessVolume found"));

	FString TexPath;
	if (!GetStringParam(Params, TEXT("texture_path"), TexPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: texture_path"));
	}
	UTexture* Tex = ColorGradingHelpers::LoadTextureFlexible(TexPath);
	if (!Tex) return FECACommandResult::Error(FString::Printf(TEXT("LUT texture not found: %s"), *TexPath));

	double Intensity = 1.0;
	GetFloatParam(Params, TEXT("intensity"), Intensity, /*bRequired=*/false);
	Intensity = FMath::Clamp(Intensity, 0.0, 1.0);

	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_ColorGradingLUT = true;
	S.ColorGradingLUT = Tex;
	S.bOverride_ColorGradingIntensity = true;
	S.ColorGradingIntensity = (float)Intensity;

	PPV->MarkPackageDirty();
	if (GEditor) GEditor->RedrawAllViewports();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());
	Result->SetStringField(TEXT("texture_path"), Tex->GetPathName());
	Result->SetNumberField(TEXT("intensity"), Intensity);
	return FECACommandResult::Success(Result);
}

// ─── dump_color_grading ──────────────────────────────────────

FECACommandResult FECACommand_DumpColorGrading::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);
	APostProcessVolume* PPV = ColorGradingHelpers::FindVolume(World, ActorName);
	if (!PPV) return FECACommandResult::Error(TEXT("No PostProcessVolume found"));

	const FPostProcessSettings& S = PPV->Settings;

	auto MakeSlot = [&](const FVector4& Saturation, bool bSat,
	                    const FVector4& Contrast, bool bCon,
	                    const FVector4& Gamma, bool bGam,
	                    const FVector4& Gain, bool bGai,
	                    const FVector4& Offset, bool bOff)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		auto AddWheel = [&](const TCHAR* K, const FVector4& V, bool bOverride)
		{
			TSharedPtr<FJsonObject> W = ColorGradingHelpers::Vec4ToJson(V);
			W->SetBoolField(TEXT("override"), bOverride);
			O->SetObjectField(K, W);
		};
		AddWheel(TEXT("saturation"), Saturation, bSat);
		AddWheel(TEXT("contrast"), Contrast, bCon);
		AddWheel(TEXT("gamma"), Gamma, bGam);
		AddWheel(TEXT("gain"), Gain, bGai);
		AddWheel(TEXT("offset"), Offset, bOff);
		return O;
	};

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());

	Result->SetObjectField(TEXT("global"), MakeSlot(
		S.ColorSaturation, S.bOverride_ColorSaturation,
		S.ColorContrast,   S.bOverride_ColorContrast,
		S.ColorGamma,      S.bOverride_ColorGamma,
		S.ColorGain,       S.bOverride_ColorGain,
		S.ColorOffset,     S.bOverride_ColorOffset));

	Result->SetObjectField(TEXT("shadows"), MakeSlot(
		S.ColorSaturationShadows, S.bOverride_ColorSaturationShadows,
		S.ColorContrastShadows,   S.bOverride_ColorContrastShadows,
		S.ColorGammaShadows,      S.bOverride_ColorGammaShadows,
		S.ColorGainShadows,       S.bOverride_ColorGainShadows,
		S.ColorOffsetShadows,     S.bOverride_ColorOffsetShadows));

	Result->SetObjectField(TEXT("midtones"), MakeSlot(
		S.ColorSaturationMidtones, S.bOverride_ColorSaturationMidtones,
		S.ColorContrastMidtones,   S.bOverride_ColorContrastMidtones,
		S.ColorGammaMidtones,      S.bOverride_ColorGammaMidtones,
		S.ColorGainMidtones,       S.bOverride_ColorGainMidtones,
		S.ColorOffsetMidtones,     S.bOverride_ColorOffsetMidtones));

	Result->SetObjectField(TEXT("highlights"), MakeSlot(
		S.ColorSaturationHighlights, S.bOverride_ColorSaturationHighlights,
		S.ColorContrastHighlights,   S.bOverride_ColorContrastHighlights,
		S.ColorGammaHighlights,      S.bOverride_ColorGammaHighlights,
		S.ColorGainHighlights,       S.bOverride_ColorGainHighlights,
		S.ColorOffsetHighlights,     S.bOverride_ColorOffsetHighlights));

	TSharedPtr<FJsonObject> Lut = MakeShared<FJsonObject>();
	Lut->SetBoolField(TEXT("override"), S.bOverride_ColorGradingLUT);
	Lut->SetStringField(TEXT("texture_path"), S.ColorGradingLUT ? S.ColorGradingLUT->GetPathName() : TEXT(""));
	Lut->SetNumberField(TEXT("intensity"), S.ColorGradingIntensity);
	Lut->SetBoolField(TEXT("intensity_override"), S.bOverride_ColorGradingIntensity);
	Result->SetObjectField(TEXT("lut"), Lut);

	return FECACommandResult::Success(Result);
}
