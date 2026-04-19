// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMetaHumanCommands.h"
#include "Commands/ECACommand.h"

// MetaHuman plugin headers — these are included at compile time because the
// MetaHumanCharacter module is listed in ECABridge.Build.cs. If the user has
// disabled the plugin at runtime, FindObject<UClass> will return null and
// each command will return a clean error before attempting to use these types.
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterSkin.h"
#include "MetaHumanCharacterEyes.h"
#include "MetaHumanCharacterMakeup.h"
#include "MetaHumanCharacterTeeth.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// Forward declarations for helpers that live further down in the file
static UObject* LoadMHCharacter(const FString& Path, FString& OutErrorMessage);
static UEditorSubsystem* GetMHEditorSubsystem(FString& OutErrorMessage);
static void OpenCharacterEditor(UObject* Character);

// Register all MetaHuman commands
REGISTER_ECA_COMMAND(FECACommand_CreateMetaHumanCharacter)
REGISTER_ECA_COMMAND(FECACommand_DumpMetaHumanCharacter)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanProperty)
REGISTER_ECA_COMMAND(FECACommand_DescribeMetaHuman)
REGISTER_ECA_COMMAND(FECACommand_OpenMetaHumanEditor)
REGISTER_ECA_COMMAND(FECACommand_BuildMetaHuman)
REGISTER_ECA_COMMAND(FECACommand_DownloadMetaHumanTextures)
REGISTER_ECA_COMMAND(FECACommand_RigMetaHuman)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanPreviewMode)
REGISTER_ECA_COMMAND(FECACommand_SpawnMetaHumanActor)
REGISTER_ECA_COMMAND(FECACommand_GetMetaHumanBodyConstraints)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanBodyConstraints)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanBodyType)
REGISTER_ECA_COMMAND(FECACommand_AttachMetaHumanGroom)
REGISTER_ECA_COMMAND(FECACommand_ListMetaHumanGrooms)
REGISTER_ECA_COMMAND(FECACommand_ListMetaHumanPresets)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanFacePreset)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanMakeup)
REGISTER_ECA_COMMAND(FECACommand_RefreshMetaHumanPreview)
REGISTER_ECA_COMMAND(FECACommand_TakeMetaHumanEditorScreenshot)
REGISTER_ECA_COMMAND(FECACommand_TintMetaHumanOutfit)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanSkinParams)

// ============================================================================
// tint_metahuman_outfit
// ============================================================================

static FLinearColor ReadColorField(const TSharedPtr<FJsonObject>& Obj)
{
	double R = 1, G = 1, B = 1, A = 1;
	Obj->TryGetNumberField(TEXT("r"), R);
	Obj->TryGetNumberField(TEXT("g"), G);
	Obj->TryGetNumberField(TEXT("b"), B);
	Obj->TryGetNumberField(TEXT("a"), A);
	return FLinearColor((float)R, (float)G, (float)B, (float)A);
}

FECACommandResult FECACommand_TintMetaHumanOutfit::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing actor_name"));

	bool bHasShirt = false, bHasShorts = false;
	FLinearColor ShirtColor(1, 1, 1, 1), ShortsColor(1, 1, 1, 1);
	const TSharedPtr<FJsonObject>* ShirtObj = nullptr;
	if (Params->TryGetObjectField(TEXT("shirt_color"), ShirtObj) && ShirtObj && ShirtObj->IsValid())
	{
		ShirtColor = ReadColorField(*ShirtObj);
		bHasShirt = true;
	}
	const TSharedPtr<FJsonObject>* ShortsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("shorts_color"), ShortsObj) && ShortsObj && ShortsObj->IsValid())
	{
		ShortsColor = ReadColorField(*ShortsObj);
		bHasShorts = true;
	}
	if (!bHasShirt && !bHasShorts)
		return FECACommandResult::Error(TEXT("Provide at least one of shirt_color, shorts_color"));

	// Find actor in editor world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FECACommandResult::Error(TEXT("No editor world"));

	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && (A->GetName() == ActorName || A->GetActorLabel() == ActorName))
		{
			TargetActor = A;
			break;
		}
	}
	if (!TargetActor) return FECACommandResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));

	// Find all mesh components (SkeletalMeshComponent + StaticMeshComponent for outfit)
	TArray<UMeshComponent*> MeshComps;
	TargetActor->GetComponents<UMeshComponent>(MeshComps);

	TArray<TSharedPtr<FJsonValue>> AppliedSlots;
	int32 TintedCount = 0;

	for (UMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp) continue;
		int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 SlotIdx = 0; SlotIdx < NumMaterials; SlotIdx++)
		{
			UMaterialInterface* Mat = MeshComp->GetMaterial(SlotIdx);
			if (!Mat) continue;
			FString MatName = Mat->GetName();
			FString SlotName = MeshComp->GetMaterialSlotNames().IsValidIndex(SlotIdx)
				? MeshComp->GetMaterialSlotNames()[SlotIdx].ToString() : FString();

			// Classify. Priority:
			//   explicit "Shirt"/"Top" material name -> shirt
			//   explicit "Short"/"Pant"/"Bottom" -> shorts
			//   generic Body material (no face/eye/hair/teeth/hide) -> treat as "shirt" (full-body outfit)
			bool bIsShirt = MatName.Contains(TEXT("Shirt")) || MatName.Contains(TEXT("Top"))
				|| SlotName.Contains(TEXT("Shirt")) || SlotName.Contains(TEXT("Top"));
			bool bIsShorts = MatName.Contains(TEXT("Shorts")) || MatName.Contains(TEXT("Pant")) || MatName.Contains(TEXT("Bottom"))
				|| SlotName.Contains(TEXT("Shorts")) || SlotName.Contains(TEXT("Pant")) || SlotName.Contains(TEXT("Bottom"));

			// Fallback: treat the Body material on a skeletal mesh as the outfit proxy
			// (editor preview actors use one Body material covering where the outfit would be)
			bool bIsBodyProxy = false;
			FString CompName = MeshComp->GetName();
			if (!bIsShirt && !bIsShorts && CompName.Equals(TEXT("Body"), ESearchCase::IgnoreCase))
			{
				bIsBodyProxy = true;
			}

			FLinearColor Color;
			bool bShouldTint = false;
			const TCHAR* Category = TEXT("");
			if (bIsShirt && bHasShirt) { Color = ShirtColor; bShouldTint = true; Category = TEXT("shirt"); }
			else if (bIsShorts && bHasShorts) { Color = ShortsColor; bShouldTint = true; Category = TEXT("shorts"); }
			else if (bIsBodyProxy && bHasShirt) { Color = ShirtColor; bShouldTint = true; Category = TEXT("body_outfit_proxy"); }
			if (!bShouldTint) continue;

			// Create a dynamic material instance and set a common color parameter
			UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIdx);
			if (!MID) continue;

			// Try several common parameter names that clothing materials use
			const TCHAR* ColorParamNames[] = { TEXT("BaseColor"), TEXT("Color"), TEXT("Tint"), TEXT("MainColor"), TEXT("ClothColor"), TEXT("Albedo") };
			for (const TCHAR* ParamName : ColorParamNames)
			{
				MID->SetVectorParameterValue(FName(ParamName), Color);
			}
			// Also drive any parameter that has "Color" in its name dynamically
			TArray<FMaterialParameterInfo> VectorParams;
			TArray<FGuid> VectorGuids;
			MID->GetAllVectorParameterInfo(VectorParams, VectorGuids);
			for (const FMaterialParameterInfo& Info : VectorParams)
			{
				FString PName = Info.Name.ToString();
				if (PName.Contains(TEXT("Color")) || PName.Contains(TEXT("Tint")) || PName.Contains(TEXT("Albedo")))
				{
					MID->SetVectorParameterValueByInfo(Info, Color);
				}
			}

			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("slot_name"), SlotName);
			Entry->SetStringField(TEXT("material"), MatName);
			Entry->SetStringField(TEXT("category"), Category);
			Entry->SetStringField(TEXT("color"), FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f"), Color.R, Color.G, Color.B, Color.A));
			AppliedSlots.Add(MakeShared<FJsonValueObject>(Entry));
			TintedCount++;
		}
		MeshComp->MarkRenderStateDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("mesh_components"), MeshComps.Num());
	Result->SetNumberField(TEXT("slots_tinted"), TintedCount);
	Result->SetArrayField(TEXT("applied"), AppliedSlots);
	if (TintedCount == 0)
	{
		Result->SetStringField(TEXT("note"), TEXT("No matching material slots found. The actor may not be fully built yet — the pipeline assembles outfit meshes asynchronously. Try spawning the actor again after the build completes, or check that the character has an Outfit slot attached."));
	}
	return FECACommandResult::Success(Result);
}

// ============================================================================
// refresh_metahuman_preview
// ============================================================================

FECACommandResult FECACommand_RefreshMetaHumanPreview::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);
	OpenCharacterEditor(Character);

	UEditorSubsystem* Subsystem = GetMHEditorSubsystem(Err);
	if (!Subsystem) return FECACommandResult::Error(Err);

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("RunCharacterEditorPipelineForPreview")));
	if (!Func) return FECACommandResult::Error(TEXT("RunCharacterEditorPipelineForPreview not found"));

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(Buffer.GetData());
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
		{
			ObjProp->SetObjectPropertyValue_InContainer(Buffer.GetData(), Character);
			break;
		}
	}
	Subsystem->ProcessEvent(Func, Buffer.GetData());
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(Buffer.GetData());

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetBoolField(TEXT("triggered"), true);
	Result->SetStringField(TEXT("note"), TEXT("Preview pipeline re-run at Preview quality. Async — give it a few seconds for skin/hair/makeup to re-render."));
	return FECACommandResult::Success(Result);
}

// ============================================================================
// take_metahuman_editor_screenshot
// ============================================================================

FECACommandResult FECACommand_TakeMetaHumanEditorScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, FilePath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath) || FilePath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing file_path"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);

	// Open the MetaHuman editor and bring its tab to focus
	OpenCharacterEditor(Character);

	// Give Slate a frame to redraw
	FSlateApplication::Get().Tick();

	// Find a target window. Strategy:
	//   1. Look for a top-level window whose title contains the character name (floating MH editor)
	//   2. Fall back to the active top-level window
	//   3. Fall back to the first visible interactive top-level window (main editor)
	//   4. Fall back to the root window from the Slate renderer
	TSharedPtr<SWindow> TargetWindow;
	FString CharacterName = Character->GetName();
	TArray<TSharedRef<SWindow>> AllWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(AllWindows);
	for (const TSharedRef<SWindow>& Win : AllWindows)
	{
		FString Title = Win->GetTitle().ToString();
		if (Title.Contains(CharacterName))
		{
			TargetWindow = Win;
			break;
		}
	}
	if (!TargetWindow.IsValid())
	{
		TargetWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	}
	if (!TargetWindow.IsValid() && AllWindows.Num() > 0)
	{
		// Pick the first regular visible window (skip tooltips etc.)
		for (const TSharedRef<SWindow>& Win : AllWindows)
		{
			if (Win->IsRegularWindow() && Win->IsVisible())
			{
				TargetWindow = Win;
				break;
			}
		}
	}
	if (!TargetWindow.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not find a window (visible window count: %d)"), AllWindows.Num()));
	}

	// Capture the whole window contents
	TArray<FColor> ColorBuffer;
	FIntVector OutSize;
	bool bCaptured = FSlateApplication::Get().TakeScreenshot(TargetWindow.ToSharedRef(), ColorBuffer, OutSize);
	if (!bCaptured || ColorBuffer.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("TakeScreenshot returned no pixels"));
	}

	// Ensure directory exists
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	// Encode PNG via FImageUtils
	TArray64<uint8> PngData;
	FImageView ImgView(ColorBuffer.GetData(), OutSize.X, OutSize.Y, 1, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	bool bSaved = FImageUtils::CompressImage(PngData, TEXT("png"), ImgView);
	if (!bSaved)
	{
		return FECACommandResult::Error(TEXT("Failed to encode PNG"));
	}
	if (!FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PngData.GetData(), PngData.Num()), *FilePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to write file: %s"), *FilePath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("file_path"), FilePath);
	Result->SetNumberField(TEXT("width"), OutSize.X);
	Result->SetNumberField(TEXT("height"), OutSize.Y);
	Result->SetStringField(TEXT("window_title"), TargetWindow->GetTitle().ToString());
	return FECACommandResult::Success(Result);
}

