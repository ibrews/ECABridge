// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAEnhancedInputCommands.h"
#include "Commands/ECACommand.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "EnhancedActionKeyMapping.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "InputCoreTypes.h"
#include "GameFramework/InputSettings.h"

#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

namespace ECAEnhancedInputUtils
{
	/** Parse "/Game/Path/AssetName" into its package folder and asset name. */
	static bool SplitAssetPath(const FString& AssetPath, FString& OutPackagePath, FString& OutAssetName, FString& OutError)
	{
		int32 LastSlash = INDEX_NONE;
		if (!AssetPath.FindLastChar(TEXT('/'), LastSlash))
		{
			OutError = TEXT("Invalid asset path format. Expected format: /Game/Path/AssetName");
			return false;
		}

		OutPackagePath = AssetPath.Left(LastSlash);
		OutAssetName = AssetPath.Mid(LastSlash + 1);

		// Strip trailing .AssetName if someone passes /Game/Foo/Bar.Bar
		int32 DotIdx = INDEX_NONE;
		if (OutAssetName.FindChar(TEXT('.'), DotIdx))
		{
			OutAssetName = OutAssetName.Left(DotIdx);
		}

		if (OutPackagePath.IsEmpty() || OutAssetName.IsEmpty())
		{
			OutError = TEXT("Invalid asset path format. Expected format: /Game/Path/AssetName");
			return false;
		}

		return true;
	}

	/** Translate a string to EInputActionValueType. Accepted synonyms included. */
	static bool ParseValueType(const FString& InString, EInputActionValueType& OutType, FString& OutError)
	{
		const FString Normalized = InString.TrimStartAndEnd();

		if (Normalized.IsEmpty() || Normalized.Equals(TEXT("Digital"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
		{
			OutType = EInputActionValueType::Boolean;
			return true;
		}
		if (Normalized.Equals(TEXT("Analog1D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Axis1D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Float"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("1D"), ESearchCase::IgnoreCase))
		{
			OutType = EInputActionValueType::Axis1D;
			return true;
		}
		if (Normalized.Equals(TEXT("Analog2D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Axis2D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("2D"), ESearchCase::IgnoreCase))
		{
			OutType = EInputActionValueType::Axis2D;
			return true;
		}
		if (Normalized.Equals(TEXT("Analog3D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Axis3D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("Vector3D"), ESearchCase::IgnoreCase) ||
			Normalized.Equals(TEXT("3D"), ESearchCase::IgnoreCase))
		{
			OutType = EInputActionValueType::Axis3D;
			return true;
		}

		OutError = FString::Printf(TEXT("Unknown value_type '%s'. Valid: Digital, Analog1D, Analog2D, Analog3D."), *InString);
		return false;
	}

	/** Convert EInputActionValueType to a display string. */
	static FString ValueTypeToString(EInputActionValueType Type)
	{
		switch (Type)
		{
		case EInputActionValueType::Boolean: return TEXT("Digital");
		case EInputActionValueType::Axis1D:  return TEXT("Analog1D");
		case EInputActionValueType::Axis2D:  return TEXT("Analog2D");
		case EInputActionValueType::Axis3D:  return TEXT("Analog3D");
		default:                             return TEXT("Unknown");
		}
	}

	/** Save a package containing a freshly created asset. Returns true on success. */
	static bool SaveAssetPackage(UPackage* Package, UObject* Asset, FString& OutError)
	{
		if (!Package || !Asset)
		{
			OutError = TEXT("Invalid package or asset");
			return false;
		}

		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		const bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
		if (!bSaved)
		{
			OutError = FString::Printf(TEXT("Failed to save package to %s"), *PackageFileName);
			return false;
		}
		return true;
	}

	/** Save an existing modified asset's package back to disk. */
	static bool SaveExistingPackage(UObject* Asset, FString& OutError)
	{
		if (!Asset)
		{
			OutError = TEXT("Null asset");
			return false;
		}
		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			OutError = TEXT("Asset has no outer package");
			return false;
		}

		Package->MarkPackageDirty();

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		const bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
		if (!bSaved)
		{
			OutError = FString::Printf(TEXT("Failed to save package to %s"), *PackageFileName);
			return false;
		}
		return true;
	}

	/** Resolve a key string into an FKey. Tries EKeys registered keys first, then falls back to FKey(FName). */
	static FKey ResolveKey(const FString& KeyString)
	{
		// EKeys has a static GetKeyDetails / AddKey infrastructure; all registered keys
		// are constructible via FKey(FName). That's the canonical way.
		const FName KeyName(*KeyString);
		FKey Candidate(KeyName);
		return Candidate;
	}

	/** Best-effort human-readable class name for a UObject (stripping the default Class prefix). */
	static FString GetSimpleClassName(const UObject* Obj)
	{
		if (!Obj || !Obj->GetClass())
		{
			return FString();
		}
		return Obj->GetClass()->GetName();
	}

	/** Build a JSON array representing Modifiers by class name. */
	static TArray<TSharedPtr<FJsonValue>> ModifiersToJson(const TArray<TObjectPtr<UInputModifier>>& Modifiers)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const TObjectPtr<UInputModifier>& Mod : Modifiers)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("class"), GetSimpleClassName(Mod.Get()));
			Out.Add(MakeShared<FJsonValueObject>(Entry));
		}
		return Out;
	}

	/** Build a JSON array representing Triggers by class name. */
	static TArray<TSharedPtr<FJsonValue>> TriggersToJson(const TArray<TObjectPtr<UInputTrigger>>& Triggers)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const TObjectPtr<UInputTrigger>& Trig : Triggers)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("class"), GetSimpleClassName(Trig.Get()));
			Out.Add(MakeShared<FJsonValueObject>(Entry));
		}
		return Out;
	}
}

