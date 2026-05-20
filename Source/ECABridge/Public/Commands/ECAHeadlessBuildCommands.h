// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Build a UE plugin from its .uplugin using UnrealBuildTool's BuildPlugin action.
 *
 * Shells out to <Engine>/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe with
 * `BuildPlugin -plugin=<path> -package=<output>`. Captures stdout/stderr to a log
 * file under Project/Saved/ECABridge/BuildLogs/ and returns a summary (exit code,
 * duration, warning/error counts, log tail). Use this when an agent has edited a
 * plugin's C++ and wants to verify it still compiles against the running editor.
 *
 * Windows-only for now (Mac UBT path / mono invocation is a follow-up).
 */
class FECACommand_BuildPlugin : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("build_plugin"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Compile a UE plugin from its .uplugin via UnrealBuildTool BuildPlugin. ")
		       TEXT("Optionally builds in-place (compile_only=true) instead of packaging. ")
		       TEXT("Returns exit code, duration, warning/error counts, log path and last-4KB log excerpt.");
	}
	virtual FString GetCategory() const override { return TEXT("Build & Test"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("plugin_path"), TEXT("string"), TEXT("Absolute path to the <PluginName>.uplugin to build. Defaults to the project's primary plugin if discoverable."), false },
			{ TEXT("output_dir"), TEXT("string"), TEXT("Directory where the packaged plugin output is written. Defaults to <plugin_path>/../<PluginName>-Packaged/."), false },
			{ TEXT("target_platforms"), TEXT("array"), TEXT("Target platforms list, e.g. [\"Win64\"]. Defaults to [\"Win64\"]."), false },
			{ TEXT("compile_only"), TEXT("boolean"), TEXT("If true, build in-place against the running editor (faster, no package step)."), false, TEXT("false") },
			{ TEXT("timeout_seconds"), TEXT("integer"), TEXT("Hard-kill the build if it runs longer than this many seconds."), false, TEXT("600") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),          TEXT("boolean"), TEXT("True iff UBT exited with code 0") },
			{ TEXT("exit_code"),        TEXT("integer"), TEXT("Process exit code (-1 if launch failed)") },
			{ TEXT("duration_seconds"), TEXT("number"),  TEXT("Wall-clock time the build took, in seconds") },
			{ TEXT("warnings"),         TEXT("integer"), TEXT("Count of log lines containing the substring ' warning ' (case-insensitive)") },
			{ TEXT("errors"),           TEXT("integer"), TEXT("Count of log lines containing the substring ' error ' (case-insensitive)") },
			{ TEXT("log_excerpt"),      TEXT("string"),  TEXT("Last ~4KB of the build log") },
			{ TEXT("log_path"),         TEXT("string"),  TEXT("Absolute path to the full build log on disk") },
			{ TEXT("command"),          TEXT("string"),  TEXT("The exact command line that was invoked") },
			{ TEXT("timed_out"),        TEXT("boolean"), TEXT("True if the process was killed because timeout_seconds elapsed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Run automation tests headlessly via UnrealEditor-Cmd + Automation RunTests.
 *
 * Spawns `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests <filter>; Quit"
 * -unattended -nopause -nullrhi -log -stdout`. Optionally writes JUnit-style XML via
 * -ReportOutputPath. Parses the tail of the log (or the XML if provided) for the
 * total / passed / failed / skipped counts, and surfaces the names + first error
 * line of any failing tests.
 *
 * Windows-only for now (Mac UnrealEditor-Cmd lives inside the .app bundle).
 */
class FECACommand_RunAutomationTests : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("run_automation_tests"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Run UE automation tests headlessly via UnrealEditor-Cmd -nullrhi. ")
		       TEXT("Filter defaults to 'ECABridge' to avoid running the full engine suite. ")
		       TEXT("Returns totals, the list of failed tests with error excerpts, and the log path.");
	}
	virtual FString GetCategory() const override { return TEXT("Build & Test"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("test_filter"), TEXT("string"), TEXT("Test name prefix (e.g. 'ECABridge', 'Project.Functional'). Defaults to 'ECABridge'."), false, TEXT("ECABridge") },
			{ TEXT("project_path"), TEXT("string"), TEXT("Absolute path to the .uproject. Defaults to the current project."), false },
			{ TEXT("timeout_seconds"), TEXT("integer"), TEXT("Hard-kill if tests run longer than this."), false, TEXT("900") },
			{ TEXT("report_path"), TEXT("string"), TEXT("If set, passes -ReportOutputPath=<path> so UE writes JUnit-style XML there."), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),          TEXT("boolean"), TEXT("True iff exit code is 0 AND no failed tests were parsed") },
			{ TEXT("exit_code"),        TEXT("integer"), TEXT("UnrealEditor-Cmd exit code") },
			{ TEXT("total_tests"),      TEXT("integer"), TEXT("Total tests matched by the filter") },
			{ TEXT("passed"),           TEXT("integer"), TEXT("Number that passed") },
			{ TEXT("failed"),           TEXT("integer"), TEXT("Number that failed") },
			{ TEXT("skipped"),          TEXT("integer"), TEXT("Number that were skipped") },
			{ TEXT("duration_seconds"), TEXT("number"),  TEXT("Wall-clock time the run took") },
			{ TEXT("failed_tests"),     TEXT("array"),   TEXT("List of {name, error_excerpt} objects for each failed test"), TEXT("object") },
			{ TEXT("log_path"),         TEXT("string"),  TEXT("Absolute path to the captured stdout/stderr log") },
			{ TEXT("report_path"),      TEXT("string"),  TEXT("Absolute path to the XML report (only when report_path was supplied)") },
			{ TEXT("command"),          TEXT("string"),  TEXT("The exact command line that was invoked") },
			{ TEXT("timed_out"),        TEXT("boolean"), TEXT("True if the process was killed because timeout_seconds elapsed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Compile every blueprint in the project (or under a path filter) to surface any
 * compilation errors. Wraps UnrealEditor-Cmd's `-run=CompileAllBlueprints`.
 *
 * Useful before submitting an asset change, after a sweeping rename, or when a
 * dependency was deleted/renamed and you want to know which BPs broke.
 *
 * Windows-only for now (same reason as run_automation_tests).
 */
class FECACommand_CompileProjectBlueprints : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("compile_project_blueprints"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Compile all blueprints in the project headlessly via UnrealEditor-Cmd -run=CompileAllBlueprints. ")
		       TEXT("Use path_filter (e.g. '/Game/Player') to limit the scan. Returns counts plus the names + first error line for each errored BP.");
	}
	virtual FString GetCategory() const override { return TEXT("Build & Test"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("project_path"), TEXT("string"), TEXT("Absolute path to the .uproject. Defaults to the current project."), false },
			{ TEXT("timeout_seconds"), TEXT("integer"), TEXT("Hard-kill the compile if it runs longer than this."), false, TEXT("600") },
			{ TEXT("path_filter"), TEXT("string"), TEXT("Filter the errored_blueprints array to only entries under this content path (e.g. '/Game/Player'). The commandlet still compiles the whole project — the top-level counts (compiled/errored/warnings) describe everything; only the per-BP error list is filtered."), false }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("success"),           TEXT("boolean"), TEXT("True iff exit code is 0 AND no blueprints reported errors") },
			{ TEXT("exit_code"),         TEXT("integer"), TEXT("UnrealEditor-Cmd exit code") },
			{ TEXT("compiled"),          TEXT("integer"), TEXT("Total blueprints that were compiled") },
			{ TEXT("errored"),           TEXT("integer"), TEXT("Number that produced compile errors") },
			{ TEXT("warnings"),          TEXT("integer"), TEXT("Number that produced warnings") },
			{ TEXT("duration_seconds"),  TEXT("number"),  TEXT("Wall-clock time the compile took") },
			{ TEXT("errored_blueprints"),TEXT("array"),   TEXT("List of {blueprint, first_error} objects for each errored BP"), TEXT("object") },
			{ TEXT("log_path"),          TEXT("string"),  TEXT("Absolute path to the captured log") },
			{ TEXT("command"),           TEXT("string"),  TEXT("The exact command line that was invoked") },
			{ TEXT("timed_out"),         TEXT("boolean"), TEXT("True if the process was killed because timeout_seconds elapsed") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
