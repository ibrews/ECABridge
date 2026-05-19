// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAContentBrowserFilterCommands.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

REGISTER_ECA_COMMAND(FECACommand_SetContentBrowserFilter)
REGISTER_ECA_COMMAND(FECACommand_ClearContentBrowserFilter)

namespace
{
	IContentBrowserSingleton* GetCB()
	{
		FContentBrowserModule* Module = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
		if (!Module) return nullptr;
		return &Module->Get();
	}
}

FECACommandResult FECACommand_SetContentBrowserFilter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = TEXT("/Game");
	GetStringParam(Params, TEXT("path"), Path, false);
	if (Path.IsEmpty()) Path = TEXT("/Game");

	FString AssetType;
	GetStringParam(Params, TEXT("asset_type"), AssetType, false);

	IContentBrowserSingleton* CB = GetCB();
	if (!CB)
	{
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	}

	// Sync the browser to the requested folder.
	TArray<FString> FolderPaths = { Path };
	CB->SyncBrowserToFolders(FolderPaths);

	// Apply the asset-type keyword via the search bar so the user sees a typed filter.
	if (!AssetType.IsEmpty())
	{
		CB->SetSearchText(FText::FromString(AssetType));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), Path);
	Result->SetStringField(TEXT("asset_type"), AssetType);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ClearContentBrowserFilter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	IContentBrowserSingleton* CB = GetCB();
	if (!CB)
	{
		return FECACommandResult::Error(TEXT("ContentBrowser module not available."));
	}

	CB->SetSearchText(FText::GetEmpty());

	TArray<FString> FolderPaths = { TEXT("/Game") };
	CB->SyncBrowserToFolders(FolderPaths);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), TEXT("/Game"));
	return FECACommandResult::Success(Result);
}
