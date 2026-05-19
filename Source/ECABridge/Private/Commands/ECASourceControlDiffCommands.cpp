// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASourceControlDiffCommands.h"
#include "Commands/ECASourceControlSupport.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace
{
	FString HashFile(const FString& Filename)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
		{
			return FString();
		}
		FSHAHash Hash;
		FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash);
		return Hash.ToString();
	}

	bool LooksLikeText(const FString& Filename)
	{
		const FString Ext = FPaths::GetExtension(Filename).ToLower();
		static const TSet<FString> TextExts = {
			TEXT("ini"), TEXT("cfg"), TEXT("json"), TEXT("xml"), TEXT("yaml"), TEXT("yml"),
			TEXT("txt"), TEXT("md"), TEXT("csv"), TEXT("tsv"),
			TEXT("cpp"), TEXT("h"), TEXT("hpp"), TEXT("c"), TEXT("cs"),
			TEXT("py"), TEXT("js"), TEXT("ts"), TEXT("lua"), TEXT("usf"), TEXT("ush"),
			TEXT("build"), TEXT("target"), TEXT("uplugin"), TEXT("uproject"),
		};
		return TextExts.Contains(Ext);
	}
}

FECACommandResult FECACommand_DiffAssetAgainstDepot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, /*bRequired=*/ true))
	{
		return FECACommandResult::ValidationError(this, TEXT("'asset_path' (string) is required."));
	}

	int64 MaxTextBytes = 65536;
	if (Params.IsValid())
	{
		double D = 0.0;
		if (Params->TryGetNumberField(TEXT("max_text_bytes"), D) && D > 0.0)
		{
			MaxTextBytes = static_cast<int64>(D);
		}
	}

	FString LocalFilename;
	FString ResolveErr;
	if (!ECASourceControlSupport::ResolveAssetPathToFilename(AssetPath, LocalFilename, ResolveErr))
	{
		return FECACommandResult::Error(ResolveErr);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("local_filename"), LocalFilename);

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	if (!SCCModule.IsEnabled())
	{
		return FECACommandResult::Error(TEXT("Source control is not enabled in this project."));
	}
	ISourceControlProvider& Provider = SCCModule.GetProvider();
	if (!Provider.IsAvailable())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Source control provider '%s' is not available."), *Provider.GetName().ToString()));
	}

	Result->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Result->SetBoolField(TEXT("allows_diff_against_depot"), Provider.AllowsDiffAgainstDepot());

	// Local file stats.
	const bool bLocalExists = FPaths::FileExists(LocalFilename);
	Result->SetBoolField(TEXT("local_exists"), bLocalExists);
	int64 LocalSize = 0;
	if (bLocalExists)
	{
		LocalSize = IFileManager::Get().FileSize(*LocalFilename);
		Result->SetStringField(TEXT("local_sha1"), HashFile(LocalFilename));
	}
	Result->SetNumberField(TEXT("local_size"), LocalSize);

	// Force-update history for this file so GetCurrentRevision returns something real.
	TArray<FString> Files = { LocalFilename };
	TArray<FSourceControlStateRef> States;
	if (Provider.GetState(Files, States, EStateCacheUsage::ForceUpdate) != ECommandResult::Succeeded || States.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to query source control state for the asset."));
	}

	const FSourceControlStateRef& State = States[0];
	Result->SetBoolField(TEXT("is_modified"), State->IsModified());
	Result->SetBoolField(TEXT("is_source_controlled"), State->IsSourceControlled());

	TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> Revision = State->GetCurrentRevision();
	if (!Revision.IsValid())
	{
		Revision = State->GetHistoryItem(0);
	}
	if (!Revision.IsValid())
	{
		Result->SetStringField(TEXT("diff_status"), TEXT("no_depot_revision"));
		Result->SetStringField(TEXT("message"), TEXT("Asset has no depot revision (not yet submitted, or provider can't report history)."));
		return FECACommandResult::Success(Result);
	}

	const FString DepotTempPath = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir(), TEXT("ECASCCDiff-"), TEXT(".bin"));
	FString DepotFetchedPath = DepotTempPath;
	const bool bFetched = Revision->Get(DepotFetchedPath, EConcurrency::Synchronous);
	Result->SetBoolField(TEXT("depot_fetched"), bFetched);
	Result->SetStringField(TEXT("depot_revision"), Revision->GetRevision());
	Result->SetNumberField(TEXT("depot_revision_number"), Revision->GetRevisionNumber());

	if (!bFetched || DepotFetchedPath.IsEmpty() || !FPaths::FileExists(DepotFetchedPath))
	{
		Result->SetStringField(TEXT("diff_status"), TEXT("depot_fetch_failed"));
		return FECACommandResult::Success(Result);
	}

	const int64 DepotSize = IFileManager::Get().FileSize(*DepotFetchedPath);
	Result->SetNumberField(TEXT("depot_size"), DepotSize);
	const FString DepotHash = HashFile(DepotFetchedPath);
	Result->SetStringField(TEXT("depot_sha1"), DepotHash);

	const bool bIdentical = bLocalExists && DepotSize == LocalSize && !DepotHash.IsEmpty() && DepotHash.Equals(Result->GetStringField(TEXT("local_sha1")), ESearchCase::IgnoreCase);
	Result->SetBoolField(TEXT("identical"), bIdentical);

	if (bIdentical)
	{
		Result->SetStringField(TEXT("diff_status"), TEXT("identical"));
		IFileManager::Get().Delete(*DepotFetchedPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		return FECACommandResult::Success(Result);
	}

	if (!LooksLikeText(LocalFilename) || LocalSize > MaxTextBytes || DepotSize > MaxTextBytes)
	{
		Result->SetStringField(TEXT("diff_status"), TEXT("binary"));
		Result->SetStringField(TEXT("message"), TEXT("Binary or oversized asset: diff via the editor's asset diff tool. Local/depot size and SHA1 are reported above."));
		IFileManager::Get().Delete(*DepotFetchedPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		return FECACommandResult::Success(Result);
	}

	// Produce a coarse line-level diff for small text files. This isn't a full unified
	// diff with hunk headers �?" the editor's diff tool is the canonical UI �?" but it gives
	// an LLM client enough signal to summarize the change.
	FString LocalText;
	FString DepotText;
	if (!FFileHelper::LoadFileToString(LocalText, *LocalFilename) || !FFileHelper::LoadFileToString(DepotText, *DepotFetchedPath))
	{
		Result->SetStringField(TEXT("diff_status"), TEXT("text_load_failed"));
		IFileManager::Get().Delete(*DepotFetchedPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		return FECACommandResult::Success(Result);
	}

	TArray<FString> LocalLines;
	TArray<FString> DepotLines;
	LocalText.ParseIntoArrayLines(LocalLines, /*InCullEmpty=*/ false);
	DepotText.ParseIntoArrayLines(DepotLines, /*InCullEmpty=*/ false);

	const int32 MaxLines = FMath::Max(LocalLines.Num(), DepotLines.Num());
	TArray<TSharedPtr<FJsonValue>> DiffLines;
	int32 ChangedLineCount = 0;
	for (int32 i = 0; i < MaxLines; ++i)
	{
		const FString L = LocalLines.IsValidIndex(i) ? LocalLines[i] : TEXT("");
		const FString D = DepotLines.IsValidIndex(i) ? DepotLines[i] : TEXT("");
		if (L.Equals(D)) { continue; }

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("line"), i + 1);
		Entry->SetStringField(TEXT("local"), L);
		Entry->SetStringField(TEXT("depot"), D);
		DiffLines.Add(MakeShared<FJsonValueObject>(Entry));
		++ChangedLineCount;
	}
	Result->SetStringField(TEXT("diff_status"), TEXT("text_diff"));
	Result->SetNumberField(TEXT("changed_line_count"), ChangedLineCount);
	Result->SetArrayField(TEXT("changed_lines"), DiffLines);

	IFileManager::Get().Delete(*DepotFetchedPath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DiffAssetAgainstDepot);
