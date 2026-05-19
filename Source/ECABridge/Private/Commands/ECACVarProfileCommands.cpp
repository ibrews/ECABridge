// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECACVarProfileCommands.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

REGISTER_ECA_COMMAND(FECACommand_SaveCVarProfile)
REGISTER_ECA_COMMAND(FECACommand_LoadCVarProfile)
REGISTER_ECA_COMMAND(FECACommand_ListCVarProfiles)

namespace
{
	FString ProfileDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("CVarProfiles");
	}

	FString ProfilePath(const FString& Name)
	{
		return ProfileDir() / (Name + TEXT(".json"));
	}

	bool IsValidProfileName(const FString& Name)
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

FECACommandResult FECACommand_SaveCVarProfile::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	if (!IsValidProfileName(Name))
	{
		return FECACommandResult::Error(TEXT("Profile name contains invalid path characters."));
	}

	const TArray<TSharedPtr<FJsonValue>>* CVarListPtr = nullptr;
	if (!GetArrayParam(Params, TEXT("cvar_list"), CVarListPtr) || !CVarListPtr)
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: cvar_list"));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), Name);
	Root->SetStringField(TEXT("saved_at"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
	int32 Captured = 0;
	TArray<TSharedPtr<FJsonValue>> Missing;
	for (const TSharedPtr<FJsonValue>& V : *CVarListPtr)
	{
		const FString CVarName = V->AsString();
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
		if (!CVar)
		{
			Missing.Add(MakeShared<FJsonValueString>(CVarName));
			continue;
		}
		Values->SetStringField(CVarName, CVar->GetString());
		Captured++;
	}
	Root->SetObjectField(TEXT("values"), Values);
	if (Missing.Num() > 0)
	{
		Root->SetArrayField(TEXT("missing"), Missing);
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	IFileManager::Get().MakeDirectory(*ProfileDir(), /*Tree=*/true);
	const FString Path = ProfilePath(Name);
	if (!FFileHelper::SaveStringToFile(Serialized, *Path))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to write profile to: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), Path);
	Result->SetNumberField(TEXT("captured"), Captured);
	Result->SetNumberField(TEXT("missing"), Missing.Num());
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_LoadCVarProfile::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}
	if (!IsValidProfileName(Name))
	{
		return FECACommandResult::Error(TEXT("Profile name contains invalid path characters."));
	}

	const FString Path = ProfilePath(Name);
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *Path))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Profile not found: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FECACommandResult::Error(TEXT("Profile file is not valid JSON."));
	}

	const TSharedPtr<FJsonObject>* ValuesPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("values"), ValuesPtr) || !ValuesPtr || !ValuesPtr->IsValid())
	{
		return FECACommandResult::Error(TEXT("Profile JSON missing 'values' object."));
	}

	int32 Applied = 0;
	TArray<TSharedPtr<FJsonValue>> Skipped;
	for (const auto& Pair : (*ValuesPtr)->Values)
	{
		const FString CVarName(Pair.Key);
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
		if (!CVar)
		{
			Skipped.Add(MakeShared<FJsonValueString>(CVarName));
			continue;
		}
		const FString StoredValue = Pair.Value.IsValid() ? Pair.Value->AsString() : FString();
		CVar->Set(*StoredValue, ECVF_SetByConsole);
		Applied++;
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), Path);
	Result->SetNumberField(TEXT("applied"), Applied);
	Result->SetNumberField(TEXT("skipped"), Skipped.Num());
	if (Skipped.Num() > 0)
	{
		Result->SetArrayField(TEXT("skipped_cvars"), Skipped);
	}
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ListCVarProfiles::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const FString Dir = ProfileDir();

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), /*Files=*/true, /*Dirs=*/false);

	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FString& File : Files)
	{
		Names.Add(MakeShared<FJsonValueString>(FPaths::GetBaseFilename(File)));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("directory"), Dir);
	Result->SetArrayField(TEXT("profiles"), Names);
	Result->SetNumberField(TEXT("count"), Names.Num());
	return FECACommandResult::Success(Result);
}
