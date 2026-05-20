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

// ============================================================================
// Task 2 — Topology getters
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_GetNiagaraSystemTopology)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraEmitterTopology)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraScriptStackTopology)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraModuleTopology)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraStackInputTopology)

namespace ECANiagaraConvergence
{
	// Build the list of {script_usage, script_name} pairs an emitter exposes.
	// On 5.7/5.8 each emitter holds: spawn/update + per-stage particle scripts +
	// optional event handlers. We surface the four canonical scripts plus a slot
	// for the system spawn/update scripts (these live on the system, not the emitter).
	static TArray<ENiagaraScriptUsage> EmitterScriptUsages()
	{
		return {
			ENiagaraScriptUsage::EmitterSpawnScript,
			ENiagaraScriptUsage::EmitterUpdateScript,
			ENiagaraScriptUsage::ParticleSpawnScript,
			ENiagaraScriptUsage::ParticleUpdateScript,
		};
	}

	static TArray<UNiagaraNodeFunctionCall*> CollectModulesForUsage(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
	{
		TArray<UNiagaraNodeFunctionCall*> Modules;
		if (!Graph) return Modules;
		TArray<UNiagaraNodeFunctionCall*> All;
		Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(All);
		for (UNiagaraNodeFunctionCall* N : All)
		{
			if (!N) continue;
			if (FNiagaraStackGraphUtilities::GetOutputNodeUsage(*N) == Usage)
			{
				Modules.Add(N);
			}
		}
		return Modules;
	}

	// Build the {name, script, module_name} JSON for a function-call node, plus
	// its enabled state. Used by topology and module_enabled commands.
	static TSharedPtr<FJsonObject> ModuleNodeToJson(UNiagaraNodeFunctionCall* Node, ENiagaraScriptUsage Usage)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		if (!Node) return O;
		O->SetStringField(TEXT("module_name"), Node->GetFunctionName());
		O->SetStringField(TEXT("script"), UsageToString(Usage));
		O->SetBoolField(TEXT("enabled"), Node->GetDesiredEnabledState() == ENodeEnabledState::Enabled);
		if (UNiagaraScript* FuncScript = Node->FunctionScript)
		{
			O->SetStringField(TEXT("module_script_path"), FuncScript->GetPathName());
		}
		return O;
	}

	// Walk a system + emitter to find the graph hosting the named module.
	// Returns the function-call node + the script usage matched.
	static UNiagaraNodeFunctionCall* FindModuleAcrossUsages(
		UNiagaraSystem* System, FNiagaraEmitterHandle* Handle, const FString& ModuleName,
		ENiagaraScriptUsage Usage, UNiagaraGraph** OutGraph)
	{
		if (!System || !Handle) return nullptr;
		FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
		if (!Data) return nullptr;
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
		if (!Source || !Source->NodeGraph) return nullptr;
		UNiagaraGraph* Graph = Source->NodeGraph;
		UNiagaraNodeFunctionCall* N = FindModuleNode(Graph, ModuleName, Usage);
		if (N && OutGraph) *OutGraph = Graph;
		return N;
	}
}

