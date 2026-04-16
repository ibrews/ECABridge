// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASequencerCommands.h"

#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneObjectBindingID.h"

#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"

#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"

// ─── Helpers ───────────────────────────────────────────────────

namespace SequencerCommandHelpers
{
	/** Load a ULevelSequence by path, trying common path variants */
	static ULevelSequence* LoadSequence(const FString& SequencePath)
	{
		ULevelSequence* Seq = LoadObject<ULevelSequence>(nullptr, *SequencePath);
		if (!Seq)
		{
			FString FullPath = SequencePath;
			if (!FullPath.Contains(TEXT(".")))
			{
				FString AssetName = FPackageName::GetShortName(FullPath);
				FullPath = FullPath + TEXT(".") + AssetName;
			}
			Seq = LoadObject<ULevelSequence>(nullptr, *FullPath);
		}
		return Seq;
	}

	/** Find the binding GUID for an actor by name in the sequence's possessables */
	static bool FindActorBinding(ULevelSequence* Seq, UMovieScene* MovieScene, const FString& ActorName, FGuid& OutGuid)
	{
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			FString BindingName = MovieScene->GetObjectDisplayName(Binding.GetObjectGuid()).ToString();
			if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase))
			{
				OutGuid = Binding.GetObjectGuid();
				return true;
			}
		}

		// Also check possessable names directly
		for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
		{
			const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
			if (Possessable.GetName().Equals(ActorName, ESearchCase::IgnoreCase))
			{
				OutGuid = Possessable.GetGuid();
				return true;
			}
		}

		return false;
	}
}

// ─── REGISTER ──────────────────────────────────────────────────

REGISTER_ECA_COMMAND(FECACommand_CreateLevelSequence);
REGISTER_ECA_COMMAND(FECACommand_AddSequenceActorBinding);
REGISTER_ECA_COMMAND(FECACommand_AddSequenceTransformKey);
REGISTER_ECA_COMMAND(FECACommand_AddSequenceCamera);
REGISTER_ECA_COMMAND(FECACommand_PlaySequence);
REGISTER_ECA_COMMAND(FECACommand_GetSequenceInfo);
REGISTER_ECA_COMMAND(FECACommand_SetCameraProperties);
REGISTER_ECA_COMMAND(FECACommand_AddSequenceFloatKey);

// ─── create_level_sequence ─────────────────────────────────────

FECACommandResult FECACommand_CreateLevelSequence::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PackagePath, AssetName;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: package_path"));
	if (!GetStringParam(Params, TEXT("asset_name"), AssetName))
		return FECACommandResult::Error(TEXT("Missing required parameter: asset_name"));

	double FrameRate = 30.0;
	GetFloatParam(Params, TEXT("frame_rate"), FrameRate, false);
	if (FrameRate <= 0.0)
		return FECACommandResult::Error(TEXT("frame_rate must be a positive number"));

	// Build the full package name
	FString FullPackagePath = PackagePath / AssetName;

	// Create the package
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package at: %s"), *FullPackagePath));

	Package->FullyLoad();

	// Create the ULevelSequence asset
	ULevelSequence* NewSequence = NewObject<ULevelSequence>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!NewSequence)
		return FECACommandResult::Error(TEXT("Failed to create ULevelSequence object"));

	NewSequence->Initialize();

	// Configure the movie scene
	UMovieScene* MovieScene = NewSequence->GetMovieScene();
	if (MovieScene)
	{
		// Set the display rate
		FFrameRate DisplayRate(static_cast<int32>(FrameRate), 1);
		MovieScene->SetDisplayRate(DisplayRate);

		// Set a default playback range (5 seconds)
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameNumber StartFrame = 0;
		int32 DurationFrames = (TickResolution / DisplayRate).AsDecimal() * static_cast<int32>(FrameRate) * 5; // 5 seconds
		MovieScene->SetPlaybackRange(StartFrame, DurationFrames);
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewSequence);
	Package->MarkPackageDirty();

	// Save the asset
	FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewSequence, *PackageFilename, SaveArgs);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("path"), NewSequence->GetPathName());
	Result->SetStringField(TEXT("name"), NewSequence->GetName());
	if (MovieScene)
	{
		Result->SetNumberField(TEXT("display_rate"), MovieScene->GetDisplayRate().AsDecimal());
		Result->SetNumberField(TEXT("tick_resolution"), MovieScene->GetTickResolution().AsDecimal());
	}

	return FECACommandResult::Success(Result);
}

