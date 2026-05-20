// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECALiveLinkCommands.h"

#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "LiveLinkRole.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"

namespace
{
	ILiveLinkClient* GetLiveLinkClientIfAvailable()
	{
		IModularFeatures& Features = IModularFeatures::Get();
		if (!Features.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			return nullptr;
		}
		return &Features.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	}

	const TCHAR* SubjectStateToString(ELiveLinkSubjectState State)
	{
		switch (State)
		{
			case ELiveLinkSubjectState::Connected:        return TEXT("connected");
			case ELiveLinkSubjectState::Unresponsive:     return TEXT("unresponsive");
			case ELiveLinkSubjectState::Disconnected:     return TEXT("disconnected");
			case ELiveLinkSubjectState::InvalidOrDisabled:return TEXT("invalid_or_disabled");
			case ELiveLinkSubjectState::Paused:           return TEXT("paused");
			case ELiveLinkSubjectState::Unknown:          default: return TEXT("unknown");
		}
	}

	TSharedPtr<FJsonObject> SubjectKeyToJson(ILiveLinkClient& Client, const FLiveLinkSubjectKey& Key)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Key.SubjectName.ToString());
		Obj->SetStringField(TEXT("source_guid"), Key.Source.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("source_machine"), Client.GetSourceMachineName(Key.Source).ToString());
		Obj->SetStringField(TEXT("source_type"), Client.GetSourceType(Key.Source).ToString());

		const TSubclassOf<ULiveLinkRole> Role = Client.GetSubjectRole_AnyThread(Key);
		Obj->SetStringField(TEXT("role"), Role ? Role->GetName() : FString(TEXT("")));

		Obj->SetBoolField(TEXT("is_virtual"), Client.IsVirtualSubject(Key));
		Obj->SetBoolField(TEXT("is_enabled"), Client.IsSubjectEnabled(Key, /*bForThisFrame*/ false));
		Obj->SetStringField(TEXT("state"), SubjectStateToString(Client.GetSubjectState(Key.SubjectName)));
		return Obj;
	}
}

