// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgRuntimeCommands.h"
#include "Commands/ECACommand.h"

#include "PCGGraph.h"
#include "PCGComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"

REGISTER_ECA_COMMAND(FECACommand_RunPCGOnActor)
REGISTER_ECA_COMMAND(FECACommand_ClearPCGOutput)

namespace ECAPcgRuntimeHelpers
{
	static UPCGComponent* FindPCGComponent(AActor* Actor)
	{
		if (!Actor) return nullptr;
		return Actor->FindComponentByClass<UPCGComponent>();
	}

	static UPCGComponent* GetOrCreatePCGComponent(AActor* Actor, bool& bOutAdded)
	{
		bOutAdded = false;
		if (!Actor) return nullptr;
		if (UPCGComponent* Existing = Actor->FindComponentByClass<UPCGComponent>())
		{
			return Existing;
		}
		UPCGComponent* NewComp = NewObject<UPCGComponent>(Actor, UPCGComponent::StaticClass(), NAME_None, RF_Transactional);
		if (!NewComp) return nullptr;
		NewComp->RegisterComponent();
		Actor->AddInstanceComponent(NewComp);
		bOutAdded = true;
		return NewComp;
	}
}

//==============================================================================
// run_pcg_on_actor
//==============================================================================
FECACommandResult FECACommand_RunPCGOnActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	}
	FString GraphPath;
	GetStringParam(Params, TEXT("graph_path"), GraphPath, false);
	bool bForce = false;
	GetBoolParam(Params, TEXT("force"), bForce, false);

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	bool bAdded = false;
	UPCGComponent* PCG = ECAPcgRuntimeHelpers::GetOrCreatePCGComponent(Actor, bAdded);
	if (!PCG)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to attach or find PCGComponent on '%s'"), *ActorName));
	}

	FString AssignedGraphPath;
	if (!GraphPath.IsEmpty())
	{
		UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
		if (!Graph)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
		}
		PCG->SetGraph(Graph);
		AssignedGraphPath = Graph->GetPathName();
	}
	else if (UPCGGraphInterface* CurrentGI = PCG->GetGraphInstance())
	{
		AssignedGraphPath = CurrentGI->GetPathName();
	}

	// Drive generation. GenerateLocal is the editor-side path used by the
	// "Generate" right-click menu entry — it handles partitioned and
	// non-partitioned components alike.
	PCG->GenerateLocal(bForce);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());
	Out->SetStringField(TEXT("graph_path"), AssignedGraphPath);
	Out->SetBoolField(TEXT("component_added"), bAdded);
	return FECACommandResult::Success(Out);
}

//==============================================================================
// clear_pcg_output
//==============================================================================
FECACommandResult FECACommand_ClearPCGOutput::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("actor_name is required"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
	}

	UPCGComponent* PCG = ECAPcgRuntimeHelpers::FindPCGComponent(Actor);
	const bool bHadComponent = (PCG != nullptr);
	if (PCG)
	{
		// bRemoveComponents=true mirrors the editor right-click "Cleanup" — it
		// deletes spawned actors / ISMs but leaves the PCGComponent itself.
		PCG->CleanupLocalImmediate(/*bRemoveComponents=*/true);
	}

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());
	Out->SetBoolField(TEXT("had_component"), bHadComponent);
	return FECACommandResult::Success(Out);
}