// ============================================================================
// list_metahuman_grooms
// ============================================================================

FECACommandResult FECACommand_ListMetaHumanGrooms::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SlotFilter;
	Params->TryGetStringField(TEXT("slot_filter"), SlotFilter);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	UClass* WIClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterPalette.MetaHumanWardrobeItem"));
	if (WIClass) Filter.ClassPaths.Add(WIClass->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssets(Filter, AssetData);

	TMap<FString, TArray<TSharedPtr<FJsonValue>>> Grouped;
	for (const FAssetData& D : AssetData)
	{
		FString Path = D.GetObjectPathString();
		// Infer slot from path (e.g., /Bindings/Hair/ → Hair, /Bindings/Beards/ → Beard).
		// MetaHuman ships some folders pluralised (Beards, Mustaches); the slot FName the
		// editor expects is singular. Map both forms here.
		FString Slot = TEXT("Other");
		struct FSlotMap { const TCHAR* Folder; const TCHAR* Slot; };
		const FSlotMap KnownSlots[] = {
			{ TEXT("Hair"),       TEXT("Hair") },
			{ TEXT("Beards"),     TEXT("Beard") },
			{ TEXT("Beard"),      TEXT("Beard") },
			{ TEXT("Eyebrows"),   TEXT("Eyebrows") },
			{ TEXT("Eyelashes"),  TEXT("Eyelashes") },
			{ TEXT("Mustaches"),  TEXT("Mustache") },
			{ TEXT("Mustache"),   TEXT("Mustache") },
			{ TEXT("Peachfuzz"),  TEXT("Peachfuzz") },
			{ TEXT("Fuzz"),       TEXT("Peachfuzz") },
			{ TEXT("Clothing"),   TEXT("Outfit") },
			{ TEXT("Outfit"),     TEXT("Outfit") },
		};
		for (const FSlotMap& KS : KnownSlots)
		{
			if (Path.Contains(FString::Printf(TEXT("/%s/"), KS.Folder)))
			{
				Slot = KS.Slot;
				break;
			}
		}
		if (!SlotFilter.IsEmpty() && Slot != SlotFilter) continue;

		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("path"), Path);
		Item->SetStringField(TEXT("name"), D.AssetName.ToString());
		Grouped.FindOrAdd(Slot).Add(MakeShared<FJsonValueObject>(Item));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	int32 Total = 0;
	TSharedPtr<FJsonObject> ByCategory = MakeShared<FJsonObject>();
	for (auto& Pair : Grouped)
	{
		ByCategory->SetArrayField(Pair.Key, Pair.Value);
		Total += Pair.Value.Num();
	}
	Result->SetObjectField(TEXT("by_category"), ByCategory);
	Result->SetNumberField(TEXT("total"), Total);
	return FECACommandResult::Success(Result);
}

// ============================================================================
// list_metahuman_presets
// ============================================================================

FECACommandResult FECACommand_ListMetaHumanPresets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	UClass* MHClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (MHClass) Filter.ClassPaths.Add(MHClass->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/MetaHumanCharacter/Optional/Presets")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssets(Filter, AssetData);

	TArray<TSharedPtr<FJsonValue>> Presets;
	for (const FAssetData& D : AssetData)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), D.AssetName.ToString());
		Item->SetStringField(TEXT("path"), D.GetObjectPathString());
		Presets.Add(MakeShared<FJsonValueObject>(Item));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("presets"), Presets);
	Result->SetNumberField(TEXT("count"), Presets.Num());
	return FECACommandResult::Success(Result);
}

// ============================================================================
// set_metahuman_face_preset — copies HeadModelSettings + FaceEvaluationSettings
// ============================================================================

static bool CopyStructProperty(UObject* Src, UObject* Dst, const FString& PropName)
{
	FProperty* SrcProp = Src->GetClass()->FindPropertyByName(FName(*PropName));
	FProperty* DstProp = Dst->GetClass()->FindPropertyByName(FName(*PropName));
	if (!SrcProp || !DstProp) return false;

	void* SrcPtr = SrcProp->ContainerPtrToValuePtr<void>(Src);
	void* DstPtr = DstProp->ContainerPtrToValuePtr<void>(Dst);
	SrcProp->CopyCompleteValue(DstPtr, SrcPtr);
	return true;
}

FECACommandResult FECACommand_SetMetaHumanFacePreset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharPath, PresetPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharPath) || CharPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));
	if (!Params->TryGetStringField(TEXT("preset_path"), PresetPath) || PresetPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing preset_path"));

	bool bIncludeSkin = false, bIncludeEyes = false, bIncludeMakeup = false;
	Params->TryGetBoolField(TEXT("include_skin"), bIncludeSkin);
	Params->TryGetBoolField(TEXT("include_eyes"), bIncludeEyes);
	Params->TryGetBoolField(TEXT("include_makeup"), bIncludeMakeup);

	FString Err;
	UObject* Target = LoadMHCharacter(CharPath, Err);
	if (!Target) return FECACommandResult::Error(Err);

	UObject* Preset = LoadObject<UObject>(nullptr, *PresetPath);
	if (!Preset) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load preset: %s"), *PresetPath));
	UClass* MHClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (!Preset->IsA(MHClass))
		return FECACommandResult::Error(TEXT("preset_path is not a MetaHumanCharacter"));

	TArray<TSharedPtr<FJsonValue>> Copied;
	auto TryCopy = [&](const FString& PropName)
	{
		if (CopyStructProperty(Preset, Target, PropName))
			Copied.Add(MakeShared<FJsonValueString>(PropName));
	};

	// Face shape lives in HeadModelSettings + FaceEvaluationSettings
	TryCopy(TEXT("HeadModelSettings"));
	TryCopy(TEXT("FaceEvaluationSettings"));
	if (bIncludeSkin) TryCopy(TEXT("SkinSettings"));
	if (bIncludeEyes) TryCopy(TEXT("EyesSettings"));
	if (bIncludeMakeup) TryCopy(TEXT("MakeupSettings"));

	Target->PostEditChange();
	Target->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharPath);
	Result->SetStringField(TEXT("preset_path"), PresetPath);
	Result->SetArrayField(TEXT("properties_copied"), Copied);
	Result->SetStringField(TEXT("note"), TEXT("Face shape properties copied from preset. NOTE: This does a flat property copy. For a full mesh-level face swap, use ImportFromTemplate with a preset mesh (future work). Body shape is not included — it lives in the body identity state."));
	return FECACommandResult::Success(Result);
}

// ============================================================================
// set_metahuman_makeup — convenience wrapper
// ============================================================================

static void ApplyStringFieldToMakeup(UObject* Character, const FString& Path, const FString& Value)
{
	if (Value.IsEmpty()) return;
	// Walk the dotted path to the leaf property
	FProperty* CurProp = nullptr;
	void* CurPtr = Character;
	UStruct* CurStruct = Character->GetClass();
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("."));
	for (int32 i = 0; i < Segments.Num(); i++)
	{
		CurProp = CurStruct->FindPropertyByName(FName(*Segments[i]));
		if (!CurProp) return;
		if (i == Segments.Num() - 1)
		{
			void* LeafPtr = CurProp->ContainerPtrToValuePtr<void>(CurPtr);
			CurProp->ImportText_Direct(*Value, LeafPtr, nullptr, PPF_None);
			return;
		}
		FStructProperty* StructProp = CastField<FStructProperty>(CurProp);
		if (!StructProp) return;
		CurPtr = StructProp->ContainerPtrToValuePtr<void>(CurPtr);
		CurStruct = StructProp->Struct;
	}
}

static FString ColorJsonToString(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return FString();
	double R = 0, G = 0, B = 0, A = 1;
	Obj->TryGetNumberField(TEXT("r"), R);
	Obj->TryGetNumberField(TEXT("g"), G);
	Obj->TryGetNumberField(TEXT("b"), B);
	Obj->TryGetNumberField(TEXT("a"), A);
	return FString::Printf(TEXT("(R=%f,G=%f,B=%f,A=%f)"), R, G, B, A);
}

FECACommandResult FECACommand_SetMetaHumanMakeup::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharPath) || CharPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharPath, Err);
	if (!Character) return FECACommandResult::Error(Err);

	TArray<TSharedPtr<FJsonValue>> Applied;
	auto TryApplyString = [&](const FString& ParamName, const FString& PropPath)
	{
		FString Val;
		if (Params->TryGetStringField(ParamName, Val) && !Val.IsEmpty())
		{
			ApplyStringFieldToMakeup(Character, PropPath, Val);
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("property"), PropPath);
			Entry->SetStringField(TEXT("value"), Val);
			Applied.Add(MakeShared<FJsonValueObject>(Entry));
		}
	};
	auto TryApplyColor = [&](const FString& ParamName, const FString& PropPath)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (Params->TryGetObjectField(ParamName, ObjPtr) && ObjPtr && ObjPtr->IsValid())
		{
			FString ColorStr = ColorJsonToString(*ObjPtr);
			ApplyStringFieldToMakeup(Character, PropPath, ColorStr);
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("property"), PropPath);
			Entry->SetStringField(TEXT("value"), ColorStr);
			Applied.Add(MakeShared<FJsonValueObject>(Entry));
		}
	};

	TryApplyString(TEXT("lip_type"),   TEXT("MakeupSettings.Lips.Type"));
	TryApplyColor (TEXT("lip_color"),  TEXT("MakeupSettings.Lips.Color"));
	TryApplyString(TEXT("eye_type"),   TEXT("MakeupSettings.Eyes.Type"));
	TryApplyColor (TEXT("eye_color"),  TEXT("MakeupSettings.Eyes.PrimaryColor"));
	TryApplyString(TEXT("blush_type"), TEXT("MakeupSettings.Blush.Type"));
	TryApplyColor (TEXT("blush_color"), TEXT("MakeupSettings.Blush.Color"));

	bool bFoundation = false;
	if (Params->TryGetBoolField(TEXT("foundation"), bFoundation))
	{
		ApplyStringFieldToMakeup(Character, TEXT("MakeupSettings.Foundation.bApplyFoundation"), bFoundation ? TEXT("true") : TEXT("false"));
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("property"), TEXT("MakeupSettings.Foundation.bApplyFoundation"));
		Entry->SetStringField(TEXT("value"), bFoundation ? TEXT("true") : TEXT("false"));
		Applied.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Character->PostEditChange();
	Character->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharPath);
	Result->SetArrayField(TEXT("applied"), Applied);
	Result->SetNumberField(TEXT("count"), Applied.Num());
	return FECACommandResult::Success(Result);
}

// ============================================================================
// Common subsystem accessors for new commands
// ============================================================================

static UEditorSubsystem* GetMHEditorSubsystem(FString& OutErrorMessage)
{
	UClass* SubsystemClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEditorSubsystem"));
	if (!SubsystemClass) { OutErrorMessage = TEXT("MetaHumanCharacterEditorSubsystem not found"); return nullptr; }
	UEditorEngine* EdEngine = Cast<UEditorEngine>(GEditor);
	if (!EdEngine) { OutErrorMessage = TEXT("GEditor not available"); return nullptr; }
	UEditorSubsystem* Subsystem = EdEngine->GetEditorSubsystemBase(SubsystemClass);
	if (!Subsystem) { OutErrorMessage = TEXT("Subsystem not instantiated"); return nullptr; }
	return Subsystem;
}

static UObject* LoadMHCharacter(const FString& Path, FString& OutErrorMessage)
{
	UObject* Asset = LoadObject<UObject>(nullptr, *Path);
	if (!Asset) { OutErrorMessage = FString::Printf(TEXT("Failed to load asset: %s"), *Path); return nullptr; }
	UClass* MHClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (!MHClass || !Asset->IsA(MHClass)) { OutErrorMessage = TEXT("Asset is not a MetaHumanCharacter"); return nullptr; }
	return Asset;
}

