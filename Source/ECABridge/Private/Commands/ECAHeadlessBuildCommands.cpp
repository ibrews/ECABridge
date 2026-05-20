// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAHeadlessBuildCommands.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

REGISTER_ECA_COMMAND(FECACommand_BuildPlugin)
REGISTER_ECA_COMMAND(FECACommand_RunAutomationTests)
REGISTER_ECA_COMMAND(FECACommand_CompileProjectBlueprints)

//------------------------------------------------------------------------------
// Shared utilities
//------------------------------------------------------------------------------

namespace
{
	/** Result of running an external child process (UBT / UnrealEditor-Cmd). */
	struct FECAProcessResult
	{
		bool    bExitSuccess   = false;
		int32   ExitCode       = -1;
		double  DurationSeconds = 0.0;
		FString LogPath;       // Where the full log was written
		FString LogTail;       // Last ~4KB of captured stdout/stderr
		bool    bTimedOut      = false;
	};

	/** Quote a path for a Windows command line if it isn't already quoted and
	 *  contains a space (UBT / UnrealEditor-Cmd both happily eat quoted args). */
	FString QuoteIfHasSpace(const FString& In)
	{
		if (In.IsEmpty() || In.StartsWith(TEXT("\"")))
		{
			return In;
		}
		if (In.Contains(TEXT(" ")))
		{
			return FString::Printf(TEXT("\"%s\""), *In);
		}
		return In;
	}

