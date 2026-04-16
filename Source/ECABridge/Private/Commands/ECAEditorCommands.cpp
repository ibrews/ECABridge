// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAEditorCommands.h"
#include "Engine/StaticMeshActor.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "EditorViewportClient.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Misc/FileHelper.h"
#include "FileHelpers.h"
#include "Misc/Base64.h"
#include "ImageUtils.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "AssetRegistry/AssetRegistryModule.h"

// Register all editor commands
REGISTER_ECA_COMMAND(FECACommand_FocusViewport)
REGISTER_ECA_COMMAND(FECACommand_SelectActors)
REGISTER_ECA_COMMAND(FECACommand_GetSelectedActors)

REGISTER_ECA_COMMAND(FECACommand_TakeDepthScreenshot)
REGISTER_ECA_COMMAND(FECACommand_TakeGameplayScreenshot)
REGISTER_ECA_COMMAND(FECACommand_RunConsoleCommand)
REGISTER_ECA_COMMAND(FECACommand_GetLevelInfo)
REGISTER_ECA_COMMAND(FECACommand_OpenLevel)
REGISTER_ECA_COMMAND(FECACommand_SaveLevel)
REGISTER_ECA_COMMAND(FECACommand_PlayInEditor)
REGISTER_ECA_COMMAND(FECACommand_StopPlayInEditor)
REGISTER_ECA_COMMAND(FECACommand_GetPlayState)
REGISTER_ECA_COMMAND(FECACommand_GetProjectOverview)

//------------------------------------------------------------------------------
// FocusViewport
//------------------------------------------------------------------------------

FECACommandResult FECACommand_FocusViewport::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Collect actors to focus on
	TArray<AActor*> ActorsToFocus;
	
	// Single actor
	FString ActorName;
	if (GetStringParam(Params, TEXT("actor_name"), ActorName, false))
	{
		AActor* Actor = FindActorByName(ActorName);
		if (Actor)
		{
			ActorsToFocus.Add(Actor);
		}
	}
	
	// Multiple actors
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = NULL;
	if (GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray, false) && ActorNamesArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ActorNamesArray)
		{
			FString Name = Value->AsString();
			AActor* Actor = FindActorByName(Name);
			if (Actor)
			{
				ActorsToFocus.Add(Actor);
			}
		}
	}
	
	if (ActorsToFocus.Num() > 0)
	{
		// Select and focus on actors
		GEditor->SelectNone(true, true, false);
		for (AActor* Actor : ActorsToFocus)
		{
			GEditor->SelectActor(Actor, true, true, false);
		}
		GEditor->MoveViewportCamerasToActor(ActorsToFocus, false);
	}
	else
	{
		// Focus on location
		FVector Location;
		if (GetVectorParam(Params, TEXT("location"), Location, false))
		{
			// Get viewport client
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
			if (ActiveViewport.IsValid())
			{
				FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
				ViewportClient.SetViewLocation(Location);
			}
		}
		else
		{
			return FECACommandResult::Error(TEXT("No actor or location specified to focus on"));
		}
	}
	
	return FECACommandResult::Success();
}

//------------------------------------------------------------------------------
// SelectActors
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SelectActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	bool bDeselectAll = false;
	GetBoolParam(Params, TEXT("deselect_all"), bDeselectAll, false);
	
	if (bDeselectAll)
	{
		GEditor->SelectNone(true, true, false);
		return FECACommandResult::Success();
	}
	
	bool bAddToSelection = false;
	GetBoolParam(Params, TEXT("add_to_selection"), bAddToSelection, false);
	
	if (!bAddToSelection)
	{
		GEditor->SelectNone(true, true, false);
	}
	
	TArray<FString> ActorNames;
	
	// Single actor
	FString ActorName;
	if (GetStringParam(Params, TEXT("actor_name"), ActorName, false))
	{
		ActorNames.Add(ActorName);
	}
	
	// Multiple actors
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray = NULL;
	if (GetArrayParam(Params, TEXT("actor_names"), ActorNamesArray, false) && ActorNamesArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ActorNamesArray)
		{
			ActorNames.Add(Value->AsString());
		}
	}
	
	int32 SelectedCount = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* Actor = FindActorByName(Name);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, true, false);
			SelectedCount++;
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetNumberField(TEXT("selected_count"), SelectedCount);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetSelectedActors
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetSelectedActors::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return FECACommandResult::Error(TEXT("EditorActorSubsystem not available"));
	}
	
	TArray<AActor*> SelectedActors = EditorActorSubsystem->GetSelectedLevelActors();
	
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (AActor* Actor : SelectedActors)
	{
		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorJson->SetStringField(TEXT("path"), Actor->GetPathName());
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorJson->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
		ActorJson->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
		ActorJson->SetObjectField(TEXT("scale"), VectorToJson(Actor->GetActorScale3D()));
		
		// If it's a static mesh actor, include the mesh path
		if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor))
		{
			if (UStaticMeshComponent* SMComp = SMActor->GetStaticMeshComponent())
			{
				if (UStaticMesh* Mesh = SMComp->GetStaticMesh())
				{
					ActorJson->SetStringField(TEXT("mesh_path"), Mesh->GetPathName());
				}
			}
		}
		
		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// TakeDepthScreenshot