static void OpenCharacterEditor(UObject* Character)
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem) AssetEditorSubsystem->OpenEditorForAsset(Character);
}

// ============================================================================
// spawn_metahuman_actor
// ============================================================================

FECACommandResult FECACommand_SpawnMetaHumanActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);
	OpenCharacterEditor(Character);

	UEditorSubsystem* Subsystem = GetMHEditorSubsystem(Err);
	if (!Subsystem) return FECACommandResult::Error(Err);

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("SpawnMetaHumanActor")));
	if (!Func) return FECACommandResult::Error(TEXT("SpawnMetaHumanActor not found"));

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(Buffer.GetData());

	// Set input UObject* param
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ObjProp->SetObjectPropertyValue_InContainer(Buffer.GetData(), Character);
				break;
			}
		}
	}

	Subsystem->ProcessEvent(Func, Buffer.GetData());

	// Read the return value
	AActor* SpawnedActor = nullptr;
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
			{
				SpawnedActor = Cast<AActor>(ObjProp->GetObjectPropertyValue_InContainer(Buffer.GetData()));
			}
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	if (SpawnedActor)
	{
		Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetName());
		Result->SetStringField(TEXT("actor_label"), SpawnedActor->GetActorLabel());
		Result->SetStringField(TEXT("actor_class"), SpawnedActor->GetClass()->GetName());
		FVector Loc = SpawnedActor->GetActorLocation();
		TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
		LocJson->SetNumberField(TEXT("x"), Loc.X);
		LocJson->SetNumberField(TEXT("y"), Loc.Y);
		LocJson->SetNumberField(TEXT("z"), Loc.Z);
		Result->SetObjectField(TEXT("location"), LocJson);
		Result->SetBoolField(TEXT("spawned"), true);
	}
	else
	{
		Result->SetBoolField(TEXT("spawned"), false);
		Result->SetStringField(TEXT("note"), TEXT("SpawnMetaHumanActor returned null. The character may not be fully built (missing textures, unrigged)."));
	}

	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(Buffer.GetData());

	return FECACommandResult::Success(Result);
}

// ============================================================================
// get_metahuman_body_constraints
// ============================================================================

FECACommandResult FECACommand_GetMetaHumanBodyConstraints::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);
	OpenCharacterEditor(Character);

	UEditorSubsystem* Subsystem = GetMHEditorSubsystem(Err);
	if (!Subsystem) return FECACommandResult::Error(Err);

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("GetBodyConstraints")));
	if (!Func) return FECACommandResult::Error(TEXT("GetBodyConstraints function not found"));

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(Buffer.GetData());

	// Fill input UObject* (character)
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
		{
			ObjProp->SetObjectPropertyValue_InContainer(Buffer.GetData(), Character);
			break;
		}
	}

	Subsystem->ProcessEvent(Func, Buffer.GetData());

	// Read return TArray<FMetaHumanCharacterBodyConstraint>
	TArray<TSharedPtr<FJsonValue>> ConstraintsArray;
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(*It))
		{
			void* ArrayValPtr = ArrayProp->ContainerPtrToValuePtr<void>(Buffer.GetData());
			FScriptArrayHelper Helper(ArrayProp, ArrayValPtr);
			FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
			if (InnerStruct)
			{
				for (int32 i = 0; i < Helper.Num(); i++)
				{
					void* ElemPtr = Helper.GetRawPtr(i);
					TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
					for (TFieldIterator<FProperty> FieldIt(InnerStruct->Struct); FieldIt; ++FieldIt)
					{
						FProperty* Field = *FieldIt;
						void* FieldPtr = Field->ContainerPtrToValuePtr<void>(ElemPtr);
						FString Exported;
						Field->ExportTextItem_Direct(Exported, FieldPtr, nullptr, nullptr, PPF_None);
						Obj->SetStringField(Field->GetName(), Exported);
					}
					ConstraintsArray.Add(MakeShared<FJsonValueObject>(Obj));
				}
			}
		}
	}

	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(Buffer.GetData());

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetNumberField(TEXT("count"), ConstraintsArray.Num());
	Result->SetArrayField(TEXT("constraints"), ConstraintsArray);
	return FECACommandResult::Success(Result);
}

// ============================================================================
// set_metahuman_body_constraints
// ============================================================================

FECACommandResult FECACommand_SetMetaHumanBodyConstraints::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));

	const TArray<TSharedPtr<FJsonValue>>* ConstraintsArray;
	if (!Params->TryGetArrayField(TEXT("constraints"), ConstraintsArray))
		return FECACommandResult::Error(TEXT("Missing constraints array"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);
	OpenCharacterEditor(Character);

	UEditorSubsystem* Subsystem = GetMHEditorSubsystem(Err);
	if (!Subsystem) return FECACommandResult::Error(Err);

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("SetBodyConstraints")));
	if (!Func) return FECACommandResult::Error(TEXT("SetBodyConstraints function not found"));

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(Buffer.GetData());

	// Walk params: UObject* = Character, TArray = the constraints list
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			ObjProp->SetObjectPropertyValue_InContainer(Buffer.GetData(), Character);
		}
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
			if (!InnerStruct) continue;
			void* ArrayValPtr = ArrayProp->ContainerPtrToValuePtr<void>(Buffer.GetData());
			FScriptArrayHelper Helper(ArrayProp, ArrayValPtr);
			for (const auto& Entry : *ConstraintsArray)
			{
				const TSharedPtr<FJsonObject>* EntryObj;
				if (!Entry->TryGetObject(EntryObj)) continue;
				int32 NewIdx = Helper.AddValue();
				void* ElemPtr = Helper.GetRawPtr(NewIdx);
				FString Name;
				double Target = 0.0;
				bool bActive = true;
				(*EntryObj)->TryGetStringField(TEXT("name"), Name);
				(*EntryObj)->TryGetNumberField(TEXT("target_measurement"), Target);
				(*EntryObj)->TryGetBoolField(TEXT("active"), bActive);

				if (FProperty* NameProp = InnerStruct->Struct->FindPropertyByName(FName(TEXT("Name"))))
				{
					void* NamePtr = NameProp->ContainerPtrToValuePtr<void>(ElemPtr);
					NameProp->ImportText_Direct(*Name, NamePtr, nullptr, PPF_None);
				}
				if (FProperty* TargetProp = InnerStruct->Struct->FindPropertyByName(FName(TEXT("TargetMeasurement"))))
				{
					void* TargetPtr = TargetProp->ContainerPtrToValuePtr<void>(ElemPtr);
					float Tf = static_cast<float>(Target);
					FString TargetStr = FString::SanitizeFloat(Tf);
					TargetProp->ImportText_Direct(*TargetStr, TargetPtr, nullptr, PPF_None);
				}
				if (FProperty* ActiveProp = InnerStruct->Struct->FindPropertyByName(FName(TEXT("bIsActive"))))
				{
					void* ActivePtr = ActiveProp->ContainerPtrToValuePtr<void>(ElemPtr);
					ActiveProp->ImportText_Direct(bActive ? TEXT("true") : TEXT("false"), ActivePtr, nullptr, PPF_None);
				}
			}
		}
	}

	Subsystem->ProcessEvent(Func, Buffer.GetData());

	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(Buffer.GetData());

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetNumberField(TEXT("constraints_set"), ConstraintsArray->Num());
	Result->SetBoolField(TEXT("triggered"), true);
	Result->SetStringField(TEXT("note"), TEXT("Body morph re-evaluates async. Call get_metahuman_body_constraints afterward to verify."));
	return FECACommandResult::Success(Result);
}

// ============================================================================
// set_metahuman_body_type
// ============================================================================

FECACommandResult FECACommand_SetMetaHumanBodyType::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, BodyType;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));
	if (!Params->TryGetStringField(TEXT("body_type"), BodyType) || BodyType.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing body_type"));

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);
	OpenCharacterEditor(Character);

	UEditorSubsystem* Subsystem = GetMHEditorSubsystem(Err);
	if (!Subsystem) return FECACommandResult::Error(Err);

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("SetMetaHumanBodyType")));
	if (!Func) return FECACommandResult::Error(TEXT("SetMetaHumanBodyType function not found"));

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(Buffer.GetData());

	// Param 1: UObject*, param 2: EMetaHumanBodyType (byte enum), param 3: EBodyMeshUpdateMode (byte enum)
	int32 ParamIdx = 0;
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		if (ParamIdx == 0 && CastField<FObjectProperty>(Prop))
		{
			CastField<FObjectProperty>(Prop)->SetObjectPropertyValue_InContainer(Buffer.GetData(), Character);
		}
		else if (ParamIdx == 1)
		{
			void* EnumPtr = Prop->ContainerPtrToValuePtr<void>(Buffer.GetData());
			Prop->ImportText_Direct(*BodyType, EnumPtr, nullptr, PPF_None);
		}
		// Param 2 (update mode) stays at default (0 = immediate or whatever default is)
		ParamIdx++;
	}

	Subsystem->ProcessEvent(Func, Buffer.GetData());

	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(Buffer.GetData());

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("body_type"), BodyType);
	Result->SetBoolField(TEXT("triggered"), true);
	return FECACommandResult::Success(Result);
}

// ============================================================================
// attach_metahuman_groom
// ============================================================================

