// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECADataTableCommands.h"
#include "Commands/ECACommand.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DataTableEditorUtils.h"

// Register all DataTable commands
REGISTER_ECA_COMMAND(FECACommand_GetDataTableSchema)
REGISTER_ECA_COMMAND(FECACommand_GetDataTableRows)
REGISTER_ECA_COMMAND(FECACommand_SetDataTableRow)
REGISTER_ECA_COMMAND(FECACommand_DeleteDataTableRow)

//------------------------------------------------------------------------------
// Helper: Convert a single property value to a JSON value
//------------------------------------------------------------------------------
static TSharedPtr<FJsonValue> DataTablePropertyToJsonValue(FProperty* Property, const void* ValuePtr)
{
	if (!Property || !ValuePtr)
	{
		return MakeShared<FJsonValueNull>();
	}

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}
	else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
		}
		else if (NumProp->IsInteger())
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		FString EnumName = Enum->GetNameStringByValue(EnumValue);
		return MakeShared<FJsonValueString>(EnumName);
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
		{
			uint8 ByteValue = *static_cast<const uint8*>(ValuePtr);
			FString EnumName = Enum->GetNameStringByValue(ByteValue);
			return MakeShared<FJsonValueString>(EnumName);
		}
		else
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(*static_cast<const uint8*>(ValuePtr)));
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// For structs, recurse into their properties
		TSharedPtr<FJsonObject> StructObj = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			FProperty* InnerProp = *PropIt;
			const void* InnerValuePtr = InnerProp->ContainerPtrToValuePtr<void>(ValuePtr);
			StructObj->SetField(InnerProp->GetName(), DataTablePropertyToJsonValue(InnerProp, InnerValuePtr));
		}
		return MakeShared<FJsonValueObject>(StructObj);
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			JsonArray.Add(DataTablePropertyToJsonValue(ArrayProp->Inner, ArrayHelper.GetRawPtr(i)));
		}
		return MakeShared<FJsonValueArray>(JsonArray);
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		// Maps are exported as arrays of {key, value} objects
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		for (int32 i = 0; i < MapHelper.Num(); i++)
		{
			if (MapHelper.IsValidIndex(i))
			{
				TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
				EntryObj->SetField(TEXT("key"), DataTablePropertyToJsonValue(MapProp->KeyProp, MapHelper.GetKeyPtr(i)));
				EntryObj->SetField(TEXT("value"), DataTablePropertyToJsonValue(MapProp->ValueProp, MapHelper.GetValuePtr(i)));
				JsonArray.Add(MakeShared<FJsonValueObject>(EntryObj));
			}
		}
		return MakeShared<FJsonValueArray>(JsonArray);
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		for (int32 i = 0; i < SetHelper.Num(); i++)
		{
			if (SetHelper.IsValidIndex(i))
			{
				JsonArray.Add(DataTablePropertyToJsonValue(SetProp->ElementProp, SetHelper.GetElementPtr(i)));
			}
		}
		return MakeShared<FJsonValueArray>(JsonArray);
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		return MakeShared<FJsonValueString>(SoftPtr.ToString());
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			return MakeShared<FJsonValueString>(Obj->GetPathName());
		}
		return MakeShared<FJsonValueString>(TEXT("None"));
	}

	// Fallback: export to string using UE's text export
	FString StringValue;
	Property->ExportTextItem_Direct(StringValue, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShared<FJsonValueString>(StringValue);
}

//------------------------------------------------------------------------------
// Helper: Convert a row to a JSON object
//------------------------------------------------------------------------------
static TSharedPtr<FJsonObject> RowToJsonObject(const UScriptStruct* RowStruct, const void* RowData)
{
	TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();

	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(RowData);
		RowObj->SetField(Property->GetName(), DataTablePropertyToJsonValue(Property, ValuePtr));
	}

	return RowObj;
}