FECACommandResult FECACommand_GetNiagaraSystemTopology::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	}
	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);

	TArray<TSharedPtr<FJsonValue>> EmittersArr;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		TSharedPtr<FJsonObject> EObj = MakeShared<FJsonObject>();
		EObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
		EObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (Data)
		{
			UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
			UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;

			TArray<TSharedPtr<FJsonValue>> ScriptsArr;
			for (ENiagaraScriptUsage U : EmitterScriptUsages())
			{
				TSharedPtr<FJsonObject> SObj = MakeShared<FJsonObject>();
				SObj->SetStringField(TEXT("script"), UsageToString(U));
				int32 ModuleCount = 0;
				if (Graph)
				{
					for (UNiagaraNodeFunctionCall* M : CollectModulesForUsage(Graph, U))
					{
						ModuleCount += M ? 1 : 0;
					}
				}
				SObj->SetNumberField(TEXT("module_count"), ModuleCount);
				ScriptsArr.Add(MakeShared<FJsonValueObject>(SObj));
			}
			EObj->SetArrayField(TEXT("scripts"), ScriptsArr);

			TArray<TSharedPtr<FJsonValue>> RenderersArr;
			int32 RIndex = 0;
			for (UNiagaraRendererProperties* R : Data->GetRenderers())
			{
				TSharedPtr<FJsonObject> RObj = MakeShared<FJsonObject>();
				RObj->SetNumberField(TEXT("index"), RIndex++);
				RObj->SetStringField(TEXT("class"), R ? R->GetClass()->GetName() : TEXT("None"));
				RObj->SetBoolField(TEXT("enabled"), R ? R->GetIsEnabled() : false);
				RenderersArr.Add(MakeShared<FJsonValueObject>(RObj));
			}
			EObj->SetArrayField(TEXT("renderers"), RenderersArr);
		}
		EmittersArr.Add(MakeShared<FJsonValueObject>(EObj));
	}
	Root->SetArrayField(TEXT("emitters"), EmittersArr);
	Root->SetNumberField(TEXT("emitter_count"), EmittersArr.Num());
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraEmitterTopology::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
	UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetBoolField(TEXT("enabled"), Handle->GetIsEnabled());
	Root->SetStringField(TEXT("sim_target"),
		Data->SimTarget == ENiagaraSimTarget::CPUSim ? TEXT("cpu") : TEXT("gpu"));

	TArray<TSharedPtr<FJsonValue>> ScriptsArr;
	for (ENiagaraScriptUsage U : EmitterScriptUsages())
	{
		TSharedPtr<FJsonObject> SObj = MakeShared<FJsonObject>();
		SObj->SetStringField(TEXT("script"), UsageToString(U));
		TArray<TSharedPtr<FJsonValue>> Modules;
		for (UNiagaraNodeFunctionCall* N : CollectModulesForUsage(Graph, U))
		{
			Modules.Add(MakeShared<FJsonValueObject>(ModuleNodeToJson(N, U)));
		}
		SObj->SetArrayField(TEXT("modules"), Modules);
		ScriptsArr.Add(MakeShared<FJsonValueObject>(SObj));
	}
	Root->SetArrayField(TEXT("scripts"), ScriptsArr);

	TArray<TSharedPtr<FJsonValue>> RenderersArr;
	int32 RIndex = 0;
	for (UNiagaraRendererProperties* R : Data->GetRenderers())
	{
		TSharedPtr<FJsonObject> RObj = MakeShared<FJsonObject>();
		RObj->SetNumberField(TEXT("index"), RIndex++);
		RObj->SetStringField(TEXT("class"), R ? R->GetClass()->GetName() : TEXT("None"));
		RObj->SetBoolField(TEXT("enabled"), R ? R->GetIsEnabled() : false);
		RenderersArr.Add(MakeShared<FJsonValueObject>(RObj));
	}
	Root->SetArrayField(TEXT("renderers"), RenderersArr);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraScriptStackTopology::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName, ScriptStr;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetStringParam(Params, TEXT("script"), ScriptStr)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: script"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
	UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
	if (!Graph) return FECACommandResult::Error(TEXT("Emitter has no script graph"));

	const ENiagaraScriptUsage Usage = ParseUsage(ScriptStr);
	TArray<TSharedPtr<FJsonValue>> Modules;
	for (UNiagaraNodeFunctionCall* N : CollectModulesForUsage(Graph, Usage))
	{
		Modules.Add(MakeShared<FJsonValueObject>(ModuleNodeToJson(N, Usage)));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetStringField(TEXT("script"), UsageToString(Usage));
	Root->SetArrayField(TEXT("modules"), Modules);
	Root->SetNumberField(TEXT("module_count"), Modules.Num());
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraModuleTopology::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName, ScriptStr, ModuleName;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetStringParam(Params, TEXT("script"), ScriptStr)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: script"));
	if (!GetStringParam(Params, TEXT("module_name"), ModuleName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: module_name"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	const ENiagaraScriptUsage Usage = ParseUsage(ScriptStr);
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeFunctionCall* Node = FindModuleAcrossUsages(System, Handle, ModuleName, Usage, &Graph);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("Module '%s' not found in script '%s'"), *ModuleName, *ScriptStr));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetStringField(TEXT("script"), UsageToString(Usage));
	Root->SetStringField(TEXT("module_name"), ModuleName);
	Root->SetBoolField(TEXT("enabled"), Node->GetDesiredEnabledState() == ENodeEnabledState::Enabled);
	if (Node->FunctionScript) Root->SetStringField(TEXT("module_script_path"), Node->FunctionScript->GetPathName());

	// Enumerate user-facing inputs and current override-pin values.
	FCompileConstantResolver Resolver(Handle->GetInstance(), Usage);
	TArray<FNiagaraVariable> InputVars;
	TSet<FNiagaraVariable> Hidden;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Node, InputVars, Hidden, Resolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
		/*bIgnoreDisabled=*/false);

	TArray<TSharedPtr<FJsonValue>> Inputs;
	for (const FNiagaraVariable& V : InputVars)
	{
		TSharedPtr<FJsonObject> IObj = MakeShared<FJsonObject>();
		IObj->SetStringField(TEXT("name"), V.GetName().ToString());
		IObj->SetStringField(TEXT("type"), V.GetType().GetName());
		IObj->SetBoolField(TEXT("hidden"), Hidden.Contains(V));
		Inputs.Add(MakeShared<FJsonValueObject>(IObj));
	}
	Root->SetArrayField(TEXT("inputs"), Inputs);
	Root->SetNumberField(TEXT("input_count"), Inputs.Num());
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraStackInputTopology::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName, ScriptStr, ModuleName, InputName;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetStringParam(Params, TEXT("script"), ScriptStr)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: script"));
	if (!GetStringParam(Params, TEXT("module_name"), ModuleName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: module_name"));
	if (!GetStringParam(Params, TEXT("input_name"), InputName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: input_name"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	const ENiagaraScriptUsage Usage = ParseUsage(ScriptStr);
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeFunctionCall* Node = FindModuleAcrossUsages(System, Handle, ModuleName, Usage, &Graph);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("Module '%s' not found in script '%s'"), *ModuleName, *ScriptStr));

	FCompileConstantResolver Resolver(Handle->GetInstance(), Usage);
	TArray<FNiagaraVariable> InputVars;
	TSet<FNiagaraVariable> Hidden;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Node, InputVars, Hidden, Resolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
		/*bIgnoreDisabled=*/false);
	const FNiagaraVariable* Match = nullptr;
	for (const FNiagaraVariable& V : InputVars)
	{
		const FString Full = V.GetName().ToString();
		FString Short = Full; int32 D; if (Full.FindLastChar('.', D)) Short = Full.Mid(D + 1);
		if (Full.Equals(InputName, ESearchCase::IgnoreCase) || Short.Equals(InputName, ESearchCase::IgnoreCase))
		{
			Match = &V; break;
		}
	}
	if (!Match) return FECACommandResult::Error(FString::Printf(TEXT("Input '%s' not found on module '%s'"), *InputName, *ModuleName));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetStringField(TEXT("script"), UsageToString(Usage));
	Root->SetStringField(TEXT("module_name"), ModuleName);
	Root->SetStringField(TEXT("input_name"), Match->GetName().ToString());
	Root->SetStringField(TEXT("input_type"), Match->GetType().GetName());

	// Override-pin inspection (the dynamic-input substructure spec mentions) is
	// not exposed in the read-only Niagara public DLL surface — only the
	// create-if-missing variant is exported (FNiagaraStackGraphUtilities::
	// GetOrCreateStackFunctionInputOverridePin) and we don't want a side-effect
	// in a topology getter. Future batch can wire it in once a non-mutating
	// path is plumbed through NiagaraEditor.
	Root->SetBoolField(TEXT("has_override_inspection"), false);
	return FECACommandResult::Success(Root);
}