FECACommandResult FECACommand_ListLiveLinkSubjects::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	ILiveLinkClient* Client = GetLiveLinkClientIfAvailable();
	if (!Client)
	{
		Result->SetBoolField(TEXT("available"), false);
		Result->SetStringField(TEXT("message"), TEXT("ILiveLinkClient modular feature is not registered. Is the LiveLink plugin enabled in this project?"));
		Result->SetArrayField(TEXT("subjects"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	bool bIncludeDisabled = true;
	bool bIncludeVirtual = true;
	if (Params.IsValid())
	{
		Params->TryGetBoolField(TEXT("include_disabled"), bIncludeDisabled);
		Params->TryGetBoolField(TEXT("include_virtual"), bIncludeVirtual);
	}

	const TArray<FLiveLinkSubjectKey> Subjects = Client->GetSubjects(bIncludeDisabled, bIncludeVirtual);

	TArray<TSharedPtr<FJsonValue>> SubjectArr;
	SubjectArr.Reserve(Subjects.Num());
	for (const FLiveLinkSubjectKey& Key : Subjects)
	{
		SubjectArr.Add(MakeShared<FJsonValueObject>(SubjectKeyToJson(*Client, Key)));
	}

	Result->SetBoolField(TEXT("available"), true);
	Result->SetNumberField(TEXT("count"), SubjectArr.Num());
	Result->SetArrayField(TEXT("subjects"), SubjectArr);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListLiveLinkSubjects);

FECACommandResult FECACommand_DumpLiveLinkData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SubjectName;
	if (!GetStringParam(Params, TEXT("subject_name"), SubjectName, /*bRequired*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("subject_name is required"));
	}

	int32 RecentFrames = 5;
	if (Params.IsValid())
	{
		double Tmp = 5.0;
		if (Params->TryGetNumberField(TEXT("recent_frames"), Tmp))
		{
			RecentFrames = FMath::Clamp(FMath::FloorToInt(Tmp), 0, 200);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	ILiveLinkClient* Client = GetLiveLinkClientIfAvailable();
	if (!Client)
	{
		Result->SetBoolField(TEXT("available"), false);
		Result->SetStringField(TEXT("message"), TEXT("ILiveLinkClient modular feature is not registered. Is the LiveLink plugin enabled in this project?"));
		return FECACommandResult::Success(Result);
	}

	const FLiveLinkSubjectName SubjectFName(*SubjectName);
	const TArray<FLiveLinkSubjectKey> Candidates = Client->GetSubjects(/*bIncludeDisabled*/ true, /*bIncludeVirtual*/ true);

	const FLiveLinkSubjectKey* MatchedKey = nullptr;
	for (const FLiveLinkSubjectKey& Key : Candidates)
	{
		if (Key.SubjectName == SubjectFName)
		{
			MatchedKey = &Key;
			break;
		}
	}

	if (!MatchedKey)
	{
		Result->SetBoolField(TEXT("available"), true);
		Result->SetBoolField(TEXT("found"), false);
		Result->SetStringField(TEXT("subject_name"), SubjectName);
		Result->SetStringField(TEXT("message"), FString::Printf(TEXT("No LiveLink subject named '%s' is currently registered."), *SubjectName));
		return FECACommandResult::Success(Result);
	}

	Result->SetBoolField(TEXT("available"), true);
	Result->SetBoolField(TEXT("found"), true);
	Result->SetObjectField(TEXT("subject"), SubjectKeyToJson(*Client, *MatchedKey));

	const TArray<FLiveLinkTime> FrameTimes = Client->GetSubjectFrameTimes(*MatchedKey);
	TArray<TSharedPtr<FJsonValue>> TimeArr;
	const int32 Start = FMath::Max(0, FrameTimes.Num() - RecentFrames);
	for (int32 i = Start; i < FrameTimes.Num(); ++i)
	{
		TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("world_time_seconds"), FrameTimes[i].WorldTime);
		T->SetNumberField(TEXT("scene_time_seconds"), FrameTimes[i].SceneTime.AsSeconds());
		TimeArr.Add(MakeShared<FJsonValueObject>(T));
	}
	Result->SetNumberField(TEXT("total_frame_times"), FrameTimes.Num());
	Result->SetArrayField(TEXT("recent_frame_times"), TimeArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DumpLiveLinkData);

namespace ECALiveLinkPresetHelpers
{
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

	static bool IsLiveLinkPluginLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("LiveLink"));
	}

	static void FindLiveLinkPresets(const FString& PathFilter, const FString& NameFilter, TArray<FAssetData>& OutAssets)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		if (!PathFilter.IsEmpty()) Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/LiveLink"), TEXT("LiveLinkPreset")));
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

	static int32 CountStructArray(UObject* Asset, const TCHAR* FieldName)
	{
		if (!Asset) return 0;
		FProperty* Prop = Asset->GetClass()->FindPropertyByName(FName(FieldName));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (!ArrayProp) return 0;
		const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Asset);
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		return Helper.Num();
	}

	// Read a field on a struct element. Returns the field as a JSON value.
	static TSharedPtr<FJsonValue> ReadStructField(FStructProperty* StructProp, const void* ElemPtr, const TCHAR* FieldName)
	{
		if (!StructProp || !StructProp->Struct || !ElemPtr) return MakeShared<FJsonValueNull>();
		FProperty* Field = StructProp->Struct->FindPropertyByName(FName(FieldName));
		if (!Field) return MakeShared<FJsonValueNull>();
		const void* ValuePtr = Field->ContainerPtrToValuePtr<void>(ElemPtr);

		if (FNameProperty* N = CastField<FNameProperty>(Field))
		{
			return MakeShared<FJsonValueString>(N->GetPropertyValue(ValuePtr).ToString());
		}
		if (FStrProperty* S = CastField<FStrProperty>(Field))
		{
			return MakeShared<FJsonValueString>(S->GetPropertyValue(ValuePtr));
		}
		if (FBoolProperty* B = CastField<FBoolProperty>(Field))
		{
			return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(ValuePtr));
		}
		if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(Field))
		{
			UObject* Ref = O->GetObjectPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(Ref ? Ref->GetPathName() : FString());
		}
		if (FClassProperty* C = CastField<FClassProperty>(Field))
		{
			UObject* Ref = C->GetObjectPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(Ref ? Ref->GetName() : FString());
		}
		if (FSoftObjectProperty* SO = CastField<FSoftObjectProperty>(Field))
		{
			FSoftObjectPtr* Ptr = (FSoftObjectPtr*)ValuePtr;
			return MakeShared<FJsonValueString>(Ptr ? Ptr->ToString() : FString());
		}
		if (FSoftClassProperty* SC = CastField<FSoftClassProperty>(Field))
		{
			FSoftObjectPtr* Ptr = (FSoftObjectPtr*)ValuePtr;
			return MakeShared<FJsonValueString>(Ptr ? Ptr->ToString() : FString());
		}
		FString S;
		Field->ExportText_Direct(S, ValuePtr, ValuePtr, nullptr, PPF_None);
		return MakeShared<FJsonValueString>(S.Left(256));
	}

	// Call a UFUNCTION on a UObject reflectively. Returns the bool return value
	// (or true if the function has no return). Used to invoke ULiveLinkPreset's
	// AddToClient / BuildFromClient without linking the LiveLink module.
	static bool InvokeBoolFunction(UObject* Target, const TCHAR* FuncName, const TArray<TPair<FString, FString>>& BoolArgs)
	{
		if (!Target) return false;
		UFunction* Func = Target->FindFunction(FName(FuncName));
		if (!Func) return false;

		uint8* ParamMem = (uint8*)FMemory_Alloca(Func->ParmsSize);
		FMemory::Memzero(ParamMem, Func->ParmsSize);

		// Initialize each param using its property's default
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			FProperty* P = *It;
			if (P->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				P->InitializeValue_InContainer(ParamMem);
			}
			else
			{
				P->InitializeValue_InContainer(ParamMem);
			}
		}

		// Set bool args
		for (const TPair<FString, FString>& Arg : BoolArgs)
		{
			FProperty* P = Func->FindPropertyByName(FName(*Arg.Key));
			FBoolProperty* B = CastField<FBoolProperty>(P);
			if (B)
			{
				const bool V = Arg.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
				B->SetPropertyValue_InContainer(ParamMem, V);
			}
		}

		Target->ProcessEvent(Func, ParamMem);

		// Read return value if present and is bool
		bool ReturnValue = true;
		FProperty* RetProp = Func->GetReturnProperty();
		if (FBoolProperty* RB = CastField<FBoolProperty>(RetProp))
		{
			ReturnValue = RB->GetPropertyValue_InContainer(ParamMem);
		}

		// Destroy any non-trivially-destructible params
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			It->DestroyValue_InContainer(ParamMem);
		}

		return ReturnValue;
	}
}

