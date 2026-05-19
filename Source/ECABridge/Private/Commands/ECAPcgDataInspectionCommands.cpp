// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgDataInspectionCommands.h"
#include "Commands/ECACommand.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"
#include "Data/PCGPointData.h"

#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_DumpPCGData)

//==============================================================================
// dump_pcg_data
//==============================================================================
FECACommandResult FECACommand_DumpPCGData::Execute(const TSharedPtr<FJsonObject>& Params)
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

	UPCGComponent* PCG = Actor->FindComponentByClass<UPCGComponent>();
	if (!PCG)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' has no PCGComponent"), *ActorName));
	}

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("actor_name"), Actor->GetActorNameOrLabel());

	FString GraphPath;
	if (UPCGGraphInterface* GI = PCG->GetGraphInstance())
	{
		GraphPath = GI->GetPathName();
	}
	Out->SetStringField(TEXT("graph_path"), GraphPath);

	const FPCGDataCollection& Collection = PCG->GetGeneratedGraphOutput();
	const TArray<FPCGTaggedData>& Entries = Collection.GetAllInputs();

	TArray<TSharedPtr<FJsonValue>> DataArray;
	for (const FPCGTaggedData& Entry : Entries)
	{
		const UPCGData* Data = Entry.Data.Get();
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("pin"), Entry.Pin.ToString());
		Obj->SetStringField(TEXT("class"), Data ? Data->GetClass()->GetName() : TEXT("None"));

		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FString& Tag : Entry.Tags)
		{
			Tags.Add(MakeShared<FJsonValueString>(Tag));
		}
		Obj->SetArrayField(TEXT("tags"), Tags);

		// Point-data summary: count is the canonical "did the graph produce N points?" answer
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(Data))
		{
			Obj->SetNumberField(TEXT("point_count"), PointData->GetPoints().Num());
		}

		DataArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Out->SetArrayField(TEXT("data"), DataArray);
	Out->SetNumberField(TEXT("data_count"), DataArray.Num());
	return FECACommandResult::Success(Out);
}
