// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASubsystemCommands.h"
#include "Commands/ECACommand.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EditorSubsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/Subsystem.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_ListEditorSubsystems)
REGISTER_ECA_COMMAND(FECACommand_ListEngineSubsystems)
REGISTER_ECA_COMMAND(FECACommand_DumpSubsystem)
REGISTER_ECA_COMMAND(FECACommand_CallSubsystemMethod)

namespace ECASubsystemHelpers
{
	static TSharedPtr<FJsonObject> SubsystemToBrief(USubsystem* Subsystem)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Subsystem)
		{
			Obj->SetStringField(TEXT("class"), TEXT("None"));
			return Obj;
		}
		Obj->SetStringField(TEXT("class"),   Subsystem->GetClass()->GetName());
		Obj->SetStringField(TEXT("path"),    Subsystem->GetPathName());
		Obj->SetStringField(TEXT("package"), Subsystem->GetOutermost()->GetName());
		return Obj;
	}

	static UClass* ResolveSubsystemClass(const FString& Name)
	{
		if (Name.IsEmpty()) return nullptr;

		// Strip U prefix if user provided it
		FString Stripped = Name;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}

		const UClass* BaseClass = USubsystem::StaticClass();
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class || !Class->IsChildOf(BaseClass)) continue;
			const FString ClassName = Class->GetName();
			if (ClassName.Equals(Name, ESearchCase::IgnoreCase) ||
				ClassName.Equals(Stripped, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
		return nullptr;
	}

	static USubsystem* FindSubsystemInstance(UClass* SubsystemClass, FString& OutKind)
	{
		if (!SubsystemClass) return nullptr;

		if (SubsystemClass->IsChildOf(UEditorSubsystem::StaticClass()) && GEditor)
		{
			OutKind = TEXT("editor");
			return GEditor->GetEditorSubsystemBase(TSubclassOf<UEditorSubsystem>(SubsystemClass));
		}
		if (SubsystemClass->IsChildOf(UEngineSubsystem::StaticClass()) && GEngine)
		{
			OutKind = TEXT("engine");
			return GEngine->GetEngineSubsystemBase(TSubclassOf<UEngineSubsystem>(SubsystemClass));
		}
		if (SubsystemClass->IsChildOf(UWorldSubsystem::StaticClass()) && GEditor)
		{
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				OutKind = TEXT("world");
				return World->GetSubsystemBase(TSubclassOf<UWorldSubsystem>(SubsystemClass));
			}
		}
		OutKind = TEXT("other");
		return nullptr;
	}

	// Append a short JSON-Schema-ish string for a UFunction parameter or return type.
	static FString PropertyTypeName(const FProperty* Prop)
	{
		if (!Prop) return TEXT("void");
		return Prop->GetCPPType();
	}

	static TSharedPtr<FJsonObject> FunctionToJson(const UFunction* Func)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Func) return Obj;
		Obj->SetStringField(TEXT("name"), Func->GetName());

		const FProperty* ReturnProp = Func->GetReturnProperty();
		Obj->SetStringField(TEXT("return_type"), ReturnProp ? PropertyTypeName(ReturnProp) : TEXT("void"));

		TArray<TSharedPtr<FJsonValue>> ParamArray;
		for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (!Param || Param->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Param->GetName());
			ParamObj->SetStringField(TEXT("type"), PropertyTypeName(Param));
			ParamObj->SetBoolField  (TEXT("out_param"), Param->HasAnyPropertyFlags(CPF_OutParm));
			ParamArray.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		Obj->SetArrayField(TEXT("params"), ParamArray);
		return Obj;
	}

	static FString SimpleValueSummary(const FProperty* Prop, const void* Container)
	{
		if (!Prop || !Container) return TEXT("");
		const void* Value = Prop->ContainerPtrToValuePtr<void>(Container);
		if (!Value) return TEXT("");

		// Booleans, ints, floats, enums, names, strings — anything ExportText handles cheaply.
		if (Prop->IsA<FBoolProperty>()       ||
			Prop->IsA<FIntProperty>()        ||
			Prop->IsA<FFloatProperty>()      ||
			Prop->IsA<FDoubleProperty>()     ||
			Prop->IsA<FByteProperty>()       ||
			Prop->IsA<FEnumProperty>()       ||
			Prop->IsA<FNameProperty>()       ||
			Prop->IsA<FStrProperty>()        ||
			Prop->IsA<FTextProperty>())
		{
			FString Out;
			Prop->ExportText_Direct(Out, Value, Value, nullptr, PPF_None);
			return Out.Left(256);
		}
		return TEXT("");
	}

	static TSharedPtr<FJsonObject> PropertyToJson(const FProperty* Prop, const UObject* Subsystem)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Prop) return Obj;
		Obj->SetStringField(TEXT("name"), Prop->GetName());
		Obj->SetStringField(TEXT("type"), PropertyTypeName(Prop));
		Obj->SetStringField(TEXT("value_summary"), SimpleValueSummary(Prop, Subsystem));
		return Obj;
	}
}

