// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ECAListItemData.generated.h"

/**
 * Simple UObject wrapper for list item data.
 * UListView requires UObject* items - this wraps arbitrary JSON data into a UObject
 * that can be passed to the list and retrieved by entry widgets via IUserObjectListEntry.
 */
UCLASS(Transient, NotBlueprintable, MinimalAPI)
class UECAListItemData : public UObject
{
	GENERATED_BODY()

public:
	/** JSON string containing the item's data payload */
	UPROPERTY()
	FString ItemDataJson;

	/** Index of this item in the list */
	UPROPERTY()
	int32 ItemIndex = 0;
};
