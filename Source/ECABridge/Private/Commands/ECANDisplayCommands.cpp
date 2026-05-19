// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANDisplayCommands.h"

#if WITH_ECA_NDISPLAY

#include "IDisplayCluster.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "DisplayClusterEnums.h"
#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Editor.h"

namespace
{
	const TCHAR* OperationModeToString(EDisplayClusterOperationMode Mode)
	{
		switch (Mode)
		{
			case EDisplayClusterOperationMode::Cluster:   return TEXT("cluster");
			case EDisplayClusterOperationMode::Editor:    return TEXT("editor");
			case EDisplayClusterOperationMode::Disabled:  return TEXT("disabled");
			default:                                      return TEXT("unknown");
		}
	}

	const TCHAR* ClusterRoleToString(EDisplayClusterNodeRole Role)
	{
		switch (Role)
		{
			case EDisplayClusterNodeRole::Primary:   return TEXT("primary");
			case EDisplayClusterNodeRole::Secondary: return TEXT("secondary");
			case EDisplayClusterNodeRole::Backup:    return TEXT("backup");
			case EDisplayClusterNodeRole::None:      default: return TEXT("none");
		}
	}
}

FECACommandResult FECACommand_GetNDisplayStatus::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!IDisplayCluster::IsAvailable())
	{
		Result->SetBoolField(TEXT("module_loaded"), false);
		Result->SetStringField(TEXT("message"), TEXT("DisplayCluster module is not loaded. Enable the nDisplay plugin."));
		return FECACommandResult::Success(Result);
	}

	IDisplayCluster& DC = IDisplayCluster::Get();
	const EDisplayClusterOperationMode Mode = DC.GetOperationMode();

	Result->SetBoolField(TEXT("module_loaded"), true);
	Result->SetStringField(TEXT("operation_mode"), OperationModeToString(Mode));

	IDisplayClusterClusterManager* ClusterMgr = DC.GetClusterMgr();
	if (ClusterMgr && Mode == EDisplayClusterOperationMode::Cluster)
	{
		Result->SetStringField(TEXT("node_id"), ClusterMgr->GetNodeId());
		Result->SetStringField(TEXT("primary_node_id"), ClusterMgr->GetPrimaryNodeId());
		Result->SetStringField(TEXT("cluster_role"), ClusterRoleToString(ClusterMgr->GetClusterRole()));
		Result->SetNumberField(TEXT("node_count"), static_cast<int32>(ClusterMgr->GetNodesAmount()));

		TArray<FString> NodeIds;
		ClusterMgr->GetNodeIds(NodeIds);
		TArray<TSharedPtr<FJsonValue>> NodeArr;
		for (const FString& Id : NodeIds) { NodeArr.Add(MakeShared<FJsonValueString>(Id)); }
		Result->SetArrayField(TEXT("node_ids"), NodeArr);
	}
	else
	{
		Result->SetStringField(TEXT("cluster_role"), TEXT("none"));
		Result->SetNumberField(TEXT("node_count"), 0);
	}

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_GetNDisplayStatus);

FECACommandResult FECACommand_ListNDisplayRootActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	UWorld* World = nullptr;
	if (GEditor)
	{
		FWorldContext& Ctx = GEditor->GetEditorWorldContext();
		World = Ctx.World();
	}

	if (!World)
	{
		Result->SetStringField(TEXT("message"), TEXT("No editor world is currently loaded."));
		Result->SetArrayField(TEXT("actors"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	TArray<TSharedPtr<FJsonValue>> ActorArr;
	for (TActorIterator<ADisplayClusterRootActor> It(World); It; ++It)
	{
		ADisplayClusterRootActor* RootActor = *It;
		if (!RootActor) continue;

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), RootActor->GetName());
		Entry->SetStringField(TEXT("label"), RootActor->GetActorNameOrLabel());
		Entry->SetStringField(TEXT("path"), RootActor->GetPathName());

		FString ConfigPath;
		if (UDisplayClusterConfigurationData* Cfg = RootActor->GetConfigData())
		{
			ConfigPath = Cfg->GetPathName();
		}
		Entry->SetStringField(TEXT("config_asset"), ConfigPath);

		ActorArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Result->SetNumberField(TEXT("count"), ActorArr.Num());
	Result->SetArrayField(TEXT("actors"), ActorArr);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListNDisplayRootActors);

#endif // WITH_ECA_NDISPLAY
