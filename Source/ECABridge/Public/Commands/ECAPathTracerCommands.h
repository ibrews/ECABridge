// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// enable_path_tracer ─ flips r.PathTracing CVar
class FECACommand_EnablePathTracer : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("enable_path_tracer"); }
	virtual FString GetDescription() const override { return TEXT("Toggle the Path Tracer (sets r.PathTracing CVar)"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("enable"), TEXT("boolean"), TEXT("Enable (true) or disable (false) the Path Tracer"), false, TEXT("true") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// set_path_tracer_settings ─ sets samples, bounces, denoiser, etc. via CVars
class FECACommand_SetPathTracerSettings : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_path_tracer_settings"); }
	virtual FString GetDescription() const override { return TEXT("Configure Path Tracer CVars (samples, bounces, denoise)"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("samples_per_pixel"), TEXT("number"), TEXT("Samples per pixel (r.PathTracing.SamplesPerPixel)"), false },
			{ TEXT("max_bounces"), TEXT("number"), TEXT("Max ray bounces (r.PathTracing.MaxBounces)"), false },
			{ TEXT("max_path_intensity"), TEXT("number"), TEXT("Max path intensity clamp (r.PathTracing.MaxPathIntensity)"), false },
			{ TEXT("denoise"), TEXT("boolean"), TEXT("Enable denoiser (r.PathTracing.Denoiser)"), false },
			{ TEXT("use_path_tracer_filter"), TEXT("boolean"), TEXT("Use path tracer's anti-aliasing filter (r.PathTracing.FilterWidth>0)"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// render_path_traced_screenshot ─ captures the active editor viewport with path tracing on
class FECACommand_RenderPathTracedScreenshot : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("render_path_traced_screenshot"); }
	virtual FString GetDescription() const override { return TEXT("Enable Path Tracer and capture the active editor viewport (saved PNG)"); }
	virtual FString GetCategory() const override { return TEXT("Rendering"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("filename"), TEXT("string"), TEXT("Output PNG filename (saved to Project Saved/Screenshots). Auto-named if omitted."), false },
			{ TEXT("samples_per_pixel"), TEXT("number"), TEXT("Override r.PathTracing.SamplesPerPixel for this render"), false }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
