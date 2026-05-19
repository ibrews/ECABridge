// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECADiagnosticBundleCommands.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_CaptureDiagnosticBundle)

static FString TailFile(const FString& Path, int32 NumLines)
{
	FString Whole;
	if (!FFileHelper::LoadFileToString(Whole, *Path))
	{
		return FString();
	}
	TArray<FString> Lines;
	Whole.ParseIntoArrayLines(Lines, false);
	const int32 Start = FMath::Max(0, Lines.Num() - NumLines);
	FString Tail;
	for (int32 i = Start; i < Lines.Num(); ++i)
	{
		Tail += Lines[i];
		Tail += TEXT("\n");
	}
	return Tail;
}

FECACommandResult FECACommand_CaptureDiagnosticBundle::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Label;
	if (!GetStringParam(Params, TEXT("label"), Label))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: label"));
	}
	double LogTailD = 500.0;
	GetFloatParam(Params, TEXT("log_tail_lines"), LogTailD, false);
	int32 LogTailLines = FMath::Max(0, (int32)LogTailD);
	double TopND = 20.0;
	GetFloatParam(Params, TEXT("top_n_actors"), TopND, false);
	int32 TopNActors = FMath::Max(0, (int32)TopND);

	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString BundleDir = FPaths::ProjectSavedDir() / TEXT("Profiling/Bundles") / (Stamp + TEXT("-") + Label);
	IFileManager::Get().MakeDirectory(*BundleDir, true);

	TArray<FString> Files;

	// 1. Screenshot (queued — may not exist yet by the time the bundle is read).
	const FString ScreenshotPath = BundleDir / TEXT("screenshot.png");
	{
		FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
		Config.FilenameOverride = ScreenshotPath;
		Config.SetResolution(1920, 1080, 1.0f);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->Viewport->TakeHighResScreenShot();
		}
		else if (GEngine)
		{
			GEngine->Exec(nullptr, *FString::Printf(TEXT("HighResShot 1920x1080 \"%s\""), *ScreenshotPath));
		}
		Files.Add(ScreenshotPath);
	}

	// 2. Log tail.
	{
		const FString EditorLogPath = FPaths::ProjectLogDir() / FString::Printf(TEXT("%s.log"), FApp::GetProjectName());
		const FString Tail = TailFile(EditorLogPath, LogTailLines);
		const FString OutLog = BundleDir / TEXT("log_tail.txt");
		FFileHelper::SaveStringToFile(Tail, *OutLog);
		Files.Add(OutLog);
	}

	// 3. CVar list.
	{
		const FString OutCVars = BundleDir / TEXT("cvars.txt");
		FString Buf;
		IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
			FConsoleObjectVisitor::CreateLambda([&Buf](const TCHAR* Name, IConsoleObject* Obj)
			{
				if (IConsoleVariable* V = Obj ? Obj->AsVariable() : nullptr)
				{
					Buf += FString::Printf(TEXT("%s = %s\n"), Name, *V->GetString());
				}
			}),
			TEXT(""));
		FFileHelper::SaveStringToFile(Buf, *OutCVars);
		Files.Add(OutCVars);
	}

	// 4. Scene stats + top-N actors by component count.
	int32 ActorCount = 0;
	TSharedPtr<FJsonObject> SceneStats = MakeShared<FJsonObject>();
	TArray<TPair<FString, int32>> ActorByCost;
	{
		UWorld* World = nullptr;
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.World()) { World = Ctx.World(); break; }
			}
		}
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				++ActorCount;
				AActor* A = *It;
				const int32 Comps = A->GetComponents().Num();
				ActorByCost.Add(TPair<FString, int32>(A->GetName(), Comps));
			}
			ActorByCost.Sort([](const TPair<FString, int32>& X, const TPair<FString, int32>& Y) { return X.Value > Y.Value; });
		}
		SceneStats->SetNumberField(TEXT("actor_count"), ActorCount);
		const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
		SceneStats->SetNumberField(TEXT("used_physical_mb"), (double)Mem.UsedPhysical / (1024.0 * 1024.0));
		SceneStats->SetNumberField(TEXT("peak_used_physical_mb"), (double)Mem.PeakUsedPhysical / (1024.0 * 1024.0));

		TArray<TSharedPtr<FJsonValue>> TopActors;
		const int32 TakeN = FMath::Min(TopNActors, ActorByCost.Num());
		for (int32 i = 0; i < TakeN; ++i)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), ActorByCost[i].Key);
			Row->SetNumberField(TEXT("component_count"), ActorByCost[i].Value);
			TopActors.Add(MakeShared<FJsonValueObject>(Row));
		}
		SceneStats->SetArrayField(TEXT("top_actors_by_component_count"), TopActors);
	}
	{
		const FString OutScene = BundleDir / TEXT("scene_stats.json");
		FString Serialized;
		auto Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(SceneStats.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(Serialized, *OutScene);
		Files.Add(OutScene);
	}

	// 5. Manifest.
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("label"), Label);
	Result->SetStringField(TEXT("bundle_dir"), BundleDir);
	Result->SetStringField(TEXT("captured_at"), FDateTime::Now().ToIso8601());

	TArray<TSharedPtr<FJsonValue>> FileList;
	for (const FString& F : Files)
	{
		FileList.Add(MakeShared<FJsonValueString>(F));
	}
	Result->SetArrayField(TEXT("files"), FileList);
	Result->SetObjectField(TEXT("scene_stats"), SceneStats);
	Result->SetStringField(TEXT("note"), TEXT("Screenshot is queued; it appears on the next render tick. Bundle is a directory, not a zip."));

	// Also write the manifest to disk.
	{
		const FString ManifestPath = BundleDir / TEXT("manifest.json");
		FString Serialized;
		auto Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(Serialized, *ManifestPath);
	}

	return FECACommandResult::Success(Result);
}
