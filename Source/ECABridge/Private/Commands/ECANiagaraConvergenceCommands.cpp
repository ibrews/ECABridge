// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECANiagaraConvergenceCommands.h"
#include "Commands/ECACommand.h"

#if WITH_ECA_NIAGARA

// Engine
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "Misc/EngineVersionComparison.h"

// Niagara
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraDataInterface.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "EdGraphSchema_Niagara.h"

// ============================================================================
// Helpers (file-scope, anon)
// ============================================================================

namespace ECANiagaraConvergence
{
	static UNiagaraSystem* ResolveSystem(const FString& SystemPath)
	{
		return LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	}

	static ENiagaraScriptUsage ParseUsage(const FString& UsageStr)
	{
		if (UsageStr.Equals(TEXT("emitter_spawn"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::EmitterSpawnScript;
		if (UsageStr.Equals(TEXT("emitter_update"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::EmitterUpdateScript;
		if (UsageStr.Equals(TEXT("particle_spawn"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::ParticleSpawnScript;
		if (UsageStr.Equals(TEXT("particle_update"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::ParticleUpdateScript;
		if (UsageStr.Equals(TEXT("system_spawn"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::SystemSpawnScript;
		if (UsageStr.Equals(TEXT("system_update"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::SystemUpdateScript;
		if (UsageStr.Equals(TEXT("particle_event_handler"), ESearchCase::IgnoreCase)) return ENiagaraScriptUsage::ParticleEventScript;
		return ENiagaraScriptUsage::ParticleUpdateScript;
	}

	static FString UsageToString(ENiagaraScriptUsage Usage)
	{
		switch (Usage)
		{
			case ENiagaraScriptUsage::SystemSpawnScript: return TEXT("system_spawn");
			case ENiagaraScriptUsage::SystemUpdateScript: return TEXT("system_update");
			case ENiagaraScriptUsage::EmitterSpawnScript: return TEXT("emitter_spawn");
			case ENiagaraScriptUsage::EmitterUpdateScript: return TEXT("emitter_update");
			case ENiagaraScriptUsage::ParticleSpawnScript: return TEXT("particle_spawn");
			case ENiagaraScriptUsage::ParticleUpdateScript: return TEXT("particle_update");
			case ENiagaraScriptUsage::ParticleEventScript: return TEXT("particle_event_handler");
			default: return TEXT("unknown");
		}
	}

	static FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& EmitterName, int32* OutIndex = nullptr)
	{
		if (!System) return nullptr;
		TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
		if (EmitterName.StartsWith(TEXT("Emitter[")) && EmitterName.EndsWith(TEXT("]")))
		{
			FString IndexStr = EmitterName.Mid(8, EmitterName.Len() - 9);
			int32 Index = FCString::Atoi(*IndexStr);
			if (Index >= 0 && Index < Handles.Num())
			{
				if (OutIndex) *OutIndex = Index;
				return &Handles[Index];
			}
			return nullptr;
		}
		for (int32 i = 0; i < Handles.Num(); ++i)
		{
			FNiagaraEmitterHandle& H = Handles[i];
			if (H.GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase) ||
				H.GetUniqueInstanceName().Equals(EmitterName, ESearchCase::IgnoreCase))
			{
				if (OutIndex) *OutIndex = i;
				return &H;
			}
		}
		return nullptr;
	}

	// Map a short or full class name to a UClass that descends from BaseClass.
	// Recognizes a handful of friendly keywords for renderer classes.
	static UClass* ResolveClassByName(const FString& InName, UClass* BaseClass)
	{
		FString Name = InName.TrimStartAndEnd();
		if (Name.IsEmpty() || !BaseClass) return nullptr;

		// Friendly aliases for renderers — keep stable across engine versions.
		if (BaseClass->GetName().Contains(TEXT("RendererProperties")))
		{
			if (Name.Equals(TEXT("sprite"), ESearchCase::IgnoreCase)) Name = TEXT("NiagaraSpriteRendererProperties");
			else if (Name.Equals(TEXT("mesh"), ESearchCase::IgnoreCase)) Name = TEXT("NiagaraMeshRendererProperties");
			else if (Name.Equals(TEXT("ribbon"), ESearchCase::IgnoreCase)) Name = TEXT("NiagaraRibbonRendererProperties");
			else if (Name.Equals(TEXT("light"), ESearchCase::IgnoreCase)) Name = TEXT("NiagaraLightRendererProperties");
			else if (Name.Equals(TEXT("component"), ESearchCase::IgnoreCase)) Name = TEXT("NiagaraComponentRendererProperties");
		}

		// Allow path forms ('/Script/Niagara.NiagaraSpriteRendererProperties') and bare names.
		UClass* Found = nullptr;
		if (Name.Contains(TEXT(".")))
		{
			Found = LoadObject<UClass>(nullptr, *Name);
		}
		else
		{
			Found = FindFirstObjectSafe<UClass>(*Name);
		}
		if (!Found) return nullptr;
		if (!Found->IsChildOf(BaseClass)) return nullptr;
		return Found;
	}

	// Should this FProperty appear in the editable schema?
	// Mirrors the details-panel filter: visible-in-editor and not transient.
	static bool IsEditableForSchema(const FProperty* Prop)
	{
		if (!Prop) return false;
		const uint64 Flags = Prop->GetPropertyFlags();
		if (Flags & CPF_Transient) return false;
		if (Flags & CPF_DisableEditOnInstance) {} // still include — details panel still shows on archetype
		const bool bEditOrVisible = (Flags & (CPF_Edit | CPF_BlueprintVisible)) != 0;
		if (!bEditOrVisible) return false;
		if (Prop->HasMetaData(TEXT("DeprecatedProperty"))) return false;
		return true;
	}

	// Best-effort JSON type tag for an FProperty (the schema's "type" column).
	// Falls back to GetCPPType() string when the property is a UObject ref or
	// a custom struct.
	static FString JsonTypeForProperty(const FProperty* Prop)
	{
		if (Prop->IsA<FBoolProperty>()) return TEXT("boolean");
		if (Prop->IsA<FIntProperty>() || Prop->IsA<FInt64Property>() || Prop->IsA<FUInt32Property>() ||
			Prop->IsA<FByteProperty>() || Prop->IsA<FInt16Property>() || Prop->IsA<FInt8Property>() ||
			Prop->IsA<FUInt16Property>() || Prop->IsA<FUInt64Property>()) return TEXT("integer");
		if (Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>()) return TEXT("number");
		if (Prop->IsA<FStrProperty>() || Prop->IsA<FNameProperty>() || Prop->IsA<FTextProperty>()) return TEXT("string");
		if (Prop->IsA<FEnumProperty>() || (Prop->IsA<FByteProperty>() && CastField<const FByteProperty>(Prop)->Enum)) return TEXT("string");
		if (Prop->IsA<FArrayProperty>() || Prop->IsA<FSetProperty>()) return TEXT("array");
		if (Prop->IsA<FStructProperty>() || Prop->IsA<FMapProperty>() || Prop->IsA<FObjectPropertyBase>()) return TEXT("object");
		return Prop->GetCPPType();
	}

	// Best-effort default-value string. For schema documentation only — we read
	// from the class CDO (or the supplied container's archetype) and export as
	// the property's ExportText form.
	static FString DefaultValueForProperty(const FProperty* Prop, const void* ContainerPtr)
	{
		if (!Prop || !ContainerPtr) return FString();
		FString Out;
		Prop->ExportText_InContainer(0, Out, ContainerPtr, ContainerPtr, nullptr, PPF_None);
		return Out;
	}

	// Build a {properties:[{name,type,description,default}]} schema for any UClass.
	// SourceContainer should be either a class CDO or a concrete instance — used
	// for the "default" column. Pass nullptr to skip defaults.
	static TSharedPtr<FJsonObject> BuildClassSchema(UClass* Class, const void* SourceContainer)
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("class"), Class ? Class->GetName() : FString());
		TArray<TSharedPtr<FJsonValue>> PropsArr;
		if (Class)
		{
			for (TFieldIterator<FProperty> It(Class); It; ++It)
			{
				FProperty* P = *It;
				if (!IsEditableForSchema(P)) continue;
				TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
				PObj->SetStringField(TEXT("name"), P->GetName());
				PObj->SetStringField(TEXT("type"), JsonTypeForProperty(P));
				PObj->SetStringField(TEXT("cpp_type"), P->GetCPPType());
				PObj->SetStringField(TEXT("description"), P->GetMetaData(TEXT("ToolTip")));
				const FString Category = P->GetMetaData(TEXT("Category"));
				if (!Category.IsEmpty()) PObj->SetStringField(TEXT("category"), Category);
				if (SourceContainer)
				{
					PObj->SetStringField(TEXT("default"), DefaultValueForProperty(P, SourceContainer));
				}
				PropsArr.Add(MakeShared<FJsonValueObject>(PObj));
			}
		}
		Root->SetArrayField(TEXT("properties"), PropsArr);
		return Root;
	}

	// Serialize a UObject's editable properties to JSON. Uses FJsonObjectConverter
	// with CPF_Edit|CPF_BlueprintVisible as the include mask.
	static TSharedPtr<FJsonObject> SerializeContainerToJson(UStruct* Struct, const void* ContainerPtr)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		if (Struct && ContainerPtr)
		{
			const int64 IncludeFlags = (int64)(CPF_Edit | CPF_BlueprintVisible);
			const int64 SkipFlags = (int64)(CPF_Transient | CPF_Deprecated);
			FJsonObjectConverter::UStructToJsonObject(Struct, ContainerPtr, Out.ToSharedRef(), IncludeFlags, SkipFlags);
		}
		return Out;
	}

	// Apply a partial-update JSON map to a UObject's properties. Returns the
	// list of property names that were actually written, and the list of skipped
	// names (unknown / not-editable / type-coerce-failed).
	static void ApplyPartialUpdate(UStruct* Struct, void* ContainerPtr,
		const TSharedPtr<FJsonObject>& Props,
		TArray<FString>& OutUpdated, TArray<TPair<FString, FString>>& OutSkipped)
	{
		if (!Struct || !ContainerPtr || !Props.IsValid()) return;
		for (const auto& KV : Props->Values)
		{
			// Copy the key by value: in UE 5.8 FJsonObject::Values uses a non-FString
			// key type (FStringView-like), so binding a const FString& triggers C2440.
			const FString Key(KV.Key);
			const TSharedPtr<FJsonValue>& Val = KV.Value;
			FProperty* P = Struct->FindPropertyByName(*Key);
			if (!P)
			{
				OutSkipped.Add({ Key, TEXT("property not found") });
				continue;
			}
			if (!IsEditableForSchema(P))
			{
				OutSkipped.Add({ Key, TEXT("property not editable") });
				continue;
			}
			void* PropAddr = P->ContainerPtrToValuePtr<void>(ContainerPtr);
			if (!FJsonObjectConverter::JsonValueToUProperty(Val, P, PropAddr, 0, CPF_Transient))
			{
				OutSkipped.Add({ Key, TEXT("type coerce failed") });
				continue;
			}
			OutUpdated.Add(Key);
		}
	}

	// Find the function-call node matching ModuleName under ScriptUsage. Returns
	// nullptr if missing. ScriptSource & Graph must be valid.
	static UNiagaraNodeFunctionCall* FindModuleNode(UNiagaraGraph* Graph, const FString& ModuleName, ENiagaraScriptUsage Usage)
	{
		if (!Graph) return nullptr;
		TArray<UNiagaraNodeFunctionCall*> Nodes;
		Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(Nodes);
		for (UNiagaraNodeFunctionCall* N : Nodes)
		{
			if (!N) continue;
			if (!N->GetFunctionName().Equals(ModuleName, ESearchCase::IgnoreCase)) continue;
			if (FNiagaraStackGraphUtilities::GetOutputNodeUsage(*N) == Usage) return N;
		}
		return nullptr;
	}
}

using namespace ECANiagaraConvergence;

// ============================================================================
// Registration
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_GetNiagaraSystemSchema)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraEmitterSchema)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraRendererSchema)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraDataInterfaceSchema)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraModuleSchemaFromAsset)

// ============================================================================
// Task 1 — Schemas
// ============================================================================

FECACommandResult FECACommand_GetNiagaraSystemSchema::Execute(const TSharedPtr<FJsonObject>& /*Params*/)
{
	UClass* Cls = UNiagaraSystem::StaticClass();
	const UObject* CDO = Cls->GetDefaultObject();
	TSharedPtr<FJsonObject> Schema = BuildClassSchema(Cls, CDO);
	return FECACommandResult::Success(Schema);
}

FECACommandResult FECACommand_GetNiagaraEmitterSchema::Execute(const TSharedPtr<FJsonObject>& /*Params*/)
{
	// FVersionedNiagaraEmitterData carries the per-version emitter properties;
	// it is a USTRUCT, so we synthesize one on the stack to read its defaults.
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	{
		FVersionedNiagaraEmitterData DataDefault;
		UScriptStruct* EmitterDataStruct = FVersionedNiagaraEmitterData::StaticStruct();
		TSharedPtr<FJsonObject> EmitterDataSchema = BuildClassSchema(nullptr, nullptr);
		// BuildClassSchema only walks UClass — handle the UScriptStruct path inline.
		EmitterDataSchema->SetStringField(TEXT("class"), EmitterDataStruct->GetName());
		TArray<TSharedPtr<FJsonValue>> Props;
		for (TFieldIterator<FProperty> It(EmitterDataStruct); It; ++It)
		{
			FProperty* P = *It;
			if (!IsEditableForSchema(P)) continue;
			TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
			PObj->SetStringField(TEXT("name"), P->GetName());
			PObj->SetStringField(TEXT("type"), JsonTypeForProperty(P));
			PObj->SetStringField(TEXT("cpp_type"), P->GetCPPType());
			PObj->SetStringField(TEXT("description"), P->GetMetaData(TEXT("ToolTip")));
			PObj->SetStringField(TEXT("default"), DefaultValueForProperty(P, &DataDefault));
			Props.Add(MakeShared<FJsonValueObject>(PObj));
		}
		EmitterDataSchema->SetArrayField(TEXT("properties"), Props);
		Root->SetObjectField(TEXT("emitter_data"), EmitterDataSchema);
	}

	{
		UClass* EmitterClass = UNiagaraEmitter::StaticClass();
		TSharedPtr<FJsonObject> EmitterSchema = BuildClassSchema(EmitterClass, EmitterClass->GetDefaultObject());
		Root->SetObjectField(TEXT("emitter_object"), EmitterSchema);
	}

	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraRendererSchema::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("renderer_class"), ClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: renderer_class"));
	}
	UClass* Cls = ResolveClassByName(ClassName, UNiagaraRendererProperties::StaticClass());
	if (!Cls)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("renderer_class '%s' could not be resolved to a subclass of UNiagaraRendererProperties. Try sprite/mesh/ribbon/light/component or a full class name."),
			*ClassName));
	}
	TSharedPtr<FJsonObject> Out = BuildClassSchema(Cls, Cls->GetDefaultObject());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetNiagaraDataInterfaceSchema::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("data_interface_class"), ClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: data_interface_class"));
	}
	UClass* Cls = ResolveClassByName(ClassName, UNiagaraDataInterface::StaticClass());
	if (!Cls)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("data_interface_class '%s' is not a UNiagaraDataInterface subclass."),
			*ClassName));
	}
	TSharedPtr<FJsonObject> Out = BuildClassSchema(Cls, Cls->GetDefaultObject());
	return FECACommandResult::Success(Out);
}

