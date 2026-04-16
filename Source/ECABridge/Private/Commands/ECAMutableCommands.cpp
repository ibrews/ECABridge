// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMutableCommands.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableSkeletalMeshActor.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"

// ─── Helpers ────────────────────────────────────────────────────

namespace MutableCommandHelpers
{
	/** Load a UCustomizableObject by path */
	static UCustomizableObject* LoadCO(const FString& ObjectPath)
	{
		UCustomizableObject* CO = LoadObject<UCustomizableObject>(nullptr, *ObjectPath);
		if (!CO)
		{
			// Try with full path variants
			FString FullPath = ObjectPath;
			if (!FullPath.Contains(TEXT(".")))
			{
				// Auto-append the asset name: /Game/Foo/Bar -> /Game/Foo/Bar.Bar
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			CO = LoadObject<UCustomizableObject>(nullptr, *FullPath);
		}
		return CO;
	}

	/** Get the Source editor graph from a CO via UE reflection (the Source field is private). */
	static UEdGraph* GetSourceGraph(UCustomizableObject* CO)
	{
#if WITH_EDITORONLY_DATA
		// The "Source" UPROPERTY is private on UCustomizableObject but accessible via reflection
		FProperty* SourceProp = CO->GetClass()->FindPropertyByName(TEXT("Source"));
		if (SourceProp)
		{
			FObjectProperty* ObjProp = CastField<FObjectProperty>(SourceProp);
			if (ObjProp)
			{
				UObject* GraphObj = ObjProp->GetObjectPropertyValue(SourceProp->ContainerPtrToValuePtr<void>(CO));
				return Cast<UEdGraph>(GraphObj);
			}
		}

		// Fallback: iterate sub-objects to find the UEdGraph
		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(CO, SubObjects, false);
		for (UObject* Sub : SubObjects)
		{
			if (UEdGraph* Graph = Cast<UEdGraph>(Sub))
			{
				return Graph;
			}
		}
#endif
		return nullptr;
	}

	/** Find a node in the graph by its title/name */
	static UEdGraphNode* FindNodeByName(UEdGraph* Graph, const FString& NodeName)
	{
		if (!Graph) return nullptr;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// Match by node title or by GetName()
			FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			if (Title.Equals(NodeName, ESearchCase::IgnoreCase))
			{
				return Node;
			}

			// Also try the shorter EditableTitle or the object name
			FString EditTitle = Node->GetNodeTitle(ENodeTitleType::EditableTitle).ToString();
			if (EditTitle.Equals(NodeName, ESearchCase::IgnoreCase))
			{
				return Node;
			}

			FString ObjName = Node->GetName();
			if (ObjName.Equals(NodeName, ESearchCase::IgnoreCase))
			{
				return Node;
			}
		}
		return nullptr;
	}

	/** Find a pin on a node by name */
	static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		if (!Node) return nullptr;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;

			bool bNameMatch = Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) ||
				Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase);

			if (bNameMatch)
			{
				if (Direction == EGPD_MAX || Pin->Direction == Direction)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	/** Convert a pin direction to string */
	static FString PinDirectionToString(EEdGraphPinDirection Dir)
	{
		return Dir == EGPD_Input ? TEXT("Input") : TEXT("Output");
	}

	/** Build a JSON object describing a pin */
	static TSharedPtr<FJsonValue> PinToJson(UEdGraphPin* Pin)
	{
		TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
		PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinObj->SetStringField(TEXT("display_name"), Pin->GetDisplayName().ToString());
		PinObj->SetStringField(TEXT("direction"), PinDirectionToString(Pin->Direction));
		PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
		PinObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategory.ToString());
		PinObj->SetBoolField(TEXT("hidden"), Pin->bHidden);

		// Connections
		TArray<TSharedPtr<FJsonValue>> Connections;
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
			TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
			Link->SetStringField(TEXT("node"), LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			Link->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->GetName());
			Link->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
			Connections.Add(MakeShared<FJsonValueObject>(Link));
		}
		PinObj->SetArrayField(TEXT("connections"), Connections);

		return MakeShared<FJsonValueObject>(PinObj);
	}
}

// ─── REGISTER ───────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_GetCOInfo);
REGISTER_ECA_COMMAND(FECACommand_GetCONodePins);
REGISTER_ECA_COMMAND(FECACommand_CreateCO);
REGISTER_ECA_COMMAND(FECACommand_AddCONode);
REGISTER_ECA_COMMAND(FECACommand_ListCONodeTypes);
REGISTER_ECA_COMMAND(FECACommand_SetCONodeProperty);
REGISTER_ECA_COMMAND(FECACommand_ConnectCONodes);
REGISTER_ECA_COMMAND(FECACommand_DisconnectCONodes);
REGISTER_ECA_COMMAND(FECACommand_RemoveCONode);
REGISTER_ECA_COMMAND(FECACommand_CompileCO);
REGISTER_ECA_COMMAND(FECACommand_GetCOInstanceParams);

