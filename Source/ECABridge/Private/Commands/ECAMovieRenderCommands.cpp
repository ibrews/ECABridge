// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMovieRenderCommands.h"

#include "LevelSequence.h"

#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
// #include "MoviePipelineEXROutput.h"  // Requires Imath/OpenEXR — skip for now
#include "MoviePipelineAntiAliasingSetting.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Editor/EditorEngine.h"
#include "Engine/Level.h"
#include "Engine/World.h"

extern UNREALED_API UEditorEngine* GEditor;

// ─── Helpers ───────────────────────────────────────────────────

namespace MovieRenderCommandHelpers
{
	/** Load a ULevelSequence by path, trying common path variants */
	static ULevelSequence* LoadSequence(const FString& SequencePath)
	{
		ULevelSequence* Seq = LoadObject<ULevelSequence>(nullptr, *SequencePath);
		if (!Seq)
		{
			FString FullPath = SequencePath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Seq = LoadObject<ULevelSequence>(nullptr, *FullPath);
		}
		return Seq;
	}

	/** Resolve output directory: if relative, make it relative to project saved dir */
	static FString ResolveOutputDirectory(const FString& InDir)
	{
		if (FPaths::IsRelative(InDir))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / InDir);
		}
		return InDir;
	}
}

// ─── REGISTER ──────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_RenderSequence);
REGISTER_ECA_COMMAND(FECACommand_GetRenderStatus);

// ─── render_sequence ───────────────────────────────────────────

FECACommandResult FECACommand_RenderSequence::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// --- Parse parameters ---
	FString SequencePath;
	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));
	}

	FString OutputDir = TEXT("Saved/MovieRenders");
	GetStringParam(Params, TEXT("output_dir"), OutputDir, /*bRequired=*/false);

	FString OutputFormat = TEXT("png");
	GetStringParam(Params, TEXT("output_format"), OutputFormat, /*bRequired=*/false);
	OutputFormat = OutputFormat.ToLower();

	double ResolutionX = 1920.0;
	GetFloatParam(Params, TEXT("resolution_x"), ResolutionX, /*bRequired=*/false);

	double ResolutionY = 1080.0;
	GetFloatParam(Params, TEXT("resolution_y"), ResolutionY, /*bRequired=*/false);

	double FrameRate = 30.0;
	GetFloatParam(Params, TEXT("frame_rate"), FrameRate, /*bRequired=*/false);

	// --- Validate parameters ---
	if (OutputFormat != TEXT("png") && OutputFormat != TEXT("jpg") && OutputFormat != TEXT("exr") && OutputFormat != TEXT("mp4") && OutputFormat != TEXT("video"))
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Invalid output_format '%s'. Supported: png, jpg, exr, mp4"), *OutputFormat));
	}

	if (ResolutionX <= 0 || ResolutionY <= 0)
	{
		return FECACommandResult::Error(TEXT("resolution_x and resolution_y must be positive"));
	}

	if (FrameRate <= 0.0)
	{
		return FECACommandResult::Error(TEXT("frame_rate must be a positive number"));
	}

	// --- Load sequence ---
	ULevelSequence* Seq = MovieRenderCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not load Level Sequence at: %s"), *SequencePath));
	}

	// --- Get the editor MRQ subsystem ---
	UMoviePipelineQueueSubsystem* QueueSubsystem =
		GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	if (!QueueSubsystem)
	{
		return FECACommandResult::Error(TEXT("MoviePipelineQueueSubsystem is not available. Ensure the Movie Render Queue plugin is enabled."));
	}

	if (QueueSubsystem->IsRendering())
	{
		return FECACommandResult::Error(TEXT("A render is already in progress. Use get_render_status to check progress, or wait for it to finish."));
	}

	// --- Build the queue ---
	UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
	if (!Queue)
	{
		return FECACommandResult::Error(TEXT("Failed to get render queue from subsystem"));
	}

	// Clear any existing jobs
	Queue->DeleteAllJobs();

	// Create a new job
	UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	if (!Job)
	{
		return FECACommandResult::Error(TEXT("Failed to allocate render job"));
	}

	Job->JobName = FString::Printf(TEXT("ECA_Render_%s"), *Seq->GetName());
	Job->SetSequence(FSoftObjectPath(Seq));

	// Set the map to the currently loaded editor world
	UWorld* EditorWorld = GetEditorWorld();
	if (EditorWorld)
	{
		ULevel* PersistentLevel = EditorWorld->PersistentLevel;
		if (PersistentLevel && PersistentLevel->GetOuter())
		{
			Job->Map = FSoftObjectPath(PersistentLevel->GetOuter());
		}
	}

	// --- Configure the legacy pipeline settings ---
	UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
	if (!Config)
	{
		return FECACommandResult::Error(TEXT("Failed to get job configuration"));
	}

	// Output settings: directory, resolution, frame rate
	UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
	if (OutputSetting)
	{
		FString ResolvedDir = MovieRenderCommandHelpers::ResolveOutputDirectory(OutputDir);
		OutputSetting->OutputDirectory.Path = ResolvedDir;
		OutputSetting->OutputResolution = FIntPoint(
			static_cast<int32>(ResolutionX),
			static_cast<int32>(ResolutionY));
		OutputSetting->bUseCustomFrameRate = true;
		OutputSetting->OutputFrameRate = FFrameRate(static_cast<int32>(FrameRate), 1);
		OutputSetting->bOverrideExistingOutput = true;
		if (OutputFormat == TEXT("mp4") || OutputFormat == TEXT("video"))
			OutputSetting->FileNameFormat = TEXT("{sequence_name}/{sequence_name}");
		else
			OutputSetting->FileNameFormat = TEXT("{sequence_name}/{sequence_name}.{frame_number}");
	}

	// Render pass: Deferred Rendering (standard lit)
	Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());

	// Output format: PNG / JPG / EXR
	if (OutputFormat == TEXT("png"))
	{
		Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass());
	}
	else if (OutputFormat == TEXT("jpg"))
	{
		Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_JPG::StaticClass());
	}
	else if (OutputFormat == TEXT("exr"))
	{
		Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass());
	}
	else if (OutputFormat == TEXT("mp4") || OutputFormat == TEXT("video"))
	{
		// Use runtime class discovery for MP4 encoder (in MovieRenderPipelineMP4Encoder module)
		UClass* MP4OutputClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineMP4Encoder.MoviePipelineMP4EncoderOutput"));
		if (MP4OutputClass)
		{
			Config->FindOrAddSettingByClass(MP4OutputClass);
		}
		else
		{
			// MP4 encoder not available — fall back to PNG frames
			Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass());
		}
	}

	// --- Start the render using the PIE executor ---
	UMoviePipelineExecutorBase* Executor =
		QueueSubsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());

	if (!Executor)
	{
		return FECACommandResult::Error(TEXT("Failed to start the render executor"));
	}

	// --- Build response ---
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("status"), TEXT("render_started"));
	Result->SetStringField(TEXT("sequence"), Seq->GetPathName());
	Result->SetStringField(TEXT("output_dir"), OutputSetting ? OutputSetting->OutputDirectory.Path : OutputDir);
	Result->SetStringField(TEXT("output_format"), OutputFormat);
	Result->SetNumberField(TEXT("resolution_x"), ResolutionX);
	Result->SetNumberField(TEXT("resolution_y"), ResolutionY);
	Result->SetNumberField(TEXT("frame_rate"), FrameRate);
	Result->SetStringField(TEXT("note"), TEXT("Render is asynchronous. Use get_render_status to monitor progress."));

	return FECACommandResult::Success(Result);
}

