// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAImportGLTFCommands.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

FECACommandResult FECACommand_ImportGLTF::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath;
	if (!GetStringParam(Params, TEXT("source_path"), SourcePath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'source_path' (string) is required."));
	}
	FString DestinationPath;
	if (!GetStringParam(Params, TEXT("destination_path"), DestinationPath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'destination_path' (string) is required."));
	}

	FString AssetName;
	GetStringParam(Params, TEXT("asset_name"), AssetName, /*bRequired=*/ false);
	bool bReplaceExisting = true;
	GetBoolParam(Params, TEXT("replace_existing"), bReplaceExisting, /*bRequired=*/ false);

	if (!FPaths::FileExists(SourcePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source file does not exist: %s"), *SourcePath));
	}
	const FString Extension = FPaths::GetExtension(SourcePath).ToLower();
	if (Extension != TEXT("gltf") && Extension != TEXT("glb"))
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("File extension '%s' is not glTF (expected .gltf or .glb)."), *Extension));
	}

	if (AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourcePath);
	}
	if (!DestinationPath.EndsWith(TEXT("/")))
	{
		DestinationPath += TEXT("/");
	}

	if (!UInterchangeManager::IsInterchangeImportEnabled())
	{
		return FECACommandResult::Error(TEXT("Interchange import is disabled in Project Settings."));
	}

	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
	UInterchangeSourceData* SourceData = UInterchangeManager::CreateSourceData(SourcePath);
	if (!SourceData)
	{
		return FECACommandResult::Error(TEXT("Failed to create Interchange source data for glTF file."));
	}
	if (!InterchangeManager.CanTranslateSourceData(SourceData))
	{
		return FECACommandResult::Error(TEXT("No Interchange translator is registered for this glTF file. Ensure the Interchange glTF translator (InterchangeEditor / InterchangePipelines) is enabled."));
	}

	FImportAssetParameters ImportAssetParameters;
	ImportAssetParameters.bIsAutomated = true;
	ImportAssetParameters.bReplaceExisting = bReplaceExisting;
	ImportAssetParameters.DestinationName = AssetName;

	TArray<UObject*> ImportedObjects;
	const bool bImportStarted = InterchangeManager.ImportAsset(DestinationPath, SourceData, ImportAssetParameters, ImportedObjects);
	if (!bImportStarted)
	{
		return FECACommandResult::Error(TEXT("Interchange refused to start the glTF import."));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source_file"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	Result->SetStringField(TEXT("extension"), Extension);
	Result->SetNumberField(TEXT("imported_count"), ImportedObjects.Num());

	TArray<TSharedPtr<FJsonValue>> ImportedArray;
	int32 StaticMeshes = 0, SkeletalMeshes = 0, Materials = 0, Textures = 0, Animations = 0, Other = 0;
	FString PrimaryMeshPath;
	for (UObject* Obj : ImportedObjects)
	{
		if (!Obj) continue;
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Obj->GetName());
		Entry->SetStringField(TEXT("path"), Obj->GetPathName());
		Entry->SetStringField(TEXT("class"), Obj->GetClass()->GetName());

		if (Cast<UStaticMesh>(Obj))
		{
			++StaticMeshes;
			Entry->SetStringField(TEXT("type"), TEXT("StaticMesh"));
			if (PrimaryMeshPath.IsEmpty()) PrimaryMeshPath = Obj->GetPathName();
		}
		else if (Cast<USkeletalMesh>(Obj))
		{
			++SkeletalMeshes;
			Entry->SetStringField(TEXT("type"), TEXT("SkeletalMesh"));
			if (PrimaryMeshPath.IsEmpty()) PrimaryMeshPath = Obj->GetPathName();
		}
		else if (Cast<UMaterialInterface>(Obj))
		{
			++Materials;
			Entry->SetStringField(TEXT("type"), TEXT("Material"));
		}
		else if (Cast<UTexture>(Obj))
		{
			++Textures;
			Entry->SetStringField(TEXT("type"), TEXT("Texture"));
		}
		else if (Obj->GetClass()->GetName().Contains(TEXT("AnimSequence")))
		{
			++Animations;
			Entry->SetStringField(TEXT("type"), TEXT("Animation"));
		}
		else
		{
			++Other;
			Entry->SetStringField(TEXT("type"), TEXT("Other"));
		}
		ImportedArray.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("imported_objects"), ImportedArray);

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("static_meshes"), StaticMeshes);
	Summary->SetNumberField(TEXT("skeletal_meshes"), SkeletalMeshes);
	Summary->SetNumberField(TEXT("materials"), Materials);
	Summary->SetNumberField(TEXT("textures"), Textures);
	Summary->SetNumberField(TEXT("animations"), Animations);
	Summary->SetNumberField(TEXT("other"), Other);
	Summary->SetNumberField(TEXT("total"), ImportedObjects.Num());
	Result->SetObjectField(TEXT("summary"), Summary);

	if (!PrimaryMeshPath.IsEmpty())
	{
		Result->SetStringField(TEXT("primary_mesh_path"), PrimaryMeshPath);
	}

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ImportGLTF);
