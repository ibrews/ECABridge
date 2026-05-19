// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAViewportBookmarkCommands.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

REGISTER_ECA_COMMAND(FECACommand_CreateViewportBookmark)
REGISTER_ECA_COMMAND(FECACommand_JumpToBookmark)
REGISTER_ECA_COMMAND(FECACommand_ListBookmarks)

namespace
{
	FString BookmarkPath()
	{
		return FPaths::ProjectSavedDir() / TEXT("ViewportBookmarks.json");
	}

	bool LoadBookmarks(TSharedPtr<FJsonObject>& OutRoot)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *BookmarkPath()))
		{
			OutRoot = MakeShared<FJsonObject>();
			return true;
		}
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			return false;
		}
		return true;
	}

	bool SaveBookmarks(const TSharedPtr<FJsonObject>& Root)
	{
		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return FFileHelper::SaveStringToFile(Serialized, *BookmarkPath());
	}

	FEditorViewportClient* GetActiveViewportClient()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<IAssetViewport> Active = LevelEditorModule.GetFirstActiveViewport();
		if (!Active.IsValid()) return nullptr;
		FViewport* Viewport = Active->GetActiveViewport();
		if (!Viewport) return nullptr;
		return static_cast<FEditorViewportClient*>(Viewport->GetClient());
	}
}

FECACommandResult FECACommand_CreateViewportBookmark::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Slot;
	if (!GetStringParam(Params, TEXT("slot"), Slot))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: slot"));
	}

	FEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		return FECACommandResult::Error(TEXT("No active editor viewport."));
	}

	FString Label;
	GetStringParam(Params, TEXT("label"), Label, false);

	const FVector  Loc = Client->GetViewLocation();
	const FRotator Rot = Client->GetViewRotation();
	const float    FOV = Client->ViewFOV;

	TSharedPtr<FJsonObject> Root;
	if (!LoadBookmarks(Root))
	{
		return FECACommandResult::Error(TEXT("Existing bookmark file is corrupt; rename or delete Saved/ViewportBookmarks.json."));
	}

	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetObjectField(TEXT("location"), VectorToJson(Loc));
	Entry->SetObjectField(TEXT("rotation"), RotatorToJson(Rot));
	Entry->SetNumberField(TEXT("fov"),      FOV);
	if (!Label.IsEmpty()) Entry->SetStringField(TEXT("label"), Label);
	Entry->SetStringField(TEXT("saved_at"), FDateTime::UtcNow().ToIso8601());

	Root->SetObjectField(Slot, Entry);

	if (!SaveBookmarks(Root))
	{
		return FECACommandResult::Error(TEXT("Failed to persist viewport bookmarks file."));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("slot"), Slot);
	Result->SetObjectField(TEXT("bookmark"), Entry);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_JumpToBookmark::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Slot;
	if (!GetStringParam(Params, TEXT("slot"), Slot))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: slot"));
	}

	TSharedPtr<FJsonObject> Root;
	if (!LoadBookmarks(Root))
	{
		return FECACommandResult::Error(TEXT("Existing bookmark file is corrupt."));
	}

	const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
	if (!Root->TryGetObjectField(Slot, EntryPtr) || !EntryPtr || !EntryPtr->IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Bookmark slot not found: %s"), *Slot));
	}
	TSharedPtr<FJsonObject> Entry = *EntryPtr;

	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	const TSharedPtr<FJsonObject>* RotObj = nullptr;
	Entry->TryGetObjectField(TEXT("location"), LocObj);
	Entry->TryGetObjectField(TEXT("rotation"), RotObj);
	if (!LocObj || !RotObj || !LocObj->IsValid() || !RotObj->IsValid())
	{
		return FECACommandResult::Error(TEXT("Bookmark entry missing location/rotation."));
	}

	FVector Loc(
		(*LocObj)->GetNumberField(TEXT("x")),
		(*LocObj)->GetNumberField(TEXT("y")),
		(*LocObj)->GetNumberField(TEXT("z")));
	FRotator Rot(
		(*RotObj)->GetNumberField(TEXT("pitch")),
		(*RotObj)->GetNumberField(TEXT("yaw")),
		(*RotObj)->GetNumberField(TEXT("roll")));
	double FOV = 0.0;
	Entry->TryGetNumberField(TEXT("fov"), FOV);

	FEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		return FECACommandResult::Error(TEXT("No active editor viewport."));
	}
	Client->SetViewLocation(Loc);
	Client->SetViewRotation(Rot);
	if (FOV > 0.0) Client->ViewFOV = static_cast<float>(FOV);
	Client->Invalidate();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("slot"), Slot);
	Result->SetObjectField(TEXT("applied"), Entry);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ListBookmarks::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Root;
	if (!LoadBookmarks(Root))
	{
		return FECACommandResult::Error(TEXT("Bookmark file is corrupt."));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), BookmarkPath());
	Result->SetObjectField(TEXT("bookmarks"), Root);
	Result->SetNumberField(TEXT("count"), Root->Values.Num());
	return FECACommandResult::Success(Result);
}
