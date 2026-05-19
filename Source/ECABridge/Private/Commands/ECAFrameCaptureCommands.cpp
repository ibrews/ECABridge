// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAFrameCaptureCommands.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Modules/ModuleManager.h"

REGISTER_ECA_COMMAND(FECACommand_CaptureFrame)

FECACommandResult FECACommand_CaptureFrame::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Method = TEXT("screenshot");
	GetStringParam(Params, TEXT("method"), Method, false);
	FString Label;
	GetStringParam(Params, TEXT("label"), Label, false);

	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Suffix = Label.IsEmpty() ? Stamp : (Stamp + TEXT("-") + Label);
	const FString OutDir = FPaths::ProjectSavedDir() / TEXT("Profiling/Captures");
	IFileManager::Get().MakeDirectory(*OutDir, true);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("method"), Method);

	if (Method.Equals(TEXT("renderdoc"), ESearchCase::IgnoreCase))
	{
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("RenderDocPlugin")))
		{
			return FECACommandResult::Error(TEXT("RenderDoc plugin is not loaded. Enable the RenderDoc plugin in the project to use method='renderdoc', or use method='screenshot'."));
		}
		if (GEngine)
		{
			GEngine->Exec(nullptr, TEXT("renderdoc.CaptureFrame"));
		}
		Result->SetStringField(TEXT("note"), TEXT("RenderDoc capture queued via console command. Capture appears in the RenderDoc UI; see Saved/RenderDocCaptures/ for any file output."));
		Result->SetStringField(TEXT("output_dir_hint"), FPaths::ProjectSavedDir() / TEXT("RenderDocCaptures"));
		return FECACommandResult::Success(Result);
	}

	const FString OutFile = OutDir / (TEXT("capture-") + Suffix + TEXT(".png"));
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	Config.FilenameOverride = OutFile;
	Config.SetResolution(1920, 1080, 1.0f);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->Viewport->TakeHighResScreenShot();
		Result->SetStringField(TEXT("artifact_path"), OutFile);
		Result->SetStringField(TEXT("note"), TEXT("High-res screenshot queued. File is written on the next render tick — may not exist immediately."));
		return FECACommandResult::Success(Result);
	}

	if (GEditor)
	{
		// Editor mode: write a viewport screenshot through the console command.
		const FString Cmd = FString::Printf(TEXT("HighResShot 1920x1080 \"%s\""), *OutFile);
		GEngine->Exec(nullptr, *Cmd);
		Result->SetStringField(TEXT("artifact_path"), OutFile);
		Result->SetStringField(TEXT("note"), TEXT("HighResShot queued via console command. File written on next render tick."));
		return FECACommandResult::Success(Result);
	}

	return FECACommandResult::Error(TEXT("No viewport available to capture from"));
}
