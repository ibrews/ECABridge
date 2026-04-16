// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/EngineTypes.h"  // For FTimerHandle
#include "Editor/UnrealEdTypes.h"  // For ELevelViewportType

class UECABridge;
class UFactory;
struct FAssetData;
class AActor;

/**
 * ECA Bridge Module
 * 
 * Provides TCP server for Epic Code Assistant to control Unreal Editor.
 * Commands are executed on the game thread to safely interact with editor subsystems.
 * Also monitors editor events and queues them for the AI to process.
 */
class FECABridgeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Get the bridge instance */
	UECABridge* GetBridge() const { return Bridge; }
	
	/** Module singleton access */
	static FECABridgeModule& Get()
	{
		return FModuleManager::GetModuleChecked<FECABridgeModule>("ECABridge");
	}
	
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ECABridge");
	}

private:
	/** The bridge instance that handles commands */
	UECABridge* Bridge = nullptr;
	
	/** Register toolbar extension */
	void RegisterToolbarExtension();
	
	/** Console commands for debugging */
	void RegisterConsoleCommands();
	
	/** Register editor event handlers for pending events */
	void RegisterEditorEventHandlers();
	
	/** Unregister editor event handlers */
	void UnregisterEditorEventHandlers();
	
	/** Called when the editor finishes initializing (for deferred handler registration) */
	void OnEditorInitialized(double Duration);
	
	/** Register handlers that depend on GEditor being valid */
	void RegisterGEditorDependentHandlers();
	
	//--------------------------------------------------------------------------
	// Event Callbacks
	//--------------------------------------------------------------------------
	
	/** Called when editor selection changes */
	void OnEditorSelectionChanged(UObject* SelectionObject);
	
	/** Called when an asset is imported via the import dialog */
	void OnAssetImported(UFactory* Factory, UObject* CreatedObject);
	
	/** Called when a new asset is added to the asset registry */
	void OnAssetAdded(const FAssetData& AssetData);
	
	/** Called when an asset is removed from the asset registry */
	void OnAssetRemoved(const FAssetData& AssetData);
	
	/** Called when an asset is renamed */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	
	/** Called when any Blueprint is compiled */
	void OnBlueprintCompiled();
	
	/** Called when a map/level is opened */
	void OnMapOpened(const FString& Filename, bool bAsTemplate);
	
	/** Called when Play In Editor starts */
	void OnPIEStarted(bool bIsSimulating);
	
	/** Called when Play In Editor ends */
	void OnPIEEnded(bool bIsSimulating);
	
	/** Called when an actor is spawned in the editor */
	void OnActorSpawned(AActor* Actor);
	
	/** Called when an actor is deleted in the editor */
	void OnActorDeleted(AActor* Actor);
	
	/** Called when the editor camera moves */
	void OnEditorCameraMoved(const FVector& Location, const FRotator& Rotation, ELevelViewportType ViewportType, int32 ViewIndex);
	
	/** Timer callback to send describe_view after camera stops moving */
	void OnCameraStoppedMoving();
	
	//--------------------------------------------------------------------------
	// Delegate Handles
	//--------------------------------------------------------------------------
	
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle AssetImportHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle BlueprintCompiledHandle;
	FDelegateHandle MapChangedHandle;
	FDelegateHandle PIEStartedHandle;
	FDelegateHandle PIEEndedHandle;
	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle CameraMovedHandle;
	
	//--------------------------------------------------------------------------
	// Camera Movement Tracking
	//--------------------------------------------------------------------------
	
	/** Timer handle for debouncing camera movement events */
	FTimerHandle CameraMovedTimerHandle;
	
	/** Last camera location when movement started */
	FVector LastCameraLocation;
	
	/** Last camera rotation when movement started */
	FRotator LastCameraRotation;
	
	/** Whether the camera is currently moving */
	bool bCameraIsMoving = false;
	
	/** Debounce time in seconds - wait this long after camera stops before sending event */
	static constexpr float CameraStoppedDebounceTime = 0.5f;
};
