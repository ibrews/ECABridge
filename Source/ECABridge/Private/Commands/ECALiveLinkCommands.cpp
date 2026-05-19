// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECALiveLinkCommands.h"

#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "LiveLinkRole.h"
#include "UObject/Class.h"

namespace
{
	ILiveLinkClient* GetLiveLinkClientIfAvailable()
	{
		IModularFeatures& Features = IModularFeatures::Get();
		if (!Features.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			return nullptr;
		}
		return &Features.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	}

	const TCHAR* SubjectStateToString(ELiveLinkSubjectState State)
	{
		switch (State)
		{
			case ELiveLinkSubjectState::Connected:        return TEXT("connected");
			case ELiveLinkSubjectState::Unresponsive:     return TEXT("unresponsive");
			case ELiveLinkSubjectState::Disconnected:     return TEXT("disconnected");
			case ELiveLinkSubjectState::InvalidOrDisabled:return TEXT("invalid_or_disabled");
			case ELiveLinkSubjectState::Paused:           return TEXT("paused");
			case ELiveLinkSubjectState::Unknown:          default: return TEXT("unknown");
		}
	}

	TSharedPtr<FJsonObject> SubjectKeyToJson(ILiveLinkClient& Client, const FLiveLinkSubjectKey& Key)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Key.SubjectName.ToString());
		Obj->SetStringField(TEXT("source_guid"), Key.Source.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("source_machine"), Client.GetSourceMachineName(Key.Source).ToString());
		Obj->SetStringField(TEXT("source_type"), Client.GetSourceType(Key.Source).ToString());

		const TSubclassOf<ULiveLinkRole> Role = Client.GetSubjectRole_AnyThread(Key);
		Obj->SetStringField(TEXT("role"), Role ? Role->GetName() : FString(TEXT("")));

		Obj->SetBoolField(TEXT("is_virtual"), Client.IsVirtualSubject(Key));
		Obj->SetBoolField(TEXT("is_enabled"), Client.IsSubjectEnabled(Key, /*bForThisFrame*/ false));
		Obj->SetStringField(TEXT("state"), SubjectStateToString(Client.GetSubjectState(Key.SubjectName)));
		return Obj;
	}
}

FECACommandResult FECACommand_ListLiveLinkSubjects::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	ILiveLinkClient* Client = GetLiveLinkClientIfAvailable();
	if (!Client)
	{
		Result->SetBoolField(TEXT("available"), false);
		Result->SetStringField(TEXT("message"), TEXT("ILiveLinkClient modular feature is not registered. Is the LiveLink plugin enabled in this project?"));
		Result->SetArrayField(TEXT("subjects"), TArray<TSharedPtr<FJsonValue>>());
		return FECACommandResult::Success(Result);
	}

	bool bIncludeDisabled = true;
	bool bIncludeVirtual = true;
	if (Params.IsValid())
	{
		Params->TryGetBoolField(TEXT("include_disabled"), bIncludeDisabled);
		Params->TryGetBoolField(TEXT("include_virtual"), bIncludeVirtual);
	}

	const TArray<FLiveLinkSubjectKey> Subjects = Client->GetSubjects(bIncludeDisabled, bIncludeVirtual);

	TArray<TSharedPtr<FJsonValue>> SubjectArr;
	SubjectArr.Reserve(Subjects.Num());
	for (const FLiveLinkSubjectKey& Key : Subjects)
	{
		SubjectArr.Add(MakeShared<FJsonValueObject>(SubjectKeyToJson(*Client, Key)));
	}

	Result->SetBoolField(TEXT("available"), true);
	Result->SetNumberField(TEXT("count"), SubjectArr.Num());
	Result->SetArrayField(TEXT("subjects"), SubjectArr);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListLiveLinkSubjects);

FECACommandResult FECACommand_DumpLiveLinkData::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SubjectName;
	if (!GetStringParam(Params, TEXT("subject_name"), SubjectName, /*bRequired*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("subject_name is required"));
	}

	int32 RecentFrames = 5;
	if (Params.IsValid())
	{
		double Tmp = 5.0;
		if (Params->TryGetNumberField(TEXT("recent_frames"), Tmp))
		{
			RecentFrames = FMath::Clamp(FMath::FloorToInt(Tmp), 0, 200);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	ILiveLinkClient* Client = GetLiveLinkClientIfAvailable();
	if (!Client)
	{
		Result->SetBoolField(TEXT("available"), false);
		Result->SetStringField(TEXT("message"), TEXT("ILiveLinkClient modular feature is not registered. Is the LiveLink plugin enabled in this project?"));
		return FECACommandResult::Success(Result);
	}

	const FLiveLinkSubjectName SubjectFName(*SubjectName);
	const TArray<FLiveLinkSubjectKey> Candidates = Client->GetSubjects(/*bIncludeDisabled*/ true, /*bIncludeVirtual*/ true);

	const FLiveLinkSubjectKey* MatchedKey = nullptr;
	for (const FLiveLinkSubjectKey& Key : Candidates)
	{
		if (Key.SubjectName == SubjectFName)
		{
			MatchedKey = &Key;
			break;
		}
	}

	if (!MatchedKey)
	{
		Result->SetBoolField(TEXT("available"), true);
		Result->SetBoolField(TEXT("found"), false);
		Result->SetStringField(TEXT("subject_name"), SubjectName);
		Result->SetStringField(TEXT("message"), FString::Printf(TEXT("No LiveLink subject named '%s' is currently registered."), *SubjectName));
		return FECACommandResult::Success(Result);
	}

	Result->SetBoolField(TEXT("available"), true);
	Result->SetBoolField(TEXT("found"), true);
	Result->SetObjectField(TEXT("subject"), SubjectKeyToJson(*Client, *MatchedKey));

	const TArray<FLiveLinkTime> FrameTimes = Client->GetSubjectFrameTimes(*MatchedKey);
	TArray<TSharedPtr<FJsonValue>> TimeArr;
	const int32 Start = FMath::Max(0, FrameTimes.Num() - RecentFrames);
	for (int32 i = Start; i < FrameTimes.Num(); ++i)
	{
		TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("world_time_seconds"), FrameTimes[i].WorldTime);
		T->SetNumberField(TEXT("scene_time_seconds"), FrameTimes[i].SceneTime.AsSeconds());
		TimeArr.Add(MakeShared<FJsonValueObject>(T));
	}
	Result->SetNumberField(TEXT("total_frame_times"), FrameTimes.Num());
	Result->SetArrayField(TEXT("recent_frame_times"), TimeArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DumpLiveLinkData);