// ─── get_co_info ────────────────────────────────────────────────

FECACommandResult FECACommand_GetCOInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Customizable Object at: %s"), *ObjectPath));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), CO->GetName());
	Result->SetStringField(TEXT("path"), CO->GetPathName());
	Result->SetBoolField(TEXT("is_compiled"), CO->IsCompiled());
	Result->SetNumberField(TEXT("component_count"), CO->GetComponentCount());

	// Components
	TArray<TSharedPtr<FJsonValue>> Components;
	for (int32 i = 0; i < CO->GetComponentCount(); ++i)
	{
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetNumberField(TEXT("index"), i);
		CompObj->SetStringField(TEXT("name"), CO->GetComponentName(i).ToString());
		Components.Add(MakeShared<FJsonValueObject>(CompObj));
	}
	Result->SetArrayField(TEXT("components"), Components);

	// Parameters (from compiled data)
	TArray<TSharedPtr<FJsonValue>> Parameters;
	for (int32 i = 0; i < CO->GetParameterCount(); ++i)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		FString ParamName = CO->GetParameterName(i);
		ParamObj->SetStringField(TEXT("name"), ParamName);

		EMutableParameterType ParamType = CO->GetParameterTypeByName(ParamName);
		FString TypeStr;
		switch (ParamType)
		{
			case EMutableParameterType::Bool: TypeStr = TEXT("Bool"); break;
			case EMutableParameterType::Int: TypeStr = TEXT("Int"); break;
			case EMutableParameterType::Float: TypeStr = TEXT("Float"); break;
			case EMutableParameterType::Color: TypeStr = TEXT("Color"); break;
			case EMutableParameterType::Texture: TypeStr = TEXT("Texture"); break;
			case EMutableParameterType::Projector: TypeStr = TEXT("Projector"); break;
			case EMutableParameterType::Transform: TypeStr = TEXT("Transform"); break;
			case EMutableParameterType::SkeletalMesh: TypeStr = TEXT("SkeletalMesh"); break;
			case EMutableParameterType::Material: TypeStr = TEXT("Material"); break;
			default: TypeStr = TEXT("Unknown"); break;
		}
		ParamObj->SetStringField(TEXT("type"), TypeStr);

		// Enum values if applicable
		int32 NumEnumValues = CO->GetEnumParameterNumValues(ParamName);
		if (NumEnumValues > 0)
		{
			TArray<TSharedPtr<FJsonValue>> EnumValues;
			for (int32 j = 0; j < NumEnumValues; ++j)
			{
				EnumValues.Add(MakeShared<FJsonValueString>(CO->GetEnumParameterValue(ParamName, j)));
			}
			ParamObj->SetArrayField(TEXT("enum_values"), EnumValues);
		}

		Parameters.Add(MakeShared<FJsonValueObject>(ParamObj));
	}
	Result->SetArrayField(TEXT("parameters"), Parameters);

	// Graph nodes (editor only)
	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (Graph)
	{
		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
			NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
			NodeObj->SetNumberField(TEXT("pin_count"), Node->Pins.Num());

			// Brief pin summary
			TArray<TSharedPtr<FJsonValue>> PinNames;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				FString PinInfo = FString::Printf(TEXT("%s (%s)"),
					*Pin->PinName.ToString(),
					*MutableCommandHelpers::PinDirectionToString(Pin->Direction));
				PinNames.Add(MakeShared<FJsonValueString>(PinInfo));
			}
			NodeObj->SetArrayField(TEXT("visible_pins"), PinNames);

			Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
		Result->SetArrayField(TEXT("nodes"), Nodes);
	}
	else
	{
		Result->SetStringField(TEXT("graph_note"), TEXT("Source graph not available. The CO may need to be opened in the editor first."));
	}

	return FECACommandResult::Success(Result);
}

// ─── get_co_node_pins ───────────────────────────────────────────

FECACommandResult FECACommand_GetCONodePins::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, NodeName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("node_name"), NodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: node_name"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available. Open the CO in the editor first."));

	UEdGraphNode* Node = MutableCommandHelpers::FindNodeByName(Graph, NodeName);
	if (!Node)
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' not found in CO graph"), *NodeName));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("node_id"), Node->GetName());
	Result->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Result->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		Pins.Add(MutableCommandHelpers::PinToJson(Pin));
	}
	Result->SetArrayField(TEXT("pins"), Pins);

	return FECACommandResult::Success(Result);
}

// ─── create_co ──────────────────────────────────────────────────