// The correct attach path is:
//   Character->InternalCollection->TryAddItemFromWardrobeItem(SlotName, WardrobeItem, OutKey)
//   Character->InternalCollection->DefaultInstance->SetSingleSlotSelection(SlotName, OutKey)
// ...which is what the editor's Wardrobe UI does internally. Writing to
// WardrobeIndividualAssets alone (as the previous implementation did) is insufficient because
// that map is a secondary registry; the editor reads the Collection's palette + the Instance's
// slot selection to determine what to render.
FECACommandResult FECACommand_AttachMetaHumanGroom::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, SlotName, WardrobePath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing character_path"));
	if (!Params->TryGetStringField(TEXT("slot_name"), SlotName) || SlotName.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing slot_name"));
	if (!Params->TryGetStringField(TEXT("wardrobe_item_path"), WardrobePath) || WardrobePath.IsEmpty())
		return FECACommandResult::Error(TEXT("Missing wardrobe_item_path"));

	// Validate slot name against the set MetaHuman's InternalCollection actually exposes.
	// These match the FName values the editor's Wardrobe UI uses internally — pluralised
	// folders (Beards, Mustaches) on disk still resolve to singular slot names here.
	{
		static const TCHAR* KnownSlots[] = {
			TEXT("Hair"), TEXT("Eyebrows"), TEXT("Eyelashes"),
			TEXT("Beard"), TEXT("Mustache"), TEXT("Peachfuzz"),
			TEXT("Outfit")
		};
		bool bValidSlot = false;
		for (const TCHAR* S : KnownSlots)
		{
			if (SlotName.Equals(S, ESearchCase::IgnoreCase)) { bValidSlot = true; break; }
		}
		if (!bValidSlot)
		{
			FString JoinedSlots;
			for (const TCHAR* S : KnownSlots) { if (!JoinedSlots.IsEmpty()) JoinedSlots += TEXT(", "); JoinedSlots += S; }
			return FECACommandResult::Error(FString::Printf(
				TEXT("Unknown slot_name '%s'. Expected one of: %s. Note: folders on disk may be plural (Beards, Mustaches) but the slot name is singular."),
				*SlotName, *JoinedSlots));
		}
	}

	FString Err;
	UObject* Character = LoadMHCharacter(CharacterPath, Err);
	if (!Character) return FECACommandResult::Error(Err);

	UObject* WardrobeItem = LoadObject<UObject>(nullptr, *WardrobePath);
	if (!WardrobeItem)
		return FECACommandResult::Error(FString::Printf(TEXT("Wardrobe item not found: %s"), *WardrobePath));

	UClass* WardrobeClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterPalette.MetaHumanWardrobeItem"));
	if (WardrobeClass && !WardrobeItem->IsA(WardrobeClass))
		return FECACommandResult::Error(TEXT("Asset is not a UMetaHumanWardrobeItem"));

	// Get the internal Collection via reflection on the InternalCollection UPROPERTY
	FProperty* CollectionProp = Character->GetClass()->FindPropertyByName(FName(TEXT("InternalCollection")));
	if (!CollectionProp) return FECACommandResult::Error(TEXT("Character has no InternalCollection property"));
	FObjectProperty* CollectionObjProp = CastField<FObjectProperty>(CollectionProp);
	if (!CollectionObjProp) return FECACommandResult::Error(TEXT("InternalCollection is not an object property"));
	UObject* Collection = CollectionObjProp->GetObjectPropertyValue_InContainer(Character);
	if (!Collection) return FECACommandResult::Error(TEXT("InternalCollection is null — is the character initialized?"));

	// Step 1: TryAddItemFromWardrobeItem(FName SlotName, UMetaHumanWardrobeItem* WardrobeItem, FMetaHumanPaletteItemKey& OutNewItemKey) -> bool
	UFunction* AddFunc = Collection->FindFunction(FName(TEXT("TryAddItemFromWardrobeItem")));
	if (!AddFunc) return FECACommandResult::Error(TEXT("TryAddItemFromWardrobeItem function not found on Collection"));

	TArray<uint8> AddBuffer;
	AddBuffer.SetNumZeroed(AddFunc->ParmsSize);
	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(AddBuffer.GetData());

	// Find each param by name and fill it in
	FName SlotKey(*SlotName);
	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		FString PropName = Prop->GetName();
		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(AddBuffer.GetData());
		if (PropName.Equals(TEXT("SlotName"), ESearchCase::IgnoreCase))
		{
			if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
				NameProp->SetPropertyValue(ValPtr, SlotKey);
		}
		else if (PropName.Equals(TEXT("WardrobeItem"), ESearchCase::IgnoreCase))
		{
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
				ObjProp->SetObjectPropertyValue(ValPtr, WardrobeItem);
		}
		// OutNewItemKey stays default-initialized; we read it after the call
	}

	Collection->ProcessEvent(AddFunc, AddBuffer.GetData());

	// Read back the return value (bool) and OutKey
	bool bAdded = false;
	TArray<uint8> SavedKeyBytes;
	FStructProperty* KeyStructProp = nullptr;
	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
				bAdded = BoolProp->GetPropertyValue_InContainer(AddBuffer.GetData());
		}
		else if (Prop->HasAnyPropertyFlags(CPF_OutParm))
		{
			if (FStructProperty* SP = CastField<FStructProperty>(Prop))
			{
				// Save a copy of the key struct bytes so we can pass them to the next call
				KeyStructProp = SP;
				void* KeyPtr = SP->ContainerPtrToValuePtr<void>(AddBuffer.GetData());
				int32 KeySize = SP->Struct->GetStructureSize();
				SavedKeyBytes.SetNumZeroed(KeySize);
				SP->Struct->CopyScriptStruct(SavedKeyBytes.GetData(), KeyPtr);
			}
		}
	}

	for (TFieldIterator<FProperty> It(AddFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(AddBuffer.GetData());

	if (!KeyStructProp)
		return FECACommandResult::Error(TEXT("Could not locate OutNewItemKey struct in TryAddItemFromWardrobeItem signature"));

	// Step 2: Get DefaultInstance from the Collection
	FProperty* InstanceProp = Collection->GetClass()->FindPropertyByName(FName(TEXT("DefaultInstance")));
	if (!InstanceProp) return FECACommandResult::Error(TEXT("Collection has no DefaultInstance property"));
	FObjectProperty* InstanceObjProp = CastField<FObjectProperty>(InstanceProp);
	UObject* Instance = InstanceObjProp ? InstanceObjProp->GetObjectPropertyValue_InContainer(Collection) : nullptr;
	if (!Instance) return FECACommandResult::Error(TEXT("DefaultInstance is null"));

	// Step 3: SetSingleSlotSelection(FName SlotName, const FMetaHumanPaletteItemKey& ItemKey)
	UFunction* SelectFunc = Instance->FindFunction(FName(TEXT("SetSingleSlotSelection")));
	if (!SelectFunc) return FECACommandResult::Error(TEXT("SetSingleSlotSelection function not found on Instance"));

	TArray<uint8> SelectBuffer;
	SelectBuffer.SetNumZeroed(SelectFunc->ParmsSize);
	for (TFieldIterator<FProperty> It(SelectFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->InitializeValue_InContainer(SelectBuffer.GetData());

	for (TFieldIterator<FProperty> It(SelectFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		FString PropName = Prop->GetName();
		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(SelectBuffer.GetData());
		if (PropName.Equals(TEXT("SlotName"), ESearchCase::IgnoreCase))
		{
			if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
				NameProp->SetPropertyValue(ValPtr, SlotKey);
		}
		else if (PropName.Equals(TEXT("ItemKey"), ESearchCase::IgnoreCase))
		{
			// Copy the saved key bytes into this struct param
			if (FStructProperty* SP = CastField<FStructProperty>(Prop))
			{
				SP->Struct->CopyScriptStruct(ValPtr, SavedKeyBytes.GetData());
			}
		}
	}

	Instance->ProcessEvent(SelectFunc, SelectBuffer.GetData());

	for (TFieldIterator<FProperty> It(SelectFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		It->DestroyValue_InContainer(SelectBuffer.GetData());

	// Also write to WardrobeIndividualAssets so the Wardrobe UI tab shows the item as "owned"
	FProperty* MapProp = Character->GetClass()->FindPropertyByName(FName(TEXT("WardrobeIndividualAssets")));
	if (FMapProperty* TypedMapProp = CastField<FMapProperty>(MapProp))
	{
		void* MapValPtr = TypedMapProp->ContainerPtrToValuePtr<void>(Character);
		FScriptMapHelper MapHelper(TypedMapProp, MapValPtr);
		int32 FoundIdx = -1;
		for (int32 i = 0; i < MapHelper.GetMaxIndex(); i++)
		{
			if (!MapHelper.IsValidIndex(i)) continue;
			FName* Key = reinterpret_cast<FName*>(MapHelper.GetKeyPtr(i));
			if (Key && *Key == SlotKey) { FoundIdx = i; break; }
		}
		int32 PairIdx = FoundIdx;
		if (PairIdx < 0)
		{
			PairIdx = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			FName* NewKey = reinterpret_cast<FName*>(MapHelper.GetKeyPtr(PairIdx));
			*NewKey = SlotKey;
		}
		if (FStructProperty* ValueStructProp = CastField<FStructProperty>(TypedMapProp->ValueProp))
		{
			void* ValuePtr = MapHelper.GetValuePtr(PairIdx);
			if (FProperty* ItemsProp = ValueStructProp->Struct->FindPropertyByName(FName(TEXT("Items"))))
			{
				if (FArrayProperty* ItemsArrayProp = CastField<FArrayProperty>(ItemsProp))
				{
					void* ItemsValPtr = ItemsArrayProp->ContainerPtrToValuePtr<void>(ValuePtr);
					FScriptArrayHelper ItemsHelper(ItemsArrayProp, ItemsValPtr);
					// Check if this wardrobe item is already in the list
					bool bAlreadyListed = false;
					if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(ItemsArrayProp->Inner))
					{
						for (int32 i = 0; i < ItemsHelper.Num(); i++)
						{
							FSoftObjectPtr Existing = SoftProp->GetPropertyValue(ItemsHelper.GetRawPtr(i));
							if (Existing.Get() == WardrobeItem) { bAlreadyListed = true; break; }
						}
						if (!bAlreadyListed)
						{
							int32 NewItemIdx = ItemsHelper.AddValue();
							void* NewItemPtr = ItemsHelper.GetRawPtr(NewItemIdx);
							FSoftObjectPtr NewPtr(WardrobeItem);
							SoftProp->SetPropertyValue(NewItemPtr, NewPtr);
						}
					}
				}
			}
		}
		MapHelper.Rehash();
	}

	Character->PostEditChange();
	Character->MarkPackageDirty();
	Collection->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("slot_name"), SlotName);
	Result->SetStringField(TEXT("wardrobe_item_path"), WardrobePath);
	Result->SetBoolField(TEXT("added_to_collection"), bAdded);
	Result->SetBoolField(TEXT("selected_in_instance"), true);
	Result->SetStringField(TEXT("note"), TEXT("Called TryAddItemFromWardrobeItem on Collection, then SetSingleSlotSelection on DefaultInstance. The groom should render in the preview after the next refresh_metahuman_preview call."));
	return FECACommandResult::Success(Result);
}

// Implementation lives further down the file after the other MH commands.
FECACommandResult FECACommand_SetMetaHumanPreviewMode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, Mode;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}
	if (!Params->TryGetStringField(TEXT("mode"), Mode) || Mode.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: mode (skin, topology, or clay)"));
	}

	// Map user-friendly mode names to the EMetaHumanCharacterSkinPreviewMaterial enum names.
	// NOTE: Epic's enum DisplayNames are inverted from their C++ names — the enum 'Default'
	// displays as "Topology" in the UI (shows zones), and 'Editable' displays as "Skin"
	// (shows actual textures). We map based on what the user sees, not what Epic named it.
	FString EnumValue;
	FString ModeLower = Mode.ToLower();
	if (ModeLower == TEXT("skin") || ModeLower == TEXT("textured") || ModeLower == TEXT("preview"))
	{
		EnumValue = TEXT("Editable");  // displays as "Skin" in UI
	}
	else if (ModeLower == TEXT("topology") || ModeLower == TEXT("zones") || ModeLower == TEXT("default"))
	{
		EnumValue = TEXT("Default");  // displays as "Topology" in UI
	}
	else if (ModeLower == TEXT("clay"))
	{
		EnumValue = TEXT("Clay");
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unknown mode '%s'. Valid: skin, topology, clay"), *Mode));
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	// Find the PreviewMaterialType property and set it via reflection
	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(TEXT("PreviewMaterialType")));
	if (!Property)
	{
		return FECACommandResult::Error(TEXT("Asset has no PreviewMaterialType property — not a MetaHumanCharacter?"));
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);

	// Capture old value for reporting
	FString OldValue;
	Property->ExportTextItem_Direct(OldValue, ValuePtr, nullptr, Asset, PPF_None);

	// Import the new enum value
	const TCHAR* ImportResult = Property->ImportText_Direct(*EnumValue, ValuePtr, Asset, PPF_None);
	if (!ImportResult)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to set PreviewMaterialType to '%s'"), *EnumValue));
	}

	Asset->PostEditChange();
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("requested_mode"), Mode);
	Result->SetStringField(TEXT("enum_value_set"), EnumValue);
	Result->SetStringField(TEXT("old_value"), OldValue);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("note"), TEXT("If the MetaHuman editor viewport is already open, click its mode dropdown once to force a visual refresh — UE caches the render path."));
	return FECACommandResult::Success(Result);
}