//------------------------------------------------------------------------------
// create_input_action
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateInputAction::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString ValueTypeString = TEXT("Digital");
	GetStringParam(Params, TEXT("value_type"), ValueTypeString, false);

	FString Description;
	GetStringParam(Params, TEXT("description"), Description, false);

	EInputActionValueType ValueType = EInputActionValueType::Boolean;
	{
		FString ParseError;
		if (!ECAEnhancedInputUtils::ParseValueType(ValueTypeString, ValueType, ParseError))
		{
			return FECACommandResult::Error(ParseError);
		}
	}

	FString PackagePath, AssetName, SplitError;
	if (!ECAEnhancedInputUtils::SplitAssetPath(AssetPath, PackagePath, AssetName, SplitError))
	{
		return FECACommandResult::Error(SplitError);
	}

	// Fail early if an asset already exists at the target path.
	if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *AssetPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at: %s"), *AssetPath));
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// InputAction has no special factory in base engine; create directly in a new package.
	const FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package at: %s"), *FullPackagePath));
	}
	Package->FullyLoad();

	UInputAction* NewAction = NewObject<UInputAction>(Package, UInputAction::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone);
	if (!NewAction)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create UInputAction at: %s"), *AssetPath));
	}

	NewAction->ValueType = ValueType;
	if (!Description.IsEmpty())
	{
		NewAction->ActionDescription = FText::FromString(Description);
	}

	FString SaveError;
	if (!ECAEnhancedInputUtils::SaveAssetPackage(Package, NewAction, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), NewAction->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewAction->GetName());
	Result->SetStringField(TEXT("package_path"), PackagePath);
	Result->SetStringField(TEXT("value_type"), ECAEnhancedInputUtils::ValueTypeToString(NewAction->ValueType));
	Result->SetStringField(TEXT("description"), NewAction->ActionDescription.ToString());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// create_input_mapping_context
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateInputMappingContext::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString Description;
	GetStringParam(Params, TEXT("description"), Description, false);

	FString PackagePath, AssetName, SplitError;
	if (!ECAEnhancedInputUtils::SplitAssetPath(AssetPath, PackagePath, AssetName, SplitError))
	{
		return FECACommandResult::Error(SplitError);
	}

	if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *AssetPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at: %s"), *AssetPath));
	}

	const FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package at: %s"), *FullPackagePath));
	}
	Package->FullyLoad();

	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, UInputMappingContext::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone);
	if (!NewContext)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create UInputMappingContext at: %s"), *AssetPath));
	}

	if (!Description.IsEmpty())
	{
		NewContext->ContextDescription = FText::FromString(Description);
	}

	FString SaveError;
	if (!ECAEnhancedInputUtils::SaveAssetPackage(Package, NewContext, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), NewContext->GetPathName());
	Result->SetStringField(TEXT("asset_name"), NewContext->GetName());
	Result->SetStringField(TEXT("package_path"), PackagePath);
	Result->SetStringField(TEXT("description"), NewContext->ContextDescription.ToString());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// add_input_mapping
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddInputMapping::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	if (!GetStringParam(Params, TEXT("context_path"), ContextPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: context_path"));
	}

	FString ActionPath;
	if (!GetStringParam(Params, TEXT("action_path"), ActionPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: action_path"));
	}

	FString KeyString;
	if (!GetStringParam(Params, TEXT("key"), KeyString))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: key"));
	}

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("InputMappingContext not found at: %s"), *ContextPath));
	}

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("InputAction not found at: %s"), *ActionPath));
	}

	const FKey ResolvedKey = ECAEnhancedInputUtils::ResolveKey(KeyString);
	if (!ResolvedKey.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Invalid key '%s'. Use a valid FKey name (e.g., SpaceBar, W, LeftMouseButton, Gamepad_FaceButton_Bottom)."),
			*KeyString));
	}

	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, ResolvedKey);
	Context->MarkPackageDirty();

	FString SaveError;
	if (!ECAEnhancedInputUtils::SaveExistingPackage(Context, SaveError))
	{
		// Still report what was added; just flag the save failure.
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), SaveError);
		Result->SetStringField(TEXT("context_path"), ContextPath);
		Result->SetStringField(TEXT("action_path"), ActionPath);
		Result->SetStringField(TEXT("key"), ResolvedKey.ToString());
		return FECACommandResult::Success(Result);
	}

	TSharedPtr<FJsonObject> Binding = MakeShared<FJsonObject>();
	Binding->SetStringField(TEXT("key"), ResolvedKey.ToString());
	Binding->SetStringField(TEXT("key_name"), ResolvedKey.GetFName().ToString());
	Binding->SetStringField(TEXT("action_path"), Action->GetPathName());
	Binding->SetStringField(TEXT("action_name"), Action->GetName());

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("context_path"), Context->GetPathName());
	Result->SetObjectField(TEXT("binding_added"), Binding);
	Result->SetNumberField(TEXT("total_mappings"), Context->GetMappings().Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// dump_input_mapping_context
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DumpInputMappingContext::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	if (!GetStringParam(Params, TEXT("context_path"), ContextPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: context_path"));
	}

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("InputMappingContext not found at: %s"), *ContextPath));
	}

	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();

	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();

		MappingObj->SetStringField(TEXT("key"), Mapping.Key.ToString());
		MappingObj->SetStringField(TEXT("key_name"), Mapping.Key.GetFName().ToString());

		if (const UInputAction* Action = Mapping.Action.Get())
		{
			MappingObj->SetStringField(TEXT("action_path"), Action->GetPathName());
			MappingObj->SetStringField(TEXT("action_name"), Action->GetName());
			MappingObj->SetStringField(TEXT("action_value_type"), ECAEnhancedInputUtils::ValueTypeToString(Action->ValueType));
		}
		else
		{
			MappingObj->SetStringField(TEXT("action_path"), FString());
			MappingObj->SetStringField(TEXT("action_name"), FString());
		}

		MappingObj->SetArrayField(TEXT("modifiers"), ECAEnhancedInputUtils::ModifiersToJson(Mapping.Modifiers));
		MappingObj->SetArrayField(TEXT("triggers"), ECAEnhancedInputUtils::TriggersToJson(Mapping.Triggers));

		MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("context_path"), Context->GetPathName());
	Result->SetStringField(TEXT("description"), Context->ContextDescription.ToString());
	Result->SetArrayField(TEXT("mappings"), MappingsArray);
	Result->SetNumberField(TEXT("mapping_count"), MappingsArray.Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// dump_input_action
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DumpInputAction::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActionPath;
	if (!GetStringParam(Params, TEXT("action_path"), ActionPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: action_path"));
	}

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("InputAction not found at: %s"), *ActionPath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("action_path"), Action->GetPathName());
	Result->SetStringField(TEXT("action_name"), Action->GetName());
	Result->SetStringField(TEXT("value_type"), ECAEnhancedInputUtils::ValueTypeToString(Action->ValueType));
	Result->SetStringField(TEXT("description"), Action->ActionDescription.ToString());
	Result->SetBoolField(TEXT("consume_input"), Action->bConsumeInput);
	Result->SetBoolField(TEXT("trigger_when_paused"), Action->bTriggerWhenPaused);
	Result->SetBoolField(TEXT("reserve_all_mappings"), Action->bReserveAllMappings);
	Result->SetBoolField(TEXT("consumes_action_and_axis_mappings"), Action->bConsumesActionAndAxisMappings);

	Result->SetArrayField(TEXT("modifiers"), ECAEnhancedInputUtils::ModifiersToJson(Action->Modifiers));
	Result->SetArrayField(TEXT("triggers"), ECAEnhancedInputUtils::TriggersToJson(Action->Triggers));

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// Registration
//------------------------------------------------------------------------------

REGISTER_ECA_COMMAND(FECACommand_CreateInputAction)
REGISTER_ECA_COMMAND(FECACommand_CreateInputMappingContext)
REGISTER_ECA_COMMAND(FECACommand_AddInputMapping)
REGISTER_ECA_COMMAND(FECACommand_DumpInputMappingContext)
REGISTER_ECA_COMMAND(FECACommand_DumpInputAction)