//------------------------------------------------------------------------------

FECACommandResult FECACommand_TakeDepthScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Get the level editor viewport
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
	
	if (!ActiveViewport.IsValid())
	{
		return FECACommandResult::Error(TEXT("No active viewport"));
	}
	
	// Get viewport client for camera info
	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetActiveViewport()->GetClient());
	if (!ViewportClient)
	{
		return FECACommandResult::Error(TEXT("Could not get viewport client"));
	}
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	// Get optional parameters
	int32 Width = 1024;
	int32 Height = 1024;
	GetIntParam(Params, TEXT("width"), Width, false);
	GetIntParam(Params, TEXT("height"), Height, false);
	
	double MaxDepth = 10000.0;  // 100 meters default
	GetFloatParam(Params, TEXT("max_depth"), MaxDepth, false);
	
	// Get camera transform from viewport
	FVector CameraLocation = ViewportClient->GetViewLocation();
	FRotator CameraRotation = ViewportClient->GetViewRotation();
	float FOV = ViewportClient->ViewFOV;
	
	// Create a temporary render target for depth capture
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->InitCustomFormat(Width, Height, PF_FloatRGBA, false);
	RenderTarget->UpdateResourceImmediate(true);
	
	// Create a temporary scene capture actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(CameraLocation, CameraRotation, SpawnParams);
	
	if (!CaptureActor)
	{
		return FECACommandResult::Error(TEXT("Failed to create scene capture actor"));
	}
	
	// Configure capture component for depth
	USceneCaptureComponent2D* CaptureComponent = CaptureActor->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	CaptureComponent->FOVAngle = FOV;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->MaxViewDistanceOverride = MaxDepth;
	
	// Capture the scene
	CaptureComponent->CaptureScene();
	
	// Read pixels from render target
	TArray<FFloat16Color> FloatPixels;
	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (RTResource)
	{
		RTResource->ReadFloat16Pixels(FloatPixels);
	}
	
	// Destroy the temporary actor
	CaptureActor->Destroy();
	
	if (FloatPixels.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to read depth pixels"));
	}
	
	// Convert depth to grayscale image (normalized 0-255)
	TArray<FColor> Bitmap;
	Bitmap.SetNum(FloatPixels.Num());
	
	for (int32 i = 0; i < FloatPixels.Num(); i++)
	{
		// Get depth value (in R channel for SceneDepth)
		float Depth = FloatPixels[i].R.GetFloat();
		
		// Normalize to 0-1 range based on max depth
		float NormalizedDepth = FMath::Clamp(Depth / MaxDepth, 0.0f, 1.0f);
		
		// Convert to grayscale (0 = close, 255 = far)
		uint8 Gray = static_cast<uint8>(NormalizedDepth * 255.0f);
		
		Bitmap[i] = FColor(Gray, Gray, Gray, 255);
	}
	
	// Check if we should save to file
	FString FilePath;
	if (GetStringParam(Params, TEXT("file_path"), FilePath, false))
	{
		// Save to file
		TArray64<uint8> CompressedData;
		FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedData);
		
		if (FFileHelper::SaveArrayToFile(TArrayView<const uint8>(CompressedData.GetData(), CompressedData.Num()), *FilePath))
		{
			TSharedPtr<FJsonObject> Result = MakeResult();
			Result->SetStringField(TEXT("file_path"), FilePath);
			Result->SetNumberField(TEXT("width"), Width);
			Result->SetNumberField(TEXT("height"), Height);
			Result->SetNumberField(TEXT("max_depth"), MaxDepth);
			
			// Include camera info
			TSharedPtr<FJsonObject> CameraJson = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
			LocationJson->SetNumberField(TEXT("x"), CameraLocation.X);
			LocationJson->SetNumberField(TEXT("y"), CameraLocation.Y);
			LocationJson->SetNumberField(TEXT("z"), CameraLocation.Z);
			CameraJson->SetObjectField(TEXT("location"), LocationJson);
			
			TSharedPtr<FJsonObject> RotationJson = MakeShared<FJsonObject>();
			RotationJson->SetNumberField(TEXT("pitch"), CameraRotation.Pitch);
			RotationJson->SetNumberField(TEXT("yaw"), CameraRotation.Yaw);
			RotationJson->SetNumberField(TEXT("roll"), CameraRotation.Roll);
			CameraJson->SetObjectField(TEXT("rotation"), RotationJson);
			
			CameraJson->SetNumberField(TEXT("fov"), FOV);
			Result->SetObjectField(TEXT("camera"), CameraJson);
			
			return FECACommandResult::Success(Result);
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to save depth screenshot to: %s"), *FilePath));
		}
	}
	else
	{
		// Return as base64
		TArray64<uint8> CompressedData;
		FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedData);
		FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());
		
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetNumberField(TEXT("width"), Width);
		Result->SetNumberField(TEXT("height"), Height);
		Result->SetNumberField(TEXT("max_depth"), MaxDepth);
		Result->SetStringField(TEXT("image_base64"), Base64Image);
		Result->SetStringField(TEXT("format"), TEXT("png"));
		
		// Include camera info
		TSharedPtr<FJsonObject> CameraJson = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
		LocationJson->SetNumberField(TEXT("x"), CameraLocation.X);
		LocationJson->SetNumberField(TEXT("y"), CameraLocation.Y);
		LocationJson->SetNumberField(TEXT("z"), CameraLocation.Z);
		CameraJson->SetObjectField(TEXT("location"), LocationJson);
		
		TSharedPtr<FJsonObject> RotationJson = MakeShared<FJsonObject>();
		RotationJson->SetNumberField(TEXT("pitch"), CameraRotation.Pitch);
		RotationJson->SetNumberField(TEXT("yaw"), CameraRotation.Yaw);
		RotationJson->SetNumberField(TEXT("roll"), CameraRotation.Roll);
		CameraJson->SetObjectField(TEXT("rotation"), RotationJson);
		
		CameraJson->SetNumberField(TEXT("fov"), FOV);
		Result->SetObjectField(TEXT("camera"), CameraJson);
		
		return FECACommandResult::Success(Result);
	}
}