FECACommandResult FECACommand_CreateCO::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PackagePath, AssetName;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: package_path"));
	if (!GetStringParam(Params, TEXT("asset_name"), AssetName))
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_name"));

	FNewCustomizableObjectParameters NewParams;
	NewParams.PackagePath = PackagePath;
	NewParams.AssetName = AssetName;

	UCustomizableObject* NewCO = UCustomizableObjectEditorFunctionLibrary::NewCustomizableObject(NewParams);
	if (!NewCO)
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create CO '%s' at '%s'"), *AssetName, *PackagePath));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), NewCO->GetPathName());
	Result->SetStringField(TEXT("name"), NewCO->GetName());

	// Report initial graph state
	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(NewCO);
	if (Graph)
	{
		Result->SetNumberField(TEXT("initial_node_count"), Graph->Nodes.Num());
	}

	return FECACommandResult::Success(Result);
}

// ─── list_co_node_types ─────────────────────────────────────────

FECACommandResult FECACommand_ListCONodeTypes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeResult();
	TArray<TSharedPtr<FJsonValue>> NodeTypes;

	// Find all UClass objects that inherit from UCustomizableObjectNode
	UClass* BaseNodeClass = nullptr;

	// Try to find the base class by iterating
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == TEXT("CustomizableObjectNode"))
		{
			BaseNodeClass = *It;
			break;
		}
	}

	if (!BaseNodeClass)
	{
		// Fallback: try FindObject
		BaseNodeClass = FindObject<UClass>(nullptr, TEXT("/Script/CustomizableObjectEditor.CustomizableObjectNode"));
	}

	if (BaseNodeClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->IsChildOf(BaseNodeClass) && !Class->HasAnyClassFlags(CLASS_Abstract) && Class != BaseNodeClass)
			{
				TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
				TypeObj->SetStringField(TEXT("class_name"), Class->GetName());
				TypeObj->SetStringField(TEXT("full_path"), Class->GetPathName());

				// Try to get description from a CDO
				UEdGraphNode* CDO = Cast<UEdGraphNode>(Class->GetDefaultObject());
				if (CDO)
				{
					TypeObj->SetStringField(TEXT("tooltip"), CDO->GetTooltipText().ToString());
				}

				NodeTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
			}
		}
	}

	Result->SetArrayField(TEXT("node_types"), NodeTypes);
	Result->SetNumberField(TEXT("count"), NodeTypes.Num());

	return FECACommandResult::Success(Result);
}

// ─── add_co_node ────────────────────────────────────────────────

FECACommandResult FECACommand_AddCONode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, NodeClassName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("node_class"), NodeClassName))
		return FECACommandResult::Error(TEXT("Missing required parameter: node_class"));

	double PosX = 0, PosY = 0;
	GetFloatParam(Params, TEXT("pos_x"), PosX, false);
	GetFloatParam(Params, TEXT("pos_y"), PosY, false);

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available. Open the CO in the editor first."));

	// Find the node class
	UClass* NodeClass = nullptr;

	// Try direct FindObject first
	NodeClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CustomizableObjectEditor.%s"), *NodeClassName));

	// Fallback: iterate to find by name
	if (!NodeClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == NodeClassName)
			{
				NodeClass = *It;
				break;
			}
		}
	}

	if (!NodeClass)
		return FECACommandResult::Error(FString::Printf(TEXT("Node class '%s' not found. Use list_co_node_types to see available types."), *NodeClassName));

	// Verify it's a UEdGraphNode subclass
	if (!NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
		return FECACommandResult::Error(FString::Printf(TEXT("Class '%s' is not a graph node type"), *NodeClassName));

	// Create the node
	CO->Modify();
	Graph->Modify();

	UEdGraphNode* NewNode = NewObject<UEdGraphNode>(Graph, NodeClass, NAME_None, RF_Transactional);
	if (!NewNode)
		return FECACommandResult::Error(TEXT("Failed to create node object"));

	NewNode->CreateNewGuid();
	NewNode->NodePosX = static_cast<int32>(PosX);
	NewNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(NewNode, /*bUserAction=*/false, /*bSelectNewNode=*/false);
	NewNode->AllocateDefaultPins();
	NewNode->PostPlacedNewNode();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("node_id"), NewNode->GetName());
	Result->SetStringField(TEXT("node_title"), NewNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Result->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
	Result->SetNumberField(TEXT("pin_count"), NewNode->Pins.Num());

	// List pins on the new node
	TArray<TSharedPtr<FJsonValue>> PinNames;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (!Pin || Pin->bHidden) continue;
		FString PinInfo = FString::Printf(TEXT("%s (%s, %s)"),
			*Pin->PinName.ToString(),
			*MutableCommandHelpers::PinDirectionToString(Pin->Direction),
			*Pin->PinType.PinCategory.ToString());
		PinNames.Add(MakeShared<FJsonValueString>(PinInfo));
	}
	Result->SetArrayField(TEXT("pins"), PinNames);

	return FECACommandResult::Success(Result);
}

// ─── set_co_node_property ───────────────────────────────────────

