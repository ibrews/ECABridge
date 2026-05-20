// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"

/**
 * Internal helpers shared between ECAPhysicsCommands.cpp (dump + physical
 * material) and ECAPhysicsAssetCommands.cpp (physics-asset authoring). Header
 * only; templates require visible definitions at call sites.
 */
namespace ECAPhysicsHelpers
{
	template<typename T>
	static T* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		T* Obj = LoadObject<T>(nullptr, *Path);
		if (Obj) return Obj;

		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Obj = LoadObject<T>(nullptr, *FullPath);
		}
		return Obj;
	}

	template<typename TEnum>
	static FString EnumToName(int64 Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(Value);
		}
		return FString::Printf(TEXT("%lld"), Value);
	}
}
