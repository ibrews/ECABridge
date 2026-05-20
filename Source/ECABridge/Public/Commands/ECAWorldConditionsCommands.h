// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

// All commands gated by WITH_ECA_WORLDCONDITIONS. The runtime stores conditions
// as opaque FInstancedStruct blobs; the editor-side describer materializes them
// into human-readable strings via FWorldConditionQueryDefinition::GetDescription
// and FWorldConditionBase::GetDescription.

/**
 * describe_world_condition_query â€” return the human-readable description of an
 * entire FWorldConditionQueryDefinition. Caller passes the owning object's
 * asset path and the property name on it that holds the query.
 */
class FECACommand_DescribeWorldConditionQuery : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_world_condition_query"); }
	virtual FString GetDescription() const override { return TEXT("Return a human-readable description of an FWorldConditionQueryDefinition. Pass query_owner_path (object owning the query, e.g. /Game/Foo/MyAsset) and query_property_name (UPROPERTY name on the owner whose type is FWorldConditionQueryDefinition)."); }
	virtual FString GetCategory() const override { return TEXT("WorldConditions"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("query_owner_path"),     TEXT("string"), TEXT("Asset path to the UObject that owns the query property"), true, TEXT("") },
			{ TEXT("query_property_name"),  TEXT("string"), TEXT("Name of the FWorldConditionQueryDefinition UPROPERTY on the owner"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * describe_world_condition â€” describe a single condition inside a query by index.
 * Walks Definition.GetSharedDefinition()->GetConditions() (FInstancedStructContainer)
 * and calls GetDescription() on the matched FWorldConditionBase.
 */
class FECACommand_DescribeWorldCondition : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_world_condition"); }
	virtual FString GetDescription() const override { return TEXT("Return a human-readable description of a single condition inside an FWorldConditionQueryDefinition."); }
	virtual FString GetCategory() const override { return TEXT("WorldConditions"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("query_owner_path"),     TEXT("string"),  TEXT("Asset path to the UObject that owns the query property"), true, TEXT("") },
			{ TEXT("query_property_name"),  TEXT("string"),  TEXT("Name of the FWorldConditionQueryDefinition UPROPERTY on the owner"), true, TEXT("") },
			{ TEXT("condition_index"),      TEXT("integer"), TEXT("Zero-based index into the query's conditions"), true, TEXT("") }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