FECACommandResult FECACommand_SetCONodeProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, NodeName, PropertyName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("node_name"), NodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: node_name"));
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName))
		return FECACommandResult::Error(TEXT("Missing required parameter: property_name"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available"));

	UEdGraphNode* Node = MutableCommandHelpers::FindNodeByName(Graph, NodeName);
	if (!Node)
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' not found"), *NodeName));

	// Find the property via UE reflection
	FProperty* Property = Node->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
		return FECACommandResult::Error(FString::Printf(TEXT("Property '%s' not found on node class '%s'"), *PropertyName, *Node->GetClass()->GetName()));

	Node->Modify();

	TSharedPtr<FJsonValue> JsonValue = Params->TryGetField(TEXT("property_value"));
	if (!JsonValue.IsValid())
		return FECACommandResult::Error(TEXT("Missing required parameter: property_value"));

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Node);

	// Handle different property types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			StrProp->SetPropertyValue(ValuePtr, Value);
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a string for FStrProperty"));
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*Value));
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a string for FNameProperty"));
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value;
		if (JsonValue->TryGetBool(Value))
		{
			BoolProp->SetPropertyValue(ValuePtr, Value);
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a boolean for FBoolProperty"));
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		double Value;
		if (JsonValue->TryGetNumber(Value))
		{
			IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(Value));
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a number for FIntProperty"));
		}
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		double Value;
		if (JsonValue->TryGetNumber(Value))
		{
			FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(Value));
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a number for FFloatProperty"));
		}
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		double Value;
		if (JsonValue->TryGetNumber(Value))
		{
			DoubleProp->SetPropertyValue(ValuePtr, Value);
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a number for FDoubleProperty"));
		}
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		// For object properties, treat the value as an asset path
		FString AssetPath;
		if (JsonValue->TryGetString(AssetPath))
		{
			UObject* LoadedObj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *AssetPath);
			if (LoadedObj)
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, LoadedObj);
			}
			else
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Could not load object at path: %s (expected type: %s)"), *AssetPath, *ObjProp->PropertyClass->GetName()));
			}
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a string (asset path) for object properties"));
		}
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		FString AssetPath;
		if (JsonValue->TryGetString(AssetPath))
		{
			FSoftObjectPath SoftPath{AssetPath};
			FSoftObjectPtr SoftRef{SoftPath};
			SoftObjProp->SetPropertyValue(ValuePtr, SoftRef);
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a string (asset path) for soft object properties"));
		}
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			// Try to import the value from string
			if (!Property->ImportText_Direct(*Value, ValuePtr, Node, PPF_None))
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Could not set enum value '%s' for property '%s'"), *Value, *PropertyName));
			}
		}
		else
		{
			return FECACommandResult::Error(TEXT("property_value must be a string for enum properties"));
		}
	}
	else
	{
		// Generic fallback: try ImportText
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			if (!Property->ImportText_Direct(*Value, ValuePtr, Node, PPF_None))
			{
				return FECACommandResult::Error(FString::Printf(TEXT("Could not import text '%s' for property '%s' (type: %s)"), *Value, *PropertyName, *Property->GetCPPType()));
			}
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Unsupported property type '%s' for property '%s'. Pass value as string and it will be parsed via ImportText."), *Property->GetCPPType(), *PropertyName));
		}
	}

	// Notify the node that a property changed so it can reconstruct pins if needed
	FPropertyChangedEvent PropertyChangedEvent(Property);
	Node->PostEditChangeProperty(PropertyChangedEvent);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("node"), NodeName);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetBoolField(TEXT("success"), true);

	return FECACommandResult::Success(Result);
}

// ─── connect_co_nodes ───────────────────────────────────────────