//==============================================================================
// list_editor_subsystems
//==============================================================================
FECACommandResult FECACommand_ListEditorSubsystems::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor) return FECACommandResult::Error(TEXT("GEditor is not available"));

	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TArray<UEditorSubsystem*> Subsystems = GEditor->GetEditorSubsystemArrayCopy<UEditorSubsystem>();

	TArray<TSharedPtr<FJsonValue>> Out;
	for (UEditorSubsystem* Sub : Subsystems)
	{
		if (!Sub) continue;
		if (!NameFilter.IsEmpty() && !Sub->GetClass()->GetName().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Add(MakeShared<FJsonValueObject>(ECASubsystemHelpers::SubsystemToBrief(Sub)));
	}

	Out.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("class")).Compare(B->AsObject()->GetStringField(TEXT("class")), ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Out.Num());
	Result->SetArrayField (TEXT("subsystems"), Out);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// list_engine_subsystems
//==============================================================================
FECACommandResult FECACommand_ListEngineSubsystems::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEngine) return FECACommandResult::Error(TEXT("GEngine is not available"));

	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TArray<UEngineSubsystem*> Subsystems = GEngine->GetEngineSubsystemArrayCopy<UEngineSubsystem>();

	TArray<TSharedPtr<FJsonValue>> Out;
	for (UEngineSubsystem* Sub : Subsystems)
	{
		if (!Sub) continue;
		if (!NameFilter.IsEmpty() && !Sub->GetClass()->GetName().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Add(MakeShared<FJsonValueObject>(ECASubsystemHelpers::SubsystemToBrief(Sub)));
	}

	Out.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("class")).Compare(B->AsObject()->GetStringField(TEXT("class")), ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"), Out.Num());
	Result->SetArrayField (TEXT("subsystems"), Out);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_subsystem
//==============================================================================
FECACommandResult FECACommand_DumpSubsystem::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("class_name"), ClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: class_name"));
	}

	bool bIncludeInherited = false;
	GetBoolParam(Params, TEXT("include_inherited"), bIncludeInherited, false);

	UClass* SubsystemClass = ECASubsystemHelpers::ResolveSubsystemClass(ClassName);
	if (!SubsystemClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not resolve '%s' to a USubsystem subclass"), *ClassName));
	}

	FString Kind;
	USubsystem* Instance = ECASubsystemHelpers::FindSubsystemInstance(SubsystemClass, Kind);
	if (!Instance)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not find an active instance of '%s' (resolved to class %s, kind=%s)"), *ClassName, *SubsystemClass->GetName(), *Kind));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("class"), SubsystemClass->GetName());
	Result->SetStringField(TEXT("kind"),  Kind);
	Result->SetStringField(TEXT("path"),  Instance->GetPathName());

	// Functions
	TArray<TSharedPtr<FJsonValue>> Functions;
	EFieldIterationFlags FuncFlags = bIncludeInherited ? EFieldIterationFlags::IncludeSuper : EFieldIterationFlags::None;
	for (TFieldIterator<UFunction> FuncIt(SubsystemClass, FuncFlags); FuncIt; ++FuncIt)
	{
		Functions.Add(MakeShared<FJsonValueObject>(ECASubsystemHelpers::FunctionToJson(*FuncIt)));
	}
	Result->SetArrayField(TEXT("functions"), Functions);

	// Properties
	TArray<TSharedPtr<FJsonValue>> Properties;
	EFieldIterationFlags PropFlags = bIncludeInherited ? EFieldIterationFlags::IncludeSuper : EFieldIterationFlags::None;
	for (TFieldIterator<FProperty> PropIt(SubsystemClass, PropFlags); PropIt; ++PropIt)
	{
		Properties.Add(MakeShared<FJsonValueObject>(ECASubsystemHelpers::PropertyToJson(*PropIt, Instance)));
	}
	Result->SetArrayField(TEXT("properties"), Properties);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// call_subsystem_method — reflective UFUNCTION dispatch