// Helper: call a named function on the MetaHumanCharacterEditorSubsystem.
// Builds the param buffer dynamically by inspecting the UFunction's actual parameter layout.
// Sets the first UObject* param to our character; default-initializes any struct params.
static FECACommandResult CallMHEditorFunction(const TSharedPtr<FJsonObject>& Params, const FString& FuncName, const FString& OperationLabel)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	UObject* AssetObj = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!AssetObj)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	UClass* MHClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (!MHClass || !AssetObj->IsA(MHClass))
	{
		return FECACommandResult::Error(TEXT("Asset is not a MetaHumanCharacter"));
	}

	UClass* SubsystemClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEditorSubsystem"));
	if (!SubsystemClass)
	{
		return FECACommandResult::Error(TEXT("MetaHumanCharacterEditorSubsystem not found"));
	}

	UEditorEngine* EdEngine = Cast<UEditorEngine>(GEditor);
	if (!EdEngine)
	{
		return FECACommandResult::Error(TEXT("GEditor not available"));
	}

	UEditorSubsystem* Subsystem = EdEngine->GetEditorSubsystemBase(SubsystemClass);
	if (!Subsystem)
	{
		return FECACommandResult::Error(TEXT("Could not get subsystem instance"));
	}

	UFunction* Func = Subsystem->FindFunction(FName(*FuncName));
	if (!Func)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Function '%s' not found on MetaHumanCharacterEditorSubsystem"), *FuncName));
	}

	// Build a param buffer matching the function's actual layout
	TArray<uint8> ParamBuffer;
	ParamBuffer.SetNumZeroed(Func->ParmsSize);

	// Initialize each parameter property in place (constructs structs to defaults)
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		Prop->InitializeValue_InContainer(ParamBuffer.GetData());
	}

	// Find the first UObject* parameter and inject our asset pointer
	bool bSetCharacter = false;
	int32 ParamCount = 0;
	TArray<TSharedPtr<FJsonValue>> ParamInfo;
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		ParamCount++;
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"), Prop->GetName());
		P->SetStringField(TEXT("type"), Prop->GetCPPType());
		ParamInfo.Add(MakeShared<FJsonValueObject>(P));

		if (!bSetCharacter)
		{
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
			{
				ObjProp->SetObjectPropertyValue_InContainer(ParamBuffer.GetData(), AssetObj);
				bSetCharacter = true;
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("function_called"), FuncName);
	Result->SetStringField(TEXT("operation"), OperationLabel);
	Result->SetNumberField(TEXT("param_count"), ParamCount);
	Result->SetArrayField(TEXT("parameters"), ParamInfo);

	if (!bSetCharacter)
	{
		// Clean up any initialized values
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			Prop->DestroyValue_InContainer(ParamBuffer.GetData());
		}
		return FECACommandResult::Error(FString::Printf(TEXT("Function '%s' has no UObject* parameter to inject character into"), *FuncName));
	}

	// Invoke
	Subsystem->ProcessEvent(Func, ParamBuffer.GetData());

	// Destroy in-place values to clean up structs
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		Prop->DestroyValue_InContainer(ParamBuffer.GetData());
	}

	Result->SetBoolField(TEXT("triggered"), true);
	Result->SetStringField(TEXT("note"), TEXT("Operation is async — monitor the MetaHuman editor for progress."));
	return FECACommandResult::Success(Result);
}

// These commands invoke UMetaHumanCharacterEditorSubsystem functions via reflection.
// They require the user to be signed in to Epic (click the person icon in the MetaHuman editor toolbar)
// because the cloud services (texture synthesis, auto-rigging, build pipeline) need an auth session.
// If the user hasn't signed in, the subsystem hits an assertion in a TMap auth-token lookup.
// Safe mode: before calling, this command opens the MetaHuman editor window to force auth state init.

static FECACommandResult EnsureEditorOpenThenCall(const TSharedPtr<FJsonObject>& Params, const FString& FuncName, const FString& OperationLabel)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	// Open the MetaHuman editor first — initializes toolkit state, auth callbacks, pipeline context.
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Asset);
	}

	return CallMHEditorFunction(Params, FuncName, OperationLabel);
}

FECACommandResult FECACommand_DownloadMetaHumanTextures::Execute(const TSharedPtr<FJsonObject>& Params)
{
	return EnsureEditorOpenThenCall(Params, TEXT("RequestTextureSources"), TEXT("download_textures"));
}

FECACommandResult FECACommand_RigMetaHuman::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!Params->TryGetStringField(TEXT("character_path"), CharacterPath) || CharacterPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	FString RigType = TEXT("full");
	Params->TryGetStringField(TEXT("rig_type"), RigType);
	FString RigTypeLower = RigType.ToLower();
	FString EnumValue = (RigTypeLower == TEXT("joints") || RigTypeLower == TEXT("jointsonly") || RigTypeLower == TEXT("joints_only"))
		? TEXT("JointsOnly") : TEXT("JointsAndBlendShapes");

	UObject* Asset = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	// Open editor first to initialize auth/toolkit state
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Asset);
	}

	UClass* SubsystemClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEditorSubsystem"));
	if (!SubsystemClass) return FECACommandResult::Error(TEXT("MetaHumanCharacterEditorSubsystem not found"));
	UEditorEngine* EdEngine = Cast<UEditorEngine>(GEditor);
	if (!EdEngine) return FECACommandResult::Error(TEXT("GEditor not available"));
	UEditorSubsystem* Subsystem = EdEngine->GetEditorSubsystemBase(SubsystemClass);
	if (!Subsystem) return FECACommandResult::Error(TEXT("Subsystem not instantiated"));

	UFunction* Func = Subsystem->FindFunction(FName(TEXT("RequestAutoRigging")));
	if (!Func) return FECACommandResult::Error(TEXT("RequestAutoRigging function not found"));

	TArray<uint8> ParamBuffer;
	ParamBuffer.SetNumZeroed(Func->ParmsSize);

	// Default-construct all params
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(ParamBuffer.GetData());
	}

	// Walk params: first UObject* gets our character, first struct gets RigType injected
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			ObjProp->SetObjectPropertyValue_InContainer(ParamBuffer.GetData(), Asset);
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			// Find RigType field inside the struct
			void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(ParamBuffer.GetData());
			if (FProperty* RigTypeProp = StructProp->Struct->FindPropertyByName(FName(TEXT("RigType"))))
			{
				void* RigTypeValPtr = RigTypeProp->ContainerPtrToValuePtr<void>(StructPtr);
				RigTypeProp->ImportText_Direct(*EnumValue, RigTypeValPtr, nullptr, PPF_None);
			}
		}
	}

	Subsystem->ProcessEvent(Func, ParamBuffer.GetData());

	// Destroy allocated struct contents
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(ParamBuffer.GetData());
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("function_called"), TEXT("RequestAutoRigging"));
	Result->SetStringField(TEXT("rig_type"), EnumValue);
	Result->SetBoolField(TEXT("triggered"), true);
	Result->SetStringField(TEXT("note"), TEXT("Auto-rigging is async — expect 30-60s. Full rig (Joints + Blend Shapes) is required for face animation."));
	return FECACommandResult::Success(Result);
}

//==============================================================================
// Internal helpers (file-local)
//==============================================================================

namespace
{
	/** Split a content path like /Game/Foo/Bar into package dir + asset name. */
	void SplitAssetPath(const FString& InAssetPath, FString& OutPackagePath, FString& OutAssetName)
	{
		int32 LastSlash = INDEX_NONE;
		if (InAssetPath.FindLastChar(TEXT('/'), LastSlash))
		{
			OutPackagePath = InAssetPath.Left(LastSlash);
			OutAssetName = InAssetPath.Mid(LastSlash + 1);
		}
		else
		{
			OutPackagePath = TEXT("/Game");
			OutAssetName = InAssetPath;
		}

		// If the user included ".AssetName" on the end, strip it.
		int32 DotIdx = INDEX_NONE;
		if (OutAssetName.FindChar(TEXT('.'), DotIdx))
		{
			OutAssetName = OutAssetName.Left(DotIdx);
		}
	}

	/** Look up the UMetaHumanCharacter class via reflection — returns null if plugin missing. */
	UClass* GetMetaHumanCharacterClass()
	{
		return FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	}

	/** Look up the factory class for creating MetaHuman Character assets. */
	UClass* GetMetaHumanCharacterFactoryClass()
	{
		return FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterFactoryNew"));
	}

	/** Load a MetaHumanCharacter by content path. Returns null if not a MetaHumanCharacter asset. */
	UObject* LoadMetaHumanCharacter(const FString& InCharacterPath)
	{
		UClass* MHCharClass = GetMetaHumanCharacterClass();
		if (!MHCharClass)
		{
			return nullptr;
		}

		// Try loading — this covers both "/Game/Foo/Bar" and "/Game/Foo/Bar.Bar" forms
		UObject* Loaded = StaticLoadObject(MHCharClass, nullptr, *InCharacterPath);
		if (!Loaded)
		{
			// Retry with ".AssetName" appended if path was just the package path
			FString PathWithName = InCharacterPath;
			int32 LastSlash = INDEX_NONE;
			if (PathWithName.FindLastChar(TEXT('/'), LastSlash) &&
				!PathWithName.Contains(TEXT(".")))
			{
				FString AssetName = PathWithName.Mid(LastSlash + 1);
				PathWithName = PathWithName + TEXT(".") + AssetName;
				Loaded = StaticLoadObject(MHCharClass, nullptr, *PathWithName);
			}
		}

		return Loaded;
	}