//------------------------------------------------------------------------------
// TakeGameplayScreenshot
//------------------------------------------------------------------------------

FECACommandResult FECACommand_TakeGameplayScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Determine output file path
	FString FilePath;
	if (!GetStringParam(Params, TEXT("file_path"), FilePath, false))
	{
		// Default: ProjectSaved/Screenshots/gameplay_YYYYMMDD_HHMMSS.png
		FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		FilePath = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("gameplay_%s.png"), *Timestamp));
	}

	// Determine capture target.
	// Supported values: "pie", "editor_viewport", "editor_window"
	// Default (empty): auto-detect -- PIE if playing, otherwise editor_viewport
	FString Target;
	GetStringParam(Params, TEXT("target"), Target, false);

	if (Target.IsEmpty())
	{
		// Auto-detect
		if (GEditor && GEditor->PlayWorld && GEngine && GEngine->GameViewport)
		{
			Target = TEXT("pie");
		}
		else
		{
			Target = TEXT("editor_viewport");
		}
	}

	TArray<FColor> Bitmap;
	int32 Width = 0;
	int32 Height = 0;
	FString Source;

	if (Target == TEXT("pie"))
	{
		// Capture the PIE game window via Slate -- includes all Slate/UMG/HUD overlays.
		if (!GEditor || !GEditor->PlayWorld || !GEngine || !GEngine->GameViewport)
		{
			return FECACommandResult::Error(TEXT("PIE is not running. Start Play in Editor first, or use 'editor_viewport' or 'editor_window' target."));
		}

		TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
		if (!GameWindow.IsValid())
		{
			return FECACommandResult::Error(TEXT("PIE is running but no game window found."));
		}

		FIntVector Size;
		bool bSuccess = FSlateApplication::Get().TakeScreenshot(GameWindow.ToSharedRef(), Bitmap, Size);

		if (!bSuccess || Bitmap.Num() == 0)
		{
			return FECACommandResult::Error(TEXT("Failed to capture PIE screenshot via Slate. The window may not be fully rendered."));
		}

		Width = Size.X;
		Height = Size.Y;
		Source = TEXT("pie");
	}
	else if (Target == TEXT("editor_window"))
	{
		// Capture the full editor window via Slate -- includes all panels, tabs, graphs.
		// Use the editor's own internal API: the LevelEditor module's tab knows exactly
		// which SWindow the main editor frame lives in, regardless of focus or Z-order.
		TSharedPtr<SWindow> EditorWindow;

		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TWeakPtr<SDockTab> LevelEditorTab = LevelEditorModule.GetLevelEditorInstanceTab();
		if (LevelEditorTab.IsValid())
		{
			EditorWindow = LevelEditorTab.Pin()->GetParentWindow();
		}

		if (!EditorWindow.IsValid())
		{
			return FECACommandResult::Error(TEXT("No editor window found. Is the Level Editor tab loaded?"));
		}

		FIntVector Size;
		bool bSuccess = FSlateApplication::Get().TakeScreenshot(EditorWindow.ToSharedRef(), Bitmap, Size);

		if (!bSuccess || Bitmap.Num() == 0)
		{
			return FECACommandResult::Error(TEXT("Failed to capture editor window via Slate."));
		}

		Width = Size.X;
		Height = Size.Y;
		Source = TEXT("editor_window");
	}
	else if (Target == TEXT("editor_viewport"))
	{
		// Capture the editor 3D viewport via GetViewportScreenShot (render target only).
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();

		if (!ActiveViewport.IsValid())
		{
			return FECACommandResult::Error(TEXT("No active editor viewport available."));
		}

		FViewport* Viewport = ActiveViewport->GetActiveViewport();
		if (!Viewport)
		{
			return FECACommandResult::Error(TEXT("Editor viewport is not available."));
		}

		FIntPoint ViewportSize = Viewport->GetSizeXY();
		Width = ViewportSize.X;
		Height = ViewportSize.Y;

		if (Width == 0 || Height == 0)
		{
			return FECACommandResult::Error(TEXT("Editor viewport has zero size. Is it visible?"));
		}

		if (!GetViewportScreenShot(Viewport, Bitmap))
		{
			return FECACommandResult::Error(TEXT("Failed to capture editor viewport screenshot."));
		}

		Source = TEXT("editor_viewport");
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Unknown target '%s'. Valid targets: 'pie', 'editor_viewport', 'editor_window'."), *Target));
	}

	if (Bitmap.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Screenshot capture returned empty bitmap."));
	}

	// Force alpha to 255 (opaque). The editor viewport render target often has
	// alpha=0 which makes PNGs appear black/transparent in most image viewers.
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	// Ensure output directory exists
	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Directory);

	// Compress to PNG and save
	TArray64<uint8> CompressedData;
	FImageUtils::PNGCompressImageArray(Width, Height,
		TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedData);

	if (CompressedData.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to compress screenshot to PNG."));
	}

	if (!FFileHelper::SaveArrayToFile(
		TArrayView<const uint8>(CompressedData.GetData(), CompressedData.Num()), *FilePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to save screenshot to: %s"), *FilePath));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("file_path"), FilePath);
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetStringField(TEXT("source"), Source);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// RunConsoleCommand
//------------------------------------------------------------------------------

FECACommandResult FECACommand_RunConsoleCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Command;
	if (!GetStringParam(Params, TEXT("command"), Command))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: command"));
	}
	
	UWorld* World = GetEditorWorld();
	if (World)
	{
		GEditor->Exec(World, *Command);
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("executed_command"), Command);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetLevelInfo
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetLevelInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("level_name"), World->GetMapName());
	Result->SetStringField(TEXT("level_path"), World->GetPathName());
	
	// Count actors by type
	int32 TotalActors = 0;
	TMap<FString, int32> ActorCounts;
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		FString ClassName = Actor->GetClass()->GetName();
		ActorCounts.FindOrAdd(ClassName)++;
		TotalActors++;
	}
	
	Result->SetNumberField(TEXT("total_actors"), TotalActors);
	
	TSharedPtr<FJsonObject> CountsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : ActorCounts)
	{
		CountsObj->SetNumberField(Pair.Key, Pair.Value);
	}
	Result->SetObjectField(TEXT("actor_counts"), CountsObj);
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// OpenLevel
//------------------------------------------------------------------------------