// ─── add_sequence_actor_binding ────────────────────────────────

FECACommandResult FECACommand_AddSequenceActorBinding::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath, ActorName;
	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UMovieScene* MovieScene = Seq->GetMovieScene();
	if (!MovieScene)
		return FECACommandResult::Error(TEXT("Level Sequence has no MovieScene"));

	// Find the actor in the editor world
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *ActorName));

	// Check if actor is already bound
	FGuid ExistingGuid;
	if (SequencerCommandHelpers::FindActorBinding(Seq, MovieScene, ActorName, ExistingGuid))
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("binding_guid"), ExistingGuid.ToString());
		Result->SetStringField(TEXT("actor_name"), ActorName);
		Result->SetBoolField(TEXT("already_bound"), true);
		return FECACommandResult::Success(Result);
	}

	// Create a possessable binding for the actor
	FGuid NewGuid = MovieScene->AddPossessable(Actor->GetActorLabel(), Actor->GetClass());
	if (!NewGuid.IsValid())
		return FECACommandResult::Error(TEXT("Failed to create possessable binding in MovieScene"));

	// Bind the possessable to the actual actor in the level
	UWorld* World = GetEditorWorld();
	if (World)
	{
		Seq->BindPossessableObject(NewGuid, *Actor, World);
	}

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("binding_guid"), NewGuid.ToString());
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());
	Result->SetBoolField(TEXT("already_bound"), false);

	return FECACommandResult::Success(Result);
}

// ─── add_sequence_transform_key ────────────────────────────────

