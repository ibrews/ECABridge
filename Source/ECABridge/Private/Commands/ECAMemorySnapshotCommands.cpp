// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMemorySnapshotCommands.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "Misc/DateTime.h"

REGISTER_ECA_COMMAND(FECACommand_SnapshotMemory)
REGISTER_ECA_COMMAND(FECACommand_DiffMemorySnapshots)

namespace ECAMemorySnapshotStore
{
	struct FSnapshot
	{
		FDateTime CapturedAt;
		double UsedPhysicalMB = 0.0;
		double PeakUsedPhysicalMB = 0.0;
		double UsedVirtualMB = 0.0;
		double PeakUsedVirtualMB = 0.0;
		double AvailablePhysicalMB = 0.0;
		int32 UObjectCount = 0;
		TMap<FString, int32> TopClasses; // class name -> instance count
	};

	static TMap<FString, FSnapshot> Snapshots;

	static FSnapshot Capture()
	{
		FSnapshot S;
		S.CapturedAt = FDateTime::Now();
		const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
		const double MB = 1024.0 * 1024.0;
		S.UsedPhysicalMB = (double)Mem.UsedPhysical / MB;
		S.PeakUsedPhysicalMB = (double)Mem.PeakUsedPhysical / MB;
		S.UsedVirtualMB = (double)Mem.UsedVirtual / MB;
		S.PeakUsedVirtualMB = (double)Mem.PeakUsedVirtual / MB;
		S.AvailablePhysicalMB = (double)Mem.AvailablePhysical / MB;

		TMap<UClass*, int32> ByClass;
		for (FThreadSafeObjectIterator It; It; ++It)
		{
			++S.UObjectCount;
			if (UClass* C = It->GetClass())
			{
				ByClass.FindOrAdd(C, 0)++;
			}
		}
		// Keep only top 20 by count.
		ByClass.ValueSort([](int32 A, int32 B) { return A > B; });
		int32 Kept = 0;
		for (const TPair<UClass*, int32>& P : ByClass)
		{
			if (Kept++ >= 20) break;
			S.TopClasses.Add(P.Key->GetName(), P.Value);
		}
		return S;
	}
}

static void SnapshotToJson(const ECAMemorySnapshotStore::FSnapshot& S, TSharedPtr<FJsonObject>& Obj)
{
	Obj->SetStringField(TEXT("captured_at"), S.CapturedAt.ToIso8601());
	Obj->SetNumberField(TEXT("used_physical_mb"), S.UsedPhysicalMB);
	Obj->SetNumberField(TEXT("peak_used_physical_mb"), S.PeakUsedPhysicalMB);
	Obj->SetNumberField(TEXT("used_virtual_mb"), S.UsedVirtualMB);
	Obj->SetNumberField(TEXT("peak_used_virtual_mb"), S.PeakUsedVirtualMB);
	Obj->SetNumberField(TEXT("available_physical_mb"), S.AvailablePhysicalMB);
	Obj->SetNumberField(TEXT("uobject_count"), S.UObjectCount);

	TSharedPtr<FJsonObject> TopClassesObj = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& P : S.TopClasses)
	{
		TopClassesObj->SetNumberField(P.Key, P.Value);
	}
	Obj->SetObjectField(TEXT("top_classes"), TopClassesObj);
}

FECACommandResult FECACommand_SnapshotMemory::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Label;
	if (!GetStringParam(Params, TEXT("label"), Label))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: label"));
	}
	const ECAMemorySnapshotStore::FSnapshot S = ECAMemorySnapshotStore::Capture();
	ECAMemorySnapshotStore::Snapshots.Add(Label, S);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("label"), Label);
	SnapshotToJson(S, Result);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_DiffMemorySnapshots::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString LabelA, LabelB;
	if (!GetStringParam(Params, TEXT("label_a"), LabelA))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: label_a"));
	}
	if (!GetStringParam(Params, TEXT("label_b"), LabelB))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: label_b"));
	}
	const ECAMemorySnapshotStore::FSnapshot* A = ECAMemorySnapshotStore::Snapshots.Find(LabelA);
	const ECAMemorySnapshotStore::FSnapshot* B = ECAMemorySnapshotStore::Snapshots.Find(LabelB);
	if (!A)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("No snapshot labeled '%s'. Call snapshot_memory first."), *LabelA));
	}
	if (!B)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("No snapshot labeled '%s'. Call snapshot_memory first."), *LabelB));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("label_a"), LabelA);
	Result->SetStringField(TEXT("label_b"), LabelB);
	Result->SetNumberField(TEXT("seconds_between"), (B->CapturedAt - A->CapturedAt).GetTotalSeconds());
	Result->SetNumberField(TEXT("used_physical_mb_delta"), B->UsedPhysicalMB - A->UsedPhysicalMB);
	Result->SetNumberField(TEXT("used_virtual_mb_delta"), B->UsedVirtualMB - A->UsedVirtualMB);
	Result->SetNumberField(TEXT("uobject_count_delta"), B->UObjectCount - A->UObjectCount);

	// Per-class delta: union of keys, sorted by absolute delta desc.
	TMap<FString, int32> Delta;
	for (const TPair<FString, int32>& P : A->TopClasses) { Delta.Add(P.Key, -P.Value); }
	for (const TPair<FString, int32>& P : B->TopClasses) { Delta.FindOrAdd(P.Key, 0) += P.Value; }
	Delta.ValueSort([](int32 X, int32 Y) { return FMath::Abs(X) > FMath::Abs(Y); });

	TArray<TSharedPtr<FJsonValue>> Suspects;
	int32 Kept = 0;
	for (const TPair<FString, int32>& P : Delta)
	{
		if (P.Value == 0) continue;
		if (Kept++ >= 10) break;
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("class"), P.Key);
		Row->SetNumberField(TEXT("instance_count_delta"), P.Value);
		Suspects.Add(MakeShared<FJsonValueObject>(Row));
	}
	Result->SetArrayField(TEXT("leak_suspects"), Suspects);
	return FECACommandResult::Success(Result);
}
