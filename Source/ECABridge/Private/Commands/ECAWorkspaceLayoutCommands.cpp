// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWorkspaceLayoutCommands.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

REGISTER_ECA_COMMAND(FECACommand_SaveWorkspaceLayout)
REGISTER_ECA_COMMAND(FECACommand_LoadWorkspaceLayout)
REGISTER_ECA_COMMAND(FECACommand_ListWorkspaceLayouts)

namespace
{
	FString LayoutDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("WorkspaceLayouts");
	}

	FString LayoutPath(const FString& Name)
	{
		return LayoutDir() / (Name + TEXT(".ini"));
	}

	FString ActiveLayoutIni()
	{
		// UE writes the editor layout state into GEditorLayoutIni.
		return GEditorLayoutIni;
	}

	bool IsValidLayoutName(const FString& Name)
	{
		if (Name.IsEmpty()) return false;
		const FString Invalid = TEXT("/\\:*?\"<>|");
		for (TCHAR C : Name)
		{
			if (Invalid.Contains(FString::Chr(C))) return false;
		}
		return true;
	}
}

FECACommandResult FECACommand_SaveWorkspaceLayout::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	if (!IsValidLayoutName(Name))
	{
		return FECACommandResult::Error(TEXT("Layout name contains invalid path characters."));
	}

	// Flush in-memory tab/dock state to GEditorLayoutIni so the snapshot is current.
	FGlobalTabmanager::Get()->SaveAllVisualState();
	if (GConfig)
	{
		GConfig->Flush(/*Read=*/false, GEditorLayoutIni);
	}

	const FString SrcPath = ActiveLayoutIni();
	if (SrcPath.IsEmpty() || !IFileManager::Get().FileExists(*SrcPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Active editor layout ini not found at: %s"), *SrcPath));
	}

	IFileManager::Get().MakeDirectory(*LayoutDir(), /*Tree=*/true);
	const FString DestPath = LayoutPath(Name);

	if (IFileManager::Get().Copy(*DestPath, *SrcPath, /*Replace=*/true) != COPY_OK)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to copy layout from %s to %s"), *SrcPath, *DestPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("source"), SrcPath);
	Result->SetStringField(TEXT("destination"), DestPath);
	Result->SetNumberField(TEXT("size_bytes"), IFileManager::Get().FileSize(*DestPath));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_LoadWorkspaceLayout::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	if (!IsValidLayoutName(Name))
	{
		return FECACommandResult::Error(TEXT("Layout name contains invalid path characters."));
	}

	const FString SrcPath = LayoutPath(Name);
	if (!IFileManager::Get().FileExists(*SrcPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Layout snapshot not found: %s"), *SrcPath));
	}

	const FString DestPath = ActiveLayoutIni();
	if (DestPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Active editor layout ini path is empty."));
	}

	if (IFileManager::Get().Copy(*DestPath, *SrcPath, /*Replace=*/true) != COPY_OK)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to copy layout from %s to %s"), *SrcPath, *DestPath));
	}

	// Re-read the file we just overwrote so subsequent SaveAllVisualState passes do not
	// blow it away with stale in-memory state.
	if (GConfig)
	{
		GConfig->LoadFile(DestPath);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("source"), SrcPath);
	Result->SetStringField(TEXT("destination"), DestPath);
	Result->SetStringField(TEXT("note"), TEXT("Layout copied. Restart the editor for the new layout to be applied to existing windows."));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ListWorkspaceLayouts::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const FString Dir = LayoutDir();

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.ini")), /*Files=*/true, /*Dirs=*/false);

	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FString& File : Files)
	{
		Names.Add(MakeShared<FJsonValueString>(FPaths::GetBaseFilename(File)));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("directory"), Dir);
	Result->SetArrayField(TEXT("layouts"), Names);
	Result->SetNumberField(TEXT("count"), Names.Num());
	return FECACommandResult::Success(Result);
}