// ============================================================================
// Task 3 — Data read+set (8 cmds)
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_GetNiagaraSystemData)
REGISTER_ECA_COMMAND(FECACommand_SetNiagaraSystemData)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraEmitterData)
REGISTER_ECA_COMMAND(FECACommand_SetNiagaraEmitterData)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraRendererData)
REGISTER_ECA_COMMAND(FECACommand_SetNiagaraRendererData)
REGISTER_ECA_COMMAND(FECACommand_SetNiagaraModuleEnabled)
REGISTER_ECA_COMMAND(FECACommand_GetNiagaraUserVariables)

FECACommandResult FECACommand_GetNiagaraSystemData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetObjectField(TEXT("properties"), SerializeContainerToJson(UNiagaraSystem::StaticClass(), System));
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_SetNiagaraSystemData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (!GetObjectParam(Params, TEXT("properties"), PropsObj) || !PropsObj || !PropsObj->IsValid())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: properties (object)"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "SetNiagaraSystemData", "Set Niagara System Data"));
	System->Modify();

	TArray<FString> Updated;
	TArray<TPair<FString, FString>> Skipped;
	ApplyPartialUpdate(UNiagaraSystem::StaticClass(), System, *PropsObj, Updated, Skipped);

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	TArray<TSharedPtr<FJsonValue>> UArr; for (const FString& U : Updated) UArr.Add(MakeShared<FJsonValueString>(U));
	Root->SetArrayField(TEXT("updated"), UArr);
	TArray<TSharedPtr<FJsonValue>> SArr; for (const auto& S : Skipped)
	{
		TSharedPtr<FJsonObject> SO = MakeShared<FJsonObject>();
		SO->SetStringField(TEXT("name"), S.Key);
		SO->SetStringField(TEXT("reason"), S.Value);
		SArr.Add(MakeShared<FJsonValueObject>(SO));
	}
	Root->SetArrayField(TEXT("skipped"), SArr);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraEmitterData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetObjectField(TEXT("properties"), SerializeContainerToJson(FVersionedNiagaraEmitterData::StaticStruct(), Data));
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_SetNiagaraEmitterData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (!GetObjectParam(Params, TEXT("properties"), PropsObj) || !PropsObj || !PropsObj->IsValid())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: properties (object)"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "SetNiagaraEmitterData", "Set Niagara Emitter Data"));
	System->Modify();

	TArray<FString> Updated;
	TArray<TPair<FString, FString>> Skipped;
	ApplyPartialUpdate(FVersionedNiagaraEmitterData::StaticStruct(), Data, *PropsObj, Updated, Skipped);

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	TArray<TSharedPtr<FJsonValue>> UArr; for (const FString& U : Updated) UArr.Add(MakeShared<FJsonValueString>(U));
	Root->SetArrayField(TEXT("updated"), UArr);
	TArray<TSharedPtr<FJsonValue>> SArr; for (const auto& S : Skipped)
	{
		TSharedPtr<FJsonObject> SO = MakeShared<FJsonObject>();
		SO->SetStringField(TEXT("name"), S.Key);
		SO->SetStringField(TEXT("reason"), S.Value);
		SArr.Add(MakeShared<FJsonValueObject>(SO));
	}
	Root->SetArrayField(TEXT("skipped"), SArr);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraRendererData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName;
	int32 RendererIndex = -1;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetIntParam(Params, TEXT("renderer_index"), RendererIndex)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: renderer_index"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));
	const TArray<UNiagaraRendererProperties*>& Renderers = Data->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
		return FECACommandResult::Error(FString::Printf(TEXT("renderer_index %d out of range (count=%d)"), RendererIndex, Renderers.Num()));
	UNiagaraRendererProperties* R = Renderers[RendererIndex];
	if (!R) return FECACommandResult::Error(TEXT("Renderer slot is null"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetNumberField(TEXT("renderer_index"), RendererIndex);
	Root->SetStringField(TEXT("class"), R->GetClass()->GetName());
	Root->SetObjectField(TEXT("properties"), SerializeContainerToJson(R->GetClass(), R));
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_SetNiagaraRendererData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName;
	int32 RendererIndex = -1;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetIntParam(Params, TEXT("renderer_index"), RendererIndex)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: renderer_index"));
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (!GetObjectParam(Params, TEXT("properties"), PropsObj) || !PropsObj || !PropsObj->IsValid())
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: properties (object)"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	if (!Data) return FECACommandResult::Error(TEXT("Emitter has no data"));
	const TArray<UNiagaraRendererProperties*>& Renderers = Data->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
		return FECACommandResult::Error(FString::Printf(TEXT("renderer_index %d out of range (count=%d)"), RendererIndex, Renderers.Num()));
	UNiagaraRendererProperties* R = Renderers[RendererIndex];
	if (!R) return FECACommandResult::Error(TEXT("Renderer slot is null"));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "SetNiagaraRendererData", "Set Niagara Renderer Data"));
	System->Modify();
	R->Modify();

	TArray<FString> Updated;
	TArray<TPair<FString, FString>> Skipped;
	ApplyPartialUpdate(R->GetClass(), R, *PropsObj, Updated, Skipped);

	System->MarkPackageDirty();
	R->PostEditChange();
	System->PostEditChange();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetNumberField(TEXT("renderer_index"), RendererIndex);
	TArray<TSharedPtr<FJsonValue>> UArr; for (const FString& U : Updated) UArr.Add(MakeShared<FJsonValueString>(U));
	Root->SetArrayField(TEXT("updated"), UArr);
	TArray<TSharedPtr<FJsonValue>> SArr; for (const auto& S : Skipped)
	{
		TSharedPtr<FJsonObject> SO = MakeShared<FJsonObject>();
		SO->SetStringField(TEXT("name"), S.Key);
		SO->SetStringField(TEXT("reason"), S.Value);
		SArr.Add(MakeShared<FJsonValueObject>(SO));
	}
	Root->SetArrayField(TEXT("skipped"), SArr);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_SetNiagaraModuleEnabled::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath, EmitterName, ScriptStr, ModuleName;
	bool bEnabled = true;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	if (!GetStringParam(Params, TEXT("emitter_name"), EmitterName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: emitter_name"));
	if (!GetStringParam(Params, TEXT("script"), ScriptStr)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: script"));
	if (!GetStringParam(Params, TEXT("module_name"), ModuleName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: module_name"));
	if (!GetBoolParam(Params, TEXT("enabled"), bEnabled)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: enabled"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle) return FECACommandResult::Error(FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
	const ENiagaraScriptUsage Usage = ParseUsage(ScriptStr);
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeFunctionCall* Node = FindModuleAcrossUsages(System, Handle, ModuleName, Usage, &Graph);
	if (!Node) return FECACommandResult::Error(FString::Printf(TEXT("Module '%s' not found in script '%s'"), *ModuleName, *ScriptStr));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "SetNiagaraModuleEnabled", "Set Niagara Module Enabled"));
	Node->Modify();
	const bool bPrev = Node->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
	Node->SetEnabledState(bEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled, /*bUserAction*/ true);
	Node->MarkNodeRequiresSynchronization(TEXT("ECABridge: SetNiagaraModuleEnabled"), true);
	if (Graph) Graph->NotifyGraphChanged();
	System->MarkPackageDirty();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetStringField(TEXT("emitter_name"), EmitterName);
	Root->SetStringField(TEXT("script"), UsageToString(Usage));
	Root->SetStringField(TEXT("module_name"), ModuleName);
	Root->SetBoolField(TEXT("previous_enabled"), bPrev);
	Root->SetBoolField(TEXT("enabled"), bEnabled);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_GetNiagaraUserVariables::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	TArray<TSharedPtr<FJsonValue>> Vars;
	const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	for (const FNiagaraVariableWithOffset& V : Store.ReadParameterVariables())
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("name"), V.GetName().ToString());
		O->SetStringField(TEXT("type"), V.GetType().GetName());
		// default_value: ExportText form via the type definition's util.
		FString DefaultStr;
		FNiagaraVariable Tmp = (FNiagaraVariable)V;
		Tmp.AllocateData();
		if (Store.CopyParameterData(Tmp, Tmp.GetData()))
		{
			const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
			if (Schema) Schema->TryGetPinDefaultValueFromNiagaraVariable(Tmp, DefaultStr);
		}
		O->SetStringField(TEXT("default_value"), DefaultStr);
		Vars.Add(MakeShared<FJsonValueObject>(O));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetArrayField(TEXT("variables"), Vars);
	Root->SetNumberField(TEXT("count"), Vars.Num());
	return FECACommandResult::Success(Root);
}

// ============================================================================
// Task 4 — User variables CRUD
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_AddNiagaraUserVariables)
REGISTER_ECA_COMMAND(FECACommand_RemoveNiagaraUserVariables)

namespace ECANiagaraConvergence
{
	// Map a user-supplied type string to a Niagara type definition. Returns false
	// on unknown type. Recognized: float, int / int32, bool, vec2, vec3, vec4,
	// position, color / linearcolor, quat.
	static bool ResolveUserVarType(const FString& In, FNiagaraTypeDefinition& Out)
	{
		const FString T = In.ToLower();
		if (T == TEXT("float"))               { Out = FNiagaraTypeDefinition::GetFloatDef(); return true; }
		if (T == TEXT("int") || T == TEXT("int32")) { Out = FNiagaraTypeDefinition::GetIntDef(); return true; }
		if (T == TEXT("bool"))                { Out = FNiagaraTypeDefinition::GetBoolDef(); return true; }
		if (T == TEXT("vec2") || T == TEXT("vector2")) { Out = FNiagaraTypeDefinition::GetVec2Def(); return true; }
		if (T == TEXT("vec3") || T == TEXT("vector") || T == TEXT("vector3")) { Out = FNiagaraTypeDefinition::GetVec3Def(); return true; }
		if (T == TEXT("vec4") || T == TEXT("vector4")) { Out = FNiagaraTypeDefinition::GetVec4Def(); return true; }
		if (T == TEXT("position"))            { Out = FNiagaraTypeDefinition::GetPositionDef(); return true; }
		if (T == TEXT("color") || T == TEXT("linearcolor")) { Out = FNiagaraTypeDefinition::GetColorDef(); return true; }
		if (T == TEXT("quat"))                { Out = FNiagaraTypeDefinition::GetQuatDef(); return true; }
		return false;
	}

	// Set Tmp's allocated buffer from a JSON default value, typed by Tmp's
	// FNiagaraTypeDefinition. Returns false on a coerce mismatch.
	static bool AssignDefaultToVariable(FNiagaraVariable& Tmp, const TSharedPtr<FJsonValue>& DefaultVal)
	{
		const FNiagaraTypeDefinition Type = Tmp.GetType();
		if (!DefaultVal.IsValid()) return true; // accept missing default
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			double D = 0; if (!DefaultVal->TryGetNumber(D)) return false;
			Tmp.SetValue(static_cast<float>(D)); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			double D = 0; if (!DefaultVal->TryGetNumber(D)) return false;
			Tmp.SetValue(static_cast<int32>(D)); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			bool B = false; if (!DefaultVal->TryGetBool(B)) return false;
			FNiagaraBool NB; NB.SetValue(B); Tmp.SetValue(NB); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetVec2Def())
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!DefaultVal->TryGetObject(Obj) || !Obj) return false;
			FVector2f V(0,0);
			V.X = (float)(*Obj)->GetNumberField(TEXT("x"));
			V.Y = (float)(*Obj)->GetNumberField(TEXT("y"));
			Tmp.SetValue(V); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!DefaultVal->TryGetObject(Obj) || !Obj) return false;
			FVector3f V(0,0,0);
			V.X = (float)(*Obj)->GetNumberField(TEXT("x"));
			V.Y = (float)(*Obj)->GetNumberField(TEXT("y"));
			V.Z = (float)(*Obj)->GetNumberField(TEXT("z"));
			Tmp.SetValue(V); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetVec4Def() || Type == FNiagaraTypeDefinition::GetQuatDef())
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!DefaultVal->TryGetObject(Obj) || !Obj) return false;
			FVector4f V(0,0,0,0);
			V.X = (float)(*Obj)->GetNumberField(TEXT("x"));
			V.Y = (float)(*Obj)->GetNumberField(TEXT("y"));
			V.Z = (float)(*Obj)->GetNumberField(TEXT("z"));
			if ((*Obj)->HasField(TEXT("w"))) V.W = (float)(*Obj)->GetNumberField(TEXT("w"));
			Tmp.SetValue(V); return true;
		}
		if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!DefaultVal->TryGetObject(Obj) || !Obj) return false;
			FLinearColor C;
			C.R = (float)(*Obj)->GetNumberField(TEXT("r"));
			C.G = (float)(*Obj)->GetNumberField(TEXT("g"));
			C.B = (float)(*Obj)->GetNumberField(TEXT("b"));
			C.A = (*Obj)->HasField(TEXT("a")) ? (float)(*Obj)->GetNumberField(TEXT("a")) : 1.0f;
			Tmp.SetValue(C); return true;
		}
		return false;
	}
}

