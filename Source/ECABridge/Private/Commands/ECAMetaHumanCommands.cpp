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

// Register all MetaHuman commands
REGISTER_ECA_COMMAND(FECACommand_CreateMetaHumanCharacter)
REGISTER_ECA_COMMAND(FECACommand_DumpMetaHumanCharacter)
REGISTER_ECA_COMMAND(FECACommand_SetMetaHumanProperty)
REGISTER_ECA_COMMAND(FECACommand_DescribeMetaHuman)
REGISTER_ECA_COMMAND(FECACommand_OpenMetaHumanEditor)
REGISTER_ECA_COMMAND(FECACommand_BuildMetaHuman)
REGISTER_ECA_COMMAND(FECACommand_DownloadMetaHumanTextures)
REGISTER_ECA_COMMAND(FECACommand_RigMetaHuman)

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

// Note: RequestTextureSources and RequestAutoRigging have internal state preconditions
// (Epic MetaHuman service sign-in, registered UI callbacks, pipeline context) that are only
// satisfied when invoked from the MetaHuman Character editor toolkit. Calling them externally
// via ProcessEvent triggers assertion failures (TMap lookup with missing key). These commands
// therefore open the MetaHuman editor and instruct the user to click the corresponding button.
FECACommandResult FECACommand_DownloadMetaHumanTextures::Execute(const TSharedPtr<FJsonObject>& Params)
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
	if (EditorSubsystem)
	{
		EditorSubsystem->OpenEditorForAsset(Asset);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("status"), TEXT("editor_opened"));
	Result->SetStringField(TEXT("next_action"), TEXT("Click 'Download Texture Sources' button in the MetaHuman editor toolbar (top row)."));
	Result->SetStringField(TEXT("why_not_automated"), TEXT("RequestTextureSources requires Epic MetaHuman service sign-in and registered UI callbacks that are only initialized when invoked from the editor toolkit. Calling it externally causes an assertion failure."));
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_RigMetaHuman::Execute(const TSharedPtr<FJsonObject>& Params)
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
	if (EditorSubsystem)
	{
		EditorSubsystem->OpenEditorForAsset(Asset);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("status"), TEXT("editor_opened"));
	Result->SetStringField(TEXT("next_action"), TEXT("Click 'Create Full Rig' or 'Create Joints Only Rig' button in the MetaHuman editor toolbar (top row)."));
	Result->SetStringField(TEXT("why_not_automated"), TEXT("RequestAutoRigging requires registered pipeline context initialized by the editor toolkit. Calling it externally causes an assertion failure."));
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
		TEXT("body type is recorded but not directly mutated — body morphing requires the MetaHumanCharacterEditorSubsystem.")));

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
			Notes.Add(MakeShared<FJsonValueString>(
				TEXT("hair color applied via eyelash DyeColor — true hair color lives on the groom asset assembled by the pipeline.")));
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

	// Don't actually invoke BuildMetaHuman — same state-precondition issue as RequestTextureSources.
	// Instead, open the editor and tell the user to click Build.
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(AssetObj);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("character_path"), CharacterPath);
	Result->SetStringField(TEXT("status"), TEXT("editor_opened"));
	Result->SetStringField(TEXT("detected_function"), BuildFunc ? FoundName : TEXT("none"));
	Result->SetStringField(TEXT("next_action"), TEXT("Click 'Assemble' tab (left side) then the Build button in the MetaHuman editor, or use File > Build MetaHumans from the menu. Your data settings (skin, eyes, etc.) are already saved and will be applied."));
	Result->SetStringField(TEXT("why_not_automated"), TEXT("BuildMetaHuman requires pipeline context and async callback registration that are only initialized when invoked from the editor toolkit. Calling it externally causes assertion failures."));
	Result->SetBoolField(TEXT("triggered"), false);

	// Still include available_functions for future diagnosis
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
	return FECACommandResult::Success(Result);
}