	/** Recursive property-to-JSON converter (mirrors the DataTable helper style). */
	TSharedPtr<FJsonValue> MHPropertyToJsonValue(FProperty* Property, const void* ValuePtr, int32 Depth = 0)
	{
		if (!Property || !ValuePtr)
		{
			return MakeShared<FJsonValueNull>();
		}

		// Guard against runaway recursion on self-referential structs
		if (Depth > 8)
		{
			return MakeShared<FJsonValueString>(TEXT("<max depth>"));
		}

		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
		}
		if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
		{
			if (NumProp->IsFloatingPoint())
			{
				return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
			}
			if (NumProp->IsInteger())
			{
				return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
			}
		}
		if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
		}
		if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
		}
		if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
		}
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
			const int64 EnumValue = Underlying->GetSignedIntPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(Enum ? Enum->GetNameStringByValue(EnumValue) : FString::FromInt(EnumValue));
		}
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
			{
				const uint8 ByteValue = *static_cast<const uint8*>(ValuePtr);
				return MakeShared<FJsonValueString>(Enum->GetNameStringByValue(ByteValue));
			}
			return MakeShared<FJsonValueNumber>(static_cast<double>(*static_cast<const uint8*>(ValuePtr)));
		}
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			TSharedPtr<FJsonObject> StructObj = MakeShared<FJsonObject>();
			for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
			{
				FProperty* InnerProp = *PropIt;
				const void* InnerValuePtr = InnerProp->ContainerPtrToValuePtr<void>(ValuePtr);
				StructObj->SetField(InnerProp->GetName(), MHPropertyToJsonValue(InnerProp, InnerValuePtr, Depth + 1));
			}
			return MakeShared<FJsonValueObject>(StructObj);
		}
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				JsonArray.Add(MHPropertyToJsonValue(ArrayProp->Inner, ArrayHelper.GetRawPtr(i), Depth + 1));
			}
			return MakeShared<FJsonValueArray>(JsonArray);
		}
		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			FScriptMapHelper MapHelper(MapProp, ValuePtr);
			for (int32 i = 0; i < MapHelper.Num(); ++i)
			{
				if (MapHelper.IsValidIndex(i))
				{
					TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
					Entry->SetField(TEXT("key"),   MHPropertyToJsonValue(MapProp->KeyProp,   MapHelper.GetKeyPtr(i),   Depth + 1));
					Entry->SetField(TEXT("value"), MHPropertyToJsonValue(MapProp->ValueProp, MapHelper.GetValuePtr(i), Depth + 1));
					JsonArray.Add(MakeShared<FJsonValueObject>(Entry));
				}
			}
			return MakeShared<FJsonValueArray>(JsonArray);
		}
		if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
		{
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			FScriptSetHelper SetHelper(SetProp, ValuePtr);
			for (int32 i = 0; i < SetHelper.Num(); ++i)
			{
				if (SetHelper.IsValidIndex(i))
				{
					JsonArray.Add(MHPropertyToJsonValue(SetProp->ElementProp, SetHelper.GetElementPtr(i), Depth + 1));
				}
			}
			return MakeShared<FJsonValueArray>(JsonArray);
		}
		if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
			return MakeShared<FJsonValueString>(SoftPtr.ToString());
		}
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(Obj ? Obj->GetPathName() : TEXT("None"));
		}

		// Fallback: export to text
		FString StringValue;
		Property->ExportTextItem_Direct(StringValue, ValuePtr, nullptr, nullptr, PPF_None);
		return MakeShared<FJsonValueString>(StringValue);
	}

	/**
	 * Resolve a dotted property path (e.g. "SkinSettings.Skin.Roughness" or
	 * "EyesSettings.EyeLeft.Iris.PrimaryColorU") to a leaf property + value pointer.
	 *
	 * Supports:
	 *  - Struct traversal (FStructProperty)
	 *  - Leaf scalar / color / vector access
	 *
	 * Limitations:
	 *  - Does not traverse into TArray/TMap/TSet elements via the dotted path.
	 *  - The final component can name a leaf FProperty (including components of a
	 *    struct like FLinearColor — e.g., ".R" — since FLinearColor's fields are
	 *    regular FProperty members exposed via FStructProperty).
	 */
	bool ResolvePropertyPath(
		UObject* RootObject,
		const FString& PropertyPath,
		FProperty*& OutProperty,
		void*& OutValuePtr,
		FString& OutError)
	{
		OutProperty = nullptr;
		OutValuePtr = nullptr;

		if (!RootObject)
		{
			OutError = TEXT("Root object is null");
			return false;
		}

		TArray<FString> Parts;
		PropertyPath.ParseIntoArray(Parts, TEXT("."), /*bCullEmpty*/ true);
		if (Parts.Num() == 0)
		{
			OutError = TEXT("Empty property path");
			return false;
		}

		UStruct* CurrentStruct = RootObject->GetClass();
		void* CurrentContainer = static_cast<void*>(RootObject);

		for (int32 Idx = 0; Idx < Parts.Num(); ++Idx)
		{
			const FString& PartName = Parts[Idx];
			FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*PartName));
			if (!Prop)
			{
				OutError = FString::Printf(TEXT("Property '%s' not found on '%s'"),
					*PartName, *CurrentStruct->GetName());
				return false;
			}

			void* PropValuePtr = Prop->ContainerPtrToValuePtr<void>(CurrentContainer);

			// If this is the last part we're done — return the leaf
			if (Idx == Parts.Num() - 1)
			{
				OutProperty = Prop;
				OutValuePtr = PropValuePtr;
				return true;
			}

			// Intermediate — must be a struct to continue
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				CurrentStruct = StructProp->Struct;
				CurrentContainer = PropValuePtr;
			}
			else
			{
				OutError = FString::Printf(TEXT("Cannot descend into non-struct property '%s' (type %s)"),
					*PartName, *Prop->GetClass()->GetName());
				return false;
			}
		}

		return false;
	}

	/** Convert a JSON value to a string suitable for Property->ImportText_Direct. */
	FString JsonValueToImportString(const TSharedPtr<FJsonValue>& JsonValue, FProperty* TargetProperty)
	{
		if (!JsonValue.IsValid())
		{
			return FString();
		}

		// For colors/vectors represented as JSON objects {r,g,b,a} or {x,y,z}, build tuple text
		if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
			if (!Obj.IsValid())
			{
				return FString();
			}

			FStructProperty* StructProp = CastField<FStructProperty>(TargetProperty);
			if (StructProp && StructProp->Struct)
			{
				const FString StructName = StructProp->Struct->GetName();

				auto Num = [&](const TCHAR* Key, double Default) -> double
				{
					double V = Default;
					if (Obj->HasField(Key))
					{
						Obj->TryGetNumberField(Key, V);
					}
					return V;
				};

				if (StructName == TEXT("LinearColor"))
				{
					double R = Num(TEXT("r"), Num(TEXT("R"), 0.0));
					double G = Num(TEXT("g"), Num(TEXT("G"), 0.0));
					double B = Num(TEXT("b"), Num(TEXT("B"), 0.0));
					double A = Num(TEXT("a"), Num(TEXT("A"), 1.0));
					return FString::Printf(TEXT("(R=%f,G=%f,B=%f,A=%f)"), R, G, B, A);
				}
				if (StructName == TEXT("Color"))
				{
					int32 R = (int32)Num(TEXT("r"), Num(TEXT("R"), 0.0));
					int32 G = (int32)Num(TEXT("g"), Num(TEXT("G"), 0.0));
					int32 B = (int32)Num(TEXT("b"), Num(TEXT("B"), 0.0));
					int32 A = (int32)Num(TEXT("a"), Num(TEXT("A"), 255.0));
					return FString::Printf(TEXT("(R=%d,G=%d,B=%d,A=%d)"), R, G, B, A);
				}
				if (StructName == TEXT("Vector") || StructName == TEXT("Vector3d") || StructName == TEXT("Vector3f"))
				{
					double X = Num(TEXT("x"), Num(TEXT("X"), 0.0));
					double Y = Num(TEXT("y"), Num(TEXT("Y"), 0.0));
					double Z = Num(TEXT("z"), Num(TEXT("Z"), 0.0));
					return FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
				}
				if (StructName == TEXT("Vector2D") || StructName == TEXT("Vector2f"))
				{
					double X = Num(TEXT("x"), Num(TEXT("X"), 0.0));
					double Y = Num(TEXT("y"), Num(TEXT("Y"), 0.0));
					return FString::Printf(TEXT("(X=%f,Y=%f)"), X, Y);
				}
			}

			// Fallback: serialize the JSON object to a string
			FString OutString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
			FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
			return OutString;
		}

		if (JsonValue->Type == EJson::Boolean)
		{
			return JsonValue->AsBool() ? TEXT("true") : TEXT("false");
		}
		if (JsonValue->Type == EJson::Number)
		{
			return FString::SanitizeFloat(JsonValue->AsNumber());
		}
		if (JsonValue->Type == EJson::String)
		{
			return JsonValue->AsString();
		}

		return FString();
	}
}

//==============================================================================
// create_metahuman_character
//==============================================================================

FECACommandResult FECACommand_CreateMetaHumanCharacter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString BodyType, SkinTone, EyeColor;
	GetStringParam(Params, TEXT("body_type"),  BodyType,  /*bRequired*/ false);
	GetStringParam(Params, TEXT("skin_tone"),  SkinTone,  /*bRequired*/ false);
	GetStringParam(Params, TEXT("eye_color"),  EyeColor,  /*bRequired*/ false);

	UClass* MHCharClass = GetMetaHumanCharacterClass();
	if (!MHCharClass)
	{
		return FECACommandResult::Error(
			TEXT("MetaHuman Character class not found. The MetaHumanCharacter plugin is not enabled. ")
			TEXT("Enable it in Edit > Plugins > MetaHuman Character and restart the editor."));
	}

	UClass* MHFactoryClass = GetMetaHumanCharacterFactoryClass();
	if (!MHFactoryClass)
	{
		return FECACommandResult::Error(
			TEXT("MetaHuman Character factory not found. The MetaHumanCharacterEditor plugin is not loaded. ")
			TEXT("Ensure the MetaHumanCharacter plugin (editor module) is enabled."));
	}

	FString PackagePath, AssetName;
	SplitAssetPath(AssetPath, PackagePath, AssetName);
	if (AssetName.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Could not derive asset name from asset_path"));
	}

	// Create the factory instance
	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), MHFactoryClass);
	if (!Factory)
	{
		return FECACommandResult::Error(TEXT("Failed to instantiate MetaHumanCharacterFactoryNew"));
	}

	// Use AssetTools to create the asset (invokes FactoryCreateNew which does the heavy lifting)
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, MHCharClass, Factory);
	if (!NewAsset)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Failed to create MetaHumanCharacter '%s' at '%s'. Check the output log."),
			*AssetName, *PackagePath));
	}

	// Apply initial presets via reflection where possible. We keep this conservative
	// — the underlying skin tone / body type actually live inside Character State data
	// that only the MetaHumanCharacterEditorSubsystem can modify correctly. We can
	// still nudge skin/eye sub-settings that are plain UPROPERTY structs.
	TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SkippedNotes;

	auto ApplyImport = [&](const FString& Path, const FString& ImportText, const TCHAR* Label) -> bool
	{
		FProperty* Prop = nullptr;
		void* ValuePtr = nullptr;
		FString Err;
		if (!ResolvePropertyPath(NewAsset, Path, Prop, ValuePtr, Err) || !Prop || !ValuePtr)
		{
			SkippedNotes.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: %s"), Label, *Err)));
			return false;
		}
		const TCHAR* Result = Prop->ImportText_Direct(*ImportText, ValuePtr, NewAsset, PPF_None);
		if (!Result)
		{
			SkippedNotes.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: ImportText failed for '%s'"), Label, *ImportText)));
			return false;
		}
		Applied->SetStringField(Path, ImportText);
		return true;
	};

	// Skin tone → SkinSettings.Skin.U/V (these are the UV into the skin-tone chart)
	// These ranges are approximate — the real values live in character state buffers.
	if (!SkinTone.IsEmpty())
	{
		const FString Tone = SkinTone.ToLower();
		float U = 0.5f, V = 0.5f;
		if (Tone.Contains(TEXT("light")) || Tone.Contains(TEXT("pale")) || Tone.Contains(TEXT("white")))
		{
			U = 0.15f; V = 0.15f;
		}
		else if (Tone.Contains(TEXT("medium_light")))
		{
			U = 0.3f; V = 0.35f;
		}
		else if (Tone.Contains(TEXT("medium_dark")))
		{
			U = 0.6f; V = 0.65f;
		}
		else if (Tone.Contains(TEXT("dark")) || Tone.Contains(TEXT("black")))
		{
			U = 0.85f; V = 0.85f;
		}
		else if (Tone.Contains(TEXT("medium")) || Tone.Contains(TEXT("tan")))
		{
			U = 0.5f; V = 0.5f;
		}
		ApplyImport(TEXT("SkinSettings.Skin.U"), FString::SanitizeFloat(U), TEXT("skin_tone.U"));
		ApplyImport(TEXT("SkinSettings.Skin.V"), FString::SanitizeFloat(V), TEXT("skin_tone.V"));
	}

	// Eye color → both eyes' iris PrimaryColorU/V. These are UV lookups into the iris LUT.
	if (!EyeColor.IsEmpty())
	{
		const FString Color = EyeColor.ToLower();
		float U = 0.5f, V = 0.5f;
		if (Color.Contains(TEXT("blue")))       { U = 0.15f; V = 0.6f; }
		else if (Color.Contains(TEXT("green")))  { U = 0.35f; V = 0.5f; }
		else if (Color.Contains(TEXT("hazel")))  { U = 0.55f; V = 0.4f; }
		else if (Color.Contains(TEXT("brown")))  { U = 0.8f;  V = 0.3f; }
		else if (Color.Contains(TEXT("gray")) || Color.Contains(TEXT("grey")))
		{ U = 0.5f;  V = 0.8f; }

		const FString Us = FString::SanitizeFloat(U);
		const FString Vs = FString::SanitizeFloat(V);
		ApplyImport(TEXT("EyesSettings.EyeLeft.Iris.PrimaryColorU"),  Us, TEXT("eye_color.Left.U"));
		ApplyImport(TEXT("EyesSettings.EyeLeft.Iris.PrimaryColorV"),  Vs, TEXT("eye_color.Left.V"));
		ApplyImport(TEXT("EyesSettings.EyeRight.Iris.PrimaryColorU"), Us, TEXT("eye_color.Right.U"));
		ApplyImport(TEXT("EyesSettings.EyeRight.Iris.PrimaryColorV"), Vs, TEXT("eye_color.Right.V"));
	}

	// Body type — we cannot meaningfully mutate body state from C++ without going
	// through UMetaHumanCharacterEditorSubsystem and its body-state buffers. Just
	// record the request in the notes.
	if (!BodyType.IsEmpty())
	{
		SkippedNotes.Add(MakeShared<FJsonValueString>(FString::Printf(
			TEXT("body_type='%s' recorded but not applied — body morphing requires UMetaHumanCharacterEditorSubsystem. ")
			TEXT("Open the character in the MetaHuman Character editor and adjust the body preset there."),
			*BodyType)));
	}

	// Notify the asset it changed + mark dirty
	NewAsset->MarkPackageDirty();
#if WITH_EDITOR
	NewAsset->PostEditChange();
#endif

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"),   NewAsset->GetPathName());
	Result->SetStringField(TEXT("asset_name"),   NewAsset->GetName());
	Result->SetStringField(TEXT("asset_class"),  NewAsset->GetClass()->GetName());
	Result->SetStringField(TEXT("package_path"), PackagePath);
	Result->SetObjectField(TEXT("applied_settings"), Applied);
	Result->SetArrayField(TEXT("notes"), SkippedNotes);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_metahuman_character
//==============================================================================

