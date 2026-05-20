// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAAnimRetargetCommands.h"
#include "Commands/ECACommand.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"

#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyAccessUtil.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Templates/SharedPointer.h"

REGISTER_ECA_COMMAND(FECACommand_ListIKRetargeters)
REGISTER_ECA_COMMAND(FECACommand_DumpIKRetargeter)
REGISTER_ECA_COMMAND(FECACommand_ListIKRigs)

namespace ECAAnimRetargetHelpers
{
	// Resolve a class by name without compile-time dependency on its module.
	static UClass* FindClassByName(const FString& Name)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static bool IsIKRigPluginLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("IKRig"));
	}

	static UObject* LoadAssetTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		UObject* Obj = LoadObject<UObject>(nullptr, *Path);
		if (Obj) return Obj;
		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Obj = LoadObject<UObject>(nullptr, *FullPath);
		}
		return Obj;
	}

	// Read a UObject* property by name and return its asset path (or empty).
	static FString ReadObjectPropertyPath(UObject* Container, const TCHAR* PropName)
	{
		if (!Container) return FString();
		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop);
		if (!ObjProp) return FString();
		const void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(Container);
		UObject* Referenced = ObjProp->GetObjectPropertyValue(ValuePtr);
		return Referenced ? Referenced->GetPathName() : FString();
	}

	// Read a struct-typed array property (e.g. RetargetOps as TArray<FInstancedStruct>)
	// and return each element's ScriptStruct name. Used reflectively so we don't
	// have to include FInstancedStruct's header.
	static TArray<FString> ReadInstancedStructArrayTypeNames(UObject* Container, const TCHAR* PropName)
	{
		TArray<FString> Out;
		if (!Container) return Out;

		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (!ArrayProp) return Out;

		FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
		if (!InnerStruct) return Out;

		const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Container);
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			const void* ElemPtr = Helper.GetRawPtr(i);
			// FInstancedStruct has a UScriptStruct* + memory blob. Find a
			// UScriptStruct*-typed field by name on the inner struct.
			FProperty* TypeProp = InnerStruct->Struct->FindPropertyByName(TEXT("ScriptStruct"));
			if (!TypeProp)
			{
				// Try alternate field names
				for (TFieldIterator<FProperty> It(InnerStruct->Struct); It; ++It)
				{
					FObjectPropertyBase* O = CastField<FObjectPropertyBase>(*It);
					if (O && O->PropertyClass && O->PropertyClass->IsChildOf(UScriptStruct::StaticClass()))
					{
						TypeProp = *It;
						break;
					}
				}
			}
			FObjectPropertyBase* TypeObj = CastField<FObjectPropertyBase>(TypeProp);
			if (TypeObj)
			{
				const void* InnerVal = TypeObj->ContainerPtrToValuePtr<void>(ElemPtr);
				UObject* TypeRef = TypeObj->GetObjectPropertyValue(InnerVal);
				Out.Add(TypeRef ? TypeRef->GetName() : FString(TEXT("Unknown")));
			}
			else
			{
				Out.Add(InnerStruct->Struct ? InnerStruct->Struct->GetName() : FString(TEXT("Unknown")));
			}
		}
		return Out;
	}

	// Read a TArray<FStructType> property and run a per-element extractor.
	static int32 GetStructArrayNum(UObject* Container, const TCHAR* PropName)
	{
		if (!Container) return 0;
		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (!ArrayProp) return 0;
		const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Container);
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		return Helper.Num();
	}

	// Read an FName property by name.
	static FString ReadNameProperty(UObject* Container, const TCHAR* PropName)
	{
		if (!Container) return FString();
		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FNameProperty* NameProp = CastField<FNameProperty>(Prop);
		if (!NameProp) return FString();
		const void* ValuePtr = NameProp->ContainerPtrToValuePtr<void>(Container);
		FName Out = NameProp->GetPropertyValue(ValuePtr);
		return Out.ToString();
	}

	// Enumerate names of FStructType elements that have a FName "Name" or
	// equivalent field. Used to surface retarget pose names without including
	// IKRetargeter headers.
	static TArray<FString> ReadStructArrayNames(UObject* Container, const TCHAR* PropName, const TCHAR* NameFieldName = TEXT("Name"))
	{
		TArray<FString> Out;
		if (!Container) return Out;
		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (!ArrayProp) return Out;

		FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
		if (!InnerStruct || !InnerStruct->Struct) return Out;

		FProperty* NameProp = InnerStruct->Struct->FindPropertyByName(FName(NameFieldName));
		const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Container);
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			const void* ElemPtr = Helper.GetRawPtr(i);
			if (FNameProperty* N = CastField<FNameProperty>(NameProp))
			{
				FName V = N->GetPropertyValue(N->ContainerPtrToValuePtr<void>(ElemPtr));
				Out.Add(V.ToString());
			}
			else if (FStrProperty* S = CastField<FStrProperty>(NameProp))
			{
				Out.Add(S->GetPropertyValue(S->ContainerPtrToValuePtr<void>(ElemPtr)));
			}
		}
		return Out;
	}

	// Enumerate a TMap<FName, X> by key (X may be anything; we only return keys).
	static TArray<FString> ReadMapKeysAsNames(UObject* Container, const TCHAR* PropName)
	{
		TArray<FString> Out;
		if (!Container) return Out;
		FProperty* Prop = Container->GetClass()->FindPropertyByName(FName(PropName));
		FMapProperty* MapProp = CastField<FMapProperty>(Prop);
		if (!MapProp) return Out;
		FNameProperty* KeyName = CastField<FNameProperty>(MapProp->KeyProp);
		if (!KeyName) return Out;
		const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(Container);
		FScriptMapHelper Helper(MapProp, MapPtr);
		for (FScriptMapHelper::FIterator It = Helper.CreateIterator(); It; ++It)
		{
			const void* KeyData = Helper.GetKeyPtr(It);
			FName Key = KeyName->GetPropertyValue(KeyData);
			Out.Add(Key.ToString());
		}
		return Out;
	}

	// Query the asset registry for assets of a given class path string. Returns
	// FAssetData entries; the caller decides whether to load each one.
	static void FindAssetsByClass(const FString& ClassPathStr, const FString& PathFilter, const FString& NameFilter, TArray<FAssetData>& OutAssets)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		if (!PathFilter.IsEmpty()) Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(FTopLevelAssetPath(*ClassPathStr));
		Filter.bRecursiveClasses = true;
		AR.GetAssets(Filter, OutAssets);

		if (!NameFilter.IsEmpty())
		{
			OutAssets.RemoveAll([&NameFilter](const FAssetData& A)
			{
				return !A.AssetName.ToString().MatchesWildcard(NameFilter);
			});
		}
	}
}

