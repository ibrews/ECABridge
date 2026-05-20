// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANodeNameRegistry.h"

FECANodeNameRegistry& FECANodeNameRegistry::Get()
{
	static FECANodeNameRegistry Instance;
	return Instance;
}

bool FECANodeNameRegistry::Register(
	const FString& Name,
	const FString& BlueprintPath,
	const FString& GraphName,
	const FGuid& NodeGuid)
{
	if (Name.IsEmpty())
	{
		return false;
	}

	FEntry Entry;
	Entry.Name = Name;
	Entry.BlueprintPath = BlueprintPath;
	Entry.GraphName = GraphName;
	Entry.NodeGuid = NodeGuid;

	FScopeLock ScopeLock(&Lock);
	Bindings.Add(Name, MoveTemp(Entry));
	return true;
}

bool FECANodeNameRegistry::Resolve(
	const FString& Name,
	FString& OutBlueprintPath,
	FString& OutGraphName,
	FGuid& OutNodeGuid) const
{
	if (Name.IsEmpty())
	{
		return false;
	}

	FScopeLock ScopeLock(&Lock);
	const FEntry* Found = Bindings.Find(Name);
	if (!Found || !Found->NodeGuid.IsValid())
	{
		return false;
	}

	OutBlueprintPath = Found->BlueprintPath;
	OutGraphName = Found->GraphName;
	OutNodeGuid = Found->NodeGuid;
	return true;
}

bool FECANodeNameRegistry::Unregister(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return false;
	}

	FScopeLock ScopeLock(&Lock);
	return Bindings.Remove(Name) > 0;
}

void FECANodeNameRegistry::Clear()
{
	FScopeLock ScopeLock(&Lock);
	Bindings.Empty();
}

TArray<FECANodeNameRegistry::FEntry> FECANodeNameRegistry::Snapshot() const
{
	FScopeLock ScopeLock(&Lock);
	TArray<FEntry> Out;
	Out.Reserve(Bindings.Num());
	for (const TPair<FString, FEntry>& Pair : Bindings)
	{
		Out.Add(Pair.Value);
	}
	return Out;
}