FECACommandResult FECACommand_DumpMetaHumanCharacter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	UClass* MHCharClass = GetMetaHumanCharacterClass();
	if (!MHCharClass)
	{
		return FECACommandResult::Error(
			TEXT("MetaHumanCharacter plugin is not enabled."));
	}

	UObject* Character = LoadMetaHumanCharacter(CharacterPath);
	if (!Character)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not load MetaHumanCharacter at '%s'"), *CharacterPath));
	}

	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();

	// Dump every CPF_Edit-visible property on the character (VisibleAnywhere and
	// EditAnywhere both carry this flag).
	for (TFieldIterator<FProperty> PropIt(Character->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		const bool bIsEditVisible = Property->HasAnyPropertyFlags(CPF_Edit);
		const bool bIsTransient   = Property->HasAnyPropertyFlags(CPF_Transient);
		// Skip transient and non-edit — we want the persistent, user-visible state.
		if (!bIsEditVisible || bIsTransient)
		{
			continue;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Character);
		PropsObj->SetField(Property->GetName(), MHPropertyToJsonValue(Property, ValuePtr));
	}

	// Derive a friendly rigging state via the public-facing has-DNA accessors.
	// (UMetaHumanCharacter exposes HasFaceDNA / HasBodyDNA which are our best
	//  proxies for "is this rigged?" without touching non-public state.)
	FString RiggingState = TEXT("Unrigged");
	if (UMetaHumanCharacter* Typed = Cast<UMetaHumanCharacter>(Character))
	{
		const bool bFaceDNA = Typed->HasFaceDNA();
		const bool bBodyDNA = Typed->HasBodyDNA();
		if (bFaceDNA && bBodyDNA)
		{
			RiggingState = TEXT("Rigged");
		}
		else if (bFaceDNA || bBodyDNA)
		{
			RiggingState = TEXT("PartiallyRigged");
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("character_path"), Character->GetPathName());
	Result->SetStringField(TEXT("asset_class"),    Character->GetClass()->GetName());
	Result->SetStringField(TEXT("rigging_state"),  RiggingState);
	Result->SetObjectField(TEXT("properties"),     PropsObj);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_metahuman_property
//==============================================================================

FECACommandResult FECACommand_SetMetaHumanProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, PropertyPath;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}
	if (!GetStringParam(Params, TEXT("property"), PropertyPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property"));
	}

	TSharedPtr<FJsonValue> ValueField;
	if (Params.IsValid())
	{
		ValueField = Params->TryGetField(TEXT("value"));
	}
	if (!ValueField.IsValid())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: value"));
	}

	if (!GetMetaHumanCharacterClass())
	{
		return FECACommandResult::Error(TEXT("MetaHumanCharacter plugin is not enabled."));
	}

	UObject* Character = LoadMetaHumanCharacter(CharacterPath);
	if (!Character)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not load MetaHumanCharacter at '%s'"), *CharacterPath));
	}

	FProperty* LeafProp = nullptr;
	void* LeafValuePtr  = nullptr;
	FString Err;
	if (!ResolvePropertyPath(Character, PropertyPath, LeafProp, LeafValuePtr, Err) || !LeafProp || !LeafValuePtr)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not resolve property '%s': %s"), *PropertyPath, *Err));
	}

	const FString ImportString = JsonValueToImportString(ValueField, LeafProp);
	const TCHAR* ImportResult = LeafProp->ImportText_Direct(*ImportString, LeafValuePtr, Character, PPF_None);
	if (!ImportResult)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("ImportText failed for property '%s' value '%s'"), *PropertyPath, *ImportString));
	}

	Character->MarkPackageDirty();
#if WITH_EDITOR
	FPropertyChangedEvent PropertyChangedEvent(LeafProp);
	Character->PostEditChangeProperty(PropertyChangedEvent);
#endif

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("character_path"), Character->GetPathName());
	Result->SetStringField(TEXT("property"),       PropertyPath);
	Result->SetStringField(TEXT("value_import"),   ImportString);

	// Read back the value for confirmation
	TSharedPtr<FJsonValue> ReadBack = MHPropertyToJsonValue(LeafProp, LeafValuePtr);
	Result->SetField(TEXT("new_value"), ReadBack);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_metahuman_skin_params
//==============================================================================

FECACommandResult FECACommand_SetMetaHumanSkinParams::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}
	if (!GetMetaHumanCharacterClass())
	{
		return FECACommandResult::Error(TEXT("MetaHumanCharacter plugin is not enabled."));
	}
	UObject* Character = LoadMetaHumanCharacter(CharacterPath);
	if (!Character)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not load MetaHumanCharacter at '%s'"), *CharacterPath));
	}

	TArray<TSharedPtr<FJsonValue>> Applied;
	TArray<TSharedPtr<FJsonValue>> Skipped;
	FProperty* LastTouchedProp = nullptr;

	auto WriteScalar = [&](const TCHAR* PropertyPath, const TSharedPtr<FJsonValue>& Val) -> void
	{
		FProperty* Prop = nullptr;
		void* ValuePtr = nullptr;
		FString Err;
		if (!ResolvePropertyPath(Character, PropertyPath, Prop, ValuePtr, Err) || !Prop || !ValuePtr)
		{
			Skipped.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: %s"), PropertyPath, *Err)));
			return;
		}
		const FString ImportString = JsonValueToImportString(Val, Prop);
		const TCHAR* R = Prop->ImportText_Direct(*ImportString, ValuePtr, Character, PPF_None);
		if (!R)
		{
			Skipped.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: ImportText failed for '%s'"), PropertyPath, *ImportString)));
			return;
		}
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("path"), PropertyPath);
		Entry->SetStringField(TEXT("value"), ImportString);
		Applied.Add(MakeShared<FJsonValueObject>(Entry));
		LastTouchedProp = Prop;
	};

	// Top-level Skin sub-struct fields
	TSharedPtr<FJsonValue> V;
	if ((V = Params->TryGetField(TEXT("tone_u"))).IsValid())             WriteScalar(TEXT("SkinSettings.Skin.U"), V);
	if ((V = Params->TryGetField(TEXT("tone_v"))).IsValid())             WriteScalar(TEXT("SkinSettings.Skin.V"), V);
	if ((V = Params->TryGetField(TEXT("roughness"))).IsValid())          WriteScalar(TEXT("SkinSettings.Skin.Roughness"), V);
	if ((V = Params->TryGetField(TEXT("face_texture_index"))).IsValid()) WriteScalar(TEXT("SkinSettings.Skin.FaceTextureIndex"), V);
	if ((V = Params->TryGetField(TEXT("body_texture_index"))).IsValid()) WriteScalar(TEXT("SkinSettings.Skin.BodyTextureIndex"), V);

	// Freckles
	if ((V = Params->TryGetField(TEXT("freckles_density"))).IsValid())    WriteScalar(TEXT("SkinSettings.Freckles.Density"), V);
	if ((V = Params->TryGetField(TEXT("freckles_strength"))).IsValid())   WriteScalar(TEXT("SkinSettings.Freckles.Strength"), V);
	if ((V = Params->TryGetField(TEXT("freckles_saturation"))).IsValid()) WriteScalar(TEXT("SkinSettings.Freckles.Saturation"), V);
	if ((V = Params->TryGetField(TEXT("freckles_tone_shift"))).IsValid()) WriteScalar(TEXT("SkinSettings.Freckles.ToneShift"), V);

	static const TCHAR* AccentZones[] = {
		TEXT("Scalp"), TEXT("Forehead"), TEXT("Nose"), TEXT("UnderEye"),
		TEXT("Cheeks"), TEXT("Lips"), TEXT("Chin"), TEXT("Ears")
	};

	// Broadcast `redness` to every accent zone
	if ((V = Params->TryGetField(TEXT("redness"))).IsValid())
	{
		for (const TCHAR* Zone : AccentZones)
		{
			WriteScalar(*FString::Printf(TEXT("SkinSettings.Accents.%s.Redness"), Zone), V);
		}
	}

	// Per-zone overrides via `zone` object
	const TSharedPtr<FJsonObject>* ZoneObj = nullptr;
	if (Params->TryGetObjectField(TEXT("zone"), ZoneObj) && ZoneObj && ZoneObj->IsValid())
	{
		for (const auto& Pair : (*ZoneObj)->Values)
		{
			const FString& ZoneName = Pair.Key;
			bool bKnown = false;
			for (const TCHAR* Z : AccentZones) { if (ZoneName.Equals(Z, ESearchCase::IgnoreCase)) { bKnown = true; break; } }
			if (!bKnown)
			{
				Skipped.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("zone '%s' is not a known accent zone"), *ZoneName)));
				continue;
			}
			const TSharedPtr<FJsonObject> Inner = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!Inner.IsValid()) continue;

			TSharedPtr<FJsonValue> ZV;
			if ((ZV = Inner->TryGetField(TEXT("redness"))).IsValid())
				WriteScalar(*FString::Printf(TEXT("SkinSettings.Accents.%s.Redness"), *ZoneName), ZV);
			if ((ZV = Inner->TryGetField(TEXT("saturation"))).IsValid())
				WriteScalar(*FString::Printf(TEXT("SkinSettings.Accents.%s.Saturation"), *ZoneName), ZV);
			if ((ZV = Inner->TryGetField(TEXT("lightness"))).IsValid())
				WriteScalar(*FString::Printf(TEXT("SkinSettings.Accents.%s.Lightness"), *ZoneName), ZV);
		}
	}

	if (Applied.Num() == 0 && Skipped.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("No skin params provided. Pass at least one of: tone_u, tone_v, roughness, face_texture_index, body_texture_index, freckles_density, freckles_strength, freckles_saturation, freckles_tone_shift, redness, zone."));
	}

	Character->MarkPackageDirty();
#if WITH_EDITOR
	if (LastTouchedProp)
	{
		FPropertyChangedEvent Evt(LastTouchedProp);
		Character->PostEditChangeProperty(Evt);
	}
#endif

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("character_path"), Character->GetPathName());
	Result->SetArrayField(TEXT("applied"), Applied);
	if (Skipped.Num() > 0)
	{
		Result->SetArrayField(TEXT("skipped"), Skipped);
	}
	Result->SetStringField(TEXT("note"), TEXT("Properties written. Call refresh_metahuman_preview to re-render the editor viewport."));
	return FECACommandResult::Success(Result);
}

//==============================================================================
// describe_metahuman
//==============================================================================