	/** Build the build-log directory under Project/Saved/ECABridge/BuildLogs and
	 *  return a fully-qualified path for "<timestamp>-<command>.log". */
	FString BuildLogPath(const FString& CommandTag)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("ECABridge") / TEXT("BuildLogs"));
		IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		return Dir / (Stamp + TEXT("-") + CommandTag + TEXT(".log"));
	}

	/** Drain everything currently buffered on a read pipe into Sink. */
	void DrainPipe(void* ReadPipe, FString& Sink)
	{
		if (!ReadPipe)
		{
			return;
		}
		// FPlatformProcess::ReadPipe returns whatever bytes are currently
		// available (decoded as UTF-8); empty string when nothing's queued.
		FString Chunk = FPlatformProcess::ReadPipe(ReadPipe);
		while (!Chunk.IsEmpty())
		{
			Sink += Chunk;
			Chunk = FPlatformProcess::ReadPipe(ReadPipe);
		}
	}

	/** Run an external child process; capture stdout/stderr to a log file; enforce a timeout. */
	FECAProcessResult RunExternalProcess(
		const FString& ExePath,
		const FString& Args,
		const FString& WorkingDir,
		int32          TimeoutSeconds,
		const FString& CommandTag)
	{
		FECAProcessResult Out;
		Out.LogPath = BuildLogPath(CommandTag);

		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		if (!PF.FileExists(*ExePath))
		{
			const FString Msg = FString::Printf(
				TEXT("[ECABridge][HeadlessBuild] Executable not found: %s\n"), *ExePath);
			FFileHelper::SaveStringToFile(Msg, *Out.LogPath);
			Out.LogTail   = Msg;
			Out.ExitCode  = -1;
			return Out;
		}

		// Open the log file up-front so the path is valid even on early errors,
		// then stream captured pipe content into it as we read.
		TUniquePtr<FArchive> LogAr(IFileManager::Get().CreateFileWriter(*Out.LogPath));
		auto AppendToLog = [&LogAr](const FString& Chunk)
		{
			if (LogAr.IsValid() && !Chunk.IsEmpty())
			{
				FTCHARToUTF8 Utf8(*Chunk);
				LogAr->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
			}
		};

		// Header line so the log self-identifies.
		const FString Header = FString::Printf(
			TEXT("[ECABridge][HeadlessBuild] %s\nExe: %s\nArgs: %s\nCWD: %s\nTimeout: %ds\n\n"),
			*FDateTime::Now().ToIso8601(), *ExePath, *Args,
			WorkingDir.IsEmpty() ? TEXT("(default)") : *WorkingDir, TimeoutSeconds);
		AppendToLog(Header);

		void* ReadPipe  = nullptr;
		void* WritePipe = nullptr;
		if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
		{
			const FString Msg = TEXT("[ECABridge][HeadlessBuild] CreatePipe failed; running without captured output.\n");
			AppendToLog(Msg);
		}

		uint32 Pid = 0;
		const double StartTime = FPlatformTime::Seconds();

		FProcHandle Handle = FPlatformProcess::CreateProc(
			*ExePath,
			*Args,
			/*bLaunchDetached=*/false,
			/*bLaunchHidden=*/true,
			/*bLaunchReallyHidden=*/true,
			&Pid,
			/*PriorityModifier=*/0,
			WorkingDir.IsEmpty() ? nullptr : *WorkingDir,
			WritePipe);

		if (!Handle.IsValid())
		{
			const FString Msg = FString::Printf(
				TEXT("[ECABridge][HeadlessBuild] CreateProc returned an invalid handle for %s\n"), *ExePath);
			AppendToLog(Msg);
			if (ReadPipe || WritePipe)
			{
				FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			}
			if (LogAr.IsValid())
			{
				LogAr->Close();
				LogAr.Reset();
			}
			Out.ExitCode = -1;
			Out.LogTail  = Msg;
			return Out;
		}

		FString CapturedSoFar;
		auto DrainBoth = [&]()
		{
			FString Chunk;
			DrainPipe(ReadPipe, Chunk);
			if (!Chunk.IsEmpty())
			{
				AppendToLog(Chunk);
				CapturedSoFar += Chunk;
				// Trim CapturedSoFar so we don't OOM on very chatty builds —
				// we only need the tail for the result excerpt.
				constexpr int32 MaxTailKeep = 64 * 1024; // 64KB working window
				if (CapturedSoFar.Len() > MaxTailKeep)
				{
					CapturedSoFar = CapturedSoFar.RightChop(CapturedSoFar.Len() - MaxTailKeep);
				}
			}
		};

		while (FPlatformProcess::IsProcRunning(Handle))
		{
			DrainBoth();

			const double Elapsed = FPlatformTime::Seconds() - StartTime;
			if (TimeoutSeconds > 0 && Elapsed > (double)TimeoutSeconds)
			{
				Out.bTimedOut = true;
				FPlatformProcess::TerminateProc(Handle, /*KillTree=*/true);
				AppendToLog(FString::Printf(
					TEXT("\n[ECABridge][HeadlessBuild] Timed out after %.1fs (limit %ds); process killed.\n"),
					Elapsed, TimeoutSeconds));
				break;
			}

			FPlatformProcess::Sleep(0.1f);
		}

		// Final drain after the process exits (capture any remaining buffered output).
		FPlatformProcess::WaitForProc(Handle);
		DrainBoth();

		int32 ExitCode = -1;
		FPlatformProcess::GetProcReturnCode(Handle, &ExitCode);
		FPlatformProcess::CloseProc(Handle);

		if (ReadPipe || WritePipe)
		{
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		}

		Out.DurationSeconds = FPlatformTime::Seconds() - StartTime;
		Out.ExitCode        = ExitCode;
		Out.bExitSuccess    = (ExitCode == 0 && !Out.bTimedOut);

		// 4KB tail for the result payload (be a good API citizen).
		constexpr int32 TailBytes = 4 * 1024;
		Out.LogTail = (CapturedSoFar.Len() <= TailBytes)
			? CapturedSoFar
			: CapturedSoFar.RightChop(CapturedSoFar.Len() - TailBytes);

		// Footer + close the log.
		AppendToLog(FString::Printf(
			TEXT("\n[ECABridge][HeadlessBuild] Exit code %d (success=%s, duration %.2fs)\n"),
			ExitCode, Out.bExitSuccess ? TEXT("true") : TEXT("false"), Out.DurationSeconds));
		if (LogAr.IsValid())
		{
			LogAr->Close();
			LogAr.Reset();
		}

		return Out;
	}

	/** Resolve the UBT executable for the current host platform. Returns empty on
	 *  unsupported platforms (caller should bail with a clear error). */
	FString ResolveUnrealBuildToolPath()
	{
#if PLATFORM_WINDOWS
		return FPaths::ConvertRelativePathToFull(
			FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"));
#else
		return FString();
#endif
	}

	/** Resolve the headless editor binary (UnrealEditor-Cmd). */
	FString ResolveUnrealEditorCmdPath()
	{
#if PLATFORM_WINDOWS
		return FPaths::ConvertRelativePathToFull(
			FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe"));
#else
		return FString();
#endif
	}

	/** Case-insensitive substring count for warning / error tallying. We pad with
	 *  spaces in the search term (" warning " / " error ") so we don't pick up
	 *  unrelated tokens like "ErrorCode" or "warningfilter". */
	int32 CountSubstringNoCase(const FString& Haystack, const FString& Needle)
	{
		if (Haystack.IsEmpty() || Needle.IsEmpty())
		{
			return 0;
		}
		int32 Count = 0;
		int32 From  = 0;
		const FString Lower = Haystack.ToLower();
		const FString Pat   = Needle.ToLower();
		while (true)
		{
			const int32 At = Lower.Find(Pat, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				break;
			}
			++Count;
			From = At + Pat.Len();
		}
		return Count;
	}

	/** Find Token in Haystack (case-insensitive), then skip whitespace/colon
	 *  and parse the next integer. Returns true if a number was extracted.
	 *  Used to grok "Total Tests: 12" / "Passed: 11" style summary lines that
	 *  the automation framework + CompileAllBlueprints commandlet both emit. */
	bool ExtractIntAfter(const FString& Haystack, const FString& Token, int32& Out)
	{
		const int32 At = Haystack.Find(Token, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE)
		{
			return false;
		}
		int32 Cursor = At + Token.Len();
		while (Cursor < Haystack.Len())
		{
			const TCHAR C = Haystack[Cursor];
			if (C == TEXT(' ') || C == TEXT(':') || C == TEXT('\t'))
			{
				++Cursor;
				continue;
			}
			break;
		}
		FString Num;
		while (Cursor < Haystack.Len())
		{
			const TCHAR C = Haystack[Cursor];
			if (FChar::IsDigit(C))
			{
				Num.AppendChar(C);
				++Cursor;
				continue;
			}
			break;
		}
		if (Num.IsEmpty())
		{
			return false;
		}
		Out = FCString::Atoi(*Num);
		return true;
	}

	/** Read the full saved log back as text — used by post-process parsers that
	 *  need more than the 4KB tail (e.g. blueprint-error collection). Returns
	 *  empty on read failure. */
	FString ReadFullLog(const FString& LogPath)
	{
		FString Buf;
		if (!FFileHelper::LoadFileToString(Buf, *LogPath))
		{
			return FString();
		}
		return Buf;
	}

	/** Try to find a single .uplugin in the project's own Plugins/ tree — used
	 *  as the default for build_plugin when no plugin_path is supplied. Returns
	 *  empty if zero or multiple candidates exist (ambiguous => caller must specify). */
	FString DiscoverProjectPrimaryPluginPath()
	{
		const FString ProjectPluginsDir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectPluginsDir());

		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(
			Found, *ProjectPluginsDir, TEXT("*.uplugin"),
			/*Files=*/true, /*Directories=*/false, /*ClearFileNames=*/true);

		if (Found.Num() == 1)
		{
			return Found[0];
		}
		return FString();
	}
}

