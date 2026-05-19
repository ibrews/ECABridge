// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPostProcessVolumeCommands.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/World.h"

#include "UObject/Field.h"
#include "UObject/UnrealType.h"

REGISTER_ECA_COMMAND(FECACommand_CreatePostProcessVolume);
REGISTER_ECA_COMMAND(FECACommand_SetPostProcessSetting);
REGISTER_ECA_COMMAND(FECACommand_DumpPostProcessSettings);

namespace PPVHelpers
{
	static APostProcessVolume* FindVolume(UWorld* World, const FString& Name)
	{
		if (!World) return nullptr;
		if (Name.IsEmpty())
		{
			for (TActorIterator<APostProcessVolume> It(World); It; ++It) return *It;
			return nullptr;
		}
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->GetActorLabel() == Name || It->GetName() == Name) return *It;
		}
		return nullptr;
	}

	static UStruct* PostProcessSettingsStruct()
	{
		return FPostProcessSettings::StaticStruct();
	}

	// Map a FPostProcessSettings property name (e.g. "BloomIntensity") to its
	// corresponding bOverride_ bool property within the same struct, if any.
	static FBoolProperty* FindOverrideFlag(const FString& PropertyName)
	{
		UStruct* S = PostProcessSettingsStruct();
		if (!S) return nullptr;
		const FString OverrideName = FString::Printf(TEXT("bOverride_%s"), *PropertyName);
		FProperty* P = S->FindPropertyByName(*OverrideName);
		return CastField<FBoolProperty>(P);
	}

	static TSharedPtr<FJsonValue> ColorToJson(const FLinearColor& C)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("r"), C.R);
		O->SetNumberField(TEXT("g"), C.G);
		O->SetNumberField(TEXT("b"), C.B);
		O->SetNumberField(TEXT("a"), C.A);
		return MakeShared<FJsonValueObject>(O);
	}

	static TSharedPtr<FJsonValue> Vec4ToJson(const FVector4& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		O->SetNumberField(TEXT("z"), V.Z);
		O->SetNumberField(TEXT("w"), V.W);
		return MakeShared<FJsonValueObject>(O);
	}

	// Serialize a single property's current value into a JSON value (best-effort,
	// supports the scalar/vector/color types FPostProcessSettings actually uses).
	static TSharedPtr<FJsonValue> SerializeProperty(FProperty* Prop, const void* ContainerPtr)
	{
		if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(F->GetPropertyValue_InContainer(ContainerPtr));
		}
		if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(D->GetPropertyValue_InContainer(ContainerPtr));
		}
		if (FIntProperty* I = CastField<FIntProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(I->GetPropertyValue_InContainer(ContainerPtr));
		}
		if (FBoolProperty* B = CastField<FBoolProperty>(Prop))
		{
			return MakeShared<FJsonValueBoolean>(B->GetPropertyValue_InContainer(ContainerPtr));
		}
		if (FEnumProperty* E = CastField<FEnumProperty>(Prop))
		{
			const void* ValuePtr = E->ContainerPtrToValuePtr<void>(ContainerPtr);
			const int64 Val = E->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(E->GetEnum()->GetNameStringByValue(Val));
		}
		if (FByteProperty* By = CastField<FByteProperty>(Prop))
		{
			if (By->Enum)
			{
				const int64 Val = By->GetPropertyValue_InContainer(ContainerPtr);
				return MakeShared<FJsonValueString>(By->Enum->GetNameStringByValue(Val));
			}
			return MakeShared<FJsonValueNumber>(By->GetPropertyValue_InContainer(ContainerPtr));
		}
		if (FStructProperty* S = CastField<FStructProperty>(Prop))
		{
			if (S->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return ColorToJson(*S->ContainerPtrToValuePtr<FLinearColor>(ContainerPtr));
			}
			if (S->Struct == TBaseStructure<FVector4>::Get())
			{
				return Vec4ToJson(*S->ContainerPtrToValuePtr<FVector4>(ContainerPtr));
			}
			if (S->Struct == TBaseStructure<FVector>::Get())
			{
				const FVector& V = *S->ContainerPtrToValuePtr<FVector>(ContainerPtr);
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetNumberField(TEXT("x"), V.X); O->SetNumberField(TEXT("y"), V.Y); O->SetNumberField(TEXT("z"), V.Z);
				return MakeShared<FJsonValueObject>(O);
			}
			return MakeShared<FJsonValueString>(FString::Printf(TEXT("<struct:%s>"), *S->Struct->GetName()));
		}
		if (FObjectProperty* O = CastField<FObjectProperty>(Prop))
		{
			UObject* Obj = O->GetObjectPropertyValue_InContainer(ContainerPtr);
			return MakeShared<FJsonValueString>(Obj ? Obj->GetPathName() : TEXT(""));
		}
		return MakeShared<FJsonValueString>(FString::Printf(TEXT("<unsupported:%s>"), *Prop->GetClass()->GetName()));
	}
}

