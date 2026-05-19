// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgAuthoringCommands.h"
#include "Commands/ECACommand.h"

#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "PCGInputOutputSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_CreatePCGGraph)
REGISTER_ECA_COMMAND(FECACommand_AddPCGNode)
REGISTER_ECA_COMMAND(FECACommand_ConnectPCGNodes)
REGISTER_ECA_COMMAND(FECACommand_SetPCGNodeProperty)

namespace ECAPcgHelpers
{
	// Find a settings subclass by short name (with or without leading 'U', case-insensitive).
	static UClass* FindPCGSettingsClass(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		const UClass* BaseClass = UPCGSettings::StaticClass();
		FString Stripped = Name;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class || !Class->IsChildOf(BaseClass)) continue;
			if (Class->HasAnyClassFlags(CLASS_Abstract)) continue;
			const FString ClassName = Class->GetName();
			if (ClassName.Equals(Name, ESearchCase::IgnoreCase) ||
				ClassName.Equals(Stripped, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
		return nullptr;
	}

	static UPCGNode* FindNodeById(UPCGGraph* Graph, const FString& NodeId)
	{
		if (!Graph) return nullptr;
		if (UPCGNode* In = Graph->GetInputNode())
		{
			if (In->GetName() == NodeId) return In;
		}
		if (UPCGNode* Out = Graph->GetOutputNode())
		{
			if (Out->GetName() == NodeId) return Out;
		}
		for (UPCGNode* Node : Graph->GetNodes())
		{
			if (Node && Node->GetName() == NodeId) return Node;
		}
		return nullptr;
	}

	static bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
		if (!Package)
		{
			OutError = TEXT("Asset has no outer package");
			return false;
		}
		Package->MarkPackageDirty();
		const FString PackageName = Package->GetName();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
		if (!bSaved)
		{
			OutError = FString::Printf(TEXT("UPackage::SavePackage failed for '%s'"), *PackageFileName);
			return false;
		}
		return true;
	}
}

//==============================================================================
// create_pcg_graph
//==============================================================================
FECACommandResult FECACommand_CreatePCGGraph::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("asset_path is required"));
	}

	if (!AssetPath.StartsWith(TEXT("/")))
	{
		return FECACommandResult::Error(TEXT("asset_path must be a full content path starting with '/' (e.g. /Game/MyFolder/MyGraph)"));
	}

	if (StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	FString PackageName, AssetName;
	if (!AssetPath.Split(TEXT("."), &PackageName, &AssetName))
	{
		PackageName = AssetPath;
		AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	}
	if (AssetName.IsEmpty())
	{
		AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package '%s'"), *PackageName));
	}

	UPCGGraph* Graph = NewObject<UPCGGraph>(Package, UPCGGraph::StaticClass(), *AssetName, RF_Public | RF_Standalone);
	if (!Graph)
	{
		return FECACommandResult::Error(TEXT("NewObject<UPCGGraph> returned null"));
	}
	FAssetRegistryModule::AssetCreated(Graph);

	FString SaveError;
	if (!ECAPcgHelpers::SaveAssetPackage(Graph, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), Graph->GetPathName());
	Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	return FECACommandResult::Success(Result);
}

//==============================================================================
// add_pcg_node
//==============================================================================
FECACommandResult FECACommand_AddPCGNode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, SettingsClassName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("settings_class"), SettingsClassName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("settings_class is required"));
	}
	double PosX = 0.0, PosY = 0.0;
	GetFloatParam(Params, TEXT("position_x"), PosX, false);
	GetFloatParam(Params, TEXT("position_y"), PosY, false);

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	UClass* SettingsClass = ECAPcgHelpers::FindPCGSettingsClass(SettingsClassName);
	if (!SettingsClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGSettings subclass '%s' not found. Use list_pcg_node_types to discover available classes."), *SettingsClassName));
	}

	UPCGSettings* DefaultSettings = nullptr;
	UPCGNode* NewNode = Graph->AddNodeOfType(TSubclassOf<UPCGSettings>(SettingsClass), DefaultSettings);
	if (!NewNode)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGGraph::AddNodeOfType returned null for '%s'"), *SettingsClass->GetName()));
	}

#if WITH_EDITOR
	NewNode->SetNodePosition(static_cast<int32>(PosX), static_cast<int32>(PosY));
#endif

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError); // best-effort save

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("graph_path"), GraphPath);
	Result->SetStringField(TEXT("node_id"), NewNode->GetName());
	Result->SetStringField(TEXT("settings_class"), SettingsClass->GetName());
	return FECACommandResult::Success(Result);
}