FECACommandResult FECACommand_AddNiagaraUserVariables::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
	if (!GetArrayParam(Params, TEXT("variables"), Vars) || !Vars) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: variables (array)"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "AddNiagaraUserVariables", "Add Niagara User Variables"));
	System->Modify();
	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	TArray<TSharedPtr<FJsonValue>> AddedArr;
	TArray<TSharedPtr<FJsonValue>> UpdatedArr;
	TArray<TSharedPtr<FJsonValue>> ErrorsArr;

	for (const TSharedPtr<FJsonValue>& V : *Vars)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !(*Obj).IsValid())
		{
			ErrorsArr.Add(MakeShared<FJsonValueString>(TEXT("variable entry is not an object")));
			continue;
		}
		FString Name = (*Obj)->GetStringField(TEXT("name"));
		FString TypeStr = (*Obj)->GetStringField(TEXT("type"));
		if (Name.IsEmpty() || TypeStr.IsEmpty())
		{
			ErrorsArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("entry missing name/type: %s/%s"), *Name, *TypeStr)));
			continue;
		}
		FNiagaraTypeDefinition Type;
		if (!ResolveUserVarType(TypeStr, Type))
		{
			ErrorsArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("unknown type '%s' for var '%s'"), *TypeStr, *Name)));
			continue;
		}

		// "User." prefix is the canonical exposed-param namespace.
		FString FullName = Name;
		if (!FullName.StartsWith(TEXT("User."))) FullName = FString::Printf(TEXT("User.%s"), *Name);

		FNiagaraVariable Var(Type, FName(*FullName));
		Var.AllocateData();

		// Apply default if supplied; ignore missing field.
		if ((*Obj)->HasField(TEXT("default")))
		{
			TSharedPtr<FJsonValue> DefVal = (*Obj)->TryGetField(TEXT("default"));
			if (DefVal.IsValid() && !AssignDefaultToVariable(Var, DefVal))
			{
				ErrorsArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("default value type-coerce failed for var '%s'"), *Name)));
				continue;
			}
		}

		const bool bExisted = Store.IndexOf(Var) != INDEX_NONE;
		if (bExisted)
		{
			Store.SetParameterData(Var.GetData(), Var, /*bAdd*/ false);
			UpdatedArr.Add(MakeShared<FJsonValueString>(FullName));
		}
		else
		{
			Store.AddParameter(Var, /*bInitialize*/ true, /*bTriggerRebind*/ true);
			Store.SetParameterData(Var.GetData(), Var, /*bAdd*/ false);
			AddedArr.Add(MakeShared<FJsonValueString>(FullName));
		}
	}

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetArrayField(TEXT("added"), AddedArr);
	Root->SetArrayField(TEXT("updated"), UpdatedArr);
	Root->SetArrayField(TEXT("errors"), ErrorsArr);
	return FECACommandResult::Success(Root);
}