// ─── create_post_process_volume ──────────────────────────────

FECACommandResult FECACommand_CreatePostProcessVolume::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FVector Location(0, 0, 0);
	GetVectorParam(Params, TEXT("location"), Location, /*bRequired=*/false);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>(Location, FRotator::ZeroRotator, SpawnParams);
	if (!PPV) return FECACommandResult::Error(TEXT("Failed to spawn PostProcessVolume"));

	bool bUnbound = true;
	GetBoolParam(Params, TEXT("unbound"), bUnbound, /*bRequired=*/false);
	PPV->bUnbound = bUnbound;

	double Priority = 0.0;
	if (GetFloatParam(Params, TEXT("priority"), Priority, /*bRequired=*/false))
	{
		PPV->Priority = (float)Priority;
	}

	FString Name;
	if (GetStringParam(Params, TEXT("name"), Name, /*bRequired=*/false) && !Name.IsEmpty())
	{
		PPV->SetActorLabel(Name);
	}

	PPV->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());
	Result->SetStringField(TEXT("actor_path"), PPV->GetPathName());
	Result->SetBoolField(TEXT("unbound"), bUnbound);
	Result->SetNumberField(TEXT("priority"), PPV->Priority);
	return FECACommandResult::Success(Result);
}

// ─── set_post_process_setting ────────────────────────────────