FECACommandResult FECACommand_AddSequenceTransformKey::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath, ActorName;
	double Time = 0.0;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;

	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetFloatParam(Params, TEXT("time"), Time))
		return FECACommandResult::Error(TEXT("Missing required parameter: time"));
	if (!GetVectorParam(Params, TEXT("location"), Location))
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	if (!GetRotatorParam(Params, TEXT("rotation"), Rotation))
		return FECACommandResult::Error(TEXT("Missing required parameter: rotation"));

	// Scale is optional, defaults to (1,1,1)
	GetVectorParam(Params, TEXT("scale"), Scale, false);

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UMovieScene* MovieScene = Seq->GetMovieScene();
	if (!MovieScene)
		return FECACommandResult::Error(TEXT("Level Sequence has no MovieScene"));

	// Find the binding for this actor
	FGuid BindingGuid;
	if (!SequencerCommandHelpers::FindActorBinding(Seq, MovieScene, ActorName, BindingGuid))
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' is not bound in the sequence. Use add_sequence_actor_binding first."), *ActorName));

	// Find or create the 3D transform track for this binding
	UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(BindingGuid);
	if (!TransformTrack)
	{
		TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
		if (!TransformTrack)
			return FECACommandResult::Error(TEXT("Failed to create 3D Transform track"));
	}

	// Get or create a section on the track
	UMovieScene3DTransformSection* TransformSection = nullptr;
	const TArray<UMovieSceneSection*>& Sections = TransformTrack->GetAllSections();
	if (Sections.Num() > 0)
	{
		TransformSection = Cast<UMovieScene3DTransformSection>(Sections[0]);
	}

	if (!TransformSection)
	{
		UMovieSceneSection* NewSection = TransformTrack->CreateNewSection();
		TransformSection = Cast<UMovieScene3DTransformSection>(NewSection);
		if (!TransformSection)
			return FECACommandResult::Error(TEXT("Failed to create transform section"));

		TransformTrack->AddSection(*TransformSection);

		// Set the section to span the full playback range
		TransformSection->SetRange(MovieScene->GetPlaybackRange());
	}

	// Convert time in seconds to FFrameNumber using the tick resolution
	FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameNumber FrameNumber = (Time * TickResolution).FloorToFrame();

	// Expand section range if needed to contain this key
	TRange<FFrameNumber> SectionRange = TransformSection->GetRange();
	if (!SectionRange.Contains(FrameNumber))
	{
		if (SectionRange.HasLowerBound() && FrameNumber < SectionRange.GetLowerBoundValue())
		{
			TransformSection->SetRange(TRange<FFrameNumber>(FrameNumber, SectionRange.GetUpperBound()));
		}
		else if (SectionRange.HasUpperBound() && FrameNumber >= SectionRange.GetUpperBoundValue())
		{
			TransformSection->SetRange(TRange<FFrameNumber>(SectionRange.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(FrameNumber + 1)));
		}
	}

	// Access the channels via the channel proxy
	// UMovieScene3DTransformSection has Translation[3], Rotation[3], Scale[3] as FMovieSceneDoubleChannel
	// We access them through the channel proxy which orders them:
	// [0-2] Translation X,Y,Z  [3-5] Rotation X,Y,Z  [6-8] Scale X,Y,Z
	TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();

	if (DoubleChannels.Num() < 9)
		return FECACommandResult::Error(FString::Printf(TEXT("Expected at least 9 double channels on transform section, found %d"), DoubleChannels.Num()));

	// Add translation keys (X, Y, Z)
	DoubleChannels[0]->AddLinearKey(FrameNumber, Location.X);
	DoubleChannels[1]->AddLinearKey(FrameNumber, Location.Y);
	DoubleChannels[2]->AddLinearKey(FrameNumber, Location.Z);

	// Add rotation keys (Roll=X, Pitch=Y, Yaw=Z) - UE stores rotation as X=Roll, Y=Pitch, Z=Yaw in channels
	DoubleChannels[3]->AddLinearKey(FrameNumber, Rotation.Roll);
	DoubleChannels[4]->AddLinearKey(FrameNumber, Rotation.Pitch);
	DoubleChannels[5]->AddLinearKey(FrameNumber, Rotation.Yaw);

	// Add scale keys (X, Y, Z)
	DoubleChannels[6]->AddLinearKey(FrameNumber, Scale.X);
	DoubleChannels[7]->AddLinearKey(FrameNumber, Scale.Y);
	DoubleChannels[8]->AddLinearKey(FrameNumber, Scale.Z);

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetNumberField(TEXT("time_seconds"), Time);
	Result->SetNumberField(TEXT("frame_number"), FrameNumber.Value);
	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));
	Result->SetObjectField(TEXT("scale"), VectorToJson(Scale));

	return FECACommandResult::Success(Result);
}

// ─── add_sequence_camera ───────────────────────────────────────