//==============================================================================
// list_ik_retargeters
//==============================================================================
FECACommandResult FECACommand_ListIKRetargeters::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAnimRetargetHelpers;

	FString PathFilter = TEXT("/Game/");
	GetStringParam(Params, TEXT("path_filter"), PathFilter, false);
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("plugin_available"), IsIKRigPluginLoaded());

	TArray<FAssetData> AssetDataList;
	FindAssetsByClass(TEXT("/Script/IKRig.IKRetargeter"), PathFilter, NameFilter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FAssetData& Data : AssetDataList)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Data.GetObjectPathString());
		Obj->SetStringField(TEXT("name"), Data.AssetName.ToString());

		UObject* Asset = Data.GetAsset();
		if (Asset)
		{
			Obj->SetStringField(TEXT("source_ik_rig"),       ReadObjectPropertyPath(Asset, TEXT("SourceIKRigAsset")));
			Obj->SetStringField(TEXT("target_ik_rig"),       ReadObjectPropertyPath(Asset, TEXT("TargetIKRigAsset")));
			Obj->SetStringField(TEXT("source_preview_mesh"), ReadObjectPropertyPath(Asset, TEXT("SourcePreviewMesh")));
			Obj->SetStringField(TEXT("target_preview_mesh"), ReadObjectPropertyPath(Asset, TEXT("TargetPreviewMesh")));
			Obj->SetNumberField(TEXT("op_count"),            GetStructArrayNum(Asset, TEXT("RetargetOps")));
		}
		Out.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Result->SetNumberField(TEXT("count"),       Out.Num());
	Result->SetArrayField (TEXT("retargeters"), Out);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_ik_retargeter