FECACommandResult FECACommand_SetPostProcessSetting::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);

	APostProcessVolume* PPV = PPVHelpers::FindVolume(World, ActorName);
	if (!PPV) return FECACommandResult::Error(TEXT("No PostProcessVolume found"));

	FString SettingName;
	if (!GetStringParam(Params, TEXT("setting_name"), SettingName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: setting_name"));
	}

	UStruct* PPStruct = PPVHelpers::PostProcessSettingsStruct();
	FProperty* Prop = PPStruct ? PPStruct->FindPropertyByName(*SettingName) : nullptr;
	if (!Prop)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("FPostProcessSettings has no property named '%s'"), *SettingName));
	}

	FPostProcessSettings& Settings = PPV->Settings;
	void* SettingsPtr = &Settings;

	// Apply value based on JSON type
	if (!Params.IsValid() || !Params->HasField(TEXT("value")))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("Parameter 'value' is null"));
	}

	bool bApplied = false;

	if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
	{
		double N; if (Value->TryGetNumber(N)) { F->SetPropertyValue_InContainer(SettingsPtr, (float)N); bApplied = true; }
	}
	else if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
	{
		double N; if (Value->TryGetNumber(N)) { D->SetPropertyValue_InContainer(SettingsPtr, N); bApplied = true; }
	}
	else if (FIntProperty* I = CastField<FIntProperty>(Prop))
	{
		double N; if (Value->TryGetNumber(N)) { I->SetPropertyValue_InContainer(SettingsPtr, (int32)N); bApplied = true; }
	}
	else if (FBoolProperty* B = CastField<FBoolProperty>(Prop))
	{
		bool Bv; if (Value->TryGetBool(Bv)) { B->SetPropertyValue_InContainer(SettingsPtr, Bv); bApplied = true; }
	}
	else if (FByteProperty* By = CastField<FByteProperty>(Prop))
	{
		double N;
		FString Sv;
		if (Value->TryGetNumber(N)) { By->SetPropertyValue_InContainer(SettingsPtr, (uint8)N); bApplied = true; }
		else if (By->Enum && Value->TryGetString(Sv))
		{
			int64 EnumVal = By->Enum->GetValueByNameString(Sv);
			if (EnumVal != INDEX_NONE) { By->SetPropertyValue_InContainer(SettingsPtr, (uint8)EnumVal); bApplied = true; }
		}
	}
	else if (FStructProperty* S = CastField<FStructProperty>(Prop))
	{
		TSharedPtr<FJsonObject>* Obj;
		if (S->Struct == TBaseStructure<FLinearColor>::Get() && Value->TryGetObject(Obj))
		{
			FLinearColor C(0,0,0,1);
			(*Obj)->TryGetNumberField(TEXT("r"), C.R);
			(*Obj)->TryGetNumberField(TEXT("g"), C.G);
			(*Obj)->TryGetNumberField(TEXT("b"), C.B);
			(*Obj)->TryGetNumberField(TEXT("a"), C.A);
			*S->ContainerPtrToValuePtr<FLinearColor>(SettingsPtr) = C;
			bApplied = true;
		}
		else if (S->Struct == TBaseStructure<FVector4>::Get() && Value->TryGetObject(Obj))
		{
			FVector4 V(0,0,0,0);
			(*Obj)->TryGetNumberField(TEXT("x"), V.X);
			(*Obj)->TryGetNumberField(TEXT("y"), V.Y);
			(*Obj)->TryGetNumberField(TEXT("z"), V.Z);
			(*Obj)->TryGetNumberField(TEXT("w"), V.W);
			*S->ContainerPtrToValuePtr<FVector4>(SettingsPtr) = V;
			bApplied = true;
		}
	}

	if (!bApplied)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not apply value to property '%s' of type '%s' — unsupported type or value coercion failed"),
			*SettingName, *Prop->GetClass()->GetName()));
	}

	// Flip the matching bOverride_ flag if it exists
	bool bOverrideFlipped = false;
	if (FBoolProperty* OverrideFlag = PPVHelpers::FindOverrideFlag(SettingName))
	{
		OverrideFlag->SetPropertyValue_InContainer(SettingsPtr, true);
		bOverrideFlipped = true;
	}

	PPV->MarkPackageDirty();
	if (GEditor) GEditor->RedrawAllViewports();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());
	Result->SetStringField(TEXT("setting_name"), SettingName);
	Result->SetBoolField(TEXT("override_flipped"), bOverrideFlipped);
	Result->SetField(TEXT("value"), PPVHelpers::SerializeProperty(Prop, SettingsPtr));
	return FECACommandResult::Success(Result);
}

// ─── dump_post_process_settings ──────────────────────────────

FECACommandResult FECACommand_DumpPostProcessSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName;
	GetStringParam(Params, TEXT("actor_name"), ActorName, /*bRequired=*/false);

	APostProcessVolume* PPV = PPVHelpers::FindVolume(World, ActorName);
	if (!PPV) return FECACommandResult::Error(TEXT("No PostProcessVolume found"));

	bool bIncludeDefaults = false;
	GetBoolParam(Params, TEXT("include_defaults"), bIncludeDefaults, /*bRequired=*/false);

	UStruct* PPStruct = PPVHelpers::PostProcessSettingsStruct();
	const void* SettingsPtr = &PPV->Settings;

	TSharedPtr<FJsonObject> Overridden = MakeShared<FJsonObject>();
	int32 Count = 0;
	for (TFieldIterator<FProperty> It(PPStruct); It; ++It)
	{
		FProperty* Prop = *It;
		const FString Name = Prop->GetName();
		if (Name.StartsWith(TEXT("bOverride_"))) continue;
		if (!bIncludeDefaults)
		{
			FBoolProperty* Flag = PPVHelpers::FindOverrideFlag(Name);
			if (Flag && !Flag->GetPropertyValue_InContainer(SettingsPtr)) continue;
		}
		Overridden->SetField(Name, PPVHelpers::SerializeProperty(Prop, SettingsPtr));
		++Count;
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), PPV->GetActorLabel());
	Result->SetBoolField(TEXT("unbound"), PPV->bUnbound);
	Result->SetNumberField(TEXT("priority"), PPV->Priority);
	Result->SetNumberField(TEXT("count"), Count);
	Result->SetObjectField(TEXT("settings"), Overridden);
	return FECACommandResult::Success(Result);
}
