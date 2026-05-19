// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPathTracerCommands.h"

#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"

REGISTER_ECA_COMMAND(FECACommand_EnablePathTracer);
REGISTER_ECA_COMMAND(FECACommand_SetPathTracerSettings);
REGISTER_ECA_COMMAND(FECACommand_RenderPathTracedScreenshot);

namespace PathTracerHelpers
{
	static bool SetIntCVar(const TCHAR* Name, int32 Value)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!CVar) return false;
		CVar->Set(Value, ECVF_SetByConsole);
		return true;
	}

	static bool SetFloatCVar(const TCHAR* Name, float Value)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!CVar) return false;
		CVar->Set(Value, ECVF_SetByConsole);
		return true;
	}

	static int32 GetIntCVar(const TCHAR* Name, int32 Fallback = 0)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		return CVar ? CVar->GetInt() : Fallback;
	}
}

// ─── enable_path_tracer ──────────────────────────────────────

FECACommandResult FECACommand_EnablePathTracer::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bEnable = true;
	GetBoolParam(Params, TEXT("enable"), bEnable, /*bRequired=*/false);

	if (!PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing"), bEnable ? 1 : 0))
	{
		return FECACommandResult::Error(TEXT("CVar 'r.PathTracing' not found — engine may lack Path Tracer support"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("enabled"), bEnable);
	Result->SetNumberField(TEXT("r_PathTracing"), PathTracerHelpers::GetIntCVar(TEXT("r.PathTracing")));
	return FECACommandResult::Success(Result);
}

// ─── set_path_tracer_settings ────────────────────────────────

FECACommandResult FECACommand_SetPathTracerSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeResult();
	int32 Applied = 0;

	double SamplesPerPixel;
	if (GetFloatParam(Params, TEXT("samples_per_pixel"), SamplesPerPixel, /*bRequired=*/false))
	{
		if (PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing.SamplesPerPixel"), FMath::Max(1, (int32)SamplesPerPixel)))
		{
			Result->SetNumberField(TEXT("samples_per_pixel"), (int32)SamplesPerPixel);
			++Applied;
		}
	}

	double MaxBounces;
	if (GetFloatParam(Params, TEXT("max_bounces"), MaxBounces, /*bRequired=*/false))
	{
		if (PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing.MaxBounces"), FMath::Max(0, (int32)MaxBounces)))
		{
			Result->SetNumberField(TEXT("max_bounces"), (int32)MaxBounces);
			++Applied;
		}
	}

	double MaxIntensity;
	if (GetFloatParam(Params, TEXT("max_path_intensity"), MaxIntensity, /*bRequired=*/false))
	{
		if (PathTracerHelpers::SetFloatCVar(TEXT("r.PathTracing.MaxPathIntensity"), (float)MaxIntensity))
		{
			Result->SetNumberField(TEXT("max_path_intensity"), MaxIntensity);
			++Applied;
		}
	}

	bool bDenoise;
	if (GetBoolParam(Params, TEXT("denoise"), bDenoise, /*bRequired=*/false))
	{
		if (PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing.Denoiser"), bDenoise ? 1 : 0))
		{
			Result->SetBoolField(TEXT("denoise"), bDenoise);
			++Applied;
		}
	}

	bool bUseFilter;
	if (GetBoolParam(Params, TEXT("use_path_tracer_filter"), bUseFilter, /*bRequired=*/false))
	{
		if (PathTracerHelpers::SetFloatCVar(TEXT("r.PathTracing.FilterWidth"), bUseFilter ? 3.0f : 0.0f))
		{
			Result->SetBoolField(TEXT("use_path_tracer_filter"), bUseFilter);
			++Applied;
		}
	}

	if (Applied == 0)
	{
		return FECACommandResult::Error(TEXT("No valid settings provided. Supported: samples_per_pixel, max_bounces, max_path_intensity, denoise, use_path_tracer_filter"));
	}

	Result->SetNumberField(TEXT("settings_applied"), Applied);
	return FECACommandResult::Success(Result);
}

// ─── render_path_traced_screenshot ──────────────────────────

FECACommandResult FECACommand_RenderPathTracedScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Enable path tracing (best-effort; succeed even if CVar missing on this engine)
	PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing"), 1);

	double SamplesPerPixel;
	if (GetFloatParam(Params, TEXT("samples_per_pixel"), SamplesPerPixel, /*bRequired=*/false))
	{
		PathTracerHelpers::SetIntCVar(TEXT("r.PathTracing.SamplesPerPixel"), FMath::Max(1, (int32)SamplesPerPixel));
	}

	FString Filename;
	GetStringParam(Params, TEXT("filename"), Filename, /*bRequired=*/false);
	if (Filename.IsEmpty())
	{
		Filename = FString::Printf(TEXT("PathTracer_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	}
	if (!Filename.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		Filename += TEXT(".png");
	}

	const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
	IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
	const FString FullPath = ScreenshotDir / Filename;

	// Trigger a high-res screenshot of the active editor viewport. The path-tracer
	// view-mode driven by r.PathTracing=1 means subsequent frames render via path
	// tracing; the screenshot dumper writes the converged image to disk.
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	Config.FilenameOverride = FullPath;
	Config.bDateTimeBasedNaming = false;

	bool bRequested = false;
	if (GEditor)
	{
		FViewport* Viewport = GEditor->GetActiveViewport();
		if (Viewport)
		{
			Viewport->TakeHighResScreenShot();
			bRequested = true;
		}
	}

	if (!bRequested)
	{
		return FECACommandResult::Error(TEXT("No active editor viewport available to capture"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("filename"), Filename);
	Result->SetStringField(TEXT("full_path"), FullPath);
	Result->SetStringField(TEXT("note"), TEXT("Screenshot requested. Path tracer convergence depends on r.PathTracing.SamplesPerPixel; file is written after that many frames are rendered."));
	return FECACommandResult::Success(Result);
}
