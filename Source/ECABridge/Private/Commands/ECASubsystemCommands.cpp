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
