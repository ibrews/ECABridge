// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWorldConditionsCommands.h"

#if WITH_ECA_WORLDCONDITIONS

#include "WorldConditionQuery.h"
#include "WorldConditionBase.h"
#include "StructUtils/InstancedStructContainer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace ECAWorldConditionsInternal
{
	UObject* LoadOwnerByPath(const FString& Path)
	{
		FSoftObjectPath SoftPath(Path);
		if (!SoftPath.IsValid())
		{
			return nullptr;
		}
		return SoftPath.TryLoad();
	}

	// Walk the owner's reflected properties to find one whose type is
	// FWorldConditionQueryDefinition, then point into the owner memory.
	const FWorldConditionQueryDefinition* ResolveQueryDefinition(UObject* Owner, const FString& PropertyName, FString& OutError)
	{
		if (!Owner)
		{
			OutError = TEXT("owner object is null");
			return nullptr;
		}
		UClass* OwnerClass = Owner->GetClass();
		FStructProperty* StructProp = FindFProperty<FStructProperty>(OwnerClass, FName(*PropertyName));
		if (!StructProp)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found on class '%s'."), *PropertyName, *OwnerClass->GetName());
			return nullptr;
		}
		if (!StructProp->Struct || StructProp->Struct != FWorldConditionQueryDefinition::StaticStruct())
		{
			OutError = FString::Printf(TEXT("Property '%s' is not an FWorldConditionQueryDefinition (got '%s')."),
				*PropertyName,
				StructProp->Struct ? *StructProp->Struct->GetName() : TEXT("null"));
			return nullptr;
		}
		return StructProp->ContainerPtrToValuePtr<FWorldConditionQueryDefinition>(Owner);
	}
}

FECACommandResult FECACommand_DescribeWorldConditionQuery::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString OwnerPath, PropertyName;
	if (!GetStringParam(Params, TEXT("query_owner_path"), OwnerPath, true))      return FECACommandResult::ValidationError(this, TEXT("query_owner_path is required"));
	if (!GetStringParam(Params, TEXT("query_property_name"), PropertyName, true)) return FECACommandResult::ValidationError(this, TEXT("query_property_name is required"));

	UObject* Owner = ECAWorldConditionsInternal::LoadOwnerByPath(OwnerPath);
	if (!Owner)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load object at '%s'."), *OwnerPath));
	}

	FString Error;
	const FWorldConditionQueryDefinition* Def = ECAWorldConditionsInternal::ResolveQueryDefinition(Owner, PropertyName, Error);
	if (!Def)
	{
		return FECACommandResult::ValidationError(this, Error);
	}

	const FText DescText = Def->GetDescription();
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("query_owner_path"), OwnerPath);
	Result->SetStringField(TEXT("query_property_name"), PropertyName);
	Result->SetStringField(TEXT("description"), DescText.ToString());
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DescribeWorldConditionQuery);

FECACommandResult FECACommand_DescribeWorldCondition::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString OwnerPath, PropertyName;
	int32 ConditionIndex = 0;
	if (!GetStringParam(Params, TEXT("query_owner_path"), OwnerPath, true))      return FECACommandResult::ValidationError(this, TEXT("query_owner_path is required"));
	if (!GetStringParam(Params, TEXT("query_property_name"), PropertyName, true)) return FECACommandResult::ValidationError(this, TEXT("query_property_name is required"));
	if (!GetIntParam(Params, TEXT("condition_index"), ConditionIndex, true))      return FECACommandResult::ValidationError(this, TEXT("condition_index is required"));
	if (ConditionIndex < 0)                                                       return FECACommandResult::ValidationError(this, TEXT("condition_index must be non-negative"));

	UObject* Owner = ECAWorldConditionsInternal::LoadOwnerByPath(OwnerPath);
	if (!Owner)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load object at '%s'."), *OwnerPath));
	}

	FString Error;
	const FWorldConditionQueryDefinition* Def = ECAWorldConditionsInternal::ResolveQueryDefinition(Owner, PropertyName, Error);
	if (!Def)
	{
		return FECACommandResult::ValidationError(this, Error);
	}

	const FWorldConditionQuerySharedDefinition* Shared = Def->GetSharedDefinition();
	if (!Shared)
	{
		return FECACommandResult::Error(TEXT("Query has no initialized shared definition (asset may not be loaded with editor data)."));
	}
	const FInstancedStructContainer& Conditions = Shared->GetConditions();
	if (ConditionIndex >= Conditions.Num())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("condition_index %d out of range (conditions: %d)"), ConditionIndex, Conditions.Num()));
	}

	const FConstStructView CondView = Conditions[ConditionIndex];
	const FWorldConditionBase* Cond = CondView.GetPtr<FWorldConditionBase>();
	FString DescStr;
	if (Cond)
	{
		DescStr = Cond->GetDescription().ToString();
	}
	if (DescStr.IsEmpty() && CondView.GetScriptStruct())
	{
		DescStr = CondView.GetScriptStruct()->GetName();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("query_owner_path"), OwnerPath);
	Result->SetStringField(TEXT("query_property_name"), PropertyName);
	Result->SetNumberField(TEXT("condition_index"), ConditionIndex);
	Result->SetStringField(TEXT("class_name"), CondView.GetScriptStruct() ? CondView.GetScriptStruct()->GetName() : FString());
	Result->SetStringField(TEXT("description"), DescStr);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DescribeWorldCondition);

#endif // WITH_ECA_WORLDCONDITIONS