FECACommandResult FECACommand_DescribeMetaHuman::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath, Description;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}
	if (!GetStringParam(Params, TEXT("description"), Description))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: description"));
	}

	if (!GetMetaHumanCharacterClass())
	{
		return FECACommandResult::Error(TEXT("MetaHumanCharacter plugin is not enabled."));
	}

	UObject* Character = LoadMetaHumanCharacter(CharacterPath);
	if (!Character)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not load MetaHumanCharacter at '%s'"), *CharacterPath));
	}

	const FString Lower = Description.ToLower();
	TArray<TSharedPtr<FJsonValue>> MatchedKeywords;
	TSharedPtr<FJsonObject> AppliedProperties = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Notes;

	auto HasWord = [&Lower](const TCHAR* Word) -> bool
	{
		return Lower.Contains(Word);
	};

	auto ApplyImport = [&](const FString& Path, const FString& ImportText) -> bool
	{
		FProperty* Prop = nullptr;
		void* ValuePtr = nullptr;
		FString Err;
		if (!ResolvePropertyPath(Character, Path, Prop, ValuePtr, Err) || !Prop || !ValuePtr)
		{
			Notes.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("skip '%s': %s"), *Path, *Err)));
			return false;
		}
		const TCHAR* R = Prop->ImportText_Direct(*ImportText, ValuePtr, Character, PPF_None);
		if (!R)
		{
			Notes.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("skip '%s': ImportText failed for '%s'"), *Path, *ImportText)));
			return false;
		}
		AppliedProperties->SetStringField(Path, ImportText);
		return true;
	};

	// ─── Size / body type ──────────────────────────────
	FString BodyKeyword;
	if (HasWord(TEXT("tiny")) || HasWord(TEXT("small")) || HasWord(TEXT("petite")) || HasWord(TEXT("short")))
	{
		BodyKeyword = TEXT("small");
	}
	else if (HasWord(TEXT("muscular")) || HasWord(TEXT("buff")) || HasWord(TEXT("jacked")))
	{
		BodyKeyword = TEXT("muscular");
	}
	else if (HasWord(TEXT("athletic")) || HasWord(TEXT("fit")) || HasWord(TEXT("toned")))
	{
		BodyKeyword = TEXT("athletic");
	}
	else if (HasWord(TEXT("large")) || HasWord(TEXT("big")) || HasWord(TEXT("tall")))
	{
		BodyKeyword = TEXT("tall");
	}
	else
	{
		BodyKeyword = TEXT("medium");
	}
	MatchedKeywords.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("body=%s"), *BodyKeyword)));
	Notes.Add(MakeShared<FJsonValueString>(
		TEXT("Body keyword recorded but NOT applied — body shape morphing requires calling UMetaHumanCharacterEditorSubsystem::SetBodyConstraints with an FMetaHumanCharacterBodyConstraints struct (height, weight, measurements). describe_metahuman only sets skin/eye/hair color properties on the asset. Body shape will stay at the default parametric body until body-constraint editing is added as a separate command.")));

	// ─── Skin tone ─────────────────────────────────────
	FString SkinKeyword;
	float SkinU = 0.5f, SkinV = 0.5f;
	if (HasWord(TEXT("very dark")) || HasWord(TEXT("black-skinned")) || HasWord(TEXT(" black ")))
	{
		SkinKeyword = TEXT("very_dark"); SkinU = 0.92f; SkinV = 0.9f;
	}
	else if (HasWord(TEXT("dark")))
	{
		SkinKeyword = TEXT("dark"); SkinU = 0.8f; SkinV = 0.8f;
	}
	else if (HasWord(TEXT("tan")) || HasWord(TEXT("olive")) || HasWord(TEXT("brown")))
	{
		SkinKeyword = TEXT("medium_dark"); SkinU = 0.6f; SkinV = 0.6f;
	}
	else if (HasWord(TEXT("pale")) || HasWord(TEXT("white")) || HasWord(TEXT("fair")) || HasWord(TEXT("light")))
	{
		SkinKeyword = TEXT("light"); SkinU = 0.15f; SkinV = 0.15f;
	}
	else if (HasWord(TEXT("medium")))
	{
		SkinKeyword = TEXT("medium"); SkinU = 0.5f; SkinV = 0.5f;
	}
	if (!SkinKeyword.IsEmpty())
	{
		MatchedKeywords.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("skin=%s"), *SkinKeyword)));
		ApplyImport(TEXT("SkinSettings.Skin.U"), FString::SanitizeFloat(SkinU));
		ApplyImport(TEXT("SkinSettings.Skin.V"), FString::SanitizeFloat(SkinV));
	}

	// ─── Eye color ─────────────────────────────────────
	FString EyeKeyword;
	float EyeU = 0.5f, EyeV = 0.5f;
	if (HasWord(TEXT("blue eye")) || HasWord(TEXT("blue-eyed")))
	{
		EyeKeyword = TEXT("blue"); EyeU = 0.15f; EyeV = 0.6f;
	}
	else if (HasWord(TEXT("green eye")) || HasWord(TEXT("green-eyed")))
	{
		EyeKeyword = TEXT("green"); EyeU = 0.35f; EyeV = 0.5f;
	}
	else if (HasWord(TEXT("hazel")))
	{
		EyeKeyword = TEXT("hazel"); EyeU = 0.55f; EyeV = 0.4f;
	}
	else if (HasWord(TEXT("brown eye")) || HasWord(TEXT("brown-eyed")))
	{
		EyeKeyword = TEXT("brown"); EyeU = 0.8f; EyeV = 0.3f;
	}
	else if (HasWord(TEXT("gray eye")) || HasWord(TEXT("grey eye")))
	{
		EyeKeyword = TEXT("gray"); EyeU = 0.5f; EyeV = 0.8f;
	}
	if (!EyeKeyword.IsEmpty())
	{
		MatchedKeywords.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("eyes=%s"), *EyeKeyword)));
		const FString Us = FString::SanitizeFloat(EyeU);
		const FString Vs = FString::SanitizeFloat(EyeV);
		ApplyImport(TEXT("EyesSettings.EyeLeft.Iris.PrimaryColorU"),  Us);
		ApplyImport(TEXT("EyesSettings.EyeLeft.Iris.PrimaryColorV"),  Vs);
		ApplyImport(TEXT("EyesSettings.EyeRight.Iris.PrimaryColorU"), Us);
		ApplyImport(TEXT("EyesSettings.EyeRight.Iris.PrimaryColorV"), Vs);
	}

	// ─── Hair color (eyelashes dye color is the closest thing exposed as UPROPERTY) ──
	// True hair color lives in a groom asset assigned by the pipeline, not on the
	// UMetaHumanCharacter directly. We tint the eyelash DyeColor so the character
	// at least reflects the description visually.
	struct FHairColor { const TCHAR* Keyword; const TCHAR* LinearColor; };
	const FHairColor HairTable[] = {
		{ TEXT("purple"), TEXT("(R=0.35,G=0.1,B=0.6,A=1.0)") },
		{ TEXT("violet"), TEXT("(R=0.45,G=0.2,B=0.65,A=1.0)") },
		{ TEXT("pink"),   TEXT("(R=0.95,G=0.4,B=0.65,A=1.0)") },
		{ TEXT("blue"),   TEXT("(R=0.1,G=0.25,B=0.7,A=1.0)")  },
		{ TEXT("green"),  TEXT("(R=0.15,G=0.55,B=0.2,A=1.0)") },
		{ TEXT("red"),    TEXT("(R=0.65,G=0.1,B=0.1,A=1.0)")  },
		{ TEXT("orange"), TEXT("(R=0.85,G=0.45,B=0.1,A=1.0)") },
		{ TEXT("blonde"), TEXT("(R=0.9,G=0.8,B=0.45,A=1.0)")  },
		{ TEXT("brown"),  TEXT("(R=0.3,G=0.18,B=0.08,A=1.0)") },
		{ TEXT("black"),  TEXT("(R=0.04,G=0.03,B=0.02,A=1.0)") },
		{ TEXT("white"),  TEXT("(R=0.95,G=0.95,B=0.95,A=1.0)") },
		{ TEXT("gray"),   TEXT("(R=0.55,G=0.55,B=0.55,A=1.0)") },
		{ TEXT("grey"),   TEXT("(R=0.55,G=0.55,B=0.55,A=1.0)") },
	};

	FString HairKeyword;
	for (const FHairColor& Entry : HairTable)
	{
		const FString Phrase = FString::Printf(TEXT("%s hair"), Entry.Keyword);
		const FString Hyphen = FString::Printf(TEXT("%s-haired"), Entry.Keyword);
		if (Lower.Contains(Phrase) || Lower.Contains(Hyphen))
		{
			HairKeyword = Entry.Keyword;
			MatchedKeywords.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("hair=%s"), Entry.Keyword)));
			// Tint eyelashes — the exposed stand-in for hair color on the character asset
			ApplyImport(TEXT("HeadModelSettings.Eyelashes.DyeColor"), Entry.LinearColor);
			// Drive melanin + lightness away from neutral so the tint actually reads
			ApplyImport(TEXT("HeadModelSettings.Eyelashes.Melanin"), TEXT("0.8"));
			ApplyImport(TEXT("HeadModelSettings.Eyelashes.Lightness"), TEXT("0.6"));
			Notes.Add(MakeShared<FJsonValueString>(
				TEXT("Hair color applied via eyelash DyeColor. TRUE hair (head groom) requires attaching a UGroomAsset via the Hair panel in the MetaHuman editor — describe_metahuman does NOT attach grooms, so characters remain bald until a groom is added.")));
			break;
		}
	}

	Character->MarkPackageDirty();
#if WITH_EDITOR
	Character->PostEditChange();
#endif

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("character_path"),     Character->GetPathName());
	Result->SetStringField(TEXT("description"),        Description);
	Result->SetArrayField (TEXT("matched_keywords"),   MatchedKeywords);
	Result->SetObjectField(TEXT("applied_properties"), AppliedProperties);
	Result->SetArrayField (TEXT("notes"),              Notes);

	return FECACommandResult::Success(Result);
}

// ============================================================================
// open_metahuman_editor
// ============================================================================

FECACommandResult FECACommand_OpenMetaHumanEditor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	UAssetEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!EditorSubsystem)
	{
		return FECACommandResult::Error(TEXT("AssetEditorSubsystem not available"));
	}

	bool bOpened = EditorSubsystem->OpenEditorForAsset(Asset);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), CharacterPath);
	Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	Result->SetBoolField(TEXT("opened"), bOpened);
	if (!bOpened)
	{
		Result->SetStringField(TEXT("note"), TEXT("OpenEditorForAsset returned false — asset editor may not be registered for this class"));
	}

	return FECACommandResult::Success(Result);
}

// ============================================================================
// build_metahuman
// ============================================================================

FECACommandResult FECACommand_BuildMetaHuman::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CharacterPath;
	if (!GetStringParam(Params, TEXT("character_path"), CharacterPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: character_path"));
	}

	UObject* AssetObj = LoadObject<UObject>(nullptr, *CharacterPath);
	if (!AssetObj)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load asset: %s"), *CharacterPath));
	}

	// Verify it's a MetaHumanCharacter
	UClass* MHClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (!MHClass || !AssetObj->IsA(MHClass))
	{
		return FECACommandResult::Error(TEXT("Asset is not a MetaHumanCharacter. MetaHuman plugin may not be loaded."));
	}

	// The MetaHuman build pipeline is driven by UMetaHumanCharacterEditorSubsystem
	// Find it by class name (avoid compile-time dependency on editor subsystem header)
	UClass* EditorSubsystemClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEditorSubsystem"));
	if (!EditorSubsystemClass)
	{
		return FECACommandResult::Error(TEXT("MetaHumanCharacterEditorSubsystem class not found. MetaHumanCharacterEditor module may not be loaded."));
	}

	UEditorEngine* EdEngine = Cast<UEditorEngine>(GEditor);
	if (!EdEngine)
	{
		return FECACommandResult::Error(TEXT("GEditor not available"));
	}

	UEditorSubsystem* MHEditorSubsystem = EdEngine->GetEditorSubsystemBase(EditorSubsystemClass);
	if (!MHEditorSubsystem)
	{
		return FECACommandResult::Error(TEXT("Could not get MetaHumanCharacterEditorSubsystem instance"));
	}

	// Try canonical UE 5.7 function names first — but use safe call path instead of inline ProcessEvent
	UFunction* BuildFunc = MHEditorSubsystem->FindFunction(FName(TEXT("BuildMetaHuman")));
	FString FoundName = BuildFunc ? TEXT("BuildMetaHuman") : TEXT("");
	if (!BuildFunc)
	{
		BuildFunc = MHEditorSubsystem->FindFunction(FName(TEXT("RunCharacterEditorPipelineForPreview")));
		if (BuildFunc) FoundName = TEXT("RunCharacterEditorPipelineForPreview");
	}
	if (!BuildFunc)
	{
		BuildFunc = MHEditorSubsystem->FindFunction(FName(TEXT("Build")));
		if (BuildFunc) FoundName = TEXT("Build");
	}
	if (!BuildFunc)
	{
		BuildFunc = MHEditorSubsystem->FindFunction(FName(TEXT("AssembleCollection")));
		if (BuildFunc) FoundName = TEXT("AssembleCollection");
	}

	// Open the MetaHuman editor first to initialize auth/toolkit state, then invoke.
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(AssetObj);
	}

	if (BuildFunc)
	{
		return CallMHEditorFunction(Params, FoundName, TEXT("build"));
	}

	// No known build function found — list what's available
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	TArray<TSharedPtr<FJsonValue>> AvailableFuncs;
	for (TFieldIterator<UFunction> FuncIt(EditorSubsystemClass); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Native))
		{
			AvailableFuncs.Add(MakeShared<FJsonValueString>(Func->GetName()));
		}
	}
	Result->SetArrayField(TEXT("available_functions"), AvailableFuncs);
	Result->SetStringField(TEXT("note"), TEXT("No known build function found. Pick one from available_functions."));
	Result->SetBoolField(TEXT("triggered"), false);
	return FECACommandResult::Success(Result);
}