FECACommandResult FECACommand_AddSequenceCamera::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath, CameraName;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));
	if (!GetStringParam(Params, TEXT("camera_name"), CameraName))
		return FECACommandResult::Error(TEXT("Missing required parameter: camera_name"));
	if (!GetVectorParam(Params, TEXT("location"), Location))
		return FECACommandResult::Error(TEXT("Missing required parameter: location"));
	if (!GetRotatorParam(Params, TEXT("rotation"), Rotation))
		return FECACommandResult::Error(TEXT("Missing required parameter: rotation"));

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UMovieScene* MovieScene = Seq->GetMovieScene();
	if (!MovieScene)
		return FECACommandResult::Error(TEXT("Level Sequence has no MovieScene"));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	// Spawn a CineCameraActor in the level
	FActorSpawnParameters SpawnParams;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = FName(*CameraName);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedCamera = World->SpawnActor(ACineCameraActor::StaticClass(), &Location, &Rotation, SpawnParams);
	ACineCameraActor* CameraActor = Cast<ACineCameraActor>(SpawnedCamera);
	if (!CameraActor)
		return FECACommandResult::Error(TEXT("Failed to spawn CineCameraActor"));

	CameraActor->SetActorLabel(CameraName);

	// Create a possessable binding for the camera
	FGuid CameraGuid = MovieScene->AddPossessable(CameraName, ACineCameraActor::StaticClass());
	if (!CameraGuid.IsValid())
		return FECACommandResult::Error(TEXT("Failed to create possessable binding for camera"));

	// Bind the possessable to the camera actor
	Seq->BindPossessableObject(CameraGuid, *CameraActor, World);

	// Create or get the camera cut track
	UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
	if (!CameraCutTrack)
	{
		CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
	}

	if (!CameraCutTrack)
		return FECACommandResult::Error(TEXT("Failed to create camera cut track"));

	// Add a camera cut section pointing to this camera, starting at frame 0
	FFrameNumber StartFrame = MovieScene->GetPlaybackRange().GetLowerBoundValue();
	UE::MovieScene::FRelativeObjectBindingID RelativeID{CameraGuid};
	FMovieSceneObjectBindingID CameraBindingID{RelativeID};
	UMovieSceneCameraCutSection* CutSection = CameraCutTrack->AddNewCameraCut(CameraBindingID, StartFrame);
	if (!CutSection)
		return FECACommandResult::Error(TEXT("Failed to add camera cut section"));

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("camera_name"), CameraActor->GetActorLabel());
	Result->SetStringField(TEXT("camera_path"), CameraActor->GetPathName());
	Result->SetStringField(TEXT("binding_guid"), CameraGuid.ToString());
	Result->SetObjectField(TEXT("location"), VectorToJson(CameraActor->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(CameraActor->GetActorRotation()));
	Result->SetBoolField(TEXT("camera_cut_track_created"), true);

	return FECACommandResult::Success(Result);
}

// ─── play_sequence ─────────────────────────────────────────────

FECACommandResult FECACommand_PlaySequence::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath;
	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UWorld* World = GetEditorWorld();
	if (!World)
		return FECACommandResult::Error(TEXT("No editor world available"));

	// Look for an existing LevelSequenceActor playing this sequence, or spawn one
	ALevelSequenceActor* SeqActor = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		if (It->GetSequence() == Seq)
		{
			SeqActor = *It;
			break;
		}
	}

	if (!SeqActor)
	{
		// Spawn a new LevelSequenceActor
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SeqActor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);
		if (!SeqActor)
			return FECACommandResult::Error(TEXT("Failed to spawn LevelSequenceActor"));

		SeqActor->SetSequence(Seq);
	}

	// Create or get the player and play
	ULevelSequencePlayer* Player = SeqActor->GetSequencePlayer();
	if (!Player)
		return FECACommandResult::Error(TEXT("Failed to get sequence player from LevelSequenceActor"));

	Player->Play();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("sequence"), Seq->GetPathName());
	Result->SetStringField(TEXT("state"), TEXT("playing"));

	return FECACommandResult::Success(Result);
}

// ─── get_sequence_info ─────────────────────────────────────────

