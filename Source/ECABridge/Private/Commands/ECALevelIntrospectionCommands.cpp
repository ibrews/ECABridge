// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECALevelIntrospectionCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"

REGISTER_ECA_COMMAND(FECACommand_DumpLevel)

// Helper: serialize actor transform to JSON
static TSharedPtr<FJsonObject> ActorTransformToJson(AActor* Actor)
{
	TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
	FVector Location = Actor->GetActorLocation();
	FRotator Rotation = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Location.X);
	LocObj->SetNumberField(TEXT("y"), Location.Y);
	LocObj->SetNumberField(TEXT("z"), Location.Z);
	TransformObj->SetObjectField(TEXT("location"), LocObj);

	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
	TransformObj->SetObjectField(TEXT("rotation"), RotObj);

	TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
	ScaleObj->SetNumberField(TEXT("x"), Scale.X);
	ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
	ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
	TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

	return TransformObj;
}

// Helper: serialize component info
static TSharedPtr<FJsonObject> ComponentToJson(UActorComponent* Comp)
{
	TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
	CompObj->SetStringField(TEXT("name"), Comp->GetName());
	CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

	if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
	{
		CompObj->SetBoolField(TEXT("is_root"), SceneComp == SceneComp->GetOwner()->GetRootComponent());

		// Relative transform
		FVector RelLoc = SceneComp->GetRelativeLocation();
		if (!RelLoc.IsNearlyZero())
		{
			TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
			LocObj->SetNumberField(TEXT("x"), RelLoc.X);
			LocObj->SetNumberField(TEXT("y"), RelLoc.Y);
			LocObj->SetNumberField(TEXT("z"), RelLoc.Z);
			CompObj->SetObjectField(TEXT("relative_location"), LocObj);
		}

		CompObj->SetStringField(TEXT("mobility"),
			SceneComp->Mobility == EComponentMobility::Static ? TEXT("Static") :
			SceneComp->Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Movable"));
	}

	// Mesh-specific info
	if (UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp))
	{
		if (SMComp->GetStaticMesh())
		{
			CompObj->SetStringField(TEXT("static_mesh"), SMComp->GetStaticMesh()->GetPathName());
		}
		CompObj->SetNumberField(TEXT("material_count"), SMComp->GetNumMaterials());
	}
	else if (USkeletalMeshComponent* SKComp = Cast<USkeletalMeshComponent>(Comp))
	{
		if (SKComp->GetSkeletalMeshAsset())
		{
			CompObj->SetStringField(TEXT("skeletal_mesh"), SKComp->GetSkeletalMeshAsset()->GetPathName());
		}
	}
	else if (ULightComponent* LightComp = Cast<ULightComponent>(Comp))
	{
		CompObj->SetNumberField(TEXT("intensity"), LightComp->Intensity);
		FLinearColor Color = LightComp->GetLightColor();
		TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
		ColorObj->SetNumberField(TEXT("r"), Color.R);
		ColorObj->SetNumberField(TEXT("g"), Color.G);
		ColorObj->SetNumberField(TEXT("b"), Color.B);
		CompObj->SetObjectField(TEXT("color"), ColorObj);
	}

	return CompObj;
}