FECACommandResult FECACommand_ConnectCONodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, SourceNodeName, SourcePinName, TargetNodeName, TargetPinName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("source_node"), SourceNodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: source_node"));
	if (!GetStringParam(Params, TEXT("source_pin"), SourcePinName))
		return FECACommandResult::Error(TEXT("Missing required parameter: source_pin"));
	if (!GetStringParam(Params, TEXT("target_node"), TargetNodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: target_node"));
	if (!GetStringParam(Params, TEXT("target_pin"), TargetPinName))
		return FECACommandResult::Error(TEXT("Missing required parameter: target_pin"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available"));

	UEdGraphNode* SourceNode = MutableCommandHelpers::FindNodeByName(Graph, SourceNodeName);
	if (!SourceNode)
		return FECACommandResult::Error(FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeName));

	UEdGraphNode* TargetNode = MutableCommandHelpers::FindNodeByName(Graph, TargetNodeName);
	if (!TargetNode)
		return FECACommandResult::Error(FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeName));

	UEdGraphPin* SourcePin = MutableCommandHelpers::FindPinByName(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin)
	{
		// Try without direction constraint
		SourcePin = MutableCommandHelpers::FindPinByName(SourceNode, SourcePinName);
		if (!SourcePin)
			return FECACommandResult::Error(FString::Printf(TEXT("Output pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeName));
	}

	UEdGraphPin* TargetPin = MutableCommandHelpers::FindPinByName(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin)
	{
		TargetPin = MutableCommandHelpers::FindPinByName(TargetNode, TargetPinName);
		if (!TargetPin)
			return FECACommandResult::Error(FString::Printf(TEXT("Input pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeName));
	}

	// Use the schema to validate and create the connection
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Connection not allowed: %s"), *Response.Message.ToString()));
		}

		bool bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
		if (!bConnected)
		{
			return FECACommandResult::Error(TEXT("TryCreateConnection failed"));
		}
	}
	else
	{
		// Direct connection fallback (no schema)
		SourcePin->MakeLinkTo(TargetPin);
	}

	CO->Modify();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("source"), FString::Printf(TEXT("%s.%s"), *SourceNodeName, *SourcePinName));
	Result->SetStringField(TEXT("target"), FString::Printf(TEXT("%s.%s"), *TargetNodeName, *TargetPinName));
	Result->SetBoolField(TEXT("connected"), true);

	return FECACommandResult::Success(Result);
}

// ─── disconnect_co_nodes ────────────────────────────────────────

FECACommandResult FECACommand_DisconnectCONodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, NodeName, PinName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("node_name"), NodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: node_name"));
	if (!GetStringParam(Params, TEXT("pin_name"), PinName))
		return FECACommandResult::Error(TEXT("Missing required parameter: pin_name"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available"));

	UEdGraphNode* Node = MutableCommandHelpers::FindNodeByName(Graph, NodeName);
	if (!Node)
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' not found"), *NodeName));

	UEdGraphPin* Pin = MutableCommandHelpers::FindPinByName(Node, PinName);
	if (!Pin)
		return FECACommandResult::Error(FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeName));

	FString TargetNodeName, TargetPinName;
	bool bHasTargetNode = GetStringParam(Params, TEXT("target_node"), TargetNodeName, false);
	bool bHasTargetPin = GetStringParam(Params, TEXT("target_pin"), TargetPinName, false);

	int32 BrokenCount = 0;
	const UEdGraphSchema* Schema = Graph->GetSchema();

	if (bHasTargetNode && bHasTargetPin)
	{
		// Break specific connection
		UEdGraphNode* TNode = MutableCommandHelpers::FindNodeByName(Graph, TargetNodeName);
		if (!TNode)
			return FECACommandResult::Error(FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeName));

		UEdGraphPin* TPin = MutableCommandHelpers::FindPinByName(TNode, TargetPinName);
		if (!TPin)
			return FECACommandResult::Error(FString::Printf(TEXT("Target pin '%s' not found on '%s'"), *TargetPinName, *TargetNodeName));

		if (Schema)
		{
			Schema->BreakSinglePinLink(Pin, TPin);
		}
		else
		{
			Pin->BreakLinkTo(TPin);
		}
		BrokenCount = 1;
	}
	else
	{
		// Break all connections on this pin
		BrokenCount = Pin->LinkedTo.Num();
		if (Schema)
		{
			Schema->BreakPinLinks(*Pin, true);
		}
		else
		{
			Pin->BreakAllPinLinks(true);
		}
	}

	CO->Modify();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("node"), NodeName);
	Result->SetStringField(TEXT("pin"), PinName);
	Result->SetNumberField(TEXT("connections_broken"), BrokenCount);

	return FECACommandResult::Success(Result);
}

// ─── remove_co_node ─────────────────────────────────────────────

FECACommandResult FECACommand_RemoveCONode::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath, NodeName;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));
	if (!GetStringParam(Params, TEXT("node_name"), NodeName))
		return FECACommandResult::Error(TEXT("Missing required parameter: node_name"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UEdGraph* Graph = MutableCommandHelpers::GetSourceGraph(CO);
	if (!Graph)
		return FECACommandResult::Error(TEXT("Source graph not available"));

	UEdGraphNode* Node = MutableCommandHelpers::FindNodeByName(Graph, NodeName);
	if (!Node)
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' not found"), *NodeName));

	// Break all connections first
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		Schema->BreakNodeLinks(*Node);
	}
	else
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				Pin->BreakAllPinLinks(true);
			}
		}
	}

	CO->Modify();
	Graph->Modify();

	FString RemovedName = Node->GetName();
	Node->DestroyNode();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("removed_node"), RemovedName);
	Result->SetBoolField(TEXT("success"), true);

	return FECACommandResult::Success(Result);
}

// ─── compile_co ─────────────────────────────────────────────────