FECACommandResult FECACommand_GetSequenceInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath;
	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UMovieScene* MovieScene = Seq->GetMovieScene();
	if (!MovieScene)
		return FECACommandResult::Error(TEXT("Level Sequence has no MovieScene"));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), Seq->GetName());
	Result->SetStringField(TEXT("path"), Seq->GetPathName());

	// Frame rate info
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	FFrameRate TickResolution = MovieScene->GetTickResolution();
	Result->SetNumberField(TEXT("display_rate"), DisplayRate.AsDecimal());
	Result->SetNumberField(TEXT("tick_resolution"), TickResolution.AsDecimal());

	// Duration
	TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	if (PlaybackRange.HasLowerBound() && PlaybackRange.HasUpperBound())
	{
		FFrameNumber DurationFrames = PlaybackRange.GetUpperBoundValue() - PlaybackRange.GetLowerBoundValue();
		double DurationSeconds = static_cast<double>(DurationFrames.Value) / TickResolution.AsDecimal();
		Result->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
		Result->SetNumberField(TEXT("duration_frames"), DurationFrames.Value);
		Result->SetNumberField(TEXT("start_frame"), PlaybackRange.GetLowerBoundValue().Value);
		Result->SetNumberField(TEXT("end_frame"), PlaybackRange.GetUpperBoundValue().Value);
	}

	// Bindings (possessables)
	TArray<TSharedPtr<FJsonValue>> BindingsArray;
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
		BindingObj->SetStringField(TEXT("guid"), Binding.GetObjectGuid().ToString());
		BindingObj->SetStringField(TEXT("name"), MovieScene->GetObjectDisplayName(Binding.GetObjectGuid()).ToString());

		// Check if it's a possessable and get the class
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
		if (Possessable)
		{
			BindingObj->SetStringField(TEXT("type"), TEXT("Possessable"));
			const UClass* BoundClass = Possessable->GetPossessedObjectClass();
			if (BoundClass)
			{
				BindingObj->SetStringField(TEXT("class"), BoundClass->GetName());
			}
		}
		else
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Binding.GetObjectGuid());
			if (Spawnable)
			{
				BindingObj->SetStringField(TEXT("type"), TEXT("Spawnable"));
				BindingObj->SetStringField(TEXT("spawnable_name"), Spawnable->GetName());
			}
		}

		// List tracks on this binding
		TArray<TSharedPtr<FJsonValue>> TracksArray;
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (!Track) continue;
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("class"), Track->GetClass()->GetName());
			TrackObj->SetStringField(TEXT("display_name"), Track->GetDisplayName().ToString());
			TrackObj->SetNumberField(TEXT("section_count"), Track->GetAllSections().Num());
			TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		BindingObj->SetArrayField(TEXT("tracks"), TracksArray);

		BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
	}
	Result->SetArrayField(TEXT("bindings"), BindingsArray);

	// Top-level (unbound) tracks
	TArray<TSharedPtr<FJsonValue>> TopLevelTracks;
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		if (!Track) continue;
		TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
		TrackObj->SetStringField(TEXT("class"), Track->GetClass()->GetName());
		TrackObj->SetStringField(TEXT("display_name"), Track->GetDisplayName().ToString());
		TrackObj->SetNumberField(TEXT("section_count"), Track->GetAllSections().Num());
		TopLevelTracks.Add(MakeShared<FJsonValueObject>(TrackObj));
	}
	Result->SetArrayField(TEXT("top_level_tracks"), TopLevelTracks);

	// Camera cut track info
	UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
	if (CameraCutTrack)
	{
		Result->SetBoolField(TEXT("has_camera_cut_track"), true);
		Result->SetNumberField(TEXT("camera_cut_section_count"), CameraCutTrack->GetAllSections().Num());
	}
	else
	{
		Result->SetBoolField(TEXT("has_camera_cut_track"), false);
	}

	// Counts
	Result->SetNumberField(TEXT("possessable_count"), MovieScene->GetPossessableCount());
	Result->SetNumberField(TEXT("spawnable_count"), MovieScene->GetSpawnableCount());
	Result->SetNumberField(TEXT("binding_count"), Bindings.Num());

	return FECACommandResult::Success(Result);
}

// ─── set_camera_properties ────────────────────────────────────