//==============================================================================
FECACommandResult FECACommand_DumpIKRetargeter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAnimRetargetHelpers;

	FString AssetPath;
	if (!GetStringParam(Params, TEXT("retargeter_path"), AssetPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: retargeter_path"));
	}

	UObject* Asset = LoadAssetTolerant(AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load asset at: %s"), *AssetPath));
	}
	if (!Asset->GetClass()->GetName().Equals(TEXT("IKRetargeter"), ESearchCase::IgnoreCase))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset at '%s' is not a UIKRetargeter (got %s)"), *AssetPath, *Asset->GetClass()->GetName()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"),                  Asset->GetPathName());
	Result->SetStringField(TEXT("name"),                  Asset->GetName());
	Result->SetStringField(TEXT("source_ik_rig"),         ReadObjectPropertyPath(Asset, TEXT("SourceIKRigAsset")));
	Result->SetStringField(TEXT("target_ik_rig"),         ReadObjectPropertyPath(Asset, TEXT("TargetIKRigAsset")));
	Result->SetStringField(TEXT("source_preview_mesh"),   ReadObjectPropertyPath(Asset, TEXT("SourcePreviewMesh")));
	Result->SetStringField(TEXT("target_preview_mesh"),   ReadObjectPropertyPath(Asset, TEXT("TargetPreviewMesh")));
	Result->SetStringField(TEXT("current_retarget_pose"), ReadNameProperty(Asset, TEXT("CurrentRetargetPose")));

	// Retarget op stack
	TArray<FString> OpStructNames = ReadInstancedStructArrayTypeNames(Asset, TEXT("RetargetOps"));
	TArray<TSharedPtr<FJsonValue>> OpsArr;
	for (const FString& OpName : OpStructNames)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("struct_type"), OpName);
		OpsArr.Add(MakeShared<FJsonValueObject>(O));
	}
	Result->SetArrayField(TEXT("retarget_ops"), OpsArr);

	// Source retarget pose names — UIKRetargeter stores these as TMap<FName, FIKRetargetPose>
	// in fields named SourceRetargetPoses / TargetRetargetPoses (or "RetargetPoses" in older versions).
	TArray<FString> PoseNames = ReadMapKeysAsNames(Asset, TEXT("SourceRetargetPoses"));
	if (PoseNames.Num() == 0) PoseNames = ReadMapKeysAsNames(Asset, TEXT("RetargetPoses"));

	TArray<TSharedPtr<FJsonValue>> PoseArr;
	for (const FString& P : PoseNames) PoseArr.Add(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));
	// Use string array for readability
	TArray<TSharedPtr<FJsonValue>> PoseStrArr;
	for (const FString& P : PoseNames) PoseStrArr.Add(MakeShared<FJsonValueString>(P));
	Result->SetArrayField(TEXT("retarget_poses"), PoseStrArr);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// list_ik_rigs
//==============================================================================
FECACommandResult FECACommand_ListIKRigs::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECAAnimRetargetHelpers;

	FString PathFilter = TEXT("/Game/");
	GetStringParam(Params, TEXT("path_filter"), PathFilter, false);
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("plugin_available"), IsIKRigPluginLoaded());

	TArray<FAssetData> AssetDataList;
	FindAssetsByClass(TEXT("/Script/IKRig.IKRigDefinition"), PathFilter, NameFilter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FAssetData& Data : AssetDataList)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Data.GetObjectPathString());
		Obj->SetStringField(TEXT("name"), Data.AssetName.ToString());

		UObject* Asset = Data.GetAsset();
		if (Asset)
		{
			Obj->SetStringField(TEXT("preview_mesh"), ReadObjectPropertyPath(Asset, TEXT("PreviewSkeletalMesh")));
			Obj->SetNumberField(TEXT("chain_count"),  GetStructArrayNum(Asset, TEXT("RetargetDefinition")));
			// IKRig stores chains under a sub-struct; try a few known field names
			int32 Chains = GetStructArrayNum(Asset, TEXT("RetargetChains"));
			if (Chains == 0) Chains = GetStructArrayNum(Asset, TEXT("Chains"));
			Obj->SetNumberField(TEXT("chain_count"),  Chains);
			Obj->SetNumberField(TEXT("goal_count"),   GetStructArrayNum(Asset, TEXT("Goals")));
		}
		Out.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Result->SetNumberField(TEXT("count"), Out.Num());
	Result->SetArrayField (TEXT("rigs"),  Out);
	return FECACommandResult::Success(Result);
}
