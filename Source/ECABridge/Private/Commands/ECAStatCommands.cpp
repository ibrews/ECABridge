// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAStatCommands.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectIterator.h"

REGISTER_ECA_COMMAND(FECACommand_EnableStatGroup)
REGISTER_ECA_COMMAND(FECACommand_DisableStatGroup)
REGISTER_ECA_COMMAND(FECACommand_DumpStatValues)

static void ExecStatCommand(const FString& Cmd)
{
	if (!GEngine)
	{
		return;
	}
	UWorld* World = nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (Ctx.World())
		{
			World = Ctx.World();
			break;
		}
	}
	GEngine->Exec(World, *Cmd);
}

FECACommandResult FECACommand_EnableStatGroup::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	ExecStatCommand(FString::Printf(TEXT("stat %s"), *Name));
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("group"), Name);
	Result->SetStringField(TEXT("state"), TEXT("toggled"));
	Result->SetStringField(TEXT("note"), TEXT("'stat <name>' is a toggle. If the group was already on, this turned it off; call again to flip back."));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_DisableStatGroup::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	const FString Lower = Name.ToLower();
	if (Lower == TEXT("all") || Lower == TEXT("none"))
	{
		ExecStatCommand(TEXT("stat none"));
	}
	else
	{
		ExecStatCommand(FString::Printf(TEXT("stat %s"), *Name));
	}
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("group"), Name);
	Result->SetStringField(TEXT("state"), TEXT("toggled-off-or-cleared"));
	return FECACommandResult::Success(Result);
}

extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

TSharedPtr<FJsonObject> FECACommand_DumpStatValues::GetOutputSchema() const
{
	return MakeECAObjectSchema({
		{ TEXT("fps_average"), TEXT("number"), TEXT("Engine moving average FPS (GAverageFPS)") },
		{ TEXT("ms_average"), TEXT("number"), TEXT("Engine moving average frame time in milliseconds (GAverageMS)") },
		{ TEXT("delta_time_seconds"), TEXT("number"), TEXT("Last frame delta time from FApp::GetDeltaTime()") },
		{ TEXT("used_physical_mb"), TEXT("number"), TEXT("Process used physical memory in MiB") },
		{ TEXT("peak_used_physical_mb"), TEXT("number"), TEXT("Process peak used physical memory in MiB") },
		{ TEXT("uobject_count"), TEXT("integer"), TEXT("Live UObject count from FUObjectArray") }
	});
}

FECACommandResult FECACommand_DumpStatValues::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();

	int32 ObjectCount = 0;
	for (FThreadSafeObjectIterator It; It; ++It)
	{
		++ObjectCount;
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("fps_average"), GAverageFPS);
	Result->SetNumberField(TEXT("ms_average"), GAverageMS);
	Result->SetNumberField(TEXT("delta_time_seconds"), FApp::GetDeltaTime());
	Result->SetNumberField(TEXT("used_physical_mb"), (double)Mem.UsedPhysical / (1024.0 * 1024.0));
	Result->SetNumberField(TEXT("peak_used_physical_mb"), (double)Mem.PeakUsedPhysical / (1024.0 * 1024.0));
	Result->SetNumberField(TEXT("uobject_count"), ObjectCount);
	Result->SetStringField(TEXT("note"), TEXT("For per-group detail (stat scenerendering, stat memory, stat gpu...) call enable_stat_group then read editor log."));
	return FECACommandResult::Success(Result);
}
