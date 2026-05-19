// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAStageActorCommands.h"

#if WITH_ECA_NDISPLAY

#include "DisplayClusterLightCardActor.h"
#include "DisplayClusterRootActor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

FECACommandResult FECACommand_SpawnLightCard::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FVector Location;
	if (!GetVectorParam(Params, TEXT("location"), Location, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("location {x,y,z} is required"));
	}

	FRotator Rotation = FRotator::ZeroRotator;
	GetRotatorParam(Params, TEXT("rotation"), Rotation, /*bRequired*/ false);

	FString Label = TEXT("LightCard");
	GetStringParam(Params, TEXT("label"), Label, /*bRequired*/ false);

	FString AttachPath;
	GetStringParam(Params, TEXT("attach_to_root_actor"), AttachPath, /*bRequired*/ false);

	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!World)
	{
		return FECACommandResult::Error(TEXT("No editor world is currently loaded."));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADisplayClusterLightCardActor* LightCard = World->SpawnActor<ADisplayClusterLightCardActor>(
		ADisplayClusterLightCardActor::StaticClass(),
		Location,
		Rotation,
		SpawnParams);

	if (!LightCard)
	{
		return FECACommandResult::Error(TEXT("Failed to spawn ADisplayClusterLightCardActor. Is the nDisplay plugin enabled?"));
	}

	if (!Label.IsEmpty())
	{
		LightCard->SetActorLabel(Label);
	}

	bool bAttached = false;
	if (!AttachPath.IsEmpty())
	{
		for (TActorIterator<ADisplayClusterRootActor> It(World); It; ++It)
		{
			ADisplayClusterRootActor* Root = *It;
			if (!Root) continue;
			if (Root->GetPathName() == AttachPath || Root->GetName() == AttachPath || Root->GetActorNameOrLabel() == AttachPath)
			{
				LightCard->AttachToActor(Root, FAttachmentTransformRules::KeepWorldTransform);
				bAttached = true;
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("actor_path"), LightCard->GetPathName());
	Result->SetStringField(TEXT("actor_name"), LightCard->GetName());
	Result->SetStringField(TEXT("label"), LightCard->GetActorNameOrLabel());
	Result->SetBoolField(TEXT("attached_to_root_actor"), bAttached);
	if (!AttachPath.IsEmpty() && !bAttached)
	{
		Result->SetStringField(TEXT("attach_warning"),
			FString::Printf(TEXT("attach_to_root_actor='%s' did not match any ADisplayClusterRootActor; card spawned unattached."), *AttachPath));
	}

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_SpawnLightCard);

#endif // WITH_ECA_NDISPLAY
