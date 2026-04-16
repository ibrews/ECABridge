// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANiagaraDataChannelCommands.h"
#include "Commands/ECACommand.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannel_Global.h"
#include "NiagaraDataChannelVariable.h"
#include "NiagaraTypes.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "PackageTools.h"
#include "Misc/PackageName.h"

//------------------------------------------------------------------------------
// Registration
//------------------------------------------------------------------------------

REGISTER_ECA_COMMAND(FECACommand_CreateNiagaraDataChannel)
REGISTER_ECA_COMMAND(FECACommand_DumpNiagaraDataChannel)

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

namespace ECANDCInternal
{
	/**
	 * Resolve a user-supplied type string to a FNiagaraTypeDefinition.
	 * Returns true on success. Supported types: Float, Int32, Bool, Vector,
	 * Vector2, Vector4, Quat, Color, LinearColor, Position.
	 */
	static bool ResolveTypeDef(const FString& InTypeName, FNiagaraTypeDefinition& OutType, FString& OutError)
	{
		const FString Normalized = InTypeName.TrimStartAndEnd();

		if (Normalized.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetFloatDef();
			return true;
		}
		if (Normalized.Equals(TEXT("Int32"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Int"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Integer"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetIntDef();
			return true;
		}
		if (Normalized.Equals(TEXT("Bool"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetBoolDef();
			return true;
		}
		if (Normalized.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vec3"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetVec3Def();
			return true;
		}
		if (Normalized.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vec2"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetVec2Def();
			return true;
		}
		if (Normalized.Equals(TEXT("Vector4"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vec4"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetVec4Def();
			return true;
		}
		if (Normalized.Equals(TEXT("Quat"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Quaternion"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetQuatDef();
			return true;
		}
		if (Normalized.Equals(TEXT("Color"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetColorDef();
			return true;
		}
		if (Normalized.Equals(TEXT("Position"), ESearchCase::IgnoreCase))
		{
			OutType = FNiagaraTypeDefinition::GetPositionDef();
			return true;
		}

		OutError = FString::Printf(TEXT("Unsupported variable type '%s'. Supported: Float, Int32, Bool, Vector, Vector2, Vector4, Quat, Color, LinearColor, Position."), *InTypeName);
		return false;
	}

	/**
	 * Split "/Game/Foo/Bar" (or "/Game/Foo/Bar.Bar") into package path and asset name.
	 */
	static void SplitAssetPath(const FString& InAssetPath, FString& OutPackagePath, FString& OutAssetName)
	{
		FString CleanPath = InAssetPath;
		CleanPath.TrimStartAndEndInline();

		// Strip the trailing ".AssetName" suffix if present.
		int32 DotIdx = INDEX_NONE;
		if (CleanPath.FindChar(TEXT('.'), DotIdx))
		{
			CleanPath = CleanPath.Left(DotIdx);
		}

		int32 SlashIdx = INDEX_NONE;
		if (CleanPath.FindLastChar(TEXT('/'), SlashIdx))
		{
			OutPackagePath = CleanPath.Left(SlashIdx);
			OutAssetName = CleanPath.RightChop(SlashIdx + 1);
		}
		else
		{
			OutPackagePath = TEXT("/Game");
			OutAssetName = CleanPath;
		}
	}

	/**
	 * Access the (private) ChannelVariables array on a UNiagaraDataChannel via reflection.
	 * Returns nullptr if the property cannot be found or is the wrong shape.
	 */
	static TArray<FNiagaraDataChannelVariable>* GetChannelVariablesArray(UNiagaraDataChannel* Channel)
	{
		if (!Channel)
		{
			return nullptr;
		}

		FProperty* Prop = Channel->GetClass()->FindPropertyByName(TEXT("ChannelVariables"));
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (!ArrayProp)
		{
			return nullptr;
		}

		void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Channel);
		return static_cast<TArray<FNiagaraDataChannelVariable>*>(ArrayPtr);
	}

	/**
	 * Human-readable name for a Niagara type definition. Falls back to struct /
	 * class name if not one of the well-known types.
	 */
	static FString TypeDefToString(const FNiagaraTypeDefinition& TypeDef)
	{
		if (!TypeDef.IsValid())
		{
			return TEXT("Invalid");
		}
		if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())    return TEXT("Float");
		if (TypeDef == FNiagaraTypeDefinition::GetIntDef())      return TEXT("Int32");
		if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())     return TEXT("Bool");
		if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())     return TEXT("Vector2");
		if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())     return TEXT("Vector");
		if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())     return TEXT("Vector4");
		if (TypeDef == FNiagaraTypeDefinition::GetQuatDef())     return TEXT("Quat");
		if (TypeDef == FNiagaraTypeDefinition::GetColorDef())    return TEXT("Color");
		if (TypeDef == FNiagaraTypeDefinition::GetPositionDef()) return TEXT("Position");

		if (UScriptStruct* ScriptStruct = TypeDef.GetScriptStruct())
		{
			return ScriptStruct->GetName();
		}
		if (UEnum* Enum = TypeDef.GetEnum())
		{
			return Enum->GetName();
		}
		if (UClass* Class = TypeDef.GetClass())
		{
			return Class->GetName();
		}
		return TEXT("Unknown");
	}
}

//------------------------------------------------------------------------------
// FECACommand_CreateNiagaraDataChannel
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateNiagaraDataChannel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	GetArrayParam(Params, TEXT("variables"), VariablesArray);

	// Split the asset path into package path + asset name.
	FString PackagePath;
	FString AssetName;
	ECANDCInternal::SplitAssetPath(AssetPath, PackagePath, AssetName);

	if (AssetName.IsEmpty())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not derive asset name from asset_path '%s'"), *AssetPath));
	}

	// Sanitize the asset name for safety.
	AssetName = UPackageTools::SanitizePackageName(AssetName);
	const FString FullPackagePath = PackagePath / AssetName;

	// Refuse to overwrite an existing asset silently.
	if (FPackageName::DoesPackageExist(FullPackagePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("An asset already exists at '%s'"), *FullPackagePath));
	}

	// Create the package.
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package at '%s'"), *FullPackagePath));
	}
	Package->FullyLoad();