//------------------------------------------------------------------------------
// build_plugin
//------------------------------------------------------------------------------

FECACommandResult FECACommand_BuildPlugin::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
	return FECACommandResult::Error(TEXT("build_plugin: platform not yet supported (Windows-only for now)."));
#else
	// Resolve plugin path (param or autodetect).
	FString PluginPath;
	GetStringParam(Params, TEXT("plugin_path"), PluginPath, /*bRequired=*/false);
	if (PluginPath.IsEmpty())
	{
		PluginPath = DiscoverProjectPrimaryPluginPath();
		if (PluginPath.IsEmpty())
		{
			return FECACommandResult::ValidationError(this,
				TEXT("plugin_path was not specified and no unique .uplugin was found under the project's Plugins/ folder."));
		}
	}
	PluginPath = FPaths::ConvertRelativePathToFull(PluginPath);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*PluginPath))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("build_plugin: .uplugin not found at '%s'."), *PluginPath));
	}

	// Pull plugin name from the filename (used for default output dir).
	const FString PluginName = FPaths::GetBaseFilename(PluginPath);

	// Resolve output directory (param or default to sibling -Packaged folder).
	FString OutputDir;
	GetStringParam(Params, TEXT("output_dir"), OutputDir, /*bRequired=*/false);
	if (OutputDir.IsEmpty())
	{
		OutputDir = FPaths::GetPath(PluginPath) / (PluginName + TEXT("-Packaged"));
	}
	OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
	IFileManager::Get().MakeDirectory(*OutputDir, /*Tree=*/true);

	// Target platforms.
	TArray<FString> TargetPlatforms;
	const TArray<TSharedPtr<FJsonValue>>* PlatformsArr = nullptr;
	if (GetArrayParam(Params, TEXT("target_platforms"), PlatformsArr, /*bRequired=*/false) && PlatformsArr)
	{
		for (const TSharedPtr<FJsonValue>& V : *PlatformsArr)
		{
			if (V.IsValid())
			{
				TargetPlatforms.Add(V->AsString());
			}
		}
	}
	if (TargetPlatforms.Num() == 0)
	{
		TargetPlatforms.Add(TEXT("Win64"));
	}

	bool bCompileOnly = false;
	GetBoolParam(Params, TEXT("compile_only"), bCompileOnly, /*bRequired=*/false);

	int32 TimeoutSeconds = 600;
	GetIntParam(Params, TEXT("timeout_seconds"), TimeoutSeconds, /*bRequired=*/false);

	// Resolve UBT.
	const FString UbtPath = ResolveUnrealBuildToolPath();
	if (UbtPath.IsEmpty() || !PF.FileExists(*UbtPath))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("build_plugin: UnrealBuildTool.exe not found. Tried '%s'."),
				UbtPath.IsEmpty() ? TEXT("<platform unsupported>") : *UbtPath));
	}

	// Build the command line. UBT's BuildPlugin action compiles + packages in a
	// single shot — `BuildPlugin -plugin=<x> -package=<y> -targetplatforms=<list>`.
	// When compile_only=true we redirect -package to a throwaway temp directory so
	// the only persisted output is the compile log; UBT still produces the same
	// pass/fail exit code that callers care about. The packaged output dir is
	// reported back via "output_dir" in the result regardless.
	FString EffectiveOutputDir = OutputDir;
	if (bCompileOnly)
	{
		EffectiveOutputDir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("ECABridge") / TEXT("BuildPluginCompileOnly") / PluginName);
		IFileManager::Get().MakeDirectory(*EffectiveOutputDir, /*Tree=*/true);
	}

	FString Args;
	Args += TEXT("BuildPlugin");
	Args += FString::Printf(TEXT(" -plugin=%s"), *QuoteIfHasSpace(PluginPath));
	Args += FString::Printf(TEXT(" -package=%s"), *QuoteIfHasSpace(EffectiveOutputDir));

	FString PlatformList;
	for (int32 i = 0; i < TargetPlatforms.Num(); ++i)
	{
		if (i > 0) PlatformList += TEXT("+");
		PlatformList += TargetPlatforms[i];
	}
	Args += FString::Printf(TEXT(" -targetplatforms=%s"), *PlatformList);

	// Always make the build chatty enough that warning/error counts mean something.
	Args += TEXT(" -unattended -nopause");

	const FString WorkingDir = FPaths::GetPath(UbtPath);

	const FECAProcessResult Proc = RunExternalProcess(
		UbtPath, Args, WorkingDir, TimeoutSeconds, TEXT("build_plugin"));

	const FString FullLog = ReadFullLog(Proc.LogPath);
	const int32 WarningCount = CountSubstringNoCase(FullLog, TEXT(" warning "));
	const int32 ErrorCount   = CountSubstringNoCase(FullLog, TEXT(" error "));

	const FString CommandLine = FString::Printf(TEXT("%s %s"), *UbtPath, *Args);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField   (TEXT("success"),          Proc.bExitSuccess);
	Result->SetNumberField (TEXT("exit_code"),        Proc.ExitCode);
	Result->SetNumberField (TEXT("duration_seconds"), Proc.DurationSeconds);
	Result->SetNumberField (TEXT("warnings"),         WarningCount);
	Result->SetNumberField (TEXT("errors"),           ErrorCount);
	Result->SetStringField (TEXT("log_excerpt"),      Proc.LogTail);
	Result->SetStringField (TEXT("log_path"),         Proc.LogPath);
	Result->SetStringField (TEXT("command"),          CommandLine);
	Result->SetBoolField   (TEXT("timed_out"),        Proc.bTimedOut);
	Result->SetStringField (TEXT("plugin_path"),      PluginPath);
	Result->SetStringField (TEXT("output_dir"),       EffectiveOutputDir);
	Result->SetBoolField   (TEXT("compile_only"),     bCompileOnly);

	return FECACommandResult::Success(Result);
