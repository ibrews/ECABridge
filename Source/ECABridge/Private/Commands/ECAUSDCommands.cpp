// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAUSDCommands.h"
#include "AssetImportTask.h"
#include "AssetExportTask.h"
#include "AssetToolsModule.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Exporters/Exporter.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	bool IsUsdExtension(const FString& Ext)
	{
		return Ext.Equals(TEXT("usd"),  ESearchCase::IgnoreCase)
			|| Ext.Equals(TEXT("usda"), ESearchCase::IgnoreCase)
			|| Ext.Equals(TEXT("usdc"), ESearchCase::IgnoreCase)
			|| Ext.Equals(TEXT("usdz"), ESearchCase::IgnoreCase);
	}

	AActor* FindActorByLabel(const FString& Label)
	{
		if (!GEditor) return nullptr;
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (A && A->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
			{
				return A;
			}
		}
		return nullptr;
	}

	UObject* ResolveMeshAsset(AActor* Actor)
	{
		if (!Actor) return nullptr;
		if (UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* SM = SMC->GetStaticMesh()) return SM;
		}
		if (USkeletalMeshComponent* SkC = Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (USkeletalMesh* Sk = SkC->GetSkeletalMeshAsset()) return Sk;
		}
		return nullptr;
	}
}

FECACommandResult FECACommand_ImportUSD::Execute(const TSharedPtr<FJsonObject>& Params)
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

	if (!FPaths::FileExists(SourcePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source file does not exist: %s"), *SourcePath));
	}
	const FString Extension = FPaths::GetExtension(SourcePath).ToLower();
	if (!IsUsdExtension(Extension))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("File extension '%s' is not a USD format (.usd/.usda/.usdc/.usdz)."), *Extension));
	}

#if WITH_ECA_USD
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("USDStageImporter")))
	{
		return FECACommandResult::Error(TEXT("USDStageImporter module is not loaded. Enable the USDImporter plugin in your .uproject."));
	}

	FString AssetName;
	GetStringParam(Params, TEXT("asset_name"), AssetName, /*bRequired=*/ false);
	bool bReplace = true;
	GetBoolParam(Params, TEXT("replace_existing"), bReplace, /*bRequired=*/ false);
	bool bSave = false;
	GetBoolParam(Params, TEXT("save_after_import"), bSave, /*bRequired=*/ false);

	UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage(), NAME_None, RF_Transient);
	Task->Filename = SourcePath;
	Task->DestinationPath = DestinationPath;
	Task->bReplaceExisting = bReplace;
	Task->bAutomated = true;
	Task->bSave = bSave;
	if (!AssetName.IsEmpty())
	{
		Task->DestinationName = AssetName;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<UAssetImportTask*> Tasks = { Task };
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	TArray<TSharedPtr<FJsonValue>> ImportedArr;
	for (const FString& ObjPath : Task->ImportedObjectPaths)
	{
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjPath, nullptr, LOAD_NoWarn);
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("path"), ObjPath);
		if (Obj)
		{
			Entry->SetStringField(TEXT("name"), Obj->GetName());
			Entry->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
		}
		ImportedArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("usd_available"), true);
	Result->SetStringField(TEXT("source_file"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	Result->SetStringField(TEXT("extension"), Extension);
	Result->SetNumberField(TEXT("imported_count"), ImportedArr.Num());
	Result->SetArrayField(TEXT("imported_objects"), ImportedArr);
	return FECACommandResult::Success(Result);
#else
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("usd_available"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without USD support (WITH_ECA_USD=0). Build against an engine that ships the USDImporter plugin to use import_usd."));
	return FECACommandResult::Success(Result);
#endif
}

FECACommandResult FECACommand_ExportActorAsUSD::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'asset_path' (string) is required."));
	}
	FString OutputPath;
	if (!GetStringParam(Params, TEXT("output_path"), OutputPath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'output_path' (string) is required."));
	}
	bool bIsActorLabel = false;
	GetBoolParam(Params, TEXT("is_actor_label"), bIsActorLabel, /*bRequired=*/ false);

	const FString OutExt = FPaths::GetExtension(OutputPath).ToLower();
	if (!IsUsdExtension(OutExt))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Output extension '%s' is not USD (.usd/.usda/.usdc/.usdz)."), *OutExt));
	}

#if WITH_ECA_USD
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("USDExporter")))
	{
		return FECACommandResult::Error(TEXT("USDExporter module is not loaded. Enable the USDImporter plugin in your .uproject."));
	}

	UObject* Target = nullptr;
	if (bIsActorLabel)
	{
		AActor* Actor = FindActorByLabel(AssetPath);
		if (!Actor)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("No actor with label '%s' in the editor world."), *AssetPath));
		}
		Target = ResolveMeshAsset(Actor);
		if (!Target)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' does not reference a StaticMesh or SkeletalMesh asset to export."), *AssetPath));
		}
	}
	else
	{
		Target = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath, nullptr, LOAD_NoWarn);
		if (!Target)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset '%s'."), *AssetPath));
		}
	}

	UExporter* Exporter = UExporter::FindExporter(Target, *OutExt);
	if (!Exporter)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("No UExporter registered for %s -> %s. Ensure the USDExporter module's exporters are registered (USDImporter plugin)."),
			*Target->GetClass()->GetName(), *OutExt));
	}

	UAssetExportTask* Task = NewObject<UAssetExportTask>(GetTransientPackage(), NAME_None, RF_Transient);
	Task->Object = Target;
	Task->Exporter = Exporter;
	Task->Filename = OutputPath;
	Task->bSelected = false;
	Task->bReplaceIdentical = true;
	Task->bPrompt = false;
	Task->bUseFileArchive = Exporter->bText ? false : true;
	Task->bWriteEmptyFiles = false;
	Task->bAutomated = true;

	const bool bOk = UExporter::RunAssetExportTask(Task);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("usd_available"), true);
	Result->SetBoolField(TEXT("success"), bOk);
	Result->SetStringField(TEXT("exporter"), Exporter->GetClass()->GetName());
	Result->SetStringField(TEXT("object_path"), Target->GetPathName());
	Result->SetStringField(TEXT("object_class"), Target->GetClass()->GetName());
	Result->SetStringField(TEXT("output_path"), OutputPath);
	return FECACommandResult::Success(Result);
#else
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("usd_available"), false);
	Result->SetStringField(TEXT("message"), TEXT("ECABridge was built without USD support (WITH_ECA_USD=0). Build against an engine that ships the USDImporter plugin to use export_actor_as_usd."));
	return FECACommandResult::Success(Result);
#endif
}

REGISTER_ECA_COMMAND(FECACommand_ImportUSD);
REGISTER_ECA_COMMAND(FECACommand_ExportActorAsUSD);