	// Create the asset.
	UNiagaraDataChannelAsset* NewAsset = NewObject<UNiagaraDataChannelAsset>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!NewAsset)
	{
		return FECACommandResult::Error(TEXT("Failed to create UNiagaraDataChannelAsset"));
	}

	// UNiagaraDataChannel is abstract — use the Global handler subclass, which is the
	// default / simplest NDC variant and makes payload visible everywhere.
	UClass* ChannelClass = UNiagaraDataChannel_Global::StaticClass();
	if (!ChannelClass || ChannelClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FECACommandResult::Error(TEXT("UNiagaraDataChannel_Global class is unavailable or abstract"));
	}

	UNiagaraDataChannel* NewChannel = NewObject<UNiagaraDataChannel>(
		NewAsset, ChannelClass, NAME_None, RF_Public | RF_Transactional);
	if (!NewChannel)
	{
		return FECACommandResult::Error(TEXT("Failed to create UNiagaraDataChannel sub-object"));
	}

	// Set the DataChannel property on the asset via reflection (the field is private).
	{
		FProperty* DataChannelProp = UNiagaraDataChannelAsset::StaticClass()->FindPropertyByName(TEXT("DataChannel"));
		FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(DataChannelProp);
		if (!ObjProp)
		{
			return FECACommandResult::Error(TEXT("UNiagaraDataChannelAsset::DataChannel property not found (Niagara API mismatch)"));
		}
		ObjProp->SetObjectPropertyValue_InContainer(NewAsset, NewChannel);
	}

	// Populate the ChannelVariables array via reflection.
	TArray<FNiagaraDataChannelVariable>* ChannelVars = ECANDCInternal::GetChannelVariablesArray(NewChannel);
	if (!ChannelVars)
	{
		return FECACommandResult::Error(TEXT("UNiagaraDataChannel::ChannelVariables property not found (Niagara API mismatch)"));
	}

	TArray<TSharedPtr<FJsonObject>> VariableResults;
	int32 VariableCount = 0;

	if (VariablesArray)
	{
		for (const TSharedPtr<FJsonValue>& VarValue : *VariablesArray)
		{
			if (!VarValue.IsValid() || VarValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject>& VarObj = VarValue->AsObject();
			if (!VarObj.IsValid())
			{
				continue;
			}

			FString VarName;
			FString VarType;
			VarObj->TryGetStringField(TEXT("name"), VarName);
			VarObj->TryGetStringField(TEXT("type"), VarType);

			if (VarName.IsEmpty() || VarType.IsEmpty())
			{
				return FECACommandResult::Error(TEXT("Each variable must have non-empty 'name' and 'type' fields"));
			}

			FNiagaraTypeDefinition TypeDef;
			FString TypeError;
			if (!ECANDCInternal::ResolveTypeDef(VarType, TypeDef, TypeError))
			{
				return FECACommandResult::Error(TypeError);
			}

			FNiagaraDataChannelVariable NewVar;
			NewVar.SetName(FName(*VarName));
			// NDC variables must be in the LWC-compatible "data channel" form (e.g. Vector3f for Vector).
			NewVar.SetType(FNiagaraDataChannelVariable::ToDataChannelType(TypeDef));

			ChannelVars->Add(NewVar);
			++VariableCount;

			TSharedPtr<FJsonObject> VarResult = MakeShared<FJsonObject>();
			VarResult->SetStringField(TEXT("name"), VarName);
			VarResult->SetStringField(TEXT("type"), ECANDCInternal::TypeDefToString(NewVar.GetType()));
			VariableResults.Add(VarResult);
		}
	}

	// Save the package.
	NewAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewAsset);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		FullPackagePath, FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(Package, NewAsset, *PackageFilename, SaveArgs);

	// Build result.
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("package_path"), FullPackagePath);
	Result->SetStringField(TEXT("asset_name"), AssetName);
	Result->SetStringField(TEXT("channel_class"), ChannelClass->GetName());
	Result->SetNumberField(TEXT("variable_count"), VariableCount);
	Result->SetBoolField(TEXT("saved"), bSaved);

	TArray<TSharedPtr<FJsonValue>> VarJsonArray;
	for (const TSharedPtr<FJsonObject>& V : VariableResults)
	{
		VarJsonArray.Add(MakeShared<FJsonValueObject>(V));
	}
	Result->SetArrayField(TEXT("variables"), VarJsonArray);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// FECACommand_DumpNiagaraDataChannel
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DumpNiagaraDataChannel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Try direct load, then add the .AssetName suffix if needed.
	UNiagaraDataChannelAsset* Asset = LoadObject<UNiagaraDataChannelAsset>(nullptr, *AssetPath);
	if (!Asset)
	{
		FString FullPath = AssetPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString ShortName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + ShortName;
		}
		Asset = LoadObject<UNiagaraDataChannelAsset>(nullptr, *FullPath);
	}

	if (!Asset)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UNiagaraDataChannelAsset at '%s'"), *AssetPath));
	}

	UNiagaraDataChannel* Channel = Asset->Get();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), Asset->GetPathName());
	Result->SetStringField(TEXT("asset_name"), Asset->GetName());

	if (!Channel)
	{
		Result->SetStringField(TEXT("channel_name"), TEXT(""));
		Result->SetStringField(TEXT("channel_class"), TEXT(""));
		Result->SetNumberField(TEXT("variable_count"), 0);
		Result->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>());
		Result->SetStringField(TEXT("warning"), TEXT("Asset has no DataChannel sub-object"));
		return FECACommandResult::Success(Result);
	}

	Result->SetStringField(TEXT("channel_name"), Channel->GetName());
	Result->SetStringField(TEXT("channel_class"), Channel->GetClass()->GetName());

	// Channel settings (read via reflection so we don't need private access).
	if (FProperty* KeepPrevProp = Channel->GetClass()->FindPropertyByName(TEXT("bKeepPreviousFrameData")))
	{
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(KeepPrevProp))
		{
			Result->SetBoolField(TEXT("keep_previous_frame_data"),
				BoolProp->GetPropertyValue_InContainer(Channel));
		}
	}
	if (FProperty* EnforceProp = Channel->GetClass()->FindPropertyByName(TEXT("bEnforceTickGroupReadWriteOrder")))
	{
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(EnforceProp))
		{
			Result->SetBoolField(TEXT("enforce_tick_group_read_write_order"),
				BoolProp->GetPropertyValue_InContainer(Channel));
		}
	}

	// Emit variables. Use the public GetVariables() accessor when available.
	TConstArrayView<FNiagaraDataChannelVariable> Vars = Channel->GetVariables();

	TArray<TSharedPtr<FJsonValue>> VarJsonArray;
	for (const FNiagaraDataChannelVariable& Var : Vars)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.GetName().ToString());

		const FNiagaraTypeDefinition& TypeDef = Var.GetType();
		VarObj->SetStringField(TEXT("type"), ECANDCInternal::TypeDefToString(TypeDef));

		FString TypeStructName;
		if (UScriptStruct* ScriptStruct = TypeDef.GetScriptStruct())
		{
			TypeStructName = ScriptStruct->GetPathName();
		}
		else if (UEnum* Enum = TypeDef.GetEnum())
		{
			TypeStructName = Enum->GetPathName();
		}
		else if (UClass* Class = TypeDef.GetClass())
		{
			TypeStructName = Class->GetPathName();
		}
		VarObj->SetStringField(TEXT("type_path"), TypeStructName);
		VarObj->SetNumberField(TEXT("size_bytes"), Var.GetSizeInBytes());

#if WITH_EDITORONLY_DATA
		VarObj->SetStringField(TEXT("version"), Var.Version.ToString());
#endif

		VarJsonArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	Result->SetNumberField(TEXT("variable_count"), Vars.Num());
	Result->SetArrayField(TEXT("variables"), VarJsonArray);

	return FECACommandResult::Success(Result);
}