#endif
}

//------------------------------------------------------------------------------
// run_automation_tests
//------------------------------------------------------------------------------

namespace
{
	/** Parse the tail of the automation log for the canonical Epic-formatted summary lines:
	 *      Total Tests: N
	 *      Passed: N
	 *      Failed: N
	 *      Skipped: N
	 *  Falls back to 0 for fields we can't find. Also collects "...: Test FAILED..." lines. */
	struct FAutomationTotals
	{
		int32 Total   = 0;
		int32 Passed  = 0;
		int32 Failed  = 0;
		int32 Skipped = 0;
		TArray<TPair<FString, FString>> FailedTests;
	};

	FAutomationTotals ParseAutomationLog(const FString& Log)
	{
		FAutomationTotals T;
		ExtractIntAfter(Log, TEXT("Total Tests"), T.Total);
		ExtractIntAfter(Log, TEXT("Passed"),      T.Passed);
		ExtractIntAfter(Log, TEXT("Failed"),      T.Failed);
		ExtractIntAfter(Log, TEXT("Skipped"),     T.Skipped);

		// Scan for failed test lines. The automation framework typically emits:
		//   LogAutomationController: Error: Test Failed. Name=MyTest.Foo  ErrorMessage=...
		//   LogAutomationController: Display: Test FAILED: MyTest.Foo
		// We do a coarse line-by-line scan so we tolerate format drift across UE versions.
		TArray<FString> Lines;
		Log.ParseIntoArrayLines(Lines, /*CullEmpty=*/true);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			const FString& Line = Lines[i];
			const FString Lower = Line.ToLower();
			const bool bIsFail = Lower.Contains(TEXT("test failed")) || Lower.Contains(TEXT("test fail:"))
			                  || Lower.Contains(TEXT(": failed"));
			if (!bIsFail)
			{
				continue;
			}

			// Try to extract a test name (Name=Foo or trailing token after ':').
			FString TestName;
			{
				const FString NameKey = TEXT("Name=");
				const int32 At = Line.Find(NameKey, ESearchCase::IgnoreCase);
				if (At != INDEX_NONE)
				{
					int32 Cursor = At + NameKey.Len();
					while (Cursor < Line.Len() && Line[Cursor] != TEXT(' ') && Line[Cursor] != TEXT('\t'))
					{
						TestName.AppendChar(Line[Cursor]);
						++Cursor;
					}
				}
			}
			if (TestName.IsEmpty())
			{
				// Fall back to the substring after the last ':' on the line.
				int32 LastColon = INDEX_NONE;
				Line.FindLastChar(TEXT(':'), LastColon);
				if (LastColon != INDEX_NONE && LastColon + 1 < Line.Len())
				{
					TestName = Line.Mid(LastColon + 1).TrimStartAndEnd();
				}
			}
			if (TestName.IsEmpty())
			{
				TestName = TEXT("(unknown)");
			}

			// Excerpt = this line (trimmed); fits in our return payload.
			FString Excerpt = Line.TrimStartAndEnd();
			constexpr int32 MaxExcerpt = 512;
			if (Excerpt.Len() > MaxExcerpt)
			{
				Excerpt = Excerpt.Left(MaxExcerpt) + TEXT("...");
			}

			T.FailedTests.Emplace(TestName, Excerpt);
		}

