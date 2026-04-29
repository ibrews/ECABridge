// Copyright Epic Games, Inc. All Rights Reserved.

#include "ECABridgeModule.h"
#include "ECABridge.h"
#include "ECABridgeSettings.h"
#include "ECAEventQueue.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "Selection.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/ImportSubsystem.h"
#include "Engine/Brush.h"
#include "Factories/Factory.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"  // For ELevelViewportType
#include "Commands/ECACommand.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "FECABridgeModule"

void FECABridgeModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Module starting..."));

	// Skip server + handler registration in commandlet/cook/build runs.
	// The HTTP server fights for ports 3000/3010 with the live editor and
	// drives a non-zero editor exit code that UAT reports as "Cook failed."
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || IsRunningDedicatedServer())
	{
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Commandlet/cook/server detected — skipping server startup"));
		return;
	}

	// Create and initialize the bridge with the transient package as outer
	Bridge = NewObject<UECABridge>(GetTransientPackage(), NAME_None, RF_Transient);
	Bridge->Initialize();

	// Register console commands
	RegisterConsoleCommands();

	// Register editor event handlers
	RegisterEditorEventHandlers();

	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Module ready"));
}

void FECABridgeModule::ShutdownModule()
{
	// Unregister editor event handlers
	UnregisterEditorEventHandlers();
	
	if (Bridge)
	{
		Bridge->Shutdown();
		Bridge = nullptr;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Module shutdown"));
}

void FECABridgeModule::RegisterConsoleCommands()
{
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ECA.Status"),
		TEXT("Show ECA Bridge server status"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (FECABridgeModule::IsAvailable())
			{
				UECABridge* B = FECABridgeModule::Get().GetBridge();
				if (B)
				{
					UE_LOG(LogTemp, Log, TEXT("[ECABridge] Status: %s"), 
						B->IsRunning() ? TEXT("Running") : TEXT("Stopped"));
					UE_LOG(LogTemp, Log, TEXT("[ECABridge] URL: http://localhost:%d/sse"), B->GetPort());
					UE_LOG(LogTemp, Log, TEXT("[ECABridge] Commands processed: %d"), B->GetCommandsProcessed());
				}
			}
		}),
		ECVF_Default
	);
	
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ECA.ListCommands"),
		TEXT("List all available ECA commands"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (FECABridgeModule::IsAvailable())
			{
				UECABridge* B = FECABridgeModule::Get().GetBridge();
				if (B)
				{
					B->LogRegisteredCommands();
				}
			}
		}),
		ECVF_Default
	);
}

void FECABridgeModule::RegisterEditorEventHandlers()
{
	// Register for actor selection changes (static delegate, always available)
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(this, &FECABridgeModule::OnEditorSelectionChanged);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered selection change handler"));
	
	// Register for asset registry events (covers asset creation, rename, delete)
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(this, &FECABridgeModule::OnAssetAdded);
		AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &FECABridgeModule::OnAssetRemoved);
		AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &FECABridgeModule::OnAssetRenamed);
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered asset registry handlers"));
	}
	
	// Register for level/map change events (static delegates, always available)
	MapChangedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FECABridgeModule::OnMapOpened);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered map change handler"));
	
	// Register for PIE (Play In Editor) events (static delegates, always available)
	PIEStartedHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FECABridgeModule::OnPIEStarted);
	PIEEndedHandle = FEditorDelegates::EndPIE.AddRaw(this, &FECABridgeModule::OnPIEEnded);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered PIE handlers"));
	
	// GEditor-dependent registrations need to be deferred until editor is ready
	// We'll register these when GEditor becomes available
	if (GEditor)
	{
		RegisterGEditorDependentHandlers();
	}
	else
	{
		// Defer registration until editor is initialized
		FEditorDelegates::OnEditorInitialized.AddRaw(this, &FECABridgeModule::OnEditorInitialized);
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Deferred GEditor-dependent handlers until editor init"));
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Editor event handlers registration complete"));
}

void FECABridgeModule::OnEditorInitialized(double Duration)
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Editor initialized (took %.2fs), registering deferred handlers"), Duration);
	RegisterGEditorDependentHandlers();
}