FECACommandResult FECACommand_CompileCO::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	FString OptLevelStr;
	GetStringParam(Params, TEXT("optimization_level"), OptLevelStr, false);

	ECustomizableObjectOptimizationLevel OptLevel = ECustomizableObjectOptimizationLevel::None;
	if (OptLevelStr.Equals(TEXT("none"), ESearchCase::IgnoreCase)) OptLevel = ECustomizableObjectOptimizationLevel::None;
	else if (OptLevelStr.Equals(TEXT("max"), ESearchCase::IgnoreCase) || OptLevelStr.Equals(TEXT("maximum"), ESearchCase::IgnoreCase)) OptLevel = ECustomizableObjectOptimizationLevel::Maximum;
	else if (OptLevelStr.Equals(TEXT("from_co"), ESearchCase::IgnoreCase)) OptLevel = ECustomizableObjectOptimizationLevel::FromCustomizableObject;

	// Use the CO's own Compile method with explicit params to bypass DDC cache
	FCompileParams CompileParams;
	CompileParams.bSkipIfCompiled = false;
	CompileParams.bSkipIfNotOutOfDate = false;  // Force recompile even if DDC thinks it's up to date
	CompileParams.bAsync = false;               // Synchronous
	CompileParams.bGatherReferences = true;
	CompileParams.OptimizationLevel = OptLevel;
	CompileParams.TextureCompression = ECustomizableObjectTextureCompression::Fast;

	// Save first to ensure the on-disk version is current, then compile so DDC matches
	{
		UPackage* Package = CO->GetOutermost();
		if (Package)
		{
			FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Package, CO, *PackageFilename, SaveArgs);
		}
	}

	CO->Compile(CompileParams);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("object"), CO->GetPathName());
	Result->SetStringField(TEXT("compilation_state"), CO->IsCompiled() ? TEXT("Completed") : TEXT("Failed"));
	Result->SetBoolField(TEXT("is_compiled"), CO->IsCompiled());
	Result->SetNumberField(TEXT("parameter_count"), CO->GetParameterCount());
	Result->SetNumberField(TEXT("component_count"), CO->GetComponentCount());

	return FECACommandResult::Success(Result);
}

// ─── get_co_instance_params ─────────────────────────────────────

FECACommandResult FECACommand_GetCOInstanceParams::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));

	// Try loading as CO first, then as COI
	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);

	TSharedPtr<FJsonObject> Result = MakeResult();

	if (CO)
	{
		// Report what the CO exposes
		Result->SetStringField(TEXT("type"), TEXT("CustomizableObject"));
		Result->SetStringField(TEXT("path"), CO->GetPathName());
		Result->SetBoolField(TEXT("is_compiled"), CO->IsCompiled());

		if (!CO->IsCompiled())
		{
			Result->SetStringField(TEXT("note"), TEXT("CO is not compiled. Compile it first to see parameters."));
			return FECACommandResult::Success(Result);
		}

		TArray<TSharedPtr<FJsonValue>> Parameters;
		for (int32 i = 0; i < CO->GetParameterCount(); ++i)
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			FString ParamName = CO->GetParameterName(i);
			ParamObj->SetStringField(TEXT("name"), ParamName);

			EMutableParameterType ParamType = CO->GetParameterTypeByName(ParamName);
			FString TypeStr;
			switch (ParamType)
			{
				case EMutableParameterType::Bool: TypeStr = TEXT("Bool"); break;
				case EMutableParameterType::Int: TypeStr = TEXT("Int"); break;
				case EMutableParameterType::Float: TypeStr = TEXT("Float"); break;
				case EMutableParameterType::Color: TypeStr = TEXT("Color"); break;
				case EMutableParameterType::Texture: TypeStr = TEXT("Texture"); break;
				case EMutableParameterType::Projector: TypeStr = TEXT("Projector"); break;
				case EMutableParameterType::Transform: TypeStr = TEXT("Transform"); break;
				case EMutableParameterType::SkeletalMesh: TypeStr = TEXT("SkeletalMesh"); break;
				case EMutableParameterType::Material: TypeStr = TEXT("Material"); break;
				default: TypeStr = TEXT("Unknown"); break;
			}
			ParamObj->SetStringField(TEXT("type"), TypeStr);

			int32 NumEnumValues = CO->GetEnumParameterNumValues(ParamName);
			if (NumEnumValues > 0)
			{
				TArray<TSharedPtr<FJsonValue>> EnumVals;
				for (int32 j = 0; j < NumEnumValues; ++j)
				{
					EnumVals.Add(MakeShared<FJsonValueString>(CO->GetEnumParameterValue(ParamName, j)));
				}
				ParamObj->SetArrayField(TEXT("enum_values"), EnumVals);
			}

			Parameters.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		Result->SetArrayField(TEXT("parameters"), Parameters);
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Customizable Object at: %s"), *ObjectPath));
	}

	return FECACommandResult::Success(Result);
}

