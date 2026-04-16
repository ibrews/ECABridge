// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * ECA Event Queue Helper Functions
 * 
 * Use these functions to queue events that will be polled by ECA.
 * Events appear in the AI conversation and can optionally trigger AI responses.
 * 
 * Example usage:
 *   #include "ECAEventQueue.h"
 *   
 *   // When user selects something in the editor:
 *   ECAEvents::QueueSelectionChanged("BP_PlayerCharacter", "/Game/Blueprints/BP_PlayerCharacter");
 *   
 *   // When an asset is imported:
 *   ECAEvents::QueueAssetImported("/Game/Meshes/SM_Chair", "StaticMesh");
 *   
 *   // For custom events:
 *   TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
 *   Data->SetStringField("my_field", "my_value");
 *   ECAEvents::QueueCustomEvent("my_event_type", "Something happened!", Data, true);
 */
namespace ECAEvents
{
	/**
	 * Queue a selection changed event.
	 * Triggers AI response by default (user explicitly selected something).
	 * 
	 * @param ActorName  Display name of the selected actor
	 * @param ActorPath  Full path to the actor
	 */
	ECABRIDGE_API void QueueSelectionChanged(const FString& ActorName, const FString& ActorPath);

	/**
	 * Queue an asset imported event.
	 * Does NOT trigger AI response by default (informational).
	 * 
	 * @param AssetPath  Path to the imported asset
	 * @param AssetType  Type of asset (e.g., "StaticMesh", "Texture2D")
	 */
	ECABRIDGE_API void QueueAssetImported(const FString& AssetPath, const FString& AssetType);

	/**
	 * Queue an asset saved event.
	 * Does NOT trigger AI response by default (informational).
	 * 
	 * @param AssetPath  Path to the saved asset
	 */
	ECABRIDGE_API void QueueAssetSaved(const FString& AssetPath);

	/**
	 * Queue a Blueprint compilation event.
	 * Triggers AI response only on failure (to help debug).
	 * 
	 * @param BlueprintPath  Path to the Blueprint
	 * @param bSuccess       Whether compilation succeeded
	 * @param ErrorMessage   Error message if compilation failed
	 */
	ECABRIDGE_API void QueueBlueprintCompiled(const FString& BlueprintPath, bool bSuccess, const FString& ErrorMessage = TEXT(""));

	/**
	 * Queue a custom event with full control over all parameters.
	 * 
	 * @param EventType         Type identifier for the event
	 * @param Message           Human-readable description
	 * @param Data              Optional JSON data object
	 * @param bTriggerAIResponse  Whether ECA should respond to this event
	 */
	ECABRIDGE_API void QueueCustomEvent(
		const FString& EventType, 
		const FString& Message, 
		TSharedPtr<FJsonObject> Data = nullptr, 
		bool bTriggerAIResponse = true
	);
}