//==============================================================================
// connect_pcg_nodes
//==============================================================================
FECACommandResult FECACommand_ConnectPCGNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, FromId, ToId;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("from_node_id"), FromId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("from_node_id is required"));
	}
	if (!GetStringParam(Params, TEXT("to_node_id"), ToId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("to_node_id is required"));
	}
	FString FromPinName, ToPinName;
	GetStringParam(Params, TEXT("from_pin"), FromPinName, false);
	GetStringParam(Params, TEXT("to_pin"), ToPinName, false);

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}

	UPCGNode* From = ECAPcgHelpers::FindNodeById(Graph, FromId);
	UPCGNode* To = ECAPcgHelpers::FindNodeById(Graph, ToId);
	if (!From) return FECACommandResult::Error(FString::Printf(TEXT("from_node_id '%s' not found in graph"), *FromId));
	if (!To)   return FECACommandResult::Error(FString::Printf(TEXT("to_node_id '%s' not found in graph"), *ToId));

	// Resolve default pins if labels omitted
	if (FromPinName.IsEmpty())
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = From->GetOutputPins();
		if (Pins.Num() == 0 || !Pins[0])
		{
			return FECACommandResult::Error(FString::Printf(TEXT("from node '%s' has no output pins"), *FromId));
		}
		FromPinName = Pins[0]->Properties.Label.ToString();
	}
	if (ToPinName.IsEmpty())
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = To->GetInputPins();
		if (Pins.Num() == 0 || !Pins[0])
		{
			return FECACommandResult::Error(FString::Printf(TEXT("to node '%s' has no input pins"), *ToId));
		}
		ToPinName = Pins[0]->Properties.Label.ToString();
	}

	UPCGNode* Result = Graph->AddEdge(From, FName(*FromPinName), To, FName(*ToPinName));
	if (!Result)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGGraph::AddEdge failed (from '%s'.%s -> '%s'.%s; check pin labels)"),
			*FromId, *FromPinName, *ToId, *ToPinName));
	}

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), GraphPath);
	Out->SetStringField(TEXT("from_node_id"), FromId);
	Out->SetStringField(TEXT("from_pin"), FromPinName);
	Out->SetStringField(TEXT("to_node_id"), ToId);
	Out->SetStringField(TEXT("to_pin"), ToPinName);
	return FECACommandResult::Success(Out);
}

//==============================================================================
// set_pcg_node_property
//==============================================================================
FECACommandResult FECACommand_SetPCGNodeProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, NodeId, PropertyName;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	}
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("property_name is required"));
	}

	const TSharedPtr<FJsonValue> ValueJson = Params.IsValid() ? Params->TryGetField(TEXT("value")) : nullptr;
	if (!ValueJson.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("value is required"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}
	UPCGNode* Node = ECAPcgHelpers::FindNodeById(Graph, NodeId);
	if (!Node)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found in graph"), *NodeId));
	}
	UPCGSettings* Settings = Node->GetSettings();
	if (!Settings)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' has no settings object"), *NodeId));
	}

	FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property '%s' not found on settings class '%s'"), *PropertyName, *Settings->GetClass()->GetName()));
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Settings);
	bool bApplied = false;
	FString TypeName = Property->GetCPPType();

	// Note: UPCGSettings overrides PreEditChange / PostEditChangeProperty as
	// protected, so we can't drive the change broadcast through the standard
	// UObject path. Saving the graph package below dirties the asset; the
	// editor refreshes on reload.

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (ValueJson->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			bApplied = true;
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(NumVal));
			bApplied = true;
		}
	}
	else if (FInt64Property* I64Prop = CastField<FInt64Property>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			I64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			bApplied = true;
		}
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(NumVal));
			bApplied = true;
		}
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		double NumVal = 0.0;
		if (ValueJson->TryGetNumber(NumVal))
		{
			DoubleProp->SetPropertyValue(ValuePtr, NumVal);
			bApplied = true;
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (ValueJson->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			bApplied = true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (ValueJson->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			bApplied = true;
		}
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FString StrVal;
		double NumVal = 0.0;
		if (ValueJson->TryGetString(StrVal))
		{
			UEnum* Enum = EnumProp->GetEnum();
			int64 EnumValue = Enum ? Enum->GetValueByNameString(StrVal) : INDEX_NONE;
			if (EnumValue == INDEX_NONE && Enum)
			{
				EnumValue = Enum->GetValueByName(FName(*StrVal));
			}
			if (EnumValue != INDEX_NONE)
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
				bApplied = true;
			}
		}
		else if (ValueJson->TryGetNumber(NumVal))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumVal));
			bApplied = true;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			const TSharedPtr<FJsonObject>* Obj;
			if (ValueJson->TryGetObject(Obj) && Obj && (*Obj).IsValid())
			{
				double X = 0, Y = 0, Z = 0;
				(*Obj)->TryGetNumberField(TEXT("x"), X);
				(*Obj)->TryGetNumberField(TEXT("y"), Y);
				(*Obj)->TryGetNumberField(TEXT("z"), Z);
				FVector V(X, Y, Z);
				StructProp->CopyCompleteValue(ValuePtr, &V);
				bApplied = true;
			}
		}
	}

	if (bApplied)
	{
		Settings->Modify();
	}

	if (!bApplied)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Property '%s' of type '%s' could not be assigned from the provided value JSON. Supported: bool, int, float/double, string, name, enum (by name or number), FVector ({x,y,z})."),
			*PropertyName, *TypeName));
	}

	FString SaveError;
	ECAPcgHelpers::SaveAssetPackage(Graph, SaveError);

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), GraphPath);
	Out->SetStringField(TEXT("node_id"), NodeId);
	Out->SetStringField(TEXT("property_name"), PropertyName);
	Out->SetStringField(TEXT("property_type"), TypeName);
	return FECACommandResult::Success(Out);
}
