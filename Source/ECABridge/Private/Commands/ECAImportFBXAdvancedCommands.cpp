// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAImportFBXAdvancedCommands.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Skeleton.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	EFBXNormalImportMethod ParseNormalImport(const FString& S)
	{
		if (S.Equals(TEXT("ImportNormals"), ESearchCase::IgnoreCase))            return FBXNIM_ImportNormals;
		if (S.Equals(TEXT("ImportNormalsAndTangents"), ESearchCase::IgnoreCase)) return FBXNIM_ImportNormalsAndTangents;
		return FBXNIM_ComputeNormals;
	}
	EFBXNormalGenerationMethod::Type ParseNormalGen(const FString& S)
	{
		if (S.Equals(TEXT("BuiltIn"), ESearchCase::IgnoreCase)) return EFBXNormalGenerationMethod::BuiltIn;
		return EFBXNormalGenerationMethod::MikkTSpace;
	}
}

FECACommandResult FECACommand_ImportFBXAdvanced::Execute(const TSharedPtr<FJsonObject>& Params)
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
	if (Extension != TEXT("fbx") && Extension != TEXT("obj"))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("File extension '%s' is not .fbx (or .obj). Use import_gltf for glTF."), *Extension));
	}

	FString AssetName;
	GetStringParam(Params, TEXT("asset_name"), AssetName, /*bRequired=*/ false);
	bool bReplace = true;
	GetBoolParam(Params, TEXT("replace_existing"), bReplace, /*bRequired=*/ false);
	bool bSaveAfter = false;
	GetBoolParam(Params, TEXT("save_after_import"), bSaveAfter, /*bRequired=*/ false);

	bool bImportAsSkeletal = false;       GetBoolParam(Params, TEXT("import_as_skeletal"), bImportAsSkeletal, false);
	bool bImportMaterials = true;         GetBoolParam(Params, TEXT("import_materials"), bImportMaterials, false);
	bool bImportTextures = true;          GetBoolParam(Params, TEXT("import_textures"), bImportTextures, false);
	bool bImportAnimations = false;       GetBoolParam(Params, TEXT("import_animations"), bImportAnimations, false);
	bool bCreatePhysicsAsset = true;      GetBoolParam(Params, TEXT("create_physics_asset"), bCreatePhysicsAsset, false);
	FString SkeletonPath;                 GetStringParam(Params, TEXT("skeleton_path"), SkeletonPath, false);
	FString OverrideAnimName;             GetStringParam(Params, TEXT("override_animation_name"), OverrideAnimName, false);

	double LodCountD = 0.0;
	if (Params.IsValid()) Params->TryGetNumberField(TEXT("lod_count"), LodCountD);
	const int32 LodCount = FMath::Max(0, static_cast<int32>(LodCountD));

	bool bAutoLodScreenSize = true;       GetBoolParam(Params, TEXT("auto_compute_lod_screen_size"), bAutoLodScreenSize, false);
	bool bRemoveDegenerates = true;       GetBoolParam(Params, TEXT("remove_degenerates"), bRemoveDegenerates, false);
	bool bBuildReversedIB = true;         GetBoolParam(Params, TEXT("build_reversed_index_buffer"), bBuildReversedIB, false);
	bool bGenerateLightmapUVs = true;     GetBoolParam(Params, TEXT("generate_lightmap_uvs"), bGenerateLightmapUVs, false);
	bool bBuildNanite = false;            GetBoolParam(Params, TEXT("build_nanite"), bBuildNanite, false);
	FString LodGroup;                     GetStringParam(Params, TEXT("static_mesh_lod_group"), LodGroup, false);

	FString NormalImportStr = TEXT("ComputeNormals");
	GetStringParam(Params, TEXT("normal_import_method"), NormalImportStr, false);
	FString NormalGenStr = TEXT("MikkTSpace");
	GetStringParam(Params, TEXT("normal_generation_method"), NormalGenStr, false);

	bool bAnimRemoveRedundant = true;     GetBoolParam(Params, TEXT("anim_remove_redundant_keys"), bAnimRemoveRedundant, false);
	bool bAnimDefaultSampleRate = false;  GetBoolParam(Params, TEXT("anim_use_default_sample_rate"), bAnimDefaultSampleRate, false);

	UFbxImportUI* ImportUI = NewObject<UFbxImportUI>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!ImportUI)
	{
		return FECACommandResult::Error(TEXT("Failed to allocate UFbxImportUI."));
	}
	ImportUI->bImportAsSkeletal = bImportAsSkeletal;
	ImportUI->MeshTypeToImport = bImportAsSkeletal ? FBXIT_SkeletalMesh : FBXIT_StaticMesh;
	ImportUI->OriginalImportType = ImportUI->MeshTypeToImport;
	ImportUI->bImportMaterials = bImportMaterials;
	ImportUI->bImportTextures = bImportTextures;
	ImportUI->bImportAnimations = bImportAnimations;
	ImportUI->bCreatePhysicsAsset = bCreatePhysicsAsset;
	ImportUI->bAutomatedImportShouldDetectType = false;
	if (!OverrideAnimName.IsEmpty())
	{
		ImportUI->OverrideAnimationName = OverrideAnimName;
	}
	if (!SkeletonPath.IsEmpty())
	{
		ImportUI->Skeleton = Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath, nullptr, LOAD_NoWarn));
	}
	if (LodCount > 0)
	{
		ImportUI->LodNumber = LodCount;
	}
	ImportUI->bAutoComputeLodDistances = bAutoLodScreenSize;

	if (UFbxStaticMeshImportData* SM = ImportUI->StaticMeshImportData)
	{
		SM->bRemoveDegenerates = bRemoveDegenerates;
		SM->bBuildReversedIndexBuffer = bBuildReversedIB;
		SM->bGenerateLightmapUVs = bGenerateLightmapUVs;
		SM->bBuildNanite = bBuildNanite;
		SM->NormalImportMethod = ParseNormalImport(NormalImportStr);
		SM->NormalGenerationMethod = ParseNormalGen(NormalGenStr);
		if (!LodGroup.IsEmpty())
		{
			SM->StaticMeshLODGroup = FName(*LodGroup);
		}
	}
	if (UFbxSkeletalMeshImportData* SK = ImportUI->SkeletalMeshImportData)
	{
		SK->NormalImportMethod = ParseNormalImport(NormalImportStr);
		SK->NormalGenerationMethod = ParseNormalGen(NormalGenStr);
	}
	if (UFbxAnimSequenceImportData* Anim = ImportUI->AnimSequenceImportData)
	{
		Anim->bRemoveRedundantKeys = bAnimRemoveRedundant;
		Anim->bUseDefaultSampleRate = bAnimDefaultSampleRate;
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage(), NAME_None, RF_Transient);
	Task->Filename = SourcePath;
	Task->DestinationPath = DestinationPath;
	Task->bReplaceExisting = bReplace;
	Task->bAutomated = true;
	Task->bSave = bSaveAfter;
	if (!AssetName.IsEmpty())
	{
		Task->DestinationName = AssetName;
	}
	UFbxFactory* Factory = NewObject<UFbxFactory>(GetTransientPackage(), NAME_None, RF_Transient);
	Factory->ImportUI = ImportUI;
	Factory->SetDetectImportTypeOnImport(false);
	Task->Factory = Factory;
	Task->Options = ImportUI;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<UAssetImportTask*> Tasks = { Task };
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source_file"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	Result->SetBoolField(TEXT("import_as_skeletal"), bImportAsSkeletal);

	TArray<TSharedPtr<FJsonValue>> ImportedArr;
	int32 StaticMeshes = 0, SkeletalMeshes = 0, Materials = 0, Textures = 0, Animations = 0, Other = 0;
	for (const FString& ObjPath : Task->ImportedObjectPaths)
	{
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjPath, nullptr, LOAD_NoWarn);
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("path"), ObjPath);
		if (Obj)
		{
			Entry->SetStringField(TEXT("name"), Obj->GetName());
			Entry->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
			if (Cast<UStaticMesh>(Obj))            { ++StaticMeshes; Entry->SetStringField(TEXT("type"), TEXT("StaticMesh")); }
			else if (Cast<USkeletalMesh>(Obj))     { ++SkeletalMeshes; Entry->SetStringField(TEXT("type"), TEXT("SkeletalMesh")); }
			else if (Cast<UMaterialInterface>(Obj)){ ++Materials; Entry->SetStringField(TEXT("type"), TEXT("Material")); }
			else if (Cast<UTexture>(Obj))          { ++Textures; Entry->SetStringField(TEXT("type"), TEXT("Texture")); }
			else if (Obj->GetClass()->GetName().Contains(TEXT("AnimSequence"))) { ++Animations; Entry->SetStringField(TEXT("type"), TEXT("Animation")); }
			else { ++Other; Entry->SetStringField(TEXT("type"), TEXT("Other")); }
		}
		ImportedArr.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("imported_objects"), ImportedArr);
	Result->SetNumberField(TEXT("imported_count"), ImportedArr.Num());

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("static_meshes"), StaticMeshes);
	Summary->SetNumberField(TEXT("skeletal_meshes"), SkeletalMeshes);
	Summary->SetNumberField(TEXT("materials"), Materials);
	Summary->SetNumberField(TEXT("textures"), Textures);
	Summary->SetNumberField(TEXT("animations"), Animations);
	Summary->SetNumberField(TEXT("other"), Other);
	Summary->SetNumberField(TEXT("total"), ImportedArr.Num());
	Result->SetObjectField(TEXT("summary"), Summary);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ImportFBXAdvanced);