FECACommandResult FECACommand_DumpLevel::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world available"));
	}

	// Parse parameters
	int32 MaxActors = 500;
	GetIntParam(Params, TEXT("max_actors"), MaxActors, false);
	if (MaxActors == 0) MaxActors = INT_MAX;

	FString FilterClass;
	GetStringParam(Params, TEXT("filter_class"), FilterClass, false);

	FString FilterFolder;
	GetStringParam(Params, TEXT("filter_folder"), FilterFolder, false);

	FString FilterTag;
	GetStringParam(Params, TEXT("filter_tag"), FilterTag, false);

	bool bIncludeProperties = false;
	GetBoolParam(Params, TEXT("include_properties"), bIncludeProperties, false);

	bool bIncludeComponents = true;
	GetBoolParam(Params, TEXT("include_components"), bIncludeComponents, false);

	// Spatial bounds filter
	bool bHasBoundsFilter = false;
	FBox BoundsFilter(ForceInit);
	FVector BoundsMin, BoundsMax;
	if (GetVectorParam(Params, TEXT("bounds_min"), BoundsMin, false) &&
		GetVectorParam(Params, TEXT("bounds_max"), BoundsMax, false))
	{
		BoundsFilter = FBox(BoundsMin, BoundsMax);
		bHasBoundsFilter = true;
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// Level metadata
	TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
	MetaObj->SetStringField(TEXT("level_name"), World->GetCurrentLevel()->GetOuter()->GetName());
	MetaObj->SetStringField(TEXT("world_name"), World->GetName());

	// World bounds
	FBox WorldBounds(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* Actor = *It)
		{
			FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
			if (ActorBounds.IsValid)
			{
				WorldBounds += ActorBounds;
			}
		}
	}
	if (WorldBounds.IsValid)
	{
		TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> MinObj = MakeShared<FJsonObject>();
		MinObj->SetNumberField(TEXT("x"), WorldBounds.Min.X);
		MinObj->SetNumberField(TEXT("y"), WorldBounds.Min.Y);
		MinObj->SetNumberField(TEXT("z"), WorldBounds.Min.Z);
		TSharedPtr<FJsonObject> MaxObj = MakeShared<FJsonObject>();
		MaxObj->SetNumberField(TEXT("x"), WorldBounds.Max.X);
		MaxObj->SetNumberField(TEXT("y"), WorldBounds.Max.Y);
		MaxObj->SetNumberField(TEXT("z"), WorldBounds.Max.Z);
		BoundsObj->SetObjectField(TEXT("min"), MinObj);
		BoundsObj->SetObjectField(TEXT("max"), MaxObj);
		MetaObj->SetObjectField(TEXT("world_bounds"), BoundsObj);
	}
	Result->SetObjectField(TEXT("metadata"), MetaObj);

	// Iterate actors
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	TMap<FString, int32> ClassCounts;
	int32 TotalActors = 0;
	int32 FilteredActors = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsHidden()) continue;

		TotalActors++;
		FString ClassName = Actor->GetClass()->GetName();

		// Apply filters
		if (!FilterClass.IsEmpty())
		{
			bool bClassMatch = ClassName.Contains(FilterClass);
			if (!bClassMatch)
			{
				// Also check parent classes
				for (UClass* C = Actor->GetClass()->GetSuperClass(); C; C = C->GetSuperClass())
				{
					if (C->GetName().Contains(FilterClass))
					{
						bClassMatch = true;
						break;
					}
				}
			}
			if (!bClassMatch) continue;
		}

		if (!FilterFolder.IsEmpty())
		{
			FString Folder = Actor->GetFolderPath().ToString();
			if (!Folder.Contains(FilterFolder)) continue;
		}

		if (!FilterTag.IsEmpty())
		{
			bool bHasTag = false;
			for (const FName& Tag : Actor->Tags)
			{
				if (Tag.ToString().Contains(FilterTag))
				{
					bHasTag = true;
					break;
				}
			}
			if (!bHasTag) continue;
		}

		if (bHasBoundsFilter)
		{
			FVector ActorLoc = Actor->GetActorLocation();
			if (!BoundsFilter.IsInside(ActorLoc)) continue;
		}

		// Passed all filters
		FilteredActors++;
		ClassCounts.FindOrAdd(ClassName)++;

		if (ActorsArray.Num() >= MaxActors) continue; // Count but don't serialize beyond limit

		// Serialize actor
		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel().IsEmpty() ? Actor->GetName() : Actor->GetActorLabel());
		ActorObj->SetStringField(TEXT("internal_name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("class"), ClassName);
		ActorObj->SetObjectField(TEXT("transform"), ActorTransformToJson(Actor));

		// Folder
		FString Folder = Actor->GetFolderPath().ToString();
		if (!Folder.IsEmpty())
		{
			ActorObj->SetStringField(TEXT("folder"), Folder);
		}

		// Tags
		if (Actor->Tags.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TagsArray;
			for (const FName& Tag : Actor->Tags)
			{
				TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ActorObj->SetArrayField(TEXT("tags"), TagsArray);
		}

		// Mobility
		if (Actor->GetRootComponent())
		{
			ActorObj->SetStringField(TEXT("mobility"),
				Actor->GetRootComponent()->Mobility == EComponentMobility::Static ? TEXT("Static") :
				Actor->GetRootComponent()->Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Movable"));
		}

		// Attachment
		if (Actor->GetAttachParentActor())
		{
			ActorObj->SetStringField(TEXT("attached_to"), Actor->GetAttachParentActor()->GetName());
		}

		// Components
		if (bIncludeComponents)
		{
			TArray<TSharedPtr<FJsonValue>> ComponentsArray;
			TInlineComponentArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (!Comp) continue;
				ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentToJson(Comp)));
			}
			ActorObj->SetArrayField(TEXT("components"), ComponentsArray);
		}

		// Full properties (deep mode)
		if (bIncludeProperties)
		{
			TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
			UObject* DefaultActor = Actor->GetClass()->GetDefaultObject();
			for (TFieldIterator<FProperty> PropIt(Actor->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (!Property->HasAnyPropertyFlags(CPF_Edit)) continue;
				if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient)) continue;

				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
				void* DefaultPtr = Property->ContainerPtrToValuePtr<void>(DefaultActor);

				// Only non-default values
				if (Property->Identical(ValuePtr, DefaultPtr)) continue;

				FString StringValue;
				Property->ExportTextItem_Direct(StringValue, ValuePtr, DefaultPtr, Actor, PPF_None);
				if (!StringValue.IsEmpty())
				{
					PropsObj->SetStringField(Property->GetName(), StringValue);
				}
			}
			ActorObj->SetObjectField(TEXT("properties"), PropsObj);
		}

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	Result->SetArrayField(TEXT("actors"), ActorsArray);

	// Statistics
	TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
	StatsObj->SetNumberField(TEXT("total_actors_in_level"), TotalActors);
	StatsObj->SetNumberField(TEXT("actors_matching_filter"), FilteredActors);
	StatsObj->SetNumberField(TEXT("actors_returned"), ActorsArray.Num());
	if (ActorsArray.Num() < FilteredActors)
	{
		StatsObj->SetNumberField(TEXT("actors_truncated"), FilteredActors - ActorsArray.Num());
	}

	// Class distribution
	TSharedPtr<FJsonObject> ClassDistObj = MakeShared<FJsonObject>();
	ClassCounts.ValueSort([](const int32& A, const int32& B) { return A > B; });
	for (const auto& Pair : ClassCounts)
	{
		ClassDistObj->SetNumberField(Pair.Key, Pair.Value);
	}
	StatsObj->SetObjectField(TEXT("class_distribution"), ClassDistObj);

	Result->SetObjectField(TEXT("statistics"), StatsObj);

	return FECACommandResult::Success(Result);
}
