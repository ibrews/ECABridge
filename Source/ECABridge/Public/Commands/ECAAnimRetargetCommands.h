// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Enumerate UIKRetargeter assets in the project.
 *
 * Returns one entry per retargeter with source / target IKRig asset paths,
 * source / target preview skeletal mesh paths, and retarget op count. Useful
 * for finding the right retargeter to drive a MetaHuman -> custom skeleton
 * workflow without opening the editor UI.
 *
 * Implementation note: uses the asset registry to discover UIKRetargeter assets
 * by class path string, then reads the UProperty graph reflectively. This means
 * the IKRig plugin doesn't need to be hard-linked — if the plugin is disabled,
 * the search simply returns zero results.
 */
class FECACommand_ListIKRetargeters : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_ik_retargeters"); }
	virtual FString GetDescription() const override { return TEXT("List every UIKRetargeter asset in the project. Returns source/target IKRig paths, preview skeletal mesh paths, and op count for each."); }
	virtual FString GetCategory() const override { return TEXT("AnimRetarget"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Package path prefix (default '/Game/')"), false, TEXT("/Game/") },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Wildcard filter on asset name (e.g. 'IKR_*')"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"),       TEXT("integer"), TEXT("Number of retargeters returned") },
			{ TEXT("retargeters"), TEXT("array"),   TEXT("[{path, name, source_ik_rig, target_ik_rig, source_preview_mesh, target_preview_mesh, op_count}]"), TEXT("object") },
			{ TEXT("plugin_available"), TEXT("boolean"), TEXT("True if the IKRig plugin is loaded in this editor session") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Inspect a single UIKRetargeter: source/target IKRig + chain mappings + retarget ops.
 *
 * Reflectively reads the asset's properties: SourceIKRigAsset, TargetIKRigAsset,
 * ChainMappings (resolved through retarget ops), and the retarget op stack
 * (each op's StaticStruct name).
 *
 * If the IKRig plugin isn't loaded, returns a clear "plugin not enabled" message.
 */
class FECACommand_DumpIKRetargeter : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_ik_retargeter"); }
	virtual FString GetDescription() const override { return TEXT("Inspect a UIKRetargeter: source/target IK rig paths, retarget op stack, retarget pose list, and a summary of chain mappings."); }
	virtual FString GetCategory() const override { return TEXT("AnimRetarget"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("retargeter_path"), TEXT("string"), TEXT("Asset path to a UIKRetargeter (e.g. '/Game/Characters/RTG_UE5ToCustom')"), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("path"),                 TEXT("string"),  TEXT("Resolved asset path") },
			{ TEXT("name"),                 TEXT("string"),  TEXT("Asset name") },
			{ TEXT("source_ik_rig"),        TEXT("string"),  TEXT("Source UIKRigDefinition asset path") },
			{ TEXT("target_ik_rig"),        TEXT("string"),  TEXT("Target UIKRigDefinition asset path") },
			{ TEXT("source_preview_mesh"),  TEXT("string"),  TEXT("Source preview USkeletalMesh path") },
			{ TEXT("target_preview_mesh"),  TEXT("string"),  TEXT("Target preview USkeletalMesh path") },
			{ TEXT("retarget_ops"),         TEXT("array"),   TEXT("[{name, struct_type}]"), TEXT("object") },
			{ TEXT("retarget_poses"),       TEXT("array"),   TEXT("Source retarget pose names"), TEXT("string") },
			{ TEXT("current_retarget_pose"),TEXT("string"),  TEXT("Currently-selected source retarget pose name") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Enumerate UIKRigDefinition assets in the project.
 *
 * Companion to list_ik_retargeters — lists the source/target rigs that
 * retargeters reference. Returns bone goal count, retarget chain count, and
 * preview skeletal mesh path for each rig.
 */
class FECACommand_ListIKRigs : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_ik_rigs"); }
	virtual FString GetDescription() const override { return TEXT("List every UIKRigDefinition asset in the project. Returns retarget chain count, goal count, and preview skeletal mesh path."); }
	virtual FString GetCategory() const override { return TEXT("AnimRetarget"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("path_filter"), TEXT("string"), TEXT("Package path prefix (default '/Game/')"), false, TEXT("/Game/") },
			{ TEXT("name_filter"), TEXT("string"), TEXT("Wildcard filter on asset name"), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("count"), TEXT("integer"), TEXT("Number of rigs returned") },
			{ TEXT("rigs"),  TEXT("array"),   TEXT("[{path, name, preview_mesh, chain_count, goal_count}]"), TEXT("object") },
			{ TEXT("plugin_available"), TEXT("boolean"), TEXT("True if the IKRig plugin is loaded in this editor session") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