FECACommandResult FECACommand_SetCameraProperties::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString CameraName;
	if (!GetStringParam(Params, TEXT("camera_name"), CameraName))
		return FECACommandResult::Error(TEXT("Missing required parameter: camera_name"));

	// At least one optional property must be provided
	double FOV = 0.0, FocalLength = 0.0, Aperture = 0.0, FocusDistance = 0.0, SensorWidth = 0.0;
	bool bHasFOV = GetFloatParam(Params, TEXT("fov"), FOV, false);
	bool bHasFocalLength = GetFloatParam(Params, TEXT("focal_length"), FocalLength, false);
	bool bHasAperture = GetFloatParam(Params, TEXT("aperture"), Aperture, false);
	bool bHasFocusDistance = GetFloatParam(Params, TEXT("focus_distance"), FocusDistance, false);
	bool bHasSensorWidth = GetFloatParam(Params, TEXT("sensor_width"), SensorWidth, false);

	if (!bHasFOV && !bHasFocalLength && !bHasAperture && !bHasFocusDistance && !bHasSensorWidth)
		return FECACommandResult::Error(TEXT("At least one camera property must be specified (fov, focal_length, aperture, focus_distance, sensor_width)"));

	// Find the camera actor in the level
	AActor* Actor = FindActorByName(CameraName);
	if (!Actor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' not found in the level"), *CameraName));

	// Try as CineCameraActor first (has full lens settings)
	ACineCameraActor* CineCamera = Cast<ACineCameraActor>(Actor);
	ACameraActor* CameraActor = Cast<ACameraActor>(Actor);

	if (!CineCamera && !CameraActor)
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' is not a CameraActor or CineCameraActor (class: %s)"), *CameraName, *Actor->GetClass()->GetName()));

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("camera_name"), CameraName);

	if (CineCamera)
	{
		UCineCameraComponent* CineComp = CineCamera->GetCineCameraComponent();
		if (!CineComp)
			return FECACommandResult::Error(TEXT("CineCameraActor has no CineCameraComponent"));

		if (bHasFOV)
		{
			CineComp->SetFieldOfView(static_cast<float>(FOV));
			Result->SetNumberField(TEXT("fov"), FOV);
		}
		if (bHasFocalLength)
		{
			CineComp->CurrentFocalLength = static_cast<float>(FocalLength);
			Result->SetNumberField(TEXT("focal_length"), FocalLength);
		}
		if (bHasAperture)
		{
			CineComp->CurrentAperture = static_cast<float>(Aperture);
			Result->SetNumberField(TEXT("aperture"), Aperture);
		}
		if (bHasFocusDistance)
		{
			CineComp->FocusSettings.ManualFocusDistance = static_cast<float>(FocusDistance);
			Result->SetNumberField(TEXT("focus_distance"), FocusDistance);
		}
		if (bHasSensorWidth)
		{
			CineComp->Filmback.SensorWidth = static_cast<float>(SensorWidth);
			Result->SetNumberField(TEXT("sensor_width"), SensorWidth);
		}

		Result->SetStringField(TEXT("camera_type"), TEXT("CineCameraActor"));
	}
	else
	{
		// Regular CameraActor - only FOV is applicable
		UCameraComponent* CamComp = CameraActor->GetCameraComponent();
		if (!CamComp)
			return FECACommandResult::Error(TEXT("CameraActor has no CameraComponent"));

		if (bHasFOV)
		{
			CamComp->SetFieldOfView(static_cast<float>(FOV));
			Result->SetNumberField(TEXT("fov"), FOV);
		}

		if (bHasFocalLength || bHasAperture || bHasFocusDistance || bHasSensorWidth)
		{
			Result->SetStringField(TEXT("warning"), TEXT("focal_length, aperture, focus_distance, and sensor_width only apply to CineCameraActor; those properties were ignored"));
		}

		Result->SetStringField(TEXT("camera_type"), TEXT("CameraActor"));
	}

	return FECACommandResult::Success(Result);
}

// ─── add_sequence_float_key ───────────────────────────────────