FECACommandResult FECACommand_OpenLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString LevelPath;
	if (!GetStringParam(Params, TEXT("level_path"), LevelPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: level_path"));
	}
	
	// Ensure it's a valid level path
	if (!LevelPath.StartsWith(TEXT("/Game/")))
	{
		LevelPath = TEXT("/Game/") + LevelPath;
	}
	
	bool bSuccess = FEditorFileUtils::LoadMap(LevelPath);
	
	if (bSuccess)
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("opened_level"), LevelPath);
		return FECACommandResult::Success(Result);
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
	}
}

//------------------------------------------------------------------------------
// SaveLevel
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SaveLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No world available"));
	}
	
	bool bSuccess = FEditorFileUtils::SaveCurrentLevel();
	
	if (bSuccess)
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("saved_level"), World->GetMapName());
		return FECACommandResult::Success(Result);
	}
	else
	{
		return FECACommandResult::Error(TEXT("Failed to save level"));
	}
}

//------------------------------------------------------------------------------
// PlayInEditor
//------------------------------------------------------------------------------

FECACommandResult FECACommand_PlayInEditor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (GEditor->PlayWorld)
	{
		return FECACommandResult::Error(TEXT("Already playing in editor"));
	}
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
	
	FRequestPlaySessionParams SessionParams;
	SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	
	GEditor->RequestPlaySession(SessionParams);
	
	return FECACommandResult::Success();
}

