// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Get available Niagara systems in the project
 */
class FECACommand_GetNiagaraSystems : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_systems"); }
	virtual FString GetDescription() const override { return TEXT("Get all available Niagara systems in the project"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Filter systems by path (partial match)"), false },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Filter systems by name (partial match)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Spawn a Niagara effect in the level
 */
class FECACommand_SpawnNiagaraEffect : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("spawn_niagara_effect"); }
	virtual FString GetDescription() const override { return TEXT("Spawn a Niagara particle effect in the level"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset (e.g., /Game/Effects/NS_Fire)"), true },
			{ TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), false },
			{ TEXT("scale"), TEXT("object"), TEXT("Scale {x, y, z}"), false },
			{ TEXT("name"), TEXT("string"), TEXT("Display name for the effect actor"), false },
			{ TEXT("auto_activate"), TEXT("boolean"), TEXT("Whether to auto-activate the effect"), false, TEXT("true") },
			{ TEXT("folder"), TEXT("string"), TEXT("Outliner folder path"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new Niagara system asset
 */
class FECACommand_CreateNiagaraSystem : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_niagara_system"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Niagara system asset"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path for the new asset (e.g., /Game/Effects/NS_NewEffect)"), true },
			{ TEXT("template"), TEXT("string"), TEXT("Template to use: empty, sprite, mesh, ribbon, or path to existing system"), false, TEXT("empty") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get Niagara effect actors in the level
 */
class FECACommand_GetNiagaraActors : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_actors"); }
	virtual FString GetDescription() const override { return TEXT("Get all Niagara effect actors in the current level"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_filter"), TEXT("string"), TEXT("Filter by system name (partial match)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a parameter on a Niagara component
 */
class FECACommand_SetNiagaraParameter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_parameter"); }
	virtual FString GetDescription() const override { return TEXT("Set a user parameter on a Niagara effect"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the Niagara actor"), true },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of the parameter to set"), true },
			{ TEXT("parameter_type"), TEXT("string"), TEXT("Type: float, int, bool, vector, color, linear_color"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set (number, boolean, or {x,y,z}/{r,g,b,a} object)"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Control Niagara effect playback
 */
class FECACommand_ControlNiagaraEffect : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("control_niagara_effect"); }
	virtual FString GetDescription() const override { return TEXT("Control playback of a Niagara effect (activate, deactivate, reset)"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the Niagara actor"), true },
			{ TEXT("action"), TEXT("string"), TEXT("Action to perform: activate, deactivate, reset, reset_system"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get Niagara system user parameters
 */
class FECACommand_GetNiagaraParameters : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_parameters"); }
	virtual FString GetDescription() const override { return TEXT("Get user-exposed parameters of a Niagara system"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the Niagara actor in the level"), false },
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a Niagara component to an existing actor
 */
class FECACommand_AddNiagaraComponent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_niagara_component"); }
	virtual FString GetDescription() const override { return TEXT("Add a Niagara component to an existing actor"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor to add the component to"), true },
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name for the new component"), false },
			{ TEXT("relative_location"), TEXT("object"), TEXT("Relative location {x, y, z}"), false },
			{ TEXT("relative_rotation"), TEXT("object"), TEXT("Relative rotation {pitch, yaw, roll}"), false },
			{ TEXT("auto_activate"), TEXT("boolean"), TEXT("Whether to auto-activate"), false, TEXT("true") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add an emitter to a Niagara system
 */
class FECACommand_AddNiagaraEmitter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_niagara_emitter"); }
	virtual FString GetDescription() const override { return TEXT("Add an emitter to a Niagara system asset"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_path"), TEXT("string"), TEXT("Path to emitter asset to add, or 'empty' for blank emitter"), false, TEXT("empty") },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name for the new emitter"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a module to a Niagara emitter
 */
class FECACommand_AddNiagaraModule : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_niagara_module"); }
	virtual FString GetDescription() const override { return TEXT("Add a module to a Niagara emitter's script stack"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter (or index like 'Emitter[0]')"), true },
			{ TEXT("module_path"), TEXT("string"), TEXT("Path to module script (e.g., '/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity')"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Target script: emitter_spawn, emitter_update, particle_spawn, particle_update"), true },
			{ TEXT("index"), TEXT("number"), TEXT("Position to insert module (-1 for end)"), false, TEXT("-1") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a module input value in a Niagara emitter
 */
class FECACommand_SetNiagaraModuleInput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_module_input"); }
	virtual FString GetDescription() const override { return TEXT("Set an input value on a Niagara module"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Name of the module"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name of the input to set"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set (number, bool, or {x,y,z}/{r,g,b,a} object)"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Script containing the module: emitter_spawn, emitter_update, particle_spawn, particle_update"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get emitters in a Niagara system
 */
class FECACommand_GetNiagaraEmitters : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_emitters"); }
	virtual FString GetDescription() const override { return TEXT("Get all emitters in a Niagara system"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get modules in a Niagara emitter
 */
class FECACommand_GetNiagaraModules : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_niagara_modules"); }
	virtual FString GetDescription() const override { return TEXT("Get all modules in a Niagara emitter's script stack"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Script to query: emitter_spawn, emitter_update, particle_spawn, particle_update, all"), false, TEXT("all") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List available Niagara modules
 */
class FECACommand_ListNiagaraModules : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_niagara_modules"); }
	virtual FString GetDescription() const override { return TEXT("List available built-in Niagara modules by category"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category"), TEXT("string"), TEXT("Filter by category: spawn, update, emitter, forces, or 'all'"), false, TEXT("all") },
			{ TEXT("search"), TEXT("string"), TEXT("Search filter for module names"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a module from a Niagara emitter
 */
class FECACommand_RemoveNiagaraModule : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("remove_niagara_module"); }
	virtual FString GetDescription() const override { return TEXT("Remove a module from a Niagara emitter"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Name of the module to remove"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Script containing the module: emitter_spawn, emitter_update, particle_spawn, particle_update"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Delete an emitter from a Niagara system
 */
class FECACommand_DeleteNiagaraEmitter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("delete_niagara_emitter"); }
	virtual FString GetDescription() const override { return TEXT("Delete an emitter from a Niagara system"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter to delete"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set emitter properties (spawn rate, lifetime, etc.)
 */
class FECACommand_SetNiagaraEmitterProperty : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_emitter_property"); }
	virtual FString GetDescription() const override { return TEXT("Set a property on a Niagara emitter"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("property"), TEXT("string"), TEXT("Property to set: sim_target (cpu/gpu), enabled, bounds_mode, local_space, determinism"), true },
			{ TEXT("value"), TEXT("any"), TEXT("Value to set"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * List available Niagara emitter templates
 */
class FECACommand_ListNiagaraEmitterTemplates : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_niagara_emitter_templates"); }
	virtual FString GetDescription() const override { return TEXT("List available Niagara emitter templates that can be used with add_niagara_emitter"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category"), TEXT("string"), TEXT("Filter by category: all, sprites, meshes, ribbons, beams, behaviors"), false, TEXT("all") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Create a new Niagara emitter asset based on a template
 */
class FECACommand_CreateNiagaraEmitter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_niagara_emitter"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual FString GetDescription() const override { return TEXT("Create a new Niagara emitter asset based on a template with configurable properties"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Path for the new emitter asset (e.g., /Game/Effects/E_MyEmitter)"), true },
			{ TEXT("template"), TEXT("string"), TEXT("Base template: 'empty', 'sprite', 'mesh', 'ribbon', or full path to existing emitter"), false, TEXT("sprite") },
			{ TEXT("sim_target"), TEXT("string"), TEXT("Simulation target: 'cpu' or 'gpu'"), false, TEXT("cpu") },
			{ TEXT("local_space"), TEXT("boolean"), TEXT("Whether to simulate in local space"), false, TEXT("false") },
			{ TEXT("determinism"), TEXT("boolean"), TEXT("Whether to use deterministic random"), false, TEXT("false") },
			{ TEXT("bounds_mode"), TEXT("string"), TEXT("Bounds calculation mode: 'dynamic' or 'fixed'"), false, TEXT("dynamic") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a renderer to a Niagara emitter
 */
class FECACommand_AddNiagaraRenderer : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("add_niagara_renderer"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual FString GetDescription() const override { return TEXT("Add a renderer (sprite, mesh, ribbon, light) to a Niagara emitter"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("renderer_type"), TEXT("string"), TEXT("Type: 'sprite', 'mesh', 'ribbon', 'light', 'component'"), true },
			{ TEXT("renderer_name"), TEXT("string"), TEXT("Name for the new renderer"), false },
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to material asset to assign"), false },
			{ TEXT("mesh_path"), TEXT("string"), TEXT("Path to static mesh asset (for mesh renderer)"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set material on a Niagara renderer
 */
class FECACommand_SetNiagaraMaterial : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_material"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual FString GetDescription() const override { return TEXT("Set a material on a Niagara renderer"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("material_path"), TEXT("string"), TEXT("Path to material asset"), true },
			{ TEXT("renderer_index"), TEXT("number"), TEXT("Index of the renderer (0-based)"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a curve on a Niagara module input
 */
class FECACommand_SetNiagaraCurve : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_curve"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual FString GetDescription() const override { return TEXT("Set a float or color curve on a Niagara module input"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Name of the module"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name of the input to set"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Script containing the module: emitter_spawn, emitter_update, particle_spawn, particle_update"), true },
			{ TEXT("curve_type"), TEXT("string"), TEXT("Type: 'float', 'vector', 'color'"), true },
			{ TEXT("keys"), TEXT("array"), TEXT("Array of curve keys: [{time: 0, value: 1}, ...] or [{time: 0, r: 1, g: 0, b: 0, a: 1}, ...]"), true }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Set a dynamic input (expression or random range) on a module input
 */
class FECACommand_SetNiagaraDynamicInput : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("set_niagara_dynamic_input"); }
	virtual FString GetCategory() const override { return TEXT("Niagara"); }
	virtual FString GetDescription() const override { return TEXT("Set a dynamic input like random range, expression, or link to parameter"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("system_path"), TEXT("string"), TEXT("Path to the Niagara system asset"), true },
			{ TEXT("emitter_name"), TEXT("string"), TEXT("Name of the emitter"), true },
			{ TEXT("module_name"), TEXT("string"), TEXT("Name of the module"), true },
			{ TEXT("input_name"), TEXT("string"), TEXT("Name of the input to set"), true },
			{ TEXT("script_usage"), TEXT("string"), TEXT("Script containing the module: emitter_spawn, emitter_update, particle_spawn, particle_update"), true },
			{ TEXT("dynamic_input_type"), TEXT("string"), TEXT("Type: 'random_range', 'uniform_random', 'parameter_link', 'custom_expression'"), true },
			{ TEXT("min_value"), TEXT("any"), TEXT("Minimum value for random range (number or {x,y,z})"), false },
			{ TEXT("max_value"), TEXT("any"), TEXT("Maximum value for random range (number or {x,y,z})"), false },
			{ TEXT("parameter_name"), TEXT("string"), TEXT("Name of parameter to link to"), false },
			{ TEXT("expression"), TEXT("string"), TEXT("Custom HLSL expression"), false }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
