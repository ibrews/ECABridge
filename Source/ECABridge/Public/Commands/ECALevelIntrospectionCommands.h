// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Dump the current level's full state to JSON.
 * Serializes all actors with class, transform, properties, components, tags, and hierarchy.
 * The "Rosetta Stone" for .umap files — makes any loaded level fully legible to an LLM.
 */
class FECACommand_DumpLevel : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("dump_level"); }
	virtual FString GetDescription() const override { return TEXT("Serialize the current level to JSON: all actors with class, transform, components, tags, folder, and optionally full properties. Supports filtering by class, folder, tag, or spatial bounds. Two modes: lightweight (class + transform per actor) or deep (full property dump)."); }
	virtual FString GetCategory() const override { return TEXT("Editor"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("max_actors"), TEXT("number"), TEXT("Maximum actors to return (default 500, 0 = unlimited)"), false, TEXT("500") },
			{ TEXT("filter_class"), TEXT("string"), TEXT("Only include actors of this class (e.g., StaticMeshActor, PointLight, BP_Enemy)"), false },
			{ TEXT("filter_folder"), TEXT("string"), TEXT("Only include actors in this editor folder path"), false },
			{ TEXT("filter_tag"), TEXT("string"), TEXT("Only include actors with this tag"), false },
			{ TEXT("include_properties"), TEXT("boolean"), TEXT("Include full property dump per actor (default false — lightweight mode with just class, name, transform)"), false, TEXT("false") },
			{ TEXT("include_components"), TEXT("boolean"), TEXT("Include component list per actor (default true)"), false, TEXT("true") },
			{ TEXT("bounds_min"), TEXT("object"), TEXT("Spatial filter minimum {x, y, z} — only actors within box"), false },
			{ TEXT("bounds_max"), TEXT("object"), TEXT("Spatial filter maximum {x, y, z} — only actors within box"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