FECACommandResult FECACommand_AddSequenceFloatKey::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath, ActorName, PropertyPath;
	double Time = 0.0, Value = 0.0;

	if (!GetStringParam(Params, TEXT("sequence_path"), SequencePath))
		return FECACommandResult::Error(TEXT("Missing required parameter: sequence_path"));
	if (!GetStringParam(Params, TEXT("actor_name"), ActorName))
		return FECACommandResult::Error(TEXT("Missing required parameter: actor_name"));
	if (!GetStringParam(Params, TEXT("property_path"), PropertyPath))
		return FECACommandResult::Error(TEXT("Missing required parameter: property_path"));
	if (!GetFloatParam(Params, TEXT("time"), Time))
		return FECACommandResult::Error(TEXT("Missing required parameter: time"));
	if (!GetFloatParam(Params, TEXT("value"), Value))
		return FECACommandResult::Error(TEXT("Missing required parameter: value"));

	ULevelSequence* Seq = SequencerCommandHelpers::LoadSequence(SequencePath);
	if (!Seq)
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load Level Sequence at: %s"), *SequencePath));

	UMovieScene* MovieScene = Seq->GetMovieScene();
	if (!MovieScene)
		return FECACommandResult::Error(TEXT("Level Sequence has no MovieScene"));

	// Find the binding for this actor
	FGuid BindingGuid;
	if (!SequencerCommandHelpers::FindActorBinding(Seq, MovieScene, ActorName, BindingGuid))
		return FECACommandResult::Error(FString::Printf(TEXT("Actor '%s' is not bound in the sequence. Use add_sequence_actor_binding first."), *ActorName));

	// Look for an existing float track with a matching property path
	UMovieSceneFloatTrack* FloatTrack = nullptr;
	const FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingGuid);
	if (!Binding)
		return FECACommandResult::Error(FString::Printf(TEXT("No binding found for actor '%s'"), *ActorName));

	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		UMovieSceneFloatTrack* Candidate = Cast<UMovieSceneFloatTrack>(Track);
		if (Candidate)
		{
			FName TrackPropertyName = Candidate->GetPropertyName();
			if (TrackPropertyName.ToString().Equals(PropertyPath, ESearchCase::IgnoreCase))
			{
				FloatTrack = Candidate;
				break;
			}
		}
	}

	// If no existing track, create one
	if (!FloatTrack)
	{
		FloatTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
		if (!FloatTrack)
			return FECACommandResult::Error(TEXT("Failed to create float track"));

		FloatTrack->SetPropertyNameAndPath(FName(*PropertyPath), PropertyPath);
	}

	// Get or create a section on the track
	UMovieSceneFloatSection* FloatSection = nullptr;
	const TArray<UMovieSceneSection*>& Sections = FloatTrack->GetAllSections();
	if (Sections.Num() > 0)
	{
		FloatSection = Cast<UMovieSceneFloatSection>(Sections[0]);
	}

	if (!FloatSection)
	{
		UMovieSceneSection* NewSection = FloatTrack->CreateNewSection();
		FloatSection = Cast<UMovieSceneFloatSection>(NewSection);
		if (!FloatSection)
			return FECACommandResult::Error(TEXT("Failed to create float section"));

		FloatTrack->AddSection(*FloatSection);
		FloatSection->SetRange(MovieScene->GetPlaybackRange());
	}

	// Convert time in seconds to FFrameNumber
	FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameNumber FrameNumber = (Time * TickResolution).FloorToFrame();

	// Expand section range if needed
	TRange<FFrameNumber> SectionRange = FloatSection->GetRange();
	if (!SectionRange.Contains(FrameNumber))
	{
		if (SectionRange.HasLowerBound() && FrameNumber < SectionRange.GetLowerBoundValue())
		{
			FloatSection->SetRange(TRange<FFrameNumber>(FrameNumber, SectionRange.GetUpperBound()));
		}
		else if (SectionRange.HasUpperBound() && FrameNumber >= SectionRange.GetUpperBoundValue())
		{
			FloatSection->SetRange(TRange<FFrameNumber>(SectionRange.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(FrameNumber + 1)));
		}
	}

	// Access the float channel and add the key
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = FloatSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	if (FloatChannels.Num() < 1)
		return FECACommandResult::Error(TEXT("Float section has no float channels"));

	FloatChannels[0]->AddLinearKey(FrameNumber, static_cast<float>(Value));

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("property_path"), PropertyPath);
	Result->SetNumberField(TEXT("time_seconds"), Time);
	Result->SetNumberField(TEXT("frame_number"), FrameNumber.Value);
	Result->SetNumberField(TEXT("value"), Value);

	return FECACommandResult::Success(Result);
}
