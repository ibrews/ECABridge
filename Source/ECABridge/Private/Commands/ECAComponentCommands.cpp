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
REGISTER_ECA_COMMAND(FECACommand_GetComponentProperty)

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
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		// Object class reference (e.g., TSubclassOf<UUserWidget>, TSubclassOf<AActor>).
		// Pass empty string to clear, otherwise an asset/class path.
		FString ClassPath = PropertyValue->AsString();
		UClass* LoadedClass = nullptr;
		if (!ClassPath.IsEmpty())
		{
			UClass* Constraint = ClassProp->MetaClass.Get();
			if (!Constraint) { Constraint = UObject::StaticClass(); }

			// Try the path as given.
			LoadedClass = StaticLoadClass(Constraint, nullptr, *ClassPath);
			if (!LoadedClass)
			{
				// Blueprint generated classes are at "/Game/Path/BP_Foo.BP_Foo_C".
				// If the user passed only the asset path without `.AssetName_C`,
				// expand it.
				if (!ClassPath.Contains(TEXT(".")))
				{
					int32 LastSlash;
					if (ClassPath.FindLastChar('/', LastSlash))
					{
						const FString AssetName = ClassPath.Mid(LastSlash + 1);
						const FString Expanded = ClassPath + TEXT(".") + AssetName + TEXT("_C");
						LoadedClass = StaticLoadClass(Constraint, nullptr, *Expanded);
					}
				}
			}
			if (!LoadedClass)
			{
				// Older fallback: append `_C` to the raw path. Rare but inexpensive.
				LoadedClass = StaticLoadClass(Constraint, nullptr, *(ClassPath + TEXT("_C")));
			}
			if (!LoadedClass)
			{
				return FECACommandResult::Error(FString::Printf(
					TEXT("Failed to load class '%s' for property '%s' (expected child of %s)"),
					*ClassPath, *PropertyName, *Constraint->GetName()));
			}
		}
		ClassProp->SetObjectPropertyValue(PropertyAddr, LoadedClass);
	}
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		// Soft class reference (TSoftClassPtr<>)
		FString ClassPath = PropertyValue->AsString();
		FSoftObjectPtr* PtrAddr = static_cast<FSoftObjectPtr*>(PropertyAddr);
		*PtrAddr = FSoftObjectPath(ClassPath);
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		// Hard object reference (USoundBase*, UStaticMesh*, UMaterialInterface*, etc.)
		FString AssetPath = PropertyValue->AsString();
		UObject* AssetObject = nullptr;
		if (!AssetPath.IsEmpty())
		{
			UClass* Constraint = ObjProp->PropertyClass.Get();
			if (!Constraint) { Constraint = UObject::StaticClass(); }
			AssetObject = StaticLoadObject(Constraint, nullptr, *AssetPath);
			if (!AssetObject)
			{
				return FECACommandResult::Error(FString::Printf(
					TEXT("Failed to load object '%s' for property '%s' (expected %s)"),
					*AssetPath, *PropertyName, *Constraint->GetName()));
			}
		}
		ObjProp->SetObjectPropertyValue(PropertyAddr, AssetObject);
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		// Soft object reference (TSoftObjectPtr<>)
		FString AssetPath = PropertyValue->AsString();
		FSoftObjectPtr* PtrAddr = static_cast<FSoftObjectPtr*>(PropertyAddr);
		*PtrAddr = FSoftObjectPath(AssetPath);
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Unsupported property type '%s' for property '%s'. Supported: bool, int, float, double, string, name, object reference, soft object, class reference, soft class."),
			*Property->GetClass()->GetName(), *PropertyName));
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetComponentProperty
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetComponentProperty::Execute(const TSharedPtr<FJsonObject>& Params)
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
	FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	const void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Component);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetStringField(TEXT("property_type"), Property->GetClass()->GetName());

	// Typed JSON value for primitives that have natural JSON representations.
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		Result->SetBoolField(TEXT("value"), BoolProp->GetPropertyValue(PropertyAddr));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		Result->SetNumberField(TEXT("value"), IntProp->GetPropertyValue(PropertyAddr));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		Result->SetNumberField(TEXT("value"), FloatProp->GetPropertyValue(PropertyAddr));
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		Result->SetNumberField(TEXT("value"), DoubleProp->GetPropertyValue(PropertyAddr));
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		Result->SetStringField(TEXT("value"), StrProp->GetPropertyValue(PropertyAddr));
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		Result->SetStringField(TEXT("value"), NameProp->GetPropertyValue(PropertyAddr).ToString());
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(PropertyAddr);
		Result->SetStringField(TEXT("value"), Obj ? Obj->GetPathName() : FString());
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr* Ptr = static_cast<const FSoftObjectPtr*>(PropertyAddr);
		Result->SetStringField(TEXT("value"), Ptr->ToString());
	}
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		UClass* Cls = Cast<UClass>(ClassProp->GetObjectPropertyValue(PropertyAddr));
		Result->SetStringField(TEXT("value"), Cls ? Cls->GetPathName() : FString());
	}
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		const FSoftObjectPtr* Ptr = static_cast<const FSoftObjectPtr*>(PropertyAddr);
		Result->SetStringField(TEXT("value"), Ptr->ToString());
	}

	// Always include the Unreal-text-format export — handles structs, arrays, enums, etc.
	FString ExportText;
	Property->ExportTextItem_Direct(ExportText, PropertyAddr, nullptr, Component, PPF_None);
	Result->SetStringField(TEXT("value_text"), ExportText);

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

	bool bIncludeOverrides = false;
	GetBoolParam(Params, TEXT("include_overrides"), bIncludeOverrides, false);

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

			USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
			if (SceneComp)
			{
				CompJson->SetObjectField(TEXT("relative_transform"), TransformToJson(SceneComp->GetRelativeTransform()));
			}

			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				CompJson->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
			}

			// Optionally enumerate properties whose template value differs from the class default.
			if (bIncludeOverrides && Node->ComponentTemplate && Node->ComponentClass)
			{
				UActorComponent* Template = Node->ComponentTemplate;
				UObject* ClassDefault = Node->ComponentClass->GetDefaultObject();
				TSharedPtr<FJsonObject> OverridesJson = MakeShared<FJsonObject>();
				int32 OverrideCount = 0;

				for (TFieldIterator<FProperty> PropIt(Node->ComponentClass); PropIt; ++PropIt)
				{
					FProperty* Prop = *PropIt;
					if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;
					if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) continue;

					const void* TemplatePtr = Prop->ContainerPtrToValuePtr<void>(Template);
					const void* DefaultPtr = ClassDefault ? Prop->ContainerPtrToValuePtr<void>(ClassDefault) : nullptr;
					if (DefaultPtr && Prop->Identical(TemplatePtr, DefaultPtr))
					{
						continue;
					}

					FString ValueStr;
					Prop->ExportTextItem_Direct(ValueStr, TemplatePtr, DefaultPtr, Template, PPF_None);
					OverridesJson->SetStringField(Prop->GetName(), ValueStr);
					OverrideCount++;
				}

				if (OverrideCount > 0)
				{
					CompJson->SetObjectField(TEXT("overrides"), OverridesJson);
					CompJson->SetNumberField(TEXT("override_count"), OverrideCount);
				}
			}

			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompJson));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("components"), ComponentsArray);
	Result->SetNumberField(TEXT("count"), ComponentsArray.Num());
	return FECACommandResult::Success(Result);
}