//------------------------------------------------------------------------------
// StopPlayInEditor
//------------------------------------------------------------------------------

FECACommandResult FECACommand_StopPlayInEditor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor->PlayWorld)
	{
		return FECACommandResult::Error(TEXT("Not currently playing in editor"));
	}
	
	GEditor->RequestEndPlayMap();
	
	return FECACommandResult::Success();
}

//------------------------------------------------------------------------------
// GetPlayState
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetPlayState::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	bool bIsPlaying = GEditor->PlayWorld != nullptr;
	bool bIsPaused = GEditor->PlayWorld ? GEditor->PlayWorld->IsPaused() : false;
	bool bIsSimulating = GEditor->bIsSimulatingInEditor;
	
	Result->SetBoolField(TEXT("is_playing"), bIsPlaying);
	Result->SetBoolField(TEXT("is_paused"), bIsPaused);
	Result->SetBoolField(TEXT("is_simulating"), bIsSimulating);
	
	FString State;
	if (bIsSimulating)
	{
		State = TEXT("Simulating");
	}
	else if (bIsPlaying)
	{
		State = bIsPaused ? TEXT("Paused") : TEXT("Playing");
	}
	else
	{
		State = TEXT("Stopped");
	}
	Result->SetStringField(TEXT("state"), State);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetProjectOverview
//------------------------------------------------------------------------------