//------------------------------------------------------------------------------
// Helper: Get property type as a friendly string
//------------------------------------------------------------------------------
static FString GetPropertyTypeString(FProperty* Property)
{
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return TEXT("bool");
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return TEXT("int32");
	}
	else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		return TEXT("int64");
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return TEXT("float");
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return TEXT("double");
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return TEXT("FString");
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return TEXT("FName");
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return TEXT("FText");
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		return FString::Printf(TEXT("enum:%s"), *Enum->GetName());
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
		{
			return FString::Printf(TEXT("enum:%s"), *Enum->GetName());
		}
		return TEXT("uint8");
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return FString::Printf(TEXT("struct:%s"), *StructProp->Struct->GetName());
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("array<%s>"), *GetPropertyTypeString(ArrayProp->Inner));
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return FString::Printf(TEXT("map<%s, %s>"), *GetPropertyTypeString(MapProp->KeyProp), *GetPropertyTypeString(MapProp->ValueProp));
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		return FString::Printf(TEXT("set<%s>"), *GetPropertyTypeString(SetProp->ElementProp));
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		return FString::Printf(TEXT("soft_object:%s"), *SoftObjProp->PropertyClass->GetName());
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return FString::Printf(TEXT("object:%s"), *ObjProp->PropertyClass->GetName());
	}

	return Property->GetCPPType();
}