//==============================================================================
namespace ECACallSubsystemHelpers
{
	// Pack a single FProperty's value from a JSON value into the live parameter
	// memory. Supports the simple scalar types most BlueprintCallable APIs use.
	static bool PackParam(FProperty* Prop, void* ContainerMem, const TSharedPtr<FJsonValue>& JsonVal, FString& OutError)
	{
		if (!Prop || !JsonVal.IsValid())
		{
			OutError = TEXT("Null property or JSON value");
			return false;
		}
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerMem);

		if (FBoolProperty* B = CastField<FBoolProperty>(Prop))
		{
			bool V = false;
			JsonVal->TryGetBool(V);
			B->SetPropertyValue(ValuePtr, V);
			return true;
		}
		if (FIntProperty* I = CastField<FIntProperty>(Prop))
		{
			I->SetPropertyValue(ValuePtr, (int32)JsonVal->AsNumber());
			return true;
		}
		if (FInt64Property* I64 = CastField<FInt64Property>(Prop))
		{
			I64->SetPropertyValue(ValuePtr, (int64)JsonVal->AsNumber());
			return true;
		}
		if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
		{
			F->SetPropertyValue(ValuePtr, (float)JsonVal->AsNumber());
			return true;
		}
		if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
		{
			D->SetPropertyValue(ValuePtr, JsonVal->AsNumber());
			return true;
		}
		if (FByteProperty* By = CastField<FByteProperty>(Prop))
		{
			By->SetPropertyValue(ValuePtr, (uint8)JsonVal->AsNumber());
			return true;
		}
		if (FStrProperty* S = CastField<FStrProperty>(Prop))
		{
			S->SetPropertyValue(ValuePtr, JsonVal->AsString());
			return true;
		}
		if (FNameProperty* N = CastField<FNameProperty>(Prop))
		{
			N->SetPropertyValue(ValuePtr, FName(*JsonVal->AsString()));
			return true;
		}
		if (FTextProperty* T = CastField<FTextProperty>(Prop))
		{
			T->SetPropertyValue(ValuePtr, FText::FromString(JsonVal->AsString()));
			return true;
		}
		if (FEnumProperty* E = CastField<FEnumProperty>(Prop))
		{
			if (UEnum* EnumType = E->GetEnum())
			{
				const FString S = JsonVal->AsString();
				int64 V = EnumType->GetValueByNameString(S, EGetByNameFlags::CaseSensitive);
				if (V == INDEX_NONE) V = (int64)JsonVal->AsNumber();
				E->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, V);
			}
			return true;
		}
		if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(Prop))
		{
			const FString Path = JsonVal->AsString();
			UObject* Ref = nullptr;
			if (!Path.IsEmpty())
			{
				Ref = LoadObject<UObject>(nullptr, *Path);
			}
			O->SetObjectPropertyValue(ValuePtr, Ref);
			return true;
		}

		OutError = FString::Printf(TEXT("Unsupported parameter type for '%s' (%s)"), *Prop->GetName(), *Prop->GetCPPType());
		return false;
	}

	// Encode a single live parameter value back into JSON. Used for return values
	// and OutParm parameters after ProcessEvent.
	static TSharedPtr<FJsonValue> UnpackParam(FProperty* Prop, void* ContainerMem)
	{
		if (!Prop) return MakeShared<FJsonValueNull>();
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerMem);

		if (FBoolProperty* B = CastField<FBoolProperty>(Prop))      return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(ValuePtr));
		if (FIntProperty* I = CastField<FIntProperty>(Prop))         return MakeShared<FJsonValueNumber>(I->GetPropertyValue(ValuePtr));
		if (FInt64Property* I64 = CastField<FInt64Property>(Prop))   return MakeShared<FJsonValueNumber>((double)I64->GetPropertyValue(ValuePtr));
		if (FFloatProperty* F = CastField<FFloatProperty>(Prop))     return MakeShared<FJsonValueNumber>(F->GetPropertyValue(ValuePtr));
		if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))   return MakeShared<FJsonValueNumber>(D->GetPropertyValue(ValuePtr));
		if (FByteProperty* By = CastField<FByteProperty>(Prop))      return MakeShared<FJsonValueNumber>(By->GetPropertyValue(ValuePtr));
		if (FStrProperty* S = CastField<FStrProperty>(Prop))         return MakeShared<FJsonValueString>(S->GetPropertyValue(ValuePtr));
		if (FNameProperty* N = CastField<FNameProperty>(Prop))       return MakeShared<FJsonValueString>(N->GetPropertyValue(ValuePtr).ToString());
		if (FTextProperty* T = CastField<FTextProperty>(Prop))       return MakeShared<FJsonValueString>(T->GetPropertyValue(ValuePtr).ToString());
		if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(Prop))
		{
			UObject* Ref = O->GetObjectPropertyValue(ValuePtr);
			return MakeShared<FJsonValueString>(Ref ? Ref->GetPathName() : FString());
		}
		FString Exported;
		Prop->ExportText_Direct(Exported, ValuePtr, ValuePtr, nullptr, PPF_None);
		return MakeShared<FJsonValueString>(Exported.Left(512));
	}
}