//==============================================================================
// list_livelink_presets
//==============================================================================
FECACommandResult FECACommand_ListLiveLinkPresets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECALiveLinkPresetHelpers;

	FString PathFilter = TEXT("/Game/");
	GetStringParam(Params, TEXT("path_filter"), PathFilter, false);
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("plugin_available"), IsLiveLinkPluginLoaded());

	TArray<FAssetData> Assets;
	FindLiveLinkPresets(PathFilter, NameFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FAssetData& Data : Assets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Data.GetObjectPathString());
		Obj->SetStringField(TEXT("name"), Data.AssetName.ToString());

		UObject* Asset = Data.GetAsset();
		Obj->SetNumberField(TEXT("source_count"),  CountStructArray(Asset, TEXT("Sources")));
		Obj->SetNumberField(TEXT("subject_count"), CountStructArray(Asset, TEXT("Subjects")));

		Out.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Result->SetNumberField(TEXT("count"),   Out.Num());
	Result->SetArrayField (TEXT("presets"), Out);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListLiveLinkPresets);

//==============================================================================
// dump_livelink_preset
//==============================================================================
FECACommandResult FECACommand_DumpLiveLinkPreset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECALiveLinkPresetHelpers;

	FString AssetPath;
	if (!GetStringParam(Params, TEXT("preset_path"), AssetPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: preset_path"));
	}

	UObject* Asset = LoadAssetTolerant(AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load asset at: %s"), *AssetPath));
	}
	if (!Asset->GetClass()->GetName().Equals(TEXT("LiveLinkPreset"), ESearchCase::IgnoreCase))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset at '%s' is not a ULiveLinkPreset (got %s)"), *AssetPath, *Asset->GetClass()->GetName()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), Asset->GetPathName());
	Result->SetStringField(TEXT("name"), Asset->GetName());

	// Sources
	TArray<TSharedPtr<FJsonValue>> SourcesArr;
	{
		FProperty* P = Asset->GetClass()->FindPropertyByName(TEXT("Sources"));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
		FStructProperty* StructInner = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (ArrayProp && StructInner)
		{
			const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Asset);
			FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				const void* Elem = Helper.GetRawPtr(i);
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetField(TEXT("factory"),     ReadStructField(StructInner, Elem, TEXT("SourceFactory")));
				O->SetField(TEXT("source_type"), ReadStructField(StructInner, Elem, TEXT("SourceType")));
				O->SetField(TEXT("guid"),        ReadStructField(StructInner, Elem, TEXT("Guid")));
				SourcesArr.Add(MakeShared<FJsonValueObject>(O));
			}
		}
	}
	Result->SetArrayField(TEXT("sources"), SourcesArr);

	// Subjects
	TArray<TSharedPtr<FJsonValue>> SubjectsArr;
	{
		FProperty* P = Asset->GetClass()->FindPropertyByName(TEXT("Subjects"));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(P);
		FStructProperty* StructInner = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (ArrayProp && StructInner)
		{
			const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Asset);
			FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				const void* Elem = Helper.GetRawPtr(i);
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetField(TEXT("name"),    ReadStructField(StructInner, Elem, TEXT("Name")));
				O->SetField(TEXT("role"),    ReadStructField(StructInner, Elem, TEXT("Role")));
				O->SetField(TEXT("enabled"), ReadStructField(StructInner, Elem, TEXT("bEnabled")));
				SubjectsArr.Add(MakeShared<FJsonValueObject>(O));
			}
		}
	}
	Result->SetArrayField(TEXT("subjects"), SubjectsArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DumpLiveLinkPreset);

