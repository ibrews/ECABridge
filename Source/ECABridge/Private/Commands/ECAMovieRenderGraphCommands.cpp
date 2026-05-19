// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMovieRenderGraphCommands.h"

#if WITH_ECA_MOVIE_RENDER_PIPELINE && !UE_VERSION_OLDER_THAN(5, 8, 0)

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphConfigFactory.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelinePIEExecutor.h"
#include "LevelSequence.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Editor.h"

REGISTER_ECA_COMMAND(FECACommand_CreateMrgGraph);
REGISTER_ECA_COMMAND(FECACommand_AddMrgNode);
REGISTER_ECA_COMMAND(FECACommand_ConnectMrgNodes);
REGISTER_ECA_COMMAND(FECACommand_ExecuteMrgGraph);

namespace MrgHelpers
{
	static UMovieGraphConfig* LoadGraphFlexible(const FString& Path)
	{
		UMovieGraphConfig* G = LoadObject<UMovieGraphConfig>(nullptr, *Path);
		if (G) return G;
		FString Full = Path;
		if (!Full.Contains(TEXT(".")))
		{
			const FString Asset = FPackageName::GetShortName(Full);
			Full = Full + TEXT(".") + Asset;
		}
		return LoadObject<UMovieGraphConfig>(nullptr, *Full);
	}

	static ULevelSequence* LoadSequenceFlexible(const FString& Path)
	{
		ULevelSequence* S = LoadObject<ULevelSequence>(nullptr, *Path);
		if (S) return S;
		FString Full = Path;
		if (!Full.Contains(TEXT(".")))
		{
			const FString Asset = FPackageName::GetShortName(Full);
			Full = Full + TEXT(".") + Asset;
		}
		return LoadObject<ULevelSequence>(nullptr, *Full);
	}

	static UClass* FindNodeClassByName(const FString& Name)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* C = *It;
			if (!C->IsChildOf(UMovieGraphNode::StaticClass())) continue;
			if (C->HasAnyClassFlags(CLASS_Abstract)) continue;
			if (C->GetName() == Name) return C;
			if (C->GetName() == TEXT("U") + Name) return C;
			if (C->GetName().EndsWith(Name)) return C;
		}
		return nullptr;
	}

	static UMovieGraphNode* FindNodeByName(UMovieGraphConfig* Graph, const FString& Name)
	{
		if (!Graph) return nullptr;
		if (Name == TEXT("Inputs"))  return Graph->GetInputNode();
		if (Name == TEXT("Outputs")) return Graph->GetOutputNode();
		for (UMovieGraphNode* N : Graph->GetNodes())
		{
			if (!N) continue;
			if (N->GetName() == Name) return N;
			if (N->GetClass()->GetName() == Name) return N;
			if (N->GetClass()->GetName().EndsWith(Name)) return N;
		}
		return nullptr;
	}
}

// ─── create_mrg_graph ────────────────────────────────────────

FECACommandResult FECACommand_CreateMrgGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: asset_path"));
	}

	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetShortName(AssetPath);

	FAssetToolsModule& Module = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = Module.Get();

	UMovieGraphConfigFactory* Factory = NewObject<UMovieGraphConfigFactory>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMovieGraphConfig::StaticClass(), Factory);
	if (!NewAsset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("AssetTools failed to create MovieGraphConfig at: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("class"), NewAsset->GetClass()->GetName());
	return FECACommandResult::Success(Result);
}

// ─── add_mrg_node ────────────────────────────────────────────

FECACommandResult FECACommand_AddMrgNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: graph_path"));
	}
	FString NodeClassName;
	if (!GetStringParam(Params, TEXT("node_class"), NodeClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: node_class"));
	}

	UMovieGraphConfig* Graph = MrgHelpers::LoadGraphFlexible(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("MovieGraph not found: %s"), *GraphPath));

	UClass* NodeClass = MrgHelpers::FindNodeClassByName(NodeClassName);
	if (!NodeClass) return FECACommandResult::Error(FString::Printf(TEXT("No UMovieGraphNode subclass matches: %s"), *NodeClassName));

	UMovieGraphNode* Node = Graph->CreateNodeByClass(NodeClass);
	if (!Node) return FECACommandResult::Error(TEXT("CreateNodeByClass returned null"));

	Graph->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Result->SetStringField(TEXT("node_class"), NodeClass->GetName());
	Result->SetStringField(TEXT("node_name"), Node->GetName());
	return FECACommandResult::Success(Result);
}