FECACommandResult FECACommand_CallSubsystemMethod::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName, FunctionName;
	if (!GetStringParam(Params, TEXT("class_name"), ClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: class_name"));
	}
	if (!GetStringParam(Params, TEXT("function_name"), FunctionName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: function_name"));
	}

	UClass* SubsystemClass = ECASubsystemHelpers::ResolveSubsystemClass(ClassName);
	if (!SubsystemClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not resolve '%s' to a USubsystem subclass"), *ClassName));
	}

	FString Kind;
	USubsystem* Instance = ECASubsystemHelpers::FindSubsystemInstance(SubsystemClass, Kind);
	if (!Instance)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not find an active instance of '%s'"), *ClassName));
	}

	// Function lookup: try the resolved class first, then walk up via FindFunction
	// (which already considers inheritance).
	UFunction* Func = Instance->FindFunction(FName(*FunctionName));
	if (!Func)
	{
		// Fall back to case-insensitive scan
		for (TFieldIterator<UFunction> It(SubsystemClass, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			if (It->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				Func = *It;
				break;
			}
		}
	}
	if (!Func)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Function '%s' not found on '%s'"), *FunctionName, *SubsystemClass->GetName()));
	}

	// Allocate parameter memory + initialize each param
	uint8* ParamMem = (uint8*)FMemory_Alloca(Func->ParmsSize);
	FMemory::Memzero(ParamMem, Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		It->InitializeValue_InContainer(ParamMem);
	}

	// Pack args
	const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
	const bool bHasArgs = Params.IsValid() && Params->TryGetObjectField(TEXT("args"), ArgsObj) && ArgsObj && (*ArgsObj).IsValid();

	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		FProperty* P = *It;
		if (P->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		if (P->HasAnyPropertyFlags(CPF_OutParm) && !P->HasAnyPropertyFlags(CPF_ReferenceParm)) continue; // pure out-param; no input

		if (!bHasArgs) continue;
		TSharedPtr<FJsonValue> ArgVal = (*ArgsObj)->TryGetField(P->GetName());
		if (!ArgVal.IsValid()) continue;

		FString Err;
		if (!ECACallSubsystemHelpers::PackParam(P, ParamMem, ArgVal, Err))
		{
			// Destroy what we initialized so we don't leak shared resources
			for (TFieldIterator<FProperty> Dest(Func); Dest; ++Dest) Dest->DestroyValue_InContainer(ParamMem);
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to pack parameter '%s': %s"), *P->GetName(), *Err));
		}
	}

	// Call
	Instance->ProcessEvent(Func, ParamMem);

	// Unpack return + out-params
	TSharedPtr<FJsonObject> Returns = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		FProperty* P = *It;
		if (P->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Returns->SetField(TEXT("value"), ECACallSubsystemHelpers::UnpackParam(P, ParamMem));
		}
		else if (P->HasAnyPropertyFlags(CPF_OutParm))
		{
			Returns->SetField(P->GetName(), ECACallSubsystemHelpers::UnpackParam(P, ParamMem));
		}
	}

	// Destroy live values
	for (TFieldIterator<FProperty> It(Func); It; ++It) It->DestroyValue_InContainer(ParamMem);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("class"),    SubsystemClass->GetName());
	Result->SetStringField(TEXT("function"), Func->GetName());
	Result->SetObjectField(TEXT("return"),   Returns);
	return FECACommandResult::Success(Result);
}
