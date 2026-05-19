// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECADDCCommands.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "Engine/Engine.h"
#include "Misc/OutputDeviceNull.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

FECACommandResult FECACommand_GetDDCStats::Execute(const TSharedPtr<FJsonObject>& /*Params*/)
{
	FDerivedDataCacheInterface* DDC = GetDerivedDataCache();
	if (!DDC)
	{
		return FECACommandResult::Error(TEXT("DerivedDataCache subsystem is not available."));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("graph_name"), DDC->GetGraphName());
	Result->SetStringField(TEXT("default_graph_name"), DDC->GetDefaultGraphName());
	Result->SetBoolField(TEXT("using_shared_ddc"), DDC->GetUsingSharedDDC());
	Result->SetBoolField(TEXT("async_pending"), DDC->AnyAsyncRequestsRemaining());

	TArray<FDerivedDataCacheResourceStat> Stats;
	DDC->GatherResourceStats(Stats);

	int64 TotalLoad = 0;
	int64 TotalBuild = 0;
	double TotalLoadMB = 0.0;
	double TotalBuildMB = 0.0;

	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FDerivedDataCacheResourceStat& Stat : Stats)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("asset_type"), Stat.AssetType);
		Row->SetNumberField(TEXT("load_count"), static_cast<double>(Stat.LoadCount));
		Row->SetNumberField(TEXT("build_count"), static_cast<double>(Stat.BuildCount));
		Row->SetNumberField(TEXT("total_count"), static_cast<double>(Stat.TotalCount));
		Row->SetNumberField(TEXT("load_time_sec"), Stat.LoadTimeSec);
		Row->SetNumberField(TEXT("build_time_sec"), Stat.BuildTimeSec);
		Row->SetNumberField(TEXT("load_size_mb"), Stat.LoadSizeMB);
		Row->SetNumberField(TEXT("build_size_mb"), Stat.BuildSizeMB);
		Row->SetNumberField(TEXT("efficiency"), Stat.Efficiency);
		Row->SetNumberField(TEXT("game_thread_time_sec"), Stat.GameThreadTimeSec);
		Rows.Add(MakeShared<FJsonValueObject>(Row));

		TotalLoad += Stat.LoadCount;
		TotalBuild += Stat.BuildCount;
		TotalLoadMB += Stat.LoadSizeMB;
		TotalBuildMB += Stat.BuildSizeMB;
	}
	Result->SetArrayField(TEXT("resource_stats"), Rows);
	Result->SetNumberField(TEXT("total_load_count"), static_cast<double>(TotalLoad));
	Result->SetNumberField(TEXT("total_build_count"), static_cast<double>(TotalBuild));
	Result->SetNumberField(TEXT("total_load_size_mb"), TotalLoadMB);
	Result->SetNumberField(TEXT("total_build_size_mb"), TotalBuildMB);

	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_PurgeDDC::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Command;
	GetStringParam(Params, TEXT("custom_command"), Command, /*bRequired=*/ false);
	if (Command.IsEmpty())
	{
		Command = TEXT("DerivedDataCache.Cleanup");
	}

	FOutputDeviceNull Sink;
	const bool bOk = GEngine ? GEngine->Exec(nullptr, *Command, Sink) : false;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("command"), Command);
	Result->SetBoolField(TEXT("executed"), bOk);
	if (!bOk)
	{
		Result->SetStringField(TEXT("message"), TEXT("Console command did not execute (unknown command, or GEngine unavailable). Verify the DDC console command name."));
	}
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_WarmDDC::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr;
	if (!GetArrayParam(Params, TEXT("asset_list"), PathsArr, /*bRequired=*/ true) || !PathsArr)
	{
		return FECACommandResult::ValidationError(this, TEXT("'asset_list' (array of strings) is required."));
	}

	FDerivedDataCacheInterface* DDC = GetDerivedDataCache();

	TArray<TSharedPtr<FJsonValue>> Loaded;
	TArray<TSharedPtr<FJsonValue>> Failed;
	for (const TSharedPtr<FJsonValue>& V : *PathsArr)
	{
		if (!V.IsValid() || V->Type != EJson::String) continue;
		const FString Path = V->AsString();
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn);
		if (Obj)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("path"), Path);
			Entry->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
			Loaded.Add(MakeShared<FJsonValueObject>(Entry));
		}
		else
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("path"), Path);
			Entry->SetStringField(TEXT("reason"), TEXT("failed_to_load"));
			Failed.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("graph_name"), DDC ? DDC->GetGraphName() : TEXT(""));
	Result->SetNumberField(TEXT("loaded_count"), Loaded.Num());
	Result->SetNumberField(TEXT("failed_count"), Failed.Num());
	Result->SetArrayField(TEXT("loaded"), Loaded);
	Result->SetArrayField(TEXT("failed"), Failed);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_GetDDCStats);
REGISTER_ECA_COMMAND(FECACommand_PurgeDDC);
REGISTER_ECA_COMMAND(FECACommand_WarmDDC);