// ─── connect_mrg_nodes ──────────────────────────────────────

FECACommandResult FECACommand_ConnectMrgNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, FromName, ToName, FromPin, ToPin;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: graph_path"));
	}
	if (!GetStringParam(Params, TEXT("from_node"), FromName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: from_node"));
	}
	if (!GetStringParam(Params, TEXT("to_node"), ToName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: to_node"));
	}
	GetStringParam(Params, TEXT("from_pin"), FromPin, /*bRequired=*/false);
	GetStringParam(Params, TEXT("to_pin"), ToPin, /*bRequired=*/false);

	UMovieGraphConfig* Graph = MrgHelpers::LoadGraphFlexible(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("MovieGraph not found: %s"), *GraphPath));

	UMovieGraphNode* From = MrgHelpers::FindNodeByName(Graph, FromName);
	UMovieGraphNode* To   = MrgHelpers::FindNodeByName(Graph, ToName);
	if (!From) return FECACommandResult::Error(FString::Printf(TEXT("from_node not found: %s"), *FromName));
	if (!To)   return FECACommandResult::Error(FString::Printf(TEXT("to_node not found: %s"), *ToName));

	const bool bOk = Graph->AddLabeledEdge(From, FName(*FromPin), To, FName(*ToPin));
	if (!bOk)
	{
		return FECACommandResult::Error(TEXT("AddLabeledEdge returned false (pin not found or type mismatch)"));
	}

	Graph->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Result->SetStringField(TEXT("from_node"), From->GetName());
	Result->SetStringField(TEXT("from_pin"), FromPin);
	Result->SetStringField(TEXT("to_node"), To->GetName());
	Result->SetStringField(TEXT("to_pin"), ToPin);
	return FECACommandResult::Success(Result);
}

// ─── execute_mrg_graph ──────────────────────────────────────

FECACommandResult FECACommand_ExecuteMrgGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, SequencePath, OutputDir = TEXT("Saved/MovieRenders");
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: graph_path"));
	}
	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: sequence_path"));
	}
	GetStringParam(Params, TEXT("output_dir"), OutputDir, /*bRequired=*/false);

	UMovieGraphConfig* Graph = MrgHelpers::LoadGraphFlexible(GraphPath);
	if (!Graph) return FECACommandResult::Error(FString::Printf(TEXT("MovieGraph not found: %s"), *GraphPath));

	ULevelSequence* Sequence = MrgHelpers::LoadSequenceFlexible(SequencePath);
	if (!Sequence) return FECACommandResult::Error(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));

	UMoviePipelineQueueSubsystem* QueueSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
	if (!QueueSubsystem) return FECACommandResult::Error(TEXT("MoviePipelineQueueSubsystem unavailable"));

	UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
	if (!Queue) return FECACommandResult::Error(TEXT("MoviePipelineQueue unavailable"));

	if (QueueSubsystem->IsRendering())
	{
		return FECACommandResult::Error(TEXT("A render is already in progress"));
	}

	UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	if (!Job) return FECACommandResult::Error(TEXT("AllocateNewJob returned null"));

	UWorld* MapWorld = Sequence->GetTypedOuter<UWorld>();
	if (!MapWorld && GEditor)
	{
		MapWorld = GEditor->GetEditorWorldContext().World();
	}
	Job->Sequence = FSoftObjectPath(Sequence);
	if (MapWorld) Job->Map = FSoftObjectPath(MapWorld);
	Job->SetGraphPreset(Graph);

	QueueSubsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), Graph->GetPathName());
	Result->SetStringField(TEXT("sequence_path"), Sequence->GetPathName());
	Result->SetStringField(TEXT("output_dir"), OutputDir);
	Result->SetStringField(TEXT("status"), TEXT("render_started"));
	Result->SetStringField(TEXT("note"), TEXT("Output dir from MovieGraph nodes overrides this argument; use get_render_status to poll."));
	return FECACommandResult::Success(Result);
}

#endif // WITH_ECA_MOVIE_RENDER_PIPELINE && !UE_VERSION_OLDER_THAN(5, 8, 0)
