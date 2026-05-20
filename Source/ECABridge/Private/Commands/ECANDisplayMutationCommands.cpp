// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANDisplayMutationCommands.h"

#if WITH_ECA_NDISPLAY

#include "DisplayClusterConfigurationTypes.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	struct FTrackedNodeProcess
	{
		FString NodeId;
		FProcHandle Handle;
		uint32 Pid = 0;
	};

	FCriticalSection& ClusterMutex()
	{
		static FCriticalSection M;
		return M;
	}

	TArray<FTrackedNodeProcess>& ClusterProcesses()
	{
		static TArray<FTrackedNodeProcess> P;
		return P;
	}

	UDisplayClusterConfigurationData* LoadConfigByPath(const FString& Path)
	{
		FSoftObjectPath Soft(Path);
		if (!Soft.IsValid())
		{
			return nullptr;
		}
		return Cast<UDisplayClusterConfigurationData>(Soft.TryLoad());
	}

	FString QuoteIfNeeded(const FString& In)
	{
		if (In.IsEmpty() || In.StartsWith(TEXT("\""))) return In;
		return FString::Printf(TEXT("\"%s\""), *In);
	}
}

FECACommandResult FECACommand_LaunchNDisplayCluster::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ConfigPath;
	if (!GetStringParam(Params, TEXT("config_path"), ConfigPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("config_path is required"));
	}

	UDisplayClusterConfigurationData* Cfg = LoadConfigByPath(ConfigPath);
	if (!Cfg || !Cfg->Cluster)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load UDisplayClusterConfigurationData at '%s'."), *ConfigPath));
	}

	TSet<FString> NodeFilter;
	const TArray<TSharedPtr<FJsonValue>>* NodesIn = nullptr;
	if (Params.IsValid() && Params->TryGetArrayField(TEXT("nodes"), NodesIn) && NodesIn)
	{
		for (const TSharedPtr<FJsonValue>& V : *NodesIn)
		{
			if (V.IsValid()) NodeFilter.Add(V->AsString());
		}
	}

	FString Executable;
	GetStringParam(Params, TEXT("executable"), Executable, false);
	if (Executable.IsEmpty())
	{
		Executable = FPlatformProcess::ExecutablePath();
	}

	FString ProjectPath;
	GetStringParam(Params, TEXT("project"), ProjectPath, false);
	if (ProjectPath.IsEmpty())
	{
		ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	}

	FString Map;
	GetStringParam(Params, TEXT("map"), Map, false);

	FString ExtraArgs;
	GetStringParam(Params, TEXT("extra_args"), ExtraArgs, false);

	const FString AbsConfigPath = ConfigPath; // SoftObjectPath form is fine for -dc_cfg

	TArray<TSharedPtr<FJsonValue>> Launched;
	int32 Skipped = 0;
	{
		FScopeLock Lock(&ClusterMutex());
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Pair : Cfg->Cluster->Nodes)
		{
			const FString& NodeId = Pair.Key;
			if (NodeFilter.Num() > 0 && !NodeFilter.Contains(NodeId))
			{
				++Skipped;
				continue;
			}

			FString Args;
			Args += QuoteIfNeeded(ProjectPath);
			if (!Map.IsEmpty()) { Args += TEXT(" "); Args += QuoteIfNeeded(Map); }
			Args += TEXT(" -dc_cluster");
			Args += FString::Printf(TEXT(" -dc_cfg=%s"), *QuoteIfNeeded(AbsConfigPath));
			Args += FString::Printf(TEXT(" -dc_node=%s"), *NodeId);
			Args += TEXT(" -unattended -nopause");
			if (!ExtraArgs.IsEmpty()) { Args += TEXT(" "); Args += ExtraArgs; }

			uint32 Pid = 0;
			FProcHandle Handle = FPlatformProcess::CreateProc(
				*Executable,
				*Args,
				/*bLaunchDetached=*/true,
				/*bLaunchHidden=*/false,
				/*bLaunchReallyHidden=*/false,
				&Pid,
				/*PriorityModifier=*/0,
				/*OptionalWorkingDirectory=*/nullptr,
				/*PipeWriteChild=*/nullptr);

			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("node_id"), NodeId);

			if (Handle.IsValid())
			{
				Entry->SetNumberField(TEXT("pid"), static_cast<int64>(Pid));
				Entry->SetBoolField(TEXT("launched"), true);

				FTrackedNodeProcess T;
				T.NodeId = NodeId;
				T.Handle = Handle;
				T.Pid = Pid;
				ClusterProcesses().Add(T);
			}
			else
			{
				Entry->SetBoolField(TEXT("launched"), false);
				Entry->SetStringField(TEXT("error"), TEXT("CreateProc returned an invalid handle"));
			}

			Launched.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("config_path"), ConfigPath);
	Result->SetStringField(TEXT("executable"), Executable);
	Result->SetStringField(TEXT("project"), ProjectPath);
	Result->SetNumberField(TEXT("requested_count"), NodeFilter.Num() > 0 ? NodeFilter.Num() : Cfg->Cluster->Nodes.Num());
	Result->SetNumberField(TEXT("launched_count"), Launched.Num());
	Result->SetNumberField(TEXT("skipped_count"), Skipped);
	Result->SetArrayField(TEXT("nodes"), Launched);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_LaunchNDisplayCluster);

FECACommandResult FECACommand_StopNDisplayCluster::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> Terminated;
	int32 AlreadyDead = 0;
	{
		FScopeLock Lock(&ClusterMutex());
		for (FTrackedNodeProcess& T : ClusterProcesses())
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("node_id"), T.NodeId);
			Entry->SetNumberField(TEXT("pid"), static_cast<int64>(T.Pid));

			if (T.Handle.IsValid() && FPlatformProcess::IsProcRunning(T.Handle))
			{
				FPlatformProcess::TerminateProc(T.Handle, /*KillTree=*/true);
				Entry->SetBoolField(TEXT("terminated"), true);
			}
			else
			{
				Entry->SetBoolField(TEXT("terminated"), false);
				Entry->SetStringField(TEXT("note"), TEXT("process was not running"));
				++AlreadyDead;
			}

			if (T.Handle.IsValid())
			{
				FPlatformProcess::CloseProc(T.Handle);
			}

			Terminated.Add(MakeShared<FJsonValueObject>(Entry));
		}
		ClusterProcesses().Reset();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Terminated.Num());
	Result->SetNumberField(TEXT("already_dead"), AlreadyDead);
	Result->SetArrayField(TEXT("nodes"), Terminated);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_StopNDisplayCluster);

#endif // WITH_ECA_NDISPLAY
