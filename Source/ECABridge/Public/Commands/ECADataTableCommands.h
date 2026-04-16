// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Get DataTable schema - list all columns/fields and their types
 */
class FECACommand_GetDataTableSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_datatable_schema"); }
	virtual FString GetDescription() const override { return TEXT("Get the row structure (schema) of a DataTable, including all column names and types"); }
	virtual FString GetCategory() const override { return TEXT("DataTable"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the DataTable asset (e.g., /Game/Data/MyDataTable)"), true },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get DataTable rows - read all or specific rows from a DataTable
 */
class FECACommand_GetDataTableRows : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_datatable_rows"); }
	virtual FString GetDescription() const override { return TEXT("Get rows from a DataTable. Returns all rows or specific rows by name. Each row includes all column values."); }
	virtual FString GetCategory() const override { return TEXT("DataTable"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the DataTable asset (e.g., /Game/Data/MyDataTable)"), true },
			{ TEXT("row_names"), TEXT("array"), TEXT("Optional array of specific row names to retrieve. If omitted, returns all rows."), false },
			{ TEXT("limit"), TEXT("number"), TEXT("Maximum number of rows to return (default: all)"), false },
			{ TEXT("offset"), TEXT("number"), TEXT("Number of rows to skip (for pagination, default: 0)"), false },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set DataTable row - add or update a row in a DataTable
 */
class FECACommand_SetDataTableRow : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_datatable_row"); }
	virtual FString GetDescription() const override { return TEXT("Add a new row or update an existing row in a DataTable. Provide the row name and column values as a JSON object."); }
	virtual FString GetCategory() const override { return TEXT("DataTable"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the DataTable asset"), true },
			{ TEXT("row_name"), TEXT("string"), TEXT("Name/key of the row to add or update"), true },
			{ TEXT("row_data"), TEXT("object"), TEXT("JSON object with column names as keys and values to set. Use get_datatable_schema to discover column names."), true },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete DataTable row - remove a row from a DataTable
 */
/**
 * Dump full DataTable: schema + all rows in one call.
 */
class FECACommand_DumpDataTable : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_datatable"); }
	virtual FString GetDescription() const override { return TEXT("Serialize a complete DataTable to JSON: row struct schema with types, plus all row data. Single-call alternative to get_datatable_schema + get_datatable_rows."); }
	virtual FString GetCategory() const override { return TEXT("DataTable"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the DataTable asset"), true },
			{ TEXT("max_rows"), TEXT("number"), TEXT("Maximum rows to return (default 500, 0 = unlimited)"), false, TEXT("500") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_DeleteDataTableRow : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_datatable_row"); }
	virtual FString GetDescription() const override { return TEXT("Delete a row from a DataTable by its row name/key"); }
	virtual FString GetCategory() const override { return TEXT("DataTable"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content path to the DataTable asset"), true },
			{ TEXT("row_name"), TEXT("string"), TEXT("Name/key of the row to delete"), true },
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