//------------------------------------------------------------------------------
// GetDataTableSchema
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetDataTableSchema::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FECACommandResult::Error(TEXT("DataTable has no row structure defined"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("row_struct_name"), RowStruct->GetName());
	Result->SetStringField(TEXT("row_struct_path"), RowStruct->GetPathName());
	Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());

	// Build column definitions
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		TSharedPtr<FJsonObject> ColumnObj = MakeShared<FJsonObject>();
		ColumnObj->SetStringField(TEXT("name"), Property->GetName());
		ColumnObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
		ColumnObj->SetStringField(TEXT("cpp_type"), Property->GetCPPType());

		// Get tooltip/description if available
		FString ToolTip = Property->GetMetaData(TEXT("ToolTip"));
		if (!ToolTip.IsEmpty())
		{
			ColumnObj->SetStringField(TEXT("tooltip"), ToolTip);
		}

		// For enum properties, list the possible values
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			TArray<TSharedPtr<FJsonValue>> EnumValues;
			for (int32 i = 0; i < Enum->NumEnums() - 1; i++) // -1 to skip _MAX
			{
				EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
			}
			ColumnObj->SetArrayField(TEXT("enum_values"), EnumValues);
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
			{
				TArray<TSharedPtr<FJsonValue>> EnumValues;
				for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
				{
					EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
				}
				ColumnObj->SetArrayField(TEXT("enum_values"), EnumValues);
			}
		}

		// For struct properties, list sub-fields
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			TArray<TSharedPtr<FJsonValue>> StructFields;
			for (TFieldIterator<FProperty> InnerIt(StructProp->Struct); InnerIt; ++InnerIt)
			{
				TSharedPtr<FJsonObject> FieldInfo = MakeShared<FJsonObject>();
				FieldInfo->SetStringField(TEXT("name"), InnerIt->GetName());
				FieldInfo->SetStringField(TEXT("type"), GetPropertyTypeString(*InnerIt));
				StructFields.Add(MakeShared<FJsonValueObject>(FieldInfo));
			}
			ColumnObj->SetArrayField(TEXT("struct_fields"), StructFields);
		}

		ColumnsArray.Add(MakeShared<FJsonValueObject>(ColumnObj));
	}
	Result->SetArrayField(TEXT("columns"), ColumnsArray);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetDataTableRows
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetDataTableRows::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FECACommandResult::Error(TEXT("DataTable has no row structure defined"));
	}

	// Check for specific row names filter
	const TArray<TSharedPtr<FJsonValue>>* RowNamesArray = nullptr;
	TSet<FString> RequestedRowNames;
	bool bFilterByName = GetArrayParam(Params, TEXT("row_names"), RowNamesArray, false) && RowNamesArray && RowNamesArray->Num() > 0;
	if (bFilterByName)
	{
		for (const TSharedPtr<FJsonValue>& Value : *RowNamesArray)
		{
			RequestedRowNames.Add(Value->AsString());
		}
	}

	// Pagination
	int32 Limit = -1;
	int32 Offset = 0;
	GetIntParam(Params, TEXT("limit"), Limit, false);
	GetIntParam(Params, TEXT("offset"), Offset, false);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("row_struct_name"), RowStruct->GetName());

	const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
	Result->SetNumberField(TEXT("total_row_count"), RowMap.Num());

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	int32 CurrentIndex = 0;
	int32 ReturnedCount = 0;

	for (const auto& Pair : RowMap)
	{
		FString RowName = Pair.Key.ToString();

		// Apply name filter
		if (bFilterByName && !RequestedRowNames.Contains(RowName))
		{
			continue;
		}

		// Apply offset
		if (CurrentIndex < Offset)
		{
			CurrentIndex++;
			continue;
		}

		// Apply limit
		if (Limit > 0 && ReturnedCount >= Limit)
		{
			break;
		}

		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("row_name"), RowName);
		RowObj->SetObjectField(TEXT("data"), RowToJsonObject(RowStruct, Pair.Value));

		RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
		CurrentIndex++;
		ReturnedCount++;
	}

	Result->SetArrayField(TEXT("rows"), RowsArray);
	Result->SetNumberField(TEXT("returned_count"), ReturnedCount);

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetDataTableRow
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetDataTableRow::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString RowName;
	if (!GetStringParam(Params, TEXT("row_name"), RowName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: row_name"));
	}

	const TSharedPtr<FJsonObject>* RowDataObj = nullptr;
	if (!GetObjectParam(Params, TEXT("row_data"), RowDataObj))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: row_data"));
	}

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FECACommandResult::Error(TEXT("DataTable has no row structure defined"));
	}

	// Check if row already exists
	FName RowFName(*RowName);
	bool bIsNewRow = (DataTable->GetRowMap().Find(RowFName) == nullptr);

	// For editing, we need to work with the row data
	// If the row exists, we modify it; if not, we add it
	if (bIsNewRow)
	{
		// Add a new row - allocate memory for the struct and initialize it
		uint8* NewRowData = (uint8*)FMemory::Malloc(RowStruct->GetStructureSize());
		RowStruct->InitializeStruct(NewRowData);

		// Set values from JSON
		TArray<FString> FailedProperties;
		for (const auto& JsonField : (*RowDataObj)->Values)
		{
			FProperty* Property = RowStruct->FindPropertyByName(FName(*JsonField.Key));
			if (!Property)
			{
				FailedProperties.Add(FString::Printf(TEXT("%s (not found)"), *JsonField.Key));
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NewRowData);

			// Convert JSON value to string for ImportText
			FString ValueStr;
			if (JsonField.Value->Type == EJson::Boolean)
			{
				ValueStr = JsonField.Value->AsBool() ? TEXT("True") : TEXT("False");
			}
			else if (JsonField.Value->Type == EJson::Number)
			{
				ValueStr = FString::SanitizeFloat(JsonField.Value->AsNumber());
			}
			else if (JsonField.Value->Type == EJson::String)
			{
				ValueStr = JsonField.Value->AsString();
			}
			else if (JsonField.Value->Type == EJson::Object || JsonField.Value->Type == EJson::Array)
			{
				// For complex types, serialize to JSON string and let ImportText parse it
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
				FJsonSerializer::Serialize(JsonField.Value, TEXT(""), Writer);
			}

			if (!ValueStr.IsEmpty())
			{
				const TCHAR* ImportResult = Property->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None);
				if (!ImportResult)
				{
					FailedProperties.Add(FString::Printf(TEXT("%s (import failed)"), *JsonField.Key));
				}
			}
		}

		// Add the row to the DataTable
		DataTable->AddRow(RowFName, *reinterpret_cast<const FTableRowBase*>(NewRowData));

		// Clean up our temporary allocation
		RowStruct->DestroyStruct(NewRowData);
		FMemory::Free(NewRowData);

		DataTable->HandleDataTableChanged(RowFName);
		DataTable->MarkPackageDirty();
		FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("asset_path"), AssetPath);
		Result->SetStringField(TEXT("row_name"), RowName);
		Result->SetBoolField(TEXT("created"), true);
		Result->SetBoolField(TEXT("updated"), false);

		if (FailedProperties.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> FailedArray;
			for (const FString& Failed : FailedProperties)
			{
				FailedArray.Add(MakeShared<FJsonValueString>(Failed));
			}
			Result->SetArrayField(TEXT("failed_properties"), FailedArray);
		}

		// Return the row as it was actually stored
		uint8* StoredRow = DataTable->GetRowMap().FindRef(RowFName);
		if (StoredRow)
		{
			Result->SetObjectField(TEXT("row_data"), RowToJsonObject(RowStruct, StoredRow));
		}

		return FECACommandResult::Success(Result);
	}
	else
	{
		// Update existing row
		uint8* ExistingRowData = DataTable->GetRowMap().FindRef(RowFName);
		if (!ExistingRowData)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to find row '%s' for update"), *RowName));
		}

		DataTable->Modify();

		TArray<FString> FailedProperties;
		TArray<FString> UpdatedProperties;

		for (const auto& JsonField : (*RowDataObj)->Values)
		{
			FProperty* Property = RowStruct->FindPropertyByName(FName(*JsonField.Key));
			if (!Property)
			{
				FailedProperties.Add(FString::Printf(TEXT("%s (not found)"), *JsonField.Key));
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ExistingRowData);

			FString ValueStr;
			if (JsonField.Value->Type == EJson::Boolean)
			{
				ValueStr = JsonField.Value->AsBool() ? TEXT("True") : TEXT("False");
			}
			else if (JsonField.Value->Type == EJson::Number)
			{
				ValueStr = FString::SanitizeFloat(JsonField.Value->AsNumber());
			}
			else if (JsonField.Value->Type == EJson::String)
			{
				ValueStr = JsonField.Value->AsString();
			}
			else if (JsonField.Value->Type == EJson::Object || JsonField.Value->Type == EJson::Array)
			{
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
				FJsonSerializer::Serialize(JsonField.Value, TEXT(""), Writer);
			}

			if (!ValueStr.IsEmpty())
			{
				const TCHAR* ImportResult = Property->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None);
				if (ImportResult)
				{
					UpdatedProperties.Add(FString(*JsonField.Key));
				}
				else
				{
					FailedProperties.Add(FString::Printf(TEXT("%s (import failed)"), *JsonField.Key));
				}
			}
		}

		DataTable->HandleDataTableChanged(RowFName);
		DataTable->MarkPackageDirty();
		FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("asset_path"), AssetPath);
		Result->SetStringField(TEXT("row_name"), RowName);
		Result->SetBoolField(TEXT("created"), false);
		Result->SetBoolField(TEXT("updated"), true);

		TArray<TSharedPtr<FJsonValue>> UpdatedArray;
		for (const FString& Updated : UpdatedProperties)
		{
			UpdatedArray.Add(MakeShared<FJsonValueString>(Updated));
		}
		Result->SetArrayField(TEXT("updated_properties"), UpdatedArray);

		if (FailedProperties.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> FailedArray;
			for (const FString& Failed : FailedProperties)
			{
				FailedArray.Add(MakeShared<FJsonValueString>(Failed));
			}
			Result->SetArrayField(TEXT("failed_properties"), FailedArray);
		}

		// Return the updated row
		Result->SetObjectField(TEXT("row_data"), RowToJsonObject(RowStruct, ExistingRowData));

		return FECACommandResult::Success(Result);
	}
}

