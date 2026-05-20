// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Enumerate UGeometryScriptLibrary_* function libraries available in this engine.
 *
 * GeometryScript exposes its surface as a family of UBlueprintFunctionLibrary
 * subclasses (one per category — MeshBasicEdit, MeshBoolean, MeshDeform, etc.).
 * This command walks the reflection registry and lists every one of them so an
 * LLM can discover what's callable without grepping the engine headers.
 */
class FECACommand_ListGeometryScriptLibraries : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_geometry_script_libraries"); }
	virtual FString GetDescription() const override { return TEXT("List every UGeometryScriptLibrary_* function library exposed by the GeometryScripting plugin. Returns class name, function count, and module path for each."); }
	virtual FString GetCategory() const override { return TEXT("GeometryScript"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("name_filter"), TEXT("string"), TEXT("Substring filter on class name (case-insensitive)"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),     TEXT("integer"), TEXT("Number of libraries returned") },
			{ TEXT("libraries"), TEXT("array"),   TEXT("[{class, function_count, package}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect one GeometryScript library: list its UFUNCTIONs (name, return type, params).
 *
 * Pass class_name like 'UGeometryScriptLibrary_MeshBooleanFunctions' (or shorter
 * 'GeometryScriptLibrary_MeshBooleanFunctions'). Returns one entry per static
 * BlueprintCallable function. Used to figure out exact parameter names + types
 * before calling a function via the Python sandbox or a future generic dispatch.
 */
class FECACommand_DumpGeometryScriptLibrary : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_geometry_script_library"); }
	virtual FString GetDescription() const override { return TEXT("Inspect one UGeometryScriptLibrary_* class: list every BlueprintCallable UFUNCTION with its return type, parameter names, and parameter types."); }
	virtual FString GetCategory() const override { return TEXT("GeometryScript"); }
	virtual bool IsMutating() const override { return false; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("class_name"), TEXT("string"), TEXT("Library class name (e.g. 'UGeometryScriptLibrary_MeshBooleanFunctions' or shorter)"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("class"),         TEXT("string"),  TEXT("Resolved class name") },
			{ TEXT("package"),       TEXT("string"),  TEXT("Containing package path") },
			{ TEXT("function_count"),TEXT("integer"), TEXT("Number of functions returned") },
			{ TEXT("functions"),     TEXT("array"),   TEXT("[{name, return_type, params:[{name, type, out_param}], tooltip}]"), TEXT("object") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Apply a chain of GeometryScript operations to a UStaticMesh asset.
 *
 * Pipeline: load StaticMesh -> copy to UDynamicMesh -> run ops -> copy back.
 * Each op is `{ "op": "<name>", ...args }` where supported ops are:
 *   - {"op":"translate","x":..,"y":..,"z":..}
 *   - {"op":"scale","x":..,"y":..,"z":..}
 *   - {"op":"rotate","pitch":..,"yaw":..,"roll":..}
 *   - {"op":"discard_uvs"}
 *   - {"op":"flip_normals"}
 *   - {"op":"recompute_normals"}
 *   - {"op":"simplify","target_triangles":<int>}
 *
 * Mutator. Marks the asset dirty; caller should save_asset to persist.
 */
class FECACommand_ApplyGeometryScriptOps : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("apply_geometry_script_ops"); }
	virtual FString GetDescription() const override { return TEXT("Apply a sequence of GeometryScript ops to a UStaticMesh and write the result back. Ops supported: translate, scale, rotate, discard_uvs, flip_normals, recompute_normals, simplify. Marks the asset dirty."); }
	virtual FString GetCategory() const override { return TEXT("GeometryScript"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("static_mesh_path"), TEXT("string"), TEXT("Asset path to a UStaticMesh"), true },
			{ TEXT("ops"),              TEXT("array"),  TEXT("[{op:'translate'|'scale'|'rotate'|'discard_uvs'|'flip_normals'|'recompute_normals'|'simplify', ...args}]"), true },
			{ TEXT("lod"),              TEXT("integer"), TEXT("LOD index to read/write (default 0)"), false, TEXT("0") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),       TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("ops_applied"),TEXT("integer"), TEXT("Number of ops successfully applied") },
			{ TEXT("vertex_count_before"), TEXT("integer"), TEXT("Vertex count before ops") },
			{ TEXT("vertex_count_after"),  TEXT("integer"), TEXT("Vertex count after ops") },
			{ TEXT("triangle_count_before"),TEXT("integer"), TEXT("Triangle count before ops") },
			{ TEXT("triangle_count_after"), TEXT("integer"), TEXT("Triangle count after ops") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