// Recursive helper: build a JSON tree of folders with asset counts by class
static void BuildFolderTree(
	IAssetRegistry& AssetRegistry,
	const FString& FolderPath,
	int32 CurrentDepth,
	int32 MaxDepth,
	TSharedPtr<FJsonObject>& OutNode,
	TMap<FString, int32>& GlobalClassCounts,
	int32& TotalAssets)
{
	// Query assets directly in this folder (non-recursive)
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*FolderPath));
	Filter.bRecursivePaths = false;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Count assets by class in this folder
	TMap<FString, int32> LocalClassCounts;
	for (const FAssetData& Data : AssetDataList)
	{
		FString ClassName = Data.AssetClassPath.GetAssetName().ToString();
		LocalClassCounts.FindOrAdd(ClassName)++;
		GlobalClassCounts.FindOrAdd(ClassName)++;
		TotalAssets++;
	}

	OutNode->SetStringField(TEXT("path"), FolderPath);
	OutNode->SetNumberField(TEXT("asset_count"), AssetDataList.Num());

	if (LocalClassCounts.Num() > 0)
	{
		TSharedPtr<FJsonObject> ClassCountsObj = MakeShared<FJsonObject>();
		for (const auto& Pair : LocalClassCounts)
		{
			ClassCountsObj->SetNumberField(Pair.Key, Pair.Value);
		}
		OutNode->SetObjectField(TEXT("assets_by_class"), ClassCountsObj);
	}

	// Recurse into subfolders if within depth limit
	if (CurrentDepth < MaxDepth)
	{
		TArray<FString> SubPaths;
		AssetRegistry.GetSubPaths(FolderPath, SubPaths, false);

		if (SubPaths.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (const FString& SubPath : SubPaths)
			{
				TSharedPtr<FJsonObject> ChildNode = MakeShared<FJsonObject>();
				BuildFolderTree(AssetRegistry, SubPath, CurrentDepth + 1, MaxDepth, ChildNode, GlobalClassCounts, TotalAssets);
				ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildNode));
			}
			OutNode->SetArrayField(TEXT("children"), ChildrenArray);
		}
	}
	else
	{
		// At max depth, count recursive assets to show there's more
		FARFilter RecFilter;
		RecFilter.PackagePaths.Add(FName(*FolderPath));
		RecFilter.bRecursivePaths = true;

		TArray<FAssetData> RecAssets;
		AssetRegistry.GetAssets(RecFilter, RecAssets);
		int32 DeepCount = RecAssets.Num() - AssetDataList.Num();
		if (DeepCount > 0)
		{
			OutNode->SetNumberField(TEXT("nested_asset_count"), DeepCount);

			// Still count them globally
			for (const FAssetData& Data : RecAssets)
			{
				// Skip the ones we already counted
				if (AssetDataList.ContainsByPredicate([&](const FAssetData& Local) {
					return Local.GetObjectPathString() == Data.GetObjectPathString();
				}))
				{
					continue;
				}
				FString ClassName = Data.AssetClassPath.GetAssetName().ToString();
				GlobalClassCounts.FindOrAdd(ClassName)++;
				TotalAssets++;
			}
		}
	}
}

FECACommandResult FECACommand_GetProjectOverview::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString RootPath = TEXT("/Game/");
	GetStringParam(Params, TEXT("path"), RootPath, false);

	int32 MaxDepth = 3;
	GetIntParam(Params, TEXT("max_depth"), MaxDepth, false);
	MaxDepth = FMath::Clamp(MaxDepth, 1, 10);

	bool bIncludeEngineContent = false;
	GetBoolParam(Params, TEXT("include_engine_content"), bIncludeEngineContent, false);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TMap<FString, int32> GlobalClassCounts;
	int32 TotalAssets = 0;

	// Build the folder tree for the main path
	TSharedPtr<FJsonObject> FolderTree = MakeShared<FJsonObject>();
	BuildFolderTree(AssetRegistry, RootPath, 1, MaxDepth, FolderTree, GlobalClassCounts, TotalAssets);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetObjectField(TEXT("folder_tree"), FolderTree);

	// Optionally add engine content as a sibling tree
	if (bIncludeEngineContent)
	{
		TSharedPtr<FJsonObject> EngineTree = MakeShared<FJsonObject>();
		BuildFolderTree(AssetRegistry, TEXT("/Engine/"), 1, FMath::Min(MaxDepth, 2), EngineTree, GlobalClassCounts, TotalAssets);
		Result->SetObjectField(TEXT("engine_folder_tree"), EngineTree);
	}

	Result->SetNumberField(TEXT("total_assets"), TotalAssets);

	// Global class counts
	TSharedPtr<FJsonObject> ClassCountsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : GlobalClassCounts)
	{
		ClassCountsObj->SetNumberField(Pair.Key, Pair.Value);
	}
	Result->SetObjectField(TEXT("assets_by_class"), ClassCountsObj);

	return FECACommandResult::Success(Result);
}