//------------------------------------------------------------------------------
// DeleteDataTableRow
//------------------------------------------------------------------------------

FECACommandResult FECACommand_DeleteDataTableRow::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString RowName;
	if (!GetStringParam(Params, TEXT("row_name"), RowName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: row_name"));
	}

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	FName RowFName(*RowName);

	// Check if row exists
	if (!DataTable->GetRowMap().Contains(RowFName))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Row '%s' not found in DataTable"), *RowName));
	}

	DataTable->Modify();
	DataTable->RemoveRow(RowFName);
	DataTable->HandleDataTableChanged(NAME_None);
	DataTable->MarkPackageDirty();
	FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("deleted_row"), RowName);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("remaining_row_count"), DataTable->GetRowMap().Num());

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// DumpDataTable
//------------------------------------------------------------------------------
REGISTER_ECA_COMMAND(FECACommand_DumpDataTable)

FECACommandResult FECACommand_DumpDataTable::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("DataTable not found: %s"), *AssetPath));
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FECACommandResult::Error(TEXT("DataTable has no row structure defined"));
	}

	int32 MaxRows = 500;
	GetIntParam(Params, TEXT("max_rows"), MaxRows, false);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("row_struct_name"), RowStruct->GetName());
	Result->SetStringField(TEXT("row_struct_path"), RowStruct->GetPathName());

	// ---- Schema (columns) ----
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		TSharedPtr<FJsonObject> ColumnObj = MakeShared<FJsonObject>();
		ColumnObj->SetStringField(TEXT("name"), Property->GetName());
		ColumnObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
		ColumnObj->SetStringField(TEXT("cpp_type"), Property->GetCPPType());

		FString ToolTip = Property->GetMetaData(TEXT("ToolTip"));
		if (!ToolTip.IsEmpty())
		{
			ColumnObj->SetStringField(TEXT("tooltip"), ToolTip);
		}

		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			TArray<TSharedPtr<FJsonValue>> EnumValues;
			for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
			{
				EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
			}
			ColumnObj->SetArrayField(TEXT("enum_values"), EnumValues);
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
			{
				TArray<TSharedPtr<FJsonValue>> EnumValues;
				for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
				{
					EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
				}
				ColumnObj->SetArrayField(TEXT("enum_values"), EnumValues);
			}
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			TArray<TSharedPtr<FJsonValue>> StructFields;
			for (TFieldIterator<FProperty> InnerIt(StructProp->Struct); InnerIt; ++InnerIt)
			{
				TSharedPtr<FJsonObject> FieldInfo = MakeShared<FJsonObject>();
				FieldInfo->SetStringField(TEXT("name"), InnerIt->GetName());
				FieldInfo->SetStringField(TEXT("type"), GetPropertyTypeString(*InnerIt));
				StructFields.Add(MakeShared<FJsonValueObject>(FieldInfo));
			}
			ColumnObj->SetArrayField(TEXT("struct_fields"), StructFields);
		}

		ColumnsArray.Add(MakeShared<FJsonValueObject>(ColumnObj));
	}
	Result->SetArrayField(TEXT("columns"), ColumnsArray);

	// ---- Rows ----
	const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
	int32 TotalRowCount = RowMap.Num();
	Result->SetNumberField(TEXT("total_row_count"), TotalRowCount);

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	int32 ReturnedCount = 0;

	for (const auto& Pair : RowMap)
	{
		if (MaxRows > 0 && ReturnedCount >= MaxRows)
		{
			break;
		}

		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("row_name"), Pair.Key.ToString());
		RowObj->SetObjectField(TEXT("data"), RowToJsonObject(RowStruct, Pair.Value));

		RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
		ReturnedCount++;
	}

	Result->SetArrayField(TEXT("rows"), RowsArray);
	Result->SetNumberField(TEXT("returned_count"), ReturnedCount);
	Result->SetBoolField(TEXT("truncated"), MaxRows > 0 && TotalRowCount > MaxRows);

	return FECACommandResult::Success(Result);
}
