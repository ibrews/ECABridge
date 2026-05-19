// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAInsightsCommands.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

REGISTER_ECA_COMMAND(FECACommand_StartInsightsTrace)
REGISTER_ECA_COMMAND(FECACommand_StopInsightsTrace)
REGISTER_ECA_COMMAND(FECACommand_DumpInsightsSummary)

namespace ECAInsightsState
{
	static FString LastTraceFile;
	static FString LastChannels;
	static FDateTime LastStartTime;
}

static FString DefaultTraceFile()
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Profiling/Traces");
	IFileManager::Get().MakeDirectory(*Dir, true);
	return Dir / (FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".utrace"));
}

FECACommandResult FECACommand_StartInsightsTrace::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString File;
	GetStringParam(Params, TEXT("file"), File, false);
	FString Channels = TEXT("cpu,frame,bookmark");
	GetStringParam(Params, TEXT("channels"), Channels, false);
	if (File.IsEmpty())
	{
		File = DefaultTraceFile();
	}

	// Trace.Start in UE 5.7/5.8 accepts `file=<path>` and `channels=<csv>`.
	const FString Cmd = FString::Printf(TEXT("Trace.Start file=\"%s\" channels=%s"), *File, *Channels);
	if (GEngine)
	{
		GEngine->Exec(nullptr, *Cmd);
	}

	ECAInsightsState::LastTraceFile = File;
	ECAInsightsState::LastChannels = Channels;
	ECAInsightsState::LastStartTime = FDateTime::Now();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("trace_file"), File);
	Result->SetStringField(TEXT("channels"), Channels);
	Result->SetStringField(TEXT("started_at"), ECAInsightsState::LastStartTime.ToIso8601());
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_StopInsightsTrace::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (GEngine)
	{
		GEngine->Exec(nullptr, TEXT("Trace.Stop"));
	}
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("trace_file"), ECAInsightsState::LastTraceFile);
	Result->SetStringField(TEXT("channels"), ECAInsightsState::LastChannels);
	if (ECAInsightsState::LastStartTime.GetTicks() != 0)
	{
		Result->SetStringField(TEXT("started_at"), ECAInsightsState::LastStartTime.ToIso8601());
		Result->SetNumberField(TEXT("duration_seconds"), (FDateTime::Now() - ECAInsightsState::LastStartTime).GetTotalSeconds());
	}
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_DumpInsightsSummary::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TraceFile;
	GetStringParam(Params, TEXT("trace_file"), TraceFile, false);
	if (TraceFile.IsEmpty())
	{
		TraceFile = ECAInsightsState::LastTraceFile;
	}
	if (TraceFile.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("No trace_file given and no recent trace recorded"));
	}

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*TraceFile))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Trace file does not exist: %s"), *TraceFile));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("trace_file"), TraceFile);
	Result->SetNumberField(TEXT("size_bytes"), (double)PF.FileSize(*TraceFile));
	Result->SetStringField(TEXT("modified_at"), PF.GetTimeStamp(*TraceFile).ToIso8601());
	if (TraceFile == ECAInsightsState::LastTraceFile)
	{
		Result->SetStringField(TEXT("channels"), ECAInsightsState::LastChannels);
		if (ECAInsightsState::LastStartTime.GetTicks() != 0)
		{
			Result->SetStringField(TEXT("started_at"), ECAInsightsState::LastStartTime.ToIso8601());
		}
	}
	Result->SetStringField(TEXT("note"), TEXT("Open in Unreal Insights (Engine/Binaries/Win64/UnrealInsights.exe) for full hot-function / frame-time analysis."));
	return FECACommandResult::Success(Result);
}