//==============================================================================
// apply_livelink_preset
//==============================================================================
FECACommandResult FECACommand_ApplyLiveLinkPreset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECALiveLinkPresetHelpers;

	FString AssetPath;
	if (!GetStringParam(Params, TEXT("preset_path"), AssetPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: preset_path"));
	}

	bool bAdditive = true;
	GetBoolParam(Params, TEXT("additive"), bAdditive, false);
	bool bRecreate = true;
	GetBoolParam(Params, TEXT("recreate"), bRecreate, false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("plugin_available"), IsLiveLinkPluginLoaded());
	Result->SetStringField(TEXT("mode"), bAdditive ? TEXT("AddToClient") : TEXT("ApplyToClient"));

	if (!IsLiveLinkPluginLoaded())
	{
		Result->SetBoolField(TEXT("applied"), false);
		Result->SetStringField(TEXT("message"), TEXT("LiveLink plugin is not loaded."));
		return FECACommandResult::Success(Result);
	}

	UObject* Asset = LoadAssetTolerant(AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load asset at: %s"), *AssetPath));
	}
	if (!Asset->GetClass()->GetName().Equals(TEXT("LiveLinkPreset"), ESearchCase::IgnoreCase))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset at '%s' is not a ULiveLinkPreset"), *AssetPath));
	}

	Result->SetStringField(TEXT("path"), Asset->GetPathName());

	bool bOk = false;
	if (bAdditive)
	{
		bOk = InvokeBoolFunction(Asset, TEXT("AddToClient"),
			{ { TEXT("bRecreatePresets"), bRecreate ? TEXT("true") : TEXT("false") } });
	}
	else
	{
		bOk = InvokeBoolFunction(Asset, TEXT("ApplyToClient"), {});
	}

	Result->SetBoolField(TEXT("applied"), bOk);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ApplyLiveLinkPreset);

//==============================================================================
// build_livelink_preset_from_client
//==============================================================================
FECACommandResult FECACommand_BuildLiveLinkPresetFromClient::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace ECALiveLinkPresetHelpers;

	FString AssetPath;
	if (!GetStringParam(Params, TEXT("preset_path"), AssetPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: preset_path"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("plugin_available"), IsLiveLinkPluginLoaded());

	if (!IsLiveLinkPluginLoaded())
	{
		Result->SetStringField(TEXT("message"), TEXT("LiveLink plugin is not loaded."));
		Result->SetNumberField(TEXT("source_count"), 0);
		Result->SetNumberField(TEXT("subject_count"), 0);
		return FECACommandResult::Success(Result);
	}

	UObject* Asset = LoadAssetTolerant(AssetPath);
	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load asset at: %s"), *AssetPath));
	}
	if (!Asset->GetClass()->GetName().Equals(TEXT("LiveLinkPreset"), ESearchCase::IgnoreCase))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset at '%s' is not a ULiveLinkPreset"), *AssetPath));
	}

	InvokeBoolFunction(Asset, TEXT("BuildFromClient"), {});
	Asset->MarkPackageDirty();

	Result->SetStringField(TEXT("path"),          Asset->GetPathName());
	Result->SetNumberField(TEXT("source_count"),  CountStructArray(Asset, TEXT("Sources")));
	Result->SetNumberField(TEXT("subject_count"), CountStructArray(Asset, TEXT("Subjects")));
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_BuildLiveLinkPresetFromClient);