		return T;
	}
}

FECACommandResult FECACommand_RunAutomationTests::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
	return FECACommandResult::Error(TEXT("run_automation_tests: platform not yet supported (Windows-only for now)."));
#else
	FString TestFilter = TEXT("ECABridge");
	GetStringParam(Params, TEXT("test_filter"), TestFilter, /*bRequired=*/false);
	if (TestFilter.IsEmpty())
	{
		TestFilter = TEXT("ECABridge");
	}

	FString ProjectPath;
	GetStringParam(Params, TEXT("project_path"), ProjectPath, /*bRequired=*/false);
	if (ProjectPath.IsEmpty())
	{
		ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	}
	ProjectPath = FPaths::ConvertRelativePathToFull(ProjectPath);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*ProjectPath))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("run_automation_tests: .uproject not found at '%s'."), *ProjectPath));
	}

	int32 TimeoutSeconds = 900;
	GetIntParam(Params, TEXT("timeout_seconds"), TimeoutSeconds, /*bRequired=*/false);

	FString ReportPath;
	GetStringParam(Params, TEXT("report_path"), ReportPath, /*bRequired=*/false);
	if (!ReportPath.IsEmpty())
	{
		ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
		IFileManager::Get().MakeDirectory(*ReportPath, /*Tree=*/true);
	}

	const FString EditorCmd = ResolveUnrealEditorCmdPath();
	if (EditorCmd.IsEmpty() || !PF.FileExists(*EditorCmd))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("run_automation_tests: UnrealEditor-Cmd.exe not found. Tried '%s'."),
				EditorCmd.IsEmpty() ? TEXT("<platform unsupported>") : *EditorCmd));
	}

	// Build argument string.
	FString Args;
	Args += QuoteIfHasSpace(ProjectPath);
	// Note: the ExecCmds value contains a semicolon; the outer quotes are
	// essential so Windows passes the whole thing as one token.
	Args += FString::Printf(
		TEXT(" -ExecCmds=\"Automation RunTests %s; Quit\""),
		*TestFilter);
	Args += TEXT(" -unattended -nopause -nullrhi -nosplash -log -stdout -FullStdOutLogOutput");
	if (!ReportPath.IsEmpty())
	{
		Args += FString::Printf(TEXT(" -ReportOutputPath=%s"), *QuoteIfHasSpace(ReportPath));
	}

	const FECAProcessResult Proc = RunExternalProcess(
		EditorCmd, Args, /*WorkingDir=*/FString(), TimeoutSeconds, TEXT("run_automation_tests"));

	const FString FullLog = ReadFullLog(Proc.LogPath);
	const FAutomationTotals Totals = ParseAutomationLog(FullLog);

	TArray<TSharedPtr<FJsonValue>> FailedArr;
	for (const TPair<FString, FString>& F : Totals.FailedTests)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"),           F.Key);
		Entry->SetStringField(TEXT("error_excerpt"),  F.Value);
		FailedArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	const bool bSuccess = Proc.bExitSuccess && (Totals.Failed == 0) && (FailedArr.Num() == 0);

	const FString CommandLine = FString::Printf(TEXT("%s %s"), *EditorCmd, *Args);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField   (TEXT("success"),          bSuccess);
	Result->SetNumberField (TEXT("exit_code"),        Proc.ExitCode);
	Result->SetNumberField (TEXT("total_tests"),      Totals.Total);
	Result->SetNumberField (TEXT("passed"),           Totals.Passed);
	Result->SetNumberField (TEXT("failed"),           FMath::Max(Totals.Failed, FailedArr.Num()));
	Result->SetNumberField (TEXT("skipped"),          Totals.Skipped);
	Result->SetNumberField (TEXT("duration_seconds"), Proc.DurationSeconds);
	Result->SetArrayField  (TEXT("failed_tests"),     FailedArr);
	Result->SetStringField (TEXT("log_path"),         Proc.LogPath);
	Result->SetStringField (TEXT("report_path"),      ReportPath);
	Result->SetStringField (TEXT("command"),          CommandLine);
	Result->SetStringField (TEXT("log_excerpt"),      Proc.LogTail);
	Result->SetBoolField   (TEXT("timed_out"),        Proc.bTimedOut);

	return FECACommandResult::Success(Result);
