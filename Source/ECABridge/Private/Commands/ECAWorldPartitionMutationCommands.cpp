// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWorldPartitionMutationCommands.h"
#include "Commands/ECACommand.h"

#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"

REGISTER_ECA_COMMAND(FECACommand_ForceLoadWPRegion)
REGISTER_ECA_COMMAND(FECACommand_PinWPActors)
REGISTER_ECA_COMMAND(FECACommand_UnpinWPActors)

namespace ECAWPMutationHelpers
{
	static bool ReadVector(const TSharedPtr<FJsonObject>& Obj, FVector& Out)
	{
		if (!Obj.IsValid()) return false;
		double X = 0.0, Y = 0.0, Z = 0.0;
		if (!Obj->TryGetNumberField(TEXT("x"), X)) return false;
		if (!Obj->TryGetNumberField(TEXT("y"), Y)) return false;
		if (!Obj->TryGetNumberField(TEXT("z"), Z)) return false;
		Out = FVector(X, Y, Z);
		return true;
	}

	static TSharedPtr<FJsonObject> VectorToJson(const FVector& V)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("x"), V.X);
		Obj->SetNumberField(TEXT("y"), V.Y);
		Obj->SetNumberField(TEXT("z"), V.Z);
		return Obj;
	}
}

//==============================================================================
// force_load_wp_region
//==============================================================================
FECACommandResult FECACommand_ForceLoadWPRegion::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP)
	{
		return FECACommandResult::Error(TEXT("Current world is not partitioned (no UWorldPartition)"));
	}

	const TSharedPtr<FJsonObject>* MinObj = nullptr;
	const TSharedPtr<FJsonObject>* MaxObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("min"), MinObj) || !MinObj->IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: min {x,y,z}"));
	}
	if (!Params->TryGetObjectField(TEXT("max"), MaxObj) || !MaxObj->IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: max {x,y,z}"));
	}

	FVector MinV, MaxV;
	if (!ECAWPMutationHelpers::ReadVector(*MinObj, MinV))
	{
		return FECACommandResult::ValidationError(this, TEXT("min must be an object with numeric x,y,z fields"));
	}
	if (!ECAWPMutationHelpers::ReadVector(*MaxObj, MaxV))
	{
		return FECACommandResult::ValidationError(this, TEXT("max must be an object with numeric x,y,z fields"));
	}

	const FBox Box(MinV, MaxV);
	if (!Box.IsValid)
	{
		return FECACommandResult::Error(TEXT("min/max produce an invalid box (min must be <= max on each axis)"));
	}

	TArray<FBox> Regions;
	Regions.Add(Box);
	WP->LoadLastLoadedRegions(Regions);

	int32 Intersecting = 0;
	if (WP->RuntimeHash)
	{
		WP->RuntimeHash->ForEachStreamingCells([&Intersecting, &Box](const UWorldPartitionRuntimeCell* Cell) -> bool
		{
			if (Cell && Cell->GetCellBounds().Intersect(Box))
			{
				++Intersecting;
			}
			return true;
		});
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("world"), World->GetPathName());
	Result->SetBoolField(TEXT("region_registered"), true);

	TSharedPtr<FJsonObject> BoxJson = MakeShared<FJsonObject>();
	BoxJson->SetObjectField(TEXT("min"), ECAWPMutationHelpers::VectorToJson(MinV));
	BoxJson->SetObjectField(TEXT("max"), ECAWPMutationHelpers::VectorToJson(MaxV));
	Result->SetObjectField(TEXT("box"), BoxJson);

	Result->SetNumberField(TEXT("cells_intersecting"), Intersecting);
	return FECACommandResult::Success(Result);
#else
	return FECACommandResult::Error(TEXT("force_load_wp_region requires WITH_EDITOR"));
#endif
}

#if WITH_EDITOR
namespace ECAWPMutationHelpers
{
	struct FParsedGuids
	{
		TArray<FGuid> Valid;
		TArray<TSharedPtr<FJsonValue>> Invalid;
	};

	static FParsedGuids ParseGuidArray(const TSharedPtr<FJsonObject>& Params)
	{
		FParsedGuids Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Params->TryGetArrayField(TEXT("actor_guids"), Arr) || !Arr)
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			FString S;
			if (V.IsValid() && V->TryGetString(S))
			{
				FGuid G;
				if (FGuid::Parse(S, G))
				{
					Out.Valid.Add(G);
				}
				else
				{
					Out.Invalid.Add(MakeShared<FJsonValueString>(S));
				}
			}
		}
		return Out;
	}
}
#endif

//==============================================================================
// pin_wp_actors
//==============================================================================
FECACommandResult FECACommand_PinWPActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}
	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP)
	{
		return FECACommandResult::Error(TEXT("Current world is not partitioned (no UWorldPartition)"));
	}

	const TArray<TSharedPtr<FJsonValue>>* RawArr = nullptr;
	if (!Params->TryGetArrayField(TEXT("actor_guids"), RawArr) || !RawArr)
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: actor_guids (array of strings)"));
	}
	const int32 Requested = RawArr->Num();

	ECAWPMutationHelpers::FParsedGuids Parsed = ECAWPMutationHelpers::ParseGuidArray(Params);
	if (Parsed.Valid.Num() > 0)
	{
		WP->PinActors(Parsed.Valid);
	}

	int32 PinnedNow = 0;
	for (const FGuid& G : Parsed.Valid)
	{
		if (WP->IsActorPinned(G)) ++PinnedNow;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("requested"),  Requested);
	Result->SetNumberField(TEXT("parsed"),     Parsed.Valid.Num());
	Result->SetNumberField(TEXT("pinned_now"), PinnedNow);
	Result->SetArrayField (TEXT("invalid"),    Parsed.Invalid);
	return FECACommandResult::Success(Result);
#else
	return FECACommandResult::Error(TEXT("pin_wp_actors requires WITH_EDITOR"));
#endif
}

//==============================================================================
// unpin_wp_actors
//==============================================================================
FECACommandResult FECACommand_UnpinWPActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}
	UWorldPartition* WP = World->GetWorldPartition();
	if (!WP)
	{
		return FECACommandResult::Error(TEXT("Current world is not partitioned (no UWorldPartition)"));
	}

	const TArray<TSharedPtr<FJsonValue>>* RawArr = nullptr;
	if (!Params->TryGetArrayField(TEXT("actor_guids"), RawArr) || !RawArr)
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: actor_guids (array of strings)"));
	}
	const int32 Requested = RawArr->Num();

	ECAWPMutationHelpers::FParsedGuids Parsed = ECAWPMutationHelpers::ParseGuidArray(Params);
	if (Parsed.Valid.Num() > 0)
	{
		WP->UnpinActors(Parsed.Valid);
	}

	int32 UnpinnedNow = 0;
	for (const FGuid& G : Parsed.Valid)
	{
		if (!WP->IsActorPinned(G)) ++UnpinnedNow;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("requested"),    Requested);
	Result->SetNumberField(TEXT("parsed"),       Parsed.Valid.Num());
	Result->SetNumberField(TEXT("unpinned_now"), UnpinnedNow);
	Result->SetArrayField (TEXT("invalid"),      Parsed.Invalid);
	return FECACommandResult::Success(Result);
#else
	return FECACommandResult::Error(TEXT("unpin_wp_actors requires WITH_EDITOR"));
#endif
}
