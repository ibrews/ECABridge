// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgIntrospectionCommands.h"
#include "Commands/ECACommand.h"

#include "PCGSettings.h"
#include "PCGPin.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_ListPCGNodeTypes)
REGISTER_ECA_COMMAND(FECACommand_GetPCGNodePins)

namespace ECAPcgIntrospectionHelpers
{
	static UClass* FindPCGSettingsClass(const FString& Name)
	{
		if (Name.IsEmpty()) return nullptr;
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

	static TSharedPtr<FJsonObject> PinPropsToJson(const FPCGPinProperties& Pin)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("label"), Pin.Label.ToString());
		Obj->SetBoolField(TEXT("allow_multiple_data"), Pin.bAllowMultipleData);
		Obj->SetBoolField(TEXT("allow_multiple_connections"), Pin.AllowsMultipleConnections());

		// Allowed-types: enum bit-field; emit the raw value plus a string list of set bits.
		// FPCGDataTypeIdentifier has an explicit operator EPCGDataType() — go through that
		// rather than the deprecated operator uint32().
		const EPCGDataType DataTypeBits = static_cast<EPCGDataType>(Pin.AllowedTypes);
		const uint32 RawTypes = static_cast<uint32>(DataTypeBits);
		Obj->SetNumberField(TEXT("allowed_types_mask"), RawTypes);
		if (UEnum* DataTypeEnum = StaticEnum<EPCGDataType>())
		{
			TArray<TSharedPtr<FJsonValue>> Bits;
			for (int32 i = 0; i < DataTypeEnum->NumEnums() - 1; ++i)
			{
				const int64 Value = DataTypeEnum->GetValueByIndex(i);
				if (Value == 0) continue;
				if ((RawTypes & static_cast<uint32>(Value)) == static_cast<uint32>(Value))
				{
					Bits.Add(MakeShared<FJsonValueString>(DataTypeEnum->GetNameStringByIndex(i)));
				}
			}
			Obj->SetArrayField(TEXT("allowed_types"), Bits);
		}
		return Obj;
	}
}

//==============================================================================
// list_pcg_node_types
//==============================================================================
FECACommandResult FECACommand_ListPCGNodeTypes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TArray<TSharedPtr<FJsonValue>> Types;
	const UClass* BaseClass = UPCGSettings::StaticClass();

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(BaseClass)) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		const FString ClassName = Class->GetName();
		if (!NameFilter.IsEmpty() && !ClassName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("class_name"), ClassName);
		Entry->SetStringField(TEXT("parent_class"), Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT("None"));

		if (const UPCGSettings* CDO = Cast<UPCGSettings>(Class->GetDefaultObject(false)))
		{
			Entry->SetStringField(TEXT("default_title"), CDO->GetDefaultNodeTitle().ToString());
			Entry->SetStringField(TEXT("default_node_name"), CDO->GetDefaultNodeName().ToString());
		}
		else
		{
			Entry->SetStringField(TEXT("default_title"), ClassName);
		}

		Types.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Stable sort by class_name for diff-friendly output
	Types.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		const FString AN = A->AsObject()->GetStringField(TEXT("class_name"));
		const FString BN = B->AsObject()->GetStringField(TEXT("class_name"));
		return AN.Compare(BN, ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetArrayField(TEXT("node_types"), Types);
	Out->SetNumberField(TEXT("count"), Types.Num());
	return FECACommandResult::Success(Out);
}

//==============================================================================
// get_pcg_node_pins
//==============================================================================
FECACommandResult FECACommand_GetPCGNodePins::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("settings_class"), ClassName, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("settings_class is required"));
	}

	UClass* Class = ECAPcgIntrospectionHelpers::FindPCGSettingsClass(ClassName);
	if (!Class)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("UPCGSettings subclass '%s' not found"), *ClassName));
	}

	UPCGSettings* CDO = Cast<UPCGSettings>(Class->GetDefaultObject(true));
	if (!CDO)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not obtain CDO for '%s'"), *ClassName));
	}

	const TArray<FPCGPinProperties> Inputs = CDO->DefaultInputPinProperties();
	const TArray<FPCGPinProperties> Outputs = CDO->DefaultOutputPinProperties();

	TArray<TSharedPtr<FJsonValue>> InputArr;
	for (const FPCGPinProperties& P : Inputs)
	{
		InputArr.Add(MakeShared<FJsonValueObject>(ECAPcgIntrospectionHelpers::PinPropsToJson(P)));
	}
	TArray<TSharedPtr<FJsonValue>> OutputArr;
	for (const FPCGPinProperties& P : Outputs)
	{
		OutputArr.Add(MakeShared<FJsonValueObject>(ECAPcgIntrospectionHelpers::PinPropsToJson(P)));
	}

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("settings_class"), Class->GetName());
	Out->SetArrayField(TEXT("input_pins"), InputArr);
	Out->SetArrayField(TEXT("output_pins"), OutputArr);
	return FECACommandResult::Success(Out);
}