void FECABridgeModule::RegisterGEditorDependentHandlers()
{
	if (!GEditor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] GEditor still null, cannot register GEditor-dependent handlers"));
		return;
	}
	
	// Register for asset import events
	if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
	{
		AssetImportHandle = ImportSubsystem->OnAssetPostImport.AddRaw(this, &FECABridgeModule::OnAssetImported);
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered asset import handler"));
	}
	
	// Register for Blueprint compilation events
	BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FECABridgeModule::OnBlueprintCompiled);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered Blueprint compilation handler"));
	
	// Register for actor spawn/delete in editor
	ActorSpawnedHandle = GEditor->OnLevelActorAdded().AddRaw(this, &FECABridgeModule::OnActorSpawned);
	ActorDeletedHandle = GEditor->OnLevelActorDeleted().AddRaw(this, &FECABridgeModule::OnActorDeleted);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered actor spawn/delete handlers"));
	
	// Register for editor camera movement events
	CameraMovedHandle = FEditorDelegates::OnEditorCameraMoved.AddRaw(this, &FECABridgeModule::OnEditorCameraMoved);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered camera movement handler"));
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] All GEditor-dependent handlers registered"));
}

void FECABridgeModule::UnregisterEditorEventHandlers()
{
	// Unregister selection change handler
	if (SelectionChangedHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
		SelectionChangedHandle.Reset();
	}
	
	// Unregister asset import handler
	if (AssetImportHandle.IsValid() && GEditor)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			ImportSubsystem->OnAssetPostImport.Remove(AssetImportHandle);
		}
		AssetImportHandle.Reset();
	}
	
	// Unregister asset registry handlers
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		if (AssetAddedHandle.IsValid())
		{
			AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
			AssetAddedHandle.Reset();
		}
		if (AssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
			AssetRemovedHandle.Reset();
		}
		if (AssetRenamedHandle.IsValid())
		{
			AssetRegistry.OnAssetRenamed().Remove(AssetRenamedHandle);
			AssetRenamedHandle.Reset();
		}
	}
	
	// Unregister Blueprint compilation handler
	if (BlueprintCompiledHandle.IsValid() && GEditor)
	{
		GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
		BlueprintCompiledHandle.Reset();
	}
	
	// Unregister map change handler
	if (MapChangedHandle.IsValid())
	{
		FEditorDelegates::OnMapOpened.Remove(MapChangedHandle);
		MapChangedHandle.Reset();
	}
	
	// Unregister PIE handlers
	if (PIEStartedHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(PIEStartedHandle);
		PIEStartedHandle.Reset();
	}
	if (PIEEndedHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PIEEndedHandle);
		PIEEndedHandle.Reset();
	}
	
	// Unregister actor spawn/delete handlers
	if (GEditor)
	{
		if (ActorSpawnedHandle.IsValid())
		{
			GEditor->OnLevelActorAdded().Remove(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}
		if (ActorDeletedHandle.IsValid())
		{
			GEditor->OnLevelActorDeleted().Remove(ActorDeletedHandle);
			ActorDeletedHandle.Reset();
		}
	}
	
	// Unregister camera movement handler
	if (CameraMovedHandle.IsValid())
	{
		FEditorDelegates::OnEditorCameraMoved.Remove(CameraMovedHandle);
		CameraMovedHandle.Reset();
	}
	
	// Clear any pending camera timer
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		GEditor->GetEditorWorldContext().World()->GetTimerManager().ClearTimer(CameraMovedTimerHandle);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] All editor event handlers unregistered"));
}

