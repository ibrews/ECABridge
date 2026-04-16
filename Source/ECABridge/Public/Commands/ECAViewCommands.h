// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Get actors visible within a camera's view frustum.
 * 
 * This command allows the AI to "see" what a camera (or the editor viewport) can see,
 * returning information about objects within the view frustum up to a specified distance.
 */
class FECACommand_GetActorsInView : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_actors_in_view"); }
	virtual FString GetDescription() const override { return TEXT("Get actors visible within a camera's view frustum. Returns near objects that the camera can see, useful for AI scene understanding."); }
	virtual FString GetCategory() const override { return TEXT("View"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("camera_name"), TEXT("string"), TEXT("Name of a CameraActor to use as the view source. If not specified, uses the active editor viewport camera."), false },
			{ TEXT("max_distance"), TEXT("number"), TEXT("Maximum distance from camera to include actors (in centimeters). Default: 10000 (100 meters)"), false, TEXT("10000") },
			{ TEXT("min_distance"), TEXT("number"), TEXT("Minimum distance from camera to include actors (in centimeters). Default: 0"), false, TEXT("0") },
			{ TEXT("include_hidden"), TEXT("boolean"), TEXT("Include hidden actors in results. Default: false"), false, TEXT("false") },
			{ TEXT("class_filter"), TEXT("string"), TEXT("Filter actors by class name (partial match)"), false },
			{ TEXT("include_components"), TEXT("boolean"), TEXT("Include component details for each actor. Default: false"), false, TEXT("false") },
			{ TEXT("max_results"), TEXT("number"), TEXT("Maximum number of actors to return, sorted by distance. Default: 100"), false, TEXT("100") },
			{ TEXT("fov_scale"), TEXT("number"), TEXT("Scale factor for the field of view (1.0 = camera's actual FOV, 2.0 = double width). Default: 1.0"), false, TEXT("1.0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Get the current editor viewport camera properties.
 * 
 * Returns information about the active editor viewport camera including
 * position, rotation, FOV, and other view settings.
 */
class FECACommand_GetViewportCamera : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_viewport_camera"); }
	virtual FString GetDescription() const override { return TEXT("Get the current editor viewport camera position, rotation, and properties."); }
	virtual FString GetCategory() const override { return TEXT("View"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("viewport_index"), TEXT("number"), TEXT("Index of the viewport to query. Default: 0 (active viewport)"), false, TEXT("0") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Describe what a camera can see in natural language.
 * 
 * Returns a structured description of the scene visible from a camera's perspective,
 * organized by distance zones and object types.
 */
class FECACommand_DescribeView : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_view"); }
	virtual FString GetDescription() const override { return TEXT("Get a structured description of what a camera can see, organized by distance zones (near/mid/far) and object categories."); }
	virtual FString GetCategory() const override { return TEXT("View"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("camera_name"), TEXT("string"), TEXT("Name of a CameraActor to use. If not specified, uses the active editor viewport camera."), false },
			{ TEXT("max_distance"), TEXT("number"), TEXT("Maximum view distance in centimeters. Default: 50000 (500 meters)"), false, TEXT("50000") },
			{ TEXT("near_threshold"), TEXT("number"), TEXT("Distance threshold for 'near' zone in cm. Default: 500 (5 meters)"), false, TEXT("500") },
			{ TEXT("mid_threshold"), TEXT("number"), TEXT("Distance threshold for 'mid' zone in cm. Default: 2000 (20 meters)"), false, TEXT("2000") },
			{ TEXT("include_hidden"), TEXT("boolean"), TEXT("Include hidden actors. Default: false"), false, TEXT("false") }
		};
	}
	
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
