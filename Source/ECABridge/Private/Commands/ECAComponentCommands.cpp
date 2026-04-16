// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAComponentCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"

// Register all component commands
REGISTER_ECA_COMMAND(FECACommand_SetStaticMeshProperties)
REGISTER_ECA_COMMAND(FECACommand_SetPhysicsProperties)
REGISTER_ECA_COMMAND(FECACommand_SetComponentProperty)
REGISTER_ECA_COMMAND(FECACommand_SetComponentTransform)
REGISTER_ECA_COMMAND(FECACommand_GetBlueprintComponents)

//------------------------------------------------------------------------------
// Helper: Find SCS Node by name
//------------------------------------------------------------------------------

static USCS_Node* FindSCSNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}
	
	TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			return Node;
		}
	}
	
	return nullptr;
}

//------------------------------------------------------------------------------
// SetStaticMeshProperties
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetStaticMeshProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}
	
	FString ComponentName;
	if (!GetStringParam(Params, TEXT("component_name"), ComponentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: component_name"));
	}
	
	FString StaticMeshPath;
	if (!GetStringParam(Params, TEXT("static_mesh"), StaticMeshPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: static_mesh"));
	}
	
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	
	USCS_Node* Node = FindSCSNodeByName(Blueprint, ComponentName);
	if (!Node)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}
	
	// Get the component template
	UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
	if (!MeshComponent)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component is not a StaticMeshComponent: %s"), *ComponentName));
	}
	
	// Load the mesh
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *StaticMeshPath);
	if (!Mesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Static mesh not found: %s"), *StaticMeshPath));
	}
	
	// Set the mesh
	MeshComponent->SetStaticMesh(Mesh);
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("static_mesh"), StaticMeshPath);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetPhysicsProperties
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetPhysicsProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}
	
	FString ComponentName;
	if (!GetStringParam(Params, TEXT("component_name"), ComponentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: component_name"));
	}
	
	bool bSimulatePhysics = true;
	bool bGravityEnabled = true;
	double Mass = 1.0;
	double LinearDamping = 0.01;
	double AngularDamping = 0.0;
	
	GetBoolParam(Params, TEXT("simulate_physics"), bSimulatePhysics, false);
	GetBoolParam(Params, TEXT("gravity_enabled"), bGravityEnabled, false);
	GetFloatParam(Params, TEXT("mass"), Mass, false);
	GetFloatParam(Params, TEXT("linear_damping"), LinearDamping, false);
	GetFloatParam(Params, TEXT("angular_damping"), AngularDamping, false);
	
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	
	USCS_Node* Node = FindSCSNodeByName(Blueprint, ComponentName);
	if (!Node)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}
	
	// Get the component template
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Node->ComponentTemplate);
	if (!PrimitiveComponent)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component is not a PrimitiveComponent: %s"), *ComponentName));
	}
	
	// Set physics properties
	PrimitiveComponent->SetSimulatePhysics(bSimulatePhysics);
	PrimitiveComponent->SetEnableGravity(bGravityEnabled);
	PrimitiveComponent->SetMassOverrideInKg(NAME_None, Mass);
	PrimitiveComponent->SetLinearDamping(LinearDamping);
	PrimitiveComponent->SetAngularDamping(AngularDamping);
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetBoolField(TEXT("simulate_physics"), bSimulatePhysics);
	Result->SetBoolField(TEXT("gravity_enabled"), bGravityEnabled);
	Result->SetNumberField(TEXT("mass"), Mass);
	Result->SetNumberField(TEXT("linear_damping"), LinearDamping);
	Result->SetNumberField(TEXT("angular_damping"), AngularDamping);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetComponentProperty
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetComponentProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}
	
	FString ComponentName;
	if (!GetStringParam(Params, TEXT("component_name"), ComponentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: component_name"));
	}
	
	FString PropertyName;
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_name"));
	}
	
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	
	USCS_Node* Node = FindSCSNodeByName(Blueprint, ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}
	
	UActorComponent* Component = Node->ComponentTemplate;
	
	// Find the property
	FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}
	
	// Get the property value from JSON
	TSharedPtr<FJsonValue> PropertyValue = Params->TryGetField(TEXT("property_value"));
	if (!PropertyValue.IsValid())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: property_value"));
	}
	
	// Set property based on type
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Component);
	
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue(PropertyAddr, PropertyValue->AsBool());
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue(PropertyAddr, FMath::RoundToInt(PropertyValue->AsNumber()));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue(PropertyAddr, PropertyValue->AsNumber());
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		DoubleProp->SetPropertyValue(PropertyAddr, PropertyValue->AsNumber());
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(PropertyAddr, PropertyValue->AsString());
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		NameProp->SetPropertyValue(PropertyAddr, FName(*PropertyValue->AsString()));
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unsupported property type for: %s"), *PropertyName));
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetComponentTransform
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetComponentTransform::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}
	
	FString ComponentName;
	if (!GetStringParam(Params, TEXT("component_name"), ComponentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: component_name"));
	}
	
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	
	USCS_Node* Node = FindSCSNodeByName(Blueprint, ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}
	
	USceneComponent* SceneComponent = Cast<USceneComponent>(Node->ComponentTemplate);
	if (!SceneComponent)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Component is not a SceneComponent: %s"), *ComponentName));
	}
	
	FVector Location;
	FRotator Rotation;
	FVector Scale;
	
	if (GetVectorParam(Params, TEXT("location"), Location, false))
	{
		SceneComponent->SetRelativeLocation(Location);
	}
	
	if (GetRotatorParam(Params, TEXT("rotation"), Rotation, false))
	{
		SceneComponent->SetRelativeRotation(Rotation);
	}
	
	if (GetVectorParam(Params, TEXT("scale"), Scale, false))
	{
		SceneComponent->SetRelativeScale3D(Scale);
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetObjectField(TEXT("transform"), TransformToJson(SceneComponent->GetRelativeTransform()));
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetBlueprintComponents
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetBlueprintComponents::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: blueprint_path"));
	}
	
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}
	
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	
	if (Blueprint->SimpleConstructionScript)
	{
		TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		
		for (USCS_Node* Node : AllNodes)
		{
			if (!Node || !Node->ComponentTemplate)
			{
				continue;
			}
			
			TSharedPtr<FJsonObject> CompJson = MakeShared<FJsonObject>();
			CompJson->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompJson->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("None"));
			
			// Check if it's a scene component and get transform
			USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
			if (SceneComp)
			{
				CompJson->SetObjectField(TEXT("relative_transform"), TransformToJson(SceneComp->GetRelativeTransform()));
			}
			
			// Get parent node name if any
			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				CompJson->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
			}
			
			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompJson));
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("components"), ComponentsArray);
	Result->SetNumberField(TEXT("count"), ComponentsArray.Num());
	return FECACommandResult::Success(Result);
}