// ─── save_co ────────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_SaveCO);
REGISTER_ECA_COMMAND(FECACommand_SpawnCOActor);
REGISTER_ECA_COMMAND(FECACommand_SetCOInstanceParam);

FECACommandResult FECACommand_SaveCO::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	UPackage* Package = CO->GetOutermost();
	if (!Package)
		return FECACommandResult::Error(TEXT("Could not get package for CO"));

	FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, CO, *PackageFilename, SaveArgs);
	bool bSaved = true;

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("saved"), bSaved);
	Result->SetStringField(TEXT("path"), CO->GetPathName());
	Result->SetStringField(TEXT("filename"), PackageFilename);

	return FECACommandResult::Success(Result);
}

// ─── spawn_co_actor ─────────────────────────────────────────────

FECACommandResult FECACommand_SpawnCOActor::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ObjectPath;
	if (!GetStringParam(Params, TEXT("object_path"), ObjectPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: object_path"));

	UCustomizableObject* CO = MutableCommandHelpers::LoadCO(ObjectPath);
	if (!CO)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load CO at: %s"), *ObjectPath));

	if (!CO->IsCompiled())
		return FECACommandResult::Error(TEXT("CO is not compiled. Compile it first before spawning."));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	FString ActorName = TEXT("CustomCharacter");
	GetStringParam(Params, TEXT("name"), ActorName, false);

	FVector Location = FVector::ZeroVector;
	GetVectorParam(Params, TEXT("location"), Location, false);

	FRotator Rotation = FRotator::ZeroRotator;
	GetRotatorParam(Params, TEXT("rotation"), Rotation, false);

	FActorSpawnParameters SpawnParams;
	// Don't force FName — let UE auto-generate a unique name to avoid crashes if the name exists
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = FName(*ActorName);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor(ACustomizableSkeletalMeshActor::StaticClass(), &Location, &Rotation, SpawnParams);
	ACustomizableSkeletalMeshActor* NewActor = Cast<ACustomizableSkeletalMeshActor>(SpawnedActor);
	if (!NewActor)
		return FECACommandResult::Error(TEXT("Failed to spawn CustomizableSkeletalMeshActor"));

	NewActor->SetActorLabel(ActorName);

	// Create a properly initialized CO Instance from the CO and assign to the component
	UCustomizableObjectInstance* COInstance = CO->CreateInstance();
	bool bFoundComponent = false;

	if (COInstance)
	{
		UCustomizableSkeletalComponent* CSComp = NewActor->FindComponentByClass<UCustomizableSkeletalComponent>();
		if (CSComp)
		{
			CSComp->SetCustomizableObjectInstance(COInstance);
			bFoundComponent = true;
		}
	}

	// Note: mesh generation happens automatically when PIE starts, or can be triggered
	// via set_co_instance_param. Calling UpdateSkeletalMeshAsync in editor mode before
	// the instance is fully registered can cause PrivateData assertion failures.

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), NewActor->GetActorLabel());
	Result->SetStringField(TEXT("actor_path"), NewActor->GetPathName());
	Result->SetStringField(TEXT("actor_class"), NewActor->GetClass()->GetName());
	Result->SetBoolField(TEXT("co_assigned"), bFoundComponent);
	Result->SetStringField(TEXT("co_path"), CO->GetPathName());

	TSharedPtr<FJsonObject> LocJson = VectorToJson(NewActor->GetActorLocation());
	Result->SetObjectField(TEXT("location"), LocJson);

	return FECACommandResult::Success(Result);
}

// ─── set_co_instance_param ──────────────────────────────────────