// ─── get_render_status ─────────────────────────────────────────

FECACommandResult FECACommand_GetRenderStatus::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeResult();

	// Check the editor MRQ subsystem
	UMoviePipelineQueueSubsystem* QueueSubsystem =
		GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	if (!QueueSubsystem)
	{
		Result->SetBoolField(TEXT("is_rendering"), false);
		Result->SetStringField(TEXT("error"), TEXT("MoviePipelineQueueSubsystem is not available"));
		return FECACommandResult::Success(Result);
	}

	bool bIsRendering = QueueSubsystem->IsRendering();
	Result->SetBoolField(TEXT("is_rendering"), bIsRendering);

	if (bIsRendering)
	{
		UMoviePipelineExecutorBase* Executor = QueueSubsystem->GetActiveExecutor();
		if (Executor)
		{
			float Progress = Executor->GetStatusProgress();
			FString StatusMessage = Executor->GetStatusMessage();

			Result->SetNumberField(TEXT("progress"), Progress);
			Result->SetStringField(TEXT("status_message"), StatusMessage);

			// Try to get per-job progress from the queue
			UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
			if (Queue)
			{
				TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
				TArray<TSharedPtr<FJsonValue>> JobsArray;

				for (UMoviePipelineExecutorJob* Job : Jobs)
				{
					if (!Job) continue;

					TSharedPtr<FJsonObject> JobObj = MakeShared<FJsonObject>();
					JobObj->SetStringField(TEXT("name"), Job->JobName);
					JobObj->SetNumberField(TEXT("progress"), Job->GetStatusProgress());
					JobObj->SetStringField(TEXT("status"), Job->GetStatusMessage());
					JobObj->SetBoolField(TEXT("enabled"), Job->IsEnabled());
					JobObj->SetBoolField(TEXT("consumed"), Job->IsConsumed());

					if (Job->Sequence.IsValid())
					{
						JobObj->SetStringField(TEXT("sequence"), Job->Sequence.GetAssetPathString());
					}

					// Per-shot progress
					TArray<TSharedPtr<FJsonValue>> ShotsArray;
					for (UMoviePipelineExecutorShot* Shot : Job->ShotInfo)
					{
						if (!Shot) continue;

						TSharedPtr<FJsonObject> ShotObj = MakeShared<FJsonObject>();
						ShotObj->SetStringField(TEXT("outer_name"), Shot->OuterName);
						ShotObj->SetStringField(TEXT("inner_name"), Shot->InnerName);
						ShotObj->SetNumberField(TEXT("progress"), Shot->GetStatusProgress());
						ShotObj->SetStringField(TEXT("status"), Shot->GetStatusMessage());
						ShotObj->SetBoolField(TEXT("enabled"), Shot->ShouldRender());
						ShotsArray.Add(MakeShared<FJsonValueObject>(ShotObj));
					}

					if (ShotsArray.Num() > 0)
					{
						JobObj->SetArrayField(TEXT("shots"), ShotsArray);
					}

					JobsArray.Add(MakeShared<FJsonValueObject>(JobObj));
				}

				Result->SetArrayField(TEXT("jobs"), JobsArray);
			}
		}
		else
		{
			Result->SetNumberField(TEXT("progress"), 0.0);
			Result->SetStringField(TEXT("status_message"), TEXT("Rendering (executor details unavailable)"));
		}
	}
	else
	{
		Result->SetNumberField(TEXT("progress"), 0.0);
		Result->SetStringField(TEXT("status_message"), TEXT("No render in progress"));
	}

	return FECACommandResult::Success(Result);
}
