// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECACVarCommands.h"
#include "HAL/IConsoleManager.h"

REGISTER_ECA_COMMAND(FECACommand_GetCVar)
REGISTER_ECA_COMMAND(FECACommand_SetCVar)
REGISTER_ECA_COMMAND(FECACommand_ListCVars)

namespace
{
	void DescribeCVar(IConsoleVariable* CVar, const FString& Name, TSharedPtr<FJsonObject>& Out)
	{
		Out->SetStringField(TEXT("name"), Name);
		Out->SetStringField(TEXT("value"), CVar->GetString());
		Out->SetStringField(TEXT("default_value"), CVar->GetDefaultValue());

		FString Type = TEXT("string");
		if (CVar->IsVariableInt())        Type = TEXT("int");
		else if (CVar->IsVariableBool())  Type = TEXT("bool");
		else if (CVar->IsVariableFloat()) Type = TEXT("float");
		Out->SetStringField(TEXT("type"), Type);

		IConsoleObject* AsObj = static_cast<IConsoleObject*>(CVar);
		const TCHAR* Help = AsObj->GetHelp();
		if (Help)
		{
			Out->SetStringField(TEXT("help"), Help);
		}

		const EConsoleVariableFlags Flags = CVar->GetFlags();
		Out->SetBoolField(TEXT("read_only"), (Flags & ECVF_ReadOnly) != 0);
		Out->SetBoolField(TEXT("cheat"),     (Flags & ECVF_Cheat) != 0);
	}
}

FECACommandResult FECACommand_GetCVar::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("CVar not found: %s"), *Name));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	DescribeCVar(CVar, Name, Result);
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_SetCVar::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!GetStringParam(Params, TEXT("name"), Name))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));
	}

	// Accept string/number/bool for `value` and coerce to string.
	FString NewValueStr;
	if (!Params.IsValid() || !Params->HasField(TEXT("value")))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> JsonVal = Params->TryGetField(TEXT("value"));
	if (!JsonVal.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: value"));
	}
	switch (JsonVal->Type)
	{
	case EJson::String:  NewValueStr = JsonVal->AsString(); break;
	case EJson::Number:  NewValueStr = FString::SanitizeFloat(JsonVal->AsNumber()); break;
	case EJson::Boolean: NewValueStr = JsonVal->AsBool() ? TEXT("1") : TEXT("0"); break;
	default:
		return FECACommandResult::Error(TEXT("Unsupported value type. Use string, number, or boolean."));
	}

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("CVar not found: %s"), *Name));
	}

	const FString OldValue = CVar->GetString();
	CVar->Set(*NewValueStr, ECVF_SetByConsole);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("old_value"), OldValue);
	Result->SetStringField(TEXT("new_value"), CVar->GetString());
	return FECACommandResult::Success(Result);
}

FECACommandResult FECACommand_ListCVars::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	GetStringParam(Params, TEXT("filter"), Filter, false);

	int32 MaxCount = 500;
	GetIntParam(Params, TEXT("max_count"), MaxCount, false);
	MaxCount = FMath::Clamp(MaxCount, 1, 5000);

	TArray<TSharedPtr<FJsonValue>> CVarArray;
	int32 TotalMatched = 0;

	auto Visitor = [&](const TCHAR* Name, IConsoleObject* Obj)
	{
		if (!Obj) return;
		IConsoleVariable* CVar = Obj->AsVariable();
		if (!CVar) return; // skip console commands

		TotalMatched++;
		if (CVarArray.Num() >= MaxCount) return;

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		DescribeCVar(CVar, FString(Name), Entry);
		CVarArray.Add(MakeShared<FJsonValueObject>(Entry));
	};

	if (Filter.IsEmpty())
	{
		IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
			FConsoleObjectVisitor::CreateLambda(Visitor), TEXT(""));
	}
	else
	{
		IConsoleManager::Get().ForEachConsoleObjectThatContains(
			FConsoleObjectVisitor::CreateLambda(Visitor), *Filter);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("cvars"), CVarArray);
	Result->SetNumberField(TEXT("returned"), CVarArray.Num());
	Result->SetNumberField(TEXT("total_matched"), TotalMatched);
	Result->SetStringField(TEXT("filter"), Filter);
	return FECACommandResult::Success(Result);
}