void FECABridgeModule::OnEditorSelectionChanged(UObject* SelectionObject)
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] OnEditorSelectionChanged called! Object: %s"), 
		SelectionObject ? *SelectionObject->GetName() : TEXT("null"));
	
	if (!GEditor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] GEditor is null"));
		return;
	}
	
	// Only process actor selection changes (not component selection, etc.)
	USelection* ActorSelection = GEditor->GetSelectedActors();
	if (!ActorSelection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] GetSelectedActors() returned null"));
		return;
	}
	
	// Check if this is actor selection (vs component selection, etc.)
	if (SelectionObject != ActorSelection)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ECABridge] Ignoring non-actor selection change"));
		return;
	}
	
	// Get selected actors
	TArray<AActor*> SelectedActors;
	ActorSelection->GetSelectedObjects<AActor>(SelectedActors);
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Actor selection changed: %d actors"), SelectedActors.Num());
	
	if (SelectedActors.Num() == 0)
	{
		// Selection cleared - queue a deselection event
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("count"), 0);
		Data->SetArrayField(TEXT("actors"), TArray<TSharedPtr<FJsonValue>>());
		ECAEvents::QueueCustomEvent(TEXT("selection_changed"), TEXT("Selection cleared"), Data, false);
		return;
	}
	
	// Build selection data
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("count"), SelectedActors.Num());
	
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (AActor* Actor : SelectedActors)
	{
		if (Actor)
		{
			TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
			ActorInfo->SetStringField(TEXT("name"), Actor->GetActorNameOrLabel());
			ActorInfo->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorInfo->SetStringField(TEXT("path"), Actor->GetPathName());
			
			// Add location
			FVector Location = Actor->GetActorLocation();
			ActorInfo->SetNumberField(TEXT("x"), Location.X);
			ActorInfo->SetNumberField(TEXT("y"), Location.Y);
			ActorInfo->SetNumberField(TEXT("z"), Location.Z);
			
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorInfo));
		}
	}
	Data->SetArrayField(TEXT("actors"), ActorsArray);
	
	// Queue the event - single selection uses the simpler helper, multi-selection uses custom
	if (SelectedActors.Num() == 1)
	{
		AActor* Actor = SelectedActors[0];
		ECAEvents::QueueSelectionChanged(Actor->GetActorNameOrLabel(), Actor->GetPathName());
	}
	else
	{
		FString Message = FString::Printf(TEXT("Selected %d actors"), SelectedActors.Num());
		ECAEvents::QueueCustomEvent(TEXT("selection_changed"), Message, Data, true);
	}
}

void FECABridgeModule::OnAssetImported(UFactory* Factory, UObject* CreatedObject)
{
	if (!CreatedObject)
	{
		return;
	}
	
	FString AssetPath = CreatedObject->GetPathName();
	FString AssetType = CreatedObject->GetClass()->GetName();
	FString AssetName = CreatedObject->GetName();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Asset imported: %s (%s)"), *AssetPath, *AssetType);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Data->SetStringField(TEXT("asset_name"), AssetName);
	Data->SetStringField(TEXT("asset_type"), AssetType);
	if (Factory)
	{
		Data->SetStringField(TEXT("factory"), Factory->GetClass()->GetName());
	}
	
	FString Message = FString::Printf(TEXT("Imported asset: %s (%s)"), *AssetName, *AssetType);
	ECAEvents::QueueCustomEvent(TEXT("asset_imported"), Message, Data, false);
}

void FECABridgeModule::OnAssetAdded(const FAssetData& AssetData)
{
	// Skip transient or temporary assets
	if (AssetData.PackagePath.ToString().StartsWith(TEXT("/Temp")) ||
		AssetData.PackagePath.ToString().StartsWith(TEXT("/Engine")))
	{
		return;
	}
	
	FString AssetPath = AssetData.GetObjectPathString();
	FString AssetType = AssetData.AssetClassPath.GetAssetName().ToString();
	FString AssetName = AssetData.AssetName.ToString();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Asset added: %s (%s)"), *AssetPath, *AssetType);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Data->SetStringField(TEXT("asset_name"), AssetName);
	Data->SetStringField(TEXT("asset_type"), AssetType);
	Data->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
	
	FString Message = FString::Printf(TEXT("Asset created: %s (%s)"), *AssetName, *AssetType);
	ECAEvents::QueueCustomEvent(TEXT("asset_added"), Message, Data, false);
}