#endif
}

//------------------------------------------------------------------------------
// compile_project_blueprints
//------------------------------------------------------------------------------

namespace
{
	/** Parse the CompileAllBlueprints commandlet log for compiled/errored/warning
	 *  totals plus the names + first error line of each errored blueprint. */
	struct FCompileBpTotals
	{
		int32 Compiled = 0;
		int32 Errored  = 0;
		int32 Warnings = 0;
		// blueprint package path -> first error line excerpt
		TArray<TPair<FString, FString>> Errors;
	};

	FCompileBpTotals ParseCompileBpLog(const FString& Log)
	{
		FCompileBpTotals T;
		// Coarse — engines have shifted the wording across versions.
		// CompileAllBlueprints typically logs lines like:
		//   LogBlueprint: Display: Compiling /Game/Foo/BP_Bar...
		//   LogBlueprint: Error:   [Compiler] X is not a valid identifier ...
		// We tally compile-attempt lines + scan for error/warning markers.
		TArray<FString> Lines;
		Log.ParseIntoArrayLines(Lines, /*CullEmpty=*/true);

		FString CurrentBp;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			const FString& Line = Lines[i];
			const FString Lower = Line.ToLower();

			// Compile attempt marker — different UE versions phrase this slightly
			// differently ("Compiling", "Compile complete", "Compiled").
			if (Lower.Contains(TEXT("compiling /game/")) || Lower.Contains(TEXT("compiled /game/")))
			{
				++T.Compiled;
				// Try to capture the BP package path for follow-up error pairing.
				const int32 At = Line.Find(TEXT("/Game/"), ESearchCase::IgnoreCase);
				if (At != INDEX_NONE)
				{
					int32 Cursor = At;
					CurrentBp.Empty();
					while (Cursor < Line.Len())
					{
						const TCHAR C = Line[Cursor];
						if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\'') || C == TEXT('"')
						 || C == TEXT('.') || C == TEXT(',') || C == TEXT(')'))
						{
							break;
						}
						CurrentBp.AppendChar(C);
						++Cursor;
					}
				}
				continue;
			}

			// Error / warning markers — record only the first error per current BP.
			const bool bIsError   = Lower.Contains(TEXT("error:")) || Lower.Contains(TEXT("error "))
			                     || Lower.Contains(TEXT("failed to compile"));
			const bool bIsWarning = Lower.Contains(TEXT("warning:")) || Lower.Contains(TEXT("warning "));

			if (bIsError)
			{
				++T.Errored;
				FString Excerpt = Line.TrimStartAndEnd();
				constexpr int32 MaxExcerpt = 512;
				if (Excerpt.Len() > MaxExcerpt)
				{
					Excerpt = Excerpt.Left(MaxExcerpt) + TEXT("...");
				}
				T.Errors.Emplace(CurrentBp.IsEmpty() ? TEXT("(unknown)") : CurrentBp, Excerpt);
				// Don't double-count subsequent errors as separate BPs; keep the
				// CurrentBp pinned until the next "Compiling /Game/..." line.
			}
			else if (bIsWarning)
			{
				++T.Warnings;
			}
		}

		// Also try to read summary lines printed by the commandlet at the end.
		// "Total Blueprints Compiled: N" / "Total Blueprints With Errors: N"
		int32 SummaryCompiled = -1;
		int32 SummaryErrored  = -1;
		int32 SummaryWarnings = -1;
		ExtractIntAfter(Log, TEXT("Total Blueprints Compiled"),    SummaryCompiled);
		ExtractIntAfter(Log, TEXT("Total Blueprints With Errors"), SummaryErrored);
		ExtractIntAfter(Log, TEXT("Total Warnings"),               SummaryWarnings);
		if (SummaryCompiled >= 0) T.Compiled = SummaryCompiled;
		if (SummaryErrored  >= 0) T.Errored  = SummaryErrored;
		if (SummaryWarnings >= 0) T.Warnings = SummaryWarnings;

		return T;
	}
}