FECACommandResult FECACommand_SetCOInstanceParam::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName, ParamName;
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("param_name"), ParamName))
		return FECACommandResult::Error(TEXT("Missing required parameter: param_name"));

	TSharedPtr<FJsonValue> ParamValue = Params->TryGetField(TEXT("param_value"));
	if (!ParamValue.IsValid())
		return FECACommandResult::Error(TEXT("Missing required parameter: param_value"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));

	// Find CustomizableObjectInstance on the actor's components
	UObject* COInstance = nullptr;
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp) continue;
		FProperty* InstanceProp = Comp->GetClass()->FindPropertyByName(TEXT("CustomizableObjectInstance"));
		if (InstanceProp)
		{
			FObjectProperty* ObjProp = CastField<FObjectProperty>(InstanceProp);
			if (ObjProp)
			{
				COInstance = ObjProp->GetObjectPropertyValue(InstanceProp->ContainerPtrToValuePtr<void>(Comp));
				if (COInstance) break;
			}
		}
	}

	if (!COInstance)
		return FECACommandResult::Error(TEXT("No CustomizableObjectInstance found on actor"));

	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(COInstance);
	if (!Instance)
		return FECACommandResult::Error(TEXT("Component's instance is not a UCustomizableObjectInstance"));

	// Determine parameter type and set value
	UCustomizableObject* CO = Instance->GetCustomizableObject();
	if (!CO)
		return FECACommandResult::Error(TEXT("Instance has no Customizable Object"));

	EMutableParameterType ParamType = CO->GetParameterTypeByName(ParamName);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("param_name"), ParamName);

	switch (ParamType)
	{
		case EMutableParameterType::Color:
		{
			const TSharedPtr<FJsonObject>* ColorObj;
			if (GetObjectParam(Params, TEXT("param_value"), ColorObj, true))
			{
				float R = (*ColorObj)->HasField(TEXT("r")) ? (*ColorObj)->GetNumberField(TEXT("r")) : 1.0f;
				float G = (*ColorObj)->HasField(TEXT("g")) ? (*ColorObj)->GetNumberField(TEXT("g")) : 1.0f;
				float B = (*ColorObj)->HasField(TEXT("b")) ? (*ColorObj)->GetNumberField(TEXT("b")) : 1.0f;
				float A = (*ColorObj)->HasField(TEXT("a")) ? (*ColorObj)->GetNumberField(TEXT("a")) : 1.0f;
				Instance->SetColorParameterSelectedOption(ParamName, FLinearColor(R, G, B, A));
				Result->SetStringField(TEXT("set_value"), FString::Printf(TEXT("(%f,%f,%f,%f)"), R, G, B, A));
			}
			break;
		}
		case EMutableParameterType::Int:
		{
			// Can be an int index or an enum value name string
			FString StrValue;
			double NumValue;
			if (ParamValue->TryGetString(StrValue))
			{
				Instance->SetIntParameterSelectedOption(ParamName, StrValue);
				Result->SetStringField(TEXT("set_value"), StrValue);
			}
			else if (ParamValue->TryGetNumber(NumValue))
			{
				// Int params in Mutable use string option names, so convert index to the enum value name
				int32 Index = static_cast<int32>(NumValue);
				FString ValueName = CO->GetEnumParameterValue(ParamName, Index);
				Instance->SetIntParameterSelectedOption(ParamName, ValueName);
				Result->SetStringField(TEXT("set_value"), ValueName);
			}
			break;
		}
		case EMutableParameterType::Float:
		{
			double Value;
			if (ParamValue->TryGetNumber(Value))
			{
				Instance->SetFloatParameterSelectedOption(ParamName, static_cast<float>(Value));
				Result->SetStringField(TEXT("set_value"), FString::SanitizeFloat(Value));
			}
			break;
		}
		case EMutableParameterType::Bool:
		{
			bool Value;
			if (ParamValue->TryGetBool(Value))
			{
				Instance->SetBoolParameterSelectedOption(ParamName, Value);
				Result->SetStringField(TEXT("set_value"), Value ? TEXT("true") : TEXT("false"));
			}
			break;
		}
		default:
			return FECACommandResult::Error(FString::Printf(TEXT("Unsupported parameter type for '%s'"), *ParamName));
	}

	// Trigger update
	Instance->UpdateSkeletalMeshAsync(true, true);

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("update_triggered"), TEXT("true"));

	return FECACommandResult::Success(Result);
}

// ─── create_metahuman ──────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_CreateMetaHuman);

FECACommandResult FECACommand_CreateMetaHuman::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PackagePath, AssetName;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: package_path"));
	if (!GetStringParam(Params, TEXT("asset_name"), AssetName))
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_name"));

	// Discover MetaHuman classes via reflection — no compile-time dependency on the plugin
	UClass* MHCharClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter"));
	if (!MHCharClass)
	{
		return FECACommandResult::Error(
			TEXT("MetaHuman Character class not found. The MetaHumanCharacter plugin is not enabled in this project. ")
			TEXT("Enable it in Edit > Plugins > MetaHuman Character and restart the editor."));
	}

	UClass* MHFactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterFactoryNew"));
	if (!MHFactoryClass)
	{
		return FECACommandResult::Error(
			TEXT("MetaHuman Character factory not found. The MetaHumanCharacterEditor plugin is not loaded. ")
			TEXT("Ensure the MetaHumanCharacter plugin is enabled and the editor modules are available."));
	}

	// Create the factory instance
	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), MHFactoryClass);
	if (!Factory)
	{
		return FECACommandResult::Error(TEXT("Failed to create MetaHumanCharacterFactoryNew instance"));
	}

	// Use AssetTools to create the asset — this invokes the factory's FactoryCreateNew,
	// which calls NewObject<UMetaHumanCharacter> and then InitializeMetaHumanCharacter
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, MHCharClass, Factory);

	if (!NewAsset)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Failed to create MetaHuman Character '%s' at '%s'. Check the output log for details."),
			*AssetName, *PackagePath));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), NewAsset->GetName());
	Result->SetStringField(TEXT("class"), NewAsset->GetClass()->GetName());

	return FECACommandResult::Success(Result);
}
