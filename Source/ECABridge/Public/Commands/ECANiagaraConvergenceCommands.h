// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

#if WITH_ECA_NIAGARA

// ============================================================================
// Niagara API convergence commands (Batch V).
// Schemas, topology getters, data read/set, user-variable CRUD, diagnostics.
// Clean-room port from Epic's NiagaraToolsets — no Epic source is reproduced.
// ============================================================================

// -------- Schemas (5) -------------------------------------------------------

class FECACommand_GetNiagaraSystemSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_system_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return JSON Schema describing the editable properties of a UNiagaraSystem (name, type, description, default). Pure introspection — no asset is loaded or mutated."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraEmitterSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_emitter_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return JSON Schema describing the editable properties of a Niagara emitter (FVersionedNiagaraEmitterData + UNiagaraEmitter)."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraRendererSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_renderer_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return JSON Schema for the editable properties of a Niagara renderer subclass (sprite/mesh/ribbon/light/component)."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("renderer_class"), TEXT("string"), TEXT("Renderer class short name (e.g. NiagaraSpriteRendererProperties, NiagaraMeshRendererProperties, NiagaraRibbonRendererProperties, NiagaraLightRendererProperties, NiagaraComponentRendererProperties) or the keyword sprite/mesh/ribbon/light/component"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraDataInterfaceSchema : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_data_interface_schema"); }
	virtual FString GetDescription() const override { return TEXT("Return JSON Schema for the editable properties of a UNiagaraDataInterface subclass (e.g. NiagaraDataInterfaceCurve, NiagaraDataInterfaceTexture)."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("data_interface_class"), TEXT("string"), TEXT("Data interface class short name (e.g. NiagaraDataInterfaceCurve, NiagaraDataInterfaceVectorCurve, NiagaraDataInterfaceColorCurve, NiagaraDataInterfaceTexture)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraModuleSchemaFromAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_module_schema_from_asset"); }
	virtual FString GetDescription() const override { return TEXT("Return the input variables (name, type, default) of a Niagara module script asset, without needing a host system context."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("module_script_path"), TEXT("string"), TEXT("Package path of a UNiagaraScript module asset (e.g. /Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -------- Topology getters (5) ---------------------------------------------

class FECACommand_GetNiagaraSystemTopology : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_system_topology"); }
	virtual FString GetDescription() const override { return TEXT("Return the structural layout of a Niagara system: emitters with their scripts and renderers. Stack references are structured {system_path, emitter_name, script, module_name, input_name} so they round-trip cleanly."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraEmitterTopology : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_emitter_topology"); }
	virtual FString GetDescription() const override { return TEXT("Return scripts, modules per script, and renderers under a single emitter inside a system."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name within the system"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraScriptStackTopology : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_script_stack_topology"); }
	virtual FString GetDescription() const override { return TEXT("Return the modules that make up a named script stack on an emitter (SystemSpawn/SystemUpdate/EmitterSpawn/EmitterUpdate/ParticleSpawn/ParticleUpdate/ParticleEventHandler)."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("script"), TEXT("string"), TEXT("Script usage: system_spawn, system_update, emitter_spawn, emitter_update, particle_spawn, particle_update"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraModuleTopology : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_module_topology"); }
	virtual FString GetDescription() const override { return TEXT("Return the user-facing inputs (and current values) of a module inside an emitter's named script stack."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("script"), TEXT("string"), TEXT("Script usage hosting the module"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Module function name (as returned by get_niagara_modules / get_niagara_script_stack_topology)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraStackInputTopology : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_stack_input_topology"); }
	virtual FString GetDescription() const override { return TEXT("Return the type, current value, and any nested dynamic-input substructure for a specific module input."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("script"), TEXT("string"), TEXT("Script usage hosting the module"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Module function name"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Input name (short form e.g. 'SpawnRate' or full handle form)"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -------- Data getters/setters (8 per spec) --------------------------------

class FECACommand_GetNiagaraSystemData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_system_data"); }
	virtual FString GetDescription() const override { return TEXT("Return all editable system-level properties of a Niagara system as a JSON object."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetNiagaraSystemData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_system_data"); }
	virtual FString GetDescription() const override { return TEXT("Partial-update setter: writes only the properties listed in 'properties' on the system, leaves others untouched. Marks the system dirty + requests a recompile."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("properties"), TEXT("object"), TEXT("Map of property name -> new value (JSON). Only listed properties are touched."), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraEmitterData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_emitter_data"); }
	virtual FString GetDescription() const override { return TEXT("Return editable emitter-level properties (FVersionedNiagaraEmitterData on the emitter inside the system)."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetNiagaraEmitterData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_emitter_data"); }
	virtual FString GetDescription() const override { return TEXT("Partial-update setter for emitter-level properties."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("properties"), TEXT("object"), TEXT("Map of property name -> new value (JSON)."), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraRendererData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_renderer_data"); }
	virtual FString GetDescription() const override { return TEXT("Return editable properties of a specific renderer on an emitter."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("renderer_index"), TEXT("number"), TEXT("Zero-based index into the emitter's renderer list"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetNiagaraRendererData : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_renderer_data"); }
	virtual FString GetDescription() const override { return TEXT("Partial-update setter for a renderer's properties."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("renderer_index"), TEXT("number"), TEXT("Zero-based index into the emitter's renderer list"), true },
			{ TEXT("properties"), TEXT("object"), TEXT("Map of property name -> new value (JSON)."), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_SetNiagaraModuleEnabled : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_module_enabled"); }
	virtual FString GetDescription() const override { return TEXT("Enable or disable a single module on an emitter's named script stack."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Emitter handle name"), true },
			{ TEXT("script"), TEXT("string"), TEXT("Script usage hosting the module"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Module function name"), true },
			{ TEXT("enabled"), TEXT("boolean"), TEXT("New enabled state"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraUserVariables : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_user_variables"); }
	virtual FString GetDescription() const override { return TEXT("Return the user-facing exposed parameters of a Niagara system: {variables: [{name, type, default_value}]}. Distinct from get_niagara_parameters (which inspects a level actor) — this is asset-level."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -------- User variables (2) -----------------------------------------------

class FECACommand_AddNiagaraUserVariables : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_niagara_user_variables"); }
	virtual FString GetDescription() const override { return TEXT("Upsert user-facing exposed parameters on a Niagara system. Existing variables with matching names are updated; missing ones are created."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("variables"), TEXT("array"), TEXT("Array of {name, type, default} objects. Supported types: float, int, bool, vec2, vec3, vec4, position, color, quat, linearcolor."), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_RemoveNiagaraUserVariables : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_niagara_user_variables"); }
	virtual FString GetDescription() const override { return TEXT("Remove the named exposed parameters from a Niagara system's user variables. Unknown names are reported but do not fail the call."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("names"), TEXT("array"), TEXT("Array of user-variable names to remove"), true }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

// -------- Diagnostics (3) --------------------------------------------------

class FECACommand_GetNiagaraSystemCompileState : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_system_compile_state"); }
	virtual FString GetDescription() const override { return TEXT("Block on any in-flight compile of a Niagara system, then return per-script compile status. Status is one of success/failure/compiling/unknown."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("timeout_seconds"), TEXT("number"), TEXT("Max wall time to wait for an in-flight compile to finish (default 30)"), false, TEXT("30") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_GetNiagaraStackIssues : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_stack_issues"); }
	virtual FString GetDescription() const override { return TEXT("Block on any in-flight compile, then return the list of stack-level issues (errors, warnings, info) reported by Niagara for a system, with their location and any available auto-fixes."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("timeout_seconds"), TEXT("number"), TEXT("Max wall time to wait for an in-flight compile to finish (default 30)"), false, TEXT("30") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

class FECACommand_ApplyNiagaraStackIssueFix : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("apply_niagara_stack_issue_fix"); }
	virtual FString GetDescription() const override { return TEXT("Apply a Niagara stack-issue auto-fix by id. Only Fix-kind fixes are applied (Link-kind navigates the UI and is not exposed). Returns the post-fix issue list."); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("issue_id"), TEXT("string"), TEXT("Stable id of the stack issue (as returned by get_niagara_stack_issues)"), true },
			{ TEXT("fix_id"), TEXT("string"), TEXT("Id of the fix to apply (from the issue's fixes array)"), true },
			{ TEXT("timeout_seconds"), TEXT("number"), TEXT("Max wait for post-fix compile (default 30)"), false, TEXT("30") }
		};
	}
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif // WITH_ECA_NIAGARA