FECACommandResult FECACommand_GetNiagaraModuleSchemaFromAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ModulePath;
	if (!GetStringParam(Params, TEXT("module_script_path"), ModulePath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: module_script_path"));
	}
	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
	if (!ModuleScript)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load module script: %s"), *ModulePath));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("module_script_path"), ModulePath);
	Root->SetStringField(TEXT("script_name"), ModuleScript->GetName());

	TArray<TSharedPtr<FJsonValue>> Inputs;

	// Walk the module's parameter store for input variables.
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(ModuleScript->GetLatestSource());
	if (Source && Source->NodeGraph)
	{
		UNiagaraGraph* Graph = Source->NodeGraph;
		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);
		for (UNiagaraNodeOutput* OutNode : OutputNodes)
		{
			if (!OutNode) continue;
			for (const FNiagaraVariable& Var : OutNode->Outputs)
			{
				TSharedPtr<FJsonObject> InObj = MakeShared<FJsonObject>();
				InObj->SetStringField(TEXT("name"), Var.GetName().ToString());
				InObj->SetStringField(TEXT("type"), Var.GetType().GetName());
				InObj->SetStringField(TEXT("default"), FString());
				Inputs.Add(MakeShared<FJsonValueObject>(InObj));
			}
		}

		// Also list ScriptVariables on the graph (more complete for input metadata).
		const TMap<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& AllVars = Graph->GetAllMetaData();
		for (const auto& KV : AllVars)
		{
			UNiagaraScriptVariable* SV = KV.Value;
			if (!SV) continue;
			TSharedPtr<FJsonObject> InObj = MakeShared<FJsonObject>();
			InObj->SetStringField(TEXT("name"), SV->Variable.GetName().ToString());
			InObj->SetStringField(TEXT("type"), SV->Variable.GetType().GetName());
			InObj->SetBoolField(TEXT("is_static_switch"), SV->GetIsStaticSwitch());
			Inputs.Add(MakeShared<FJsonValueObject>(InObj));
		}
	}

	Root->SetArrayField(TEXT("inputs"), Inputs);
	Root->SetNumberField(TEXT("count"), Inputs.Num());
	return FECACommandResult::Success(Root);
}

#endif // WITH_ECA_NIAGARA