void FECABridgeModule::OnAssetRemoved(const FAssetData& AssetData)
{
	FString AssetPath = AssetData.GetObjectPathString();
	FString AssetName = AssetData.AssetName.ToString();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Asset removed: %s"), *AssetPath);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Data->SetStringField(TEXT("asset_name"), AssetName);
	
	FString Message = FString::Printf(TEXT("Asset deleted: %s"), *AssetName);
	ECAEvents::QueueCustomEvent(TEXT("asset_removed"), Message, Data, false);
}

void FECABridgeModule::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	FString NewPath = AssetData.GetObjectPathString();
	FString AssetName = AssetData.AssetName.ToString();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Asset renamed: %s -> %s"), *OldObjectPath, *NewPath);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("old_path"), OldObjectPath);
	Data->SetStringField(TEXT("new_path"), NewPath);
	Data->SetStringField(TEXT("asset_name"), AssetName);
	
	FString Message = FString::Printf(TEXT("Asset renamed: %s"), *AssetName);
	ECAEvents::QueueCustomEvent(TEXT("asset_renamed"), Message, Data, false);
}

void FECABridgeModule::OnBlueprintCompiled()
{
	// This delegate doesn't give us the specific Blueprint, so we log a generic message
	// For more specific Blueprint compilation tracking, we'd need to hook into FKismetCompilerContext
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] A Blueprint was compiled"));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	ECAEvents::QueueCustomEvent(TEXT("blueprint_compiled"), TEXT("A Blueprint was compiled"), Data, false);
}

void FECABridgeModule::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Map opened: %s (template: %s)"), *Filename, bAsTemplate ? TEXT("true") : TEXT("false"));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("filename"), Filename);
	Data->SetBoolField(TEXT("as_template"), bAsTemplate);
	
	// Extract just the map name from the path
	FString MapName = FPaths::GetBaseFilename(Filename);
	
	FString Message = FString::Printf(TEXT("Opened map: %s"), *MapName);
	ECAEvents::QueueCustomEvent(TEXT("map_opened"), Message, Data, false);
}

void FECABridgeModule::OnPIEStarted(bool bIsSimulating)
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] PIE started (simulating: %s)"), bIsSimulating ? TEXT("true") : TEXT("false"));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_simulating"), bIsSimulating);
	
	FString Message = bIsSimulating ? TEXT("Started Simulate In Editor") : TEXT("Started Play In Editor");
	ECAEvents::QueueCustomEvent(TEXT("pie_started"), Message, Data, false);
}

void FECABridgeModule::OnPIEEnded(bool bIsSimulating)
{
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] PIE ended (simulating: %s)"), bIsSimulating ? TEXT("true") : TEXT("false"));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_simulating"), bIsSimulating);
	
	FString Message = bIsSimulating ? TEXT("Stopped Simulate In Editor") : TEXT("Stopped Play In Editor");
	ECAEvents::QueueCustomEvent(TEXT("pie_ended"), Message, Data, false);
}

void FECABridgeModule::OnActorSpawned(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	
	// Skip transient actors and editor-only actors that are constantly created
	if (Actor->HasAnyFlags(RF_Transient) || Actor->IsA(ABrush::StaticClass()))
	{
		return;
	}
	
	FString ActorName = Actor->GetActorNameOrLabel();
	FString ActorClass = Actor->GetClass()->GetName();
	FString ActorPath = Actor->GetPathName();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Actor spawned: %s (%s)"), *ActorName, *ActorClass);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), ActorClass);
	Data->SetStringField(TEXT("actor_path"), ActorPath);
	
	FVector Location = Actor->GetActorLocation();
	Data->SetNumberField(TEXT("x"), Location.X);
	Data->SetNumberField(TEXT("y"), Location.Y);
	Data->SetNumberField(TEXT("z"), Location.Z);
	
	FString Message = FString::Printf(TEXT("Actor spawned: %s (%s)"), *ActorName, *ActorClass);
	ECAEvents::QueueCustomEvent(TEXT("actor_spawned"), Message, Data, false);
}

