// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "Components/Widget.h"

namespace ECAUMGVerbs
{
	// Tolerant asset loader: accepts either "/Game/UI/W_Test" or "/Game/UI/W_Test.W_Test".
	// Tries the literal path first, then synthesizes the dotted form, then falls back to FindObject.
	template <typename T>
	static T* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;

		if (T* Direct = LoadObject<T>(nullptr, *Path))
		{
			return Direct;
		}

		// If no dot, try appending the leaf name as the object name within the package.
		int32 DotIdx;
		if (!Path.FindChar(TEXT('.'), DotIdx))
		{
			int32 SlashIdx;
			if (Path.FindLastChar(TEXT('/'), SlashIdx))
			{
				const FString Leaf = Path.RightChop(SlashIdx + 1);
				const FString Dotted = Path + TEXT(".") + Leaf;
				if (T* Dot = LoadObject<T>(nullptr, *Dotted))
				{
					return Dot;
				}
			}
		}

		return FindObject<T>(nullptr, *Path);
	}

	// Resolve a UWidget subclass from either an unqualified UMG name ("Button"),
	// a slashed engine path ("/Script/UMG.Button"), or a fully-qualified class path
	// for a user widget ("/Game/UI/WBP_Foo.WBP_Foo_C").
	static UClass* ResolveWidgetClass(const FString& InClassPath)
	{
		if (InClassPath.IsEmpty()) return nullptr;

		UClass* Found = nullptr;
		if (!InClassPath.Contains(TEXT(".")) && !InClassPath.Contains(TEXT("/")))
		{
			Found = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *InClassPath));
		}
		if (!Found)
		{
			Found = FindObject<UClass>(nullptr, *InClassPath);
		}
		if (!Found)
		{
			Found = LoadObject<UClass>(nullptr, *InClassPath);
		}
		return Found;
	}
}