FECACommandResult FECACommand_RemoveNiagaraUserVariables::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SystemPath;
	if (!GetStringParam(Params, TEXT("system_path"), SystemPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: system_path"));
	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	if (!GetArrayParam(Params, TEXT("names"), Names) || !Names) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: names (array)"));

	UNiagaraSystem* System = ResolveSystem(SystemPath);
	if (!System) return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));

	FScopedTransaction Tx(NSLOCTEXT("ECABridge", "RemoveNiagaraUserVariables", "Remove Niagara User Variables"));
	System->Modify();
	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	TArray<TSharedPtr<FJsonValue>> Removed;
	TArray<TSharedPtr<FJsonValue>> NotFound;
	for (const TSharedPtr<FJsonValue>& NV : *Names)
	{
		FString Name; if (!NV.IsValid() || !NV->TryGetString(Name) || Name.IsEmpty()) continue;
		FString FullName = Name;
		if (!FullName.StartsWith(TEXT("User."))) FullName = FString::Printf(TEXT("User.%s"), *Name);

		// Find any matching variable to fetch its type for the removal call.
		bool bMatched = false;
		for (const FNiagaraVariableWithOffset& V : Store.ReadParameterVariables())
		{
			if (V.GetName().ToString().Equals(FullName, ESearchCase::IgnoreCase) ||
				V.GetName().ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				Store.RemoveParameter((FNiagaraVariable)V);
				Removed.Add(MakeShared<FJsonValueString>(V.GetName().ToString()));
				bMatched = true;
				break;
			}
		}
		if (!bMatched) NotFound.Add(MakeShared<FJsonValueString>(Name));
	}

	System->MarkPackageDirty();
	System->PostEditChange();
	System->RequestCompile(false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("system_path"), SystemPath);
	Root->SetArrayField(TEXT("removed"), Removed);
	Root->SetArrayField(TEXT("not_found"), NotFound);
	return FECACommandResult::Success(Root);
}

#endif // WITH_ECA_NIAGARA