void FECABridgeModule::OnActorDeleted(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	
	// Skip transient actors
	if (Actor->HasAnyFlags(RF_Transient))
	{
		return;
	}
	
	FString ActorName = Actor->GetActorNameOrLabel();
	FString ActorClass = Actor->GetClass()->GetName();
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Actor deleted: %s (%s)"), *ActorName, *ActorClass);
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), ActorClass);
	
	FString Message = FString::Printf(TEXT("Actor deleted: %s (%s)"), *ActorName, *ActorClass);
	ECAEvents::QueueCustomEvent(TEXT("actor_deleted"), Message, Data, false);
}

// Helper functions for JSON conversion in camera movement tracking
namespace ECABridgeHelpers
{
	TSharedPtr<FJsonObject> VectorToJson(const FVector& Vec)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Vec.X);
		Json->SetNumberField(TEXT("y"), Vec.Y);
		Json->SetNumberField(TEXT("z"), Vec.Z);
		return Json;
	}
	
	TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rot)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("pitch"), Rot.Pitch);
		Json->SetNumberField(TEXT("yaw"), Rot.Yaw);
		Json->SetNumberField(TEXT("roll"), Rot.Roll);
		return Json;
	}
}

void FECABridgeModule::OnEditorCameraMoved(const FVector& Location, const FRotator& Rotation, ELevelViewportType ViewportType, int32 ViewIndex)
{
	// Only track perspective camera movements for now
	if (ViewportType != LVT_Perspective)
	{
		return;
	}
	
	// Store the starting position if this is the beginning of a movement
	if (!bCameraIsMoving)
	{
		bCameraIsMoving = true;
		LastCameraLocation = Location;
		LastCameraRotation = Rotation;
	}
	
	// Reset/extend the debounce timer
	// Every time the camera moves, we restart the timer
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		
		// Clear any existing timer
		EditorWorld->GetTimerManager().ClearTimer(CameraMovedTimerHandle);
		
		// Set a new timer - when this fires without being cleared, the camera has stopped
		EditorWorld->GetTimerManager().SetTimer(
			CameraMovedTimerHandle,
			FTimerDelegate::CreateRaw(this, &FECABridgeModule::OnCameraStoppedMoving),
			CameraStoppedDebounceTime,
			false  // Don't loop
		);
	}
}

void FECABridgeModule::OnCameraStoppedMoving()
{
	if (!bCameraIsMoving)
	{
		return;
	}
	
	bCameraIsMoving = false;
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Camera stopped moving - triggering describe_view"));
	
	// Execute the describe_view command to get scene information
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	// Use default parameters for describe_view
	
	// Find and execute the describe_view command
	TSharedPtr<IECACommand> DescribeViewCmd = FECACommandRegistry::Get().GetCommand(TEXT("describe_view"));
	if (DescribeViewCmd.IsValid())
	{
		FECACommandResult Result = DescribeViewCmd->Execute(Params);
		
		if (Result.bSuccess && Result.ResultData.IsValid())
		{
			// Create an event with the describe_view data
			TSharedPtr<FJsonObject> EventData = MakeShared<FJsonObject>();
			
			// Include movement information
			EventData->SetObjectField(TEXT("start_location"), ECABridgeHelpers::VectorToJson(LastCameraLocation));
			EventData->SetObjectField(TEXT("start_rotation"), ECABridgeHelpers::RotatorToJson(LastCameraRotation));
			
			// Include the describe_view result
			EventData->SetObjectField(TEXT("view_description"), Result.ResultData);
			
			// Queue the event - this will trigger AI response so it can describe what's now visible
			ECAEvents::QueueCustomEvent(
				TEXT("camera_moved"),
				TEXT("User moved the editor viewport camera"),
				EventData,
				true  // Trigger AI response to describe the new view
			);
		}
		else
		{
			// If describe_view failed, still queue a basic event
			TSharedPtr<FJsonObject> EventData = MakeShared<FJsonObject>();
			EventData->SetStringField(TEXT("error"), Result.ErrorMessage);
			
			ECAEvents::QueueCustomEvent(
				TEXT("camera_moved"),
				TEXT("User moved the editor viewport camera (view description unavailable)"),
				EventData,
				false  // Don't trigger AI response on failure
			);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] describe_view command not found"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FECABridgeModule, ECABridge)