FECACommandResult FECACommand_CompileProjectBlueprints::Execute(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
	return FECACommandResult::Error(TEXT("compile_project_blueprints: platform not yet supported (Windows-only for now)."));
#else
	FString ProjectPath;
	GetStringParam(Params, TEXT("project_path"), ProjectPath, /*bRequired=*/false);
	if (ProjectPath.IsEmpty())
	{
		ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	}
	ProjectPath = FPaths::ConvertRelativePathToFull(ProjectPath);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*ProjectPath))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("compile_project_blueprints: .uproject not found at '%s'."), *ProjectPath));
	}

	int32 TimeoutSeconds = 600;
	GetIntParam(Params, TEXT("timeout_seconds"), TimeoutSeconds, /*bRequired=*/false);

	FString PathFilter;
	GetStringParam(Params, TEXT("path_filter"), PathFilter, /*bRequired=*/false);

	const FString EditorCmd = ResolveUnrealEditorCmdPath();
	if (EditorCmd.IsEmpty() || !PF.FileExists(*EditorCmd))
	{
		return FECACommandResult::Error(
			FString::Printf(TEXT("compile_project_blueprints: UnrealEditor-Cmd.exe not found. Tried '%s'."),
				EditorCmd.IsEmpty() ? TEXT("<platform unsupported>") : *EditorCmd));
	}

	FString Args;
	Args += QuoteIfHasSpace(ProjectPath);
	Args += TEXT(" -run=CompileAllBlueprints");
	// Note: the CompileAllBlueprints commandlet does NOT have a positive
	// "only-compile-under-this-path" switch (it only exposes -IgnoreFolder,
	// -AllowListFile, -BlueprintBaseClass and a few tag filters). When the
	// caller supplies path_filter we still run the full compile, then filter
	// the parsed errored_blueprints list by prefix below. The full counts
	// (compiled / errored / warnings) are NOT filtered — they describe the
	// whole project so the caller can tell whether unrelated BPs also broke.
	Args += TEXT(" -unattended -nopause -nullrhi -nosplash -log -stdout -FullStdOutLogOutput");

	const FECAProcessResult Proc = RunExternalProcess(
		EditorCmd, Args, /*WorkingDir=*/FString(), TimeoutSeconds, TEXT("compile_project_blueprints"));

	const FString FullLog = ReadFullLog(Proc.LogPath);
	const FCompileBpTotals Totals = ParseCompileBpLog(FullLog);

	// Normalize a positive path filter so it's a content path style prefix.
	FString NormalizedFilter = PathFilter;
	if (!NormalizedFilter.IsEmpty() && !NormalizedFilter.StartsWith(TEXT("/")))
	{
		NormalizedFilter = TEXT("/Game/") + NormalizedFilter;
	}

	TArray<TSharedPtr<FJsonValue>> ErroredArr;
	for (const TPair<FString, FString>& E : Totals.Errors)
	{
		if (!NormalizedFilter.IsEmpty() && !E.Key.StartsWith(NormalizedFilter))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("blueprint"),  E.Key);
		Entry->SetStringField(TEXT("first_error"), E.Value);
		ErroredArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	const bool bSuccess = Proc.bExitSuccess && Totals.Errored == 0 && ErroredArr.Num() == 0;

	const FString CommandLine = FString::Printf(TEXT("%s %s"), *EditorCmd, *Args);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField   (TEXT("success"),            bSuccess);
	Result->SetNumberField (TEXT("exit_code"),          Proc.ExitCode);
	Result->SetNumberField (TEXT("compiled"),           Totals.Compiled);
	Result->SetNumberField (TEXT("errored"),            FMath::Max(Totals.Errored, ErroredArr.Num()));
	Result->SetNumberField (TEXT("warnings"),           Totals.Warnings);
	Result->SetNumberField (TEXT("duration_seconds"),   Proc.DurationSeconds);
	Result->SetArrayField  (TEXT("errored_blueprints"), ErroredArr);
	Result->SetStringField (TEXT("log_path"),           Proc.LogPath);
	Result->SetStringField (TEXT("command"),            CommandLine);
	Result->SetStringField (TEXT("log_excerpt"),        Proc.LogTail);
	Result->SetBoolField   (TEXT("timed_out"),          Proc.bTimedOut);

	return FECACommandResult::Success(Result);
#endif
}
