// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAProgrammaticCommands.h"
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

REGISTER_ECA_COMMAND(FECACommand_ExecuteScript)
REGISTER_ECA_COMMAND(FECACommand_GetExecutionEnvironment)

// ---------------------------------------------------------------------------
// UECABridgeProgrammatic - the bridge between Python and the command registry.
// Exposed to Python as `unreal.ECABridgeProgrammatic.execute_tool_json(name, args_json)`.
// ---------------------------------------------------------------------------

FString UECABridgeProgrammatic::ExecuteToolJson(const FString& ToolName, const FString& ArgsJson)
{
	TSharedPtr<FJsonObject> Args;
	if (!ArgsJson.IsEmpty())
	{
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ArgsJson);
		if (!FJsonSerializer::Deserialize(Reader, Args) || !Args.IsValid())
		{
			return TEXT("{\"success\":false,\"error\":\"Invalid JSON arguments passed to execute_tool_json\"}");
		}
	}
	else
	{
		Args = MakeShared<FJsonObject>();
	}

	FECACommandResult Result = FECACommandRegistry::Get().ExecuteCommand(ToolName, Args);
	return Result.ToJsonString();
}

// ---------------------------------------------------------------------------
// FECACommand_ExecuteScript
// ---------------------------------------------------------------------------

// The Python preamble injected before every user script. Defines `execute_tool`
// in terms of the UFUNCTION exposed on UECABridgeProgrammatic above. Python
// auto-translates `ExecuteToolJson` to `execute_tool_json` on the class.
static const TCHAR* GExecuteScriptPreamble = TEXT(R"PY(import json as _eca_json
import unreal as _eca_unreal

def execute_tool(toolset, tool, json_input=None):
    """Call any registered ECABridge command by name and return its parsed result.

    Args:
        toolset: Informational only - ECABridge uses flat command names with no
                 namespacing. Pass anything (the helper ignores it). Kept for
                 source compatibility with the native UE 5.8 ProgrammaticToolset.
        tool: Command name (e.g., 'dump_blueprint_graph', 'find_assets').
        json_input: dict or JSON string with the command's arguments. None == no args.

    Returns:
        A dict with at least {'success': bool}. On success, 'result' holds the
        command's structured output. On failure, 'error' holds the message.
    """
    if json_input is None:
        args_str = ""
    elif isinstance(json_input, (dict, list)):
        args_str = _eca_json.dumps(json_input)
    elif isinstance(json_input, str):
        args_str = json_input
    else:
        raise TypeError("json_input must be dict, list, str, or None")

    result_str = _eca_unreal.ECABridgeProgrammatic.execute_tool_json(tool, args_str)
    return _eca_json.loads(result_str)

# ---- user script begins below ----
)PY");

FECACommandResult FECACommand_ExecuteScript::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Script;
	if (!GetStringParam(Params, TEXT("script"), Script) || Script.IsEmpty())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: script"));
	}

	IPythonScriptPlugin* Py = IPythonScriptPlugin::Get();
	if (!Py || !Py->IsPythonAvailable())
	{
		return FECACommandResult::Error(TEXT("PythonScriptPlugin is not available. Make sure 'Python Editor Script Plugin' is enabled in this project."));
	}

	const FString FullScript = FString::Printf(TEXT("%s\n%s"), GExecuteScriptPreamble, *Script);

	FPythonCommandEx Cmd;
	Cmd.Command = FullScript;
	Cmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Cmd.Flags = EPythonCommandFlags::None;

	const bool bOk = Py->ExecPythonCommandEx(Cmd);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), bOk);
	Result->SetStringField(TEXT("command_result"), Cmd.CommandResult);

	TArray<TSharedPtr<FJsonValue>> Logs;
	for (const FPythonLogOutputEntry& Entry : Cmd.LogOutput)
	{
		TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>();
		L->SetStringField(TEXT("type"), Entry.Type == EPythonLogOutputType::Error ? TEXT("error") : TEXT("info"));
		L->SetStringField(TEXT("output"), Entry.Output);
		Logs.Add(MakeShared<FJsonValueObject>(L));
	}
	Result->SetArrayField(TEXT("log_output"), Logs);

	if (!bOk)
	{
		// Surface failure as MCP error so isError flag flips, but include the log too
		// so the caller can see the traceback. The MCP server only uses ErrorMessage
		// in the text block, so concatenate the most useful info.
		FString ErrLines;
		for (const FPythonLogOutputEntry& Entry : Cmd.LogOutput)
		{
			if (Entry.Type == EPythonLogOutputType::Error)
			{
				ErrLines += Entry.Output;
				if (!ErrLines.EndsWith(TEXT("\n"))) ErrLines += TEXT("\n");
			}
		}
		if (ErrLines.IsEmpty()) ErrLines = TEXT("Python execution failed (no error log captured)");
		FECACommandResult Out;
		Out.bSuccess = false;
		Out.ErrorMessage = ErrLines;
		Out.ResultData = Result;
		return Out;
	}

	return FECACommandResult::Success(Result);
}

// ---------------------------------------------------------------------------
// FECACommand_GetExecutionEnvironment
// ---------------------------------------------------------------------------

FECACommandResult FECACommand_GetExecutionEnvironment::Execute(const TSharedPtr<FJsonObject>& /*Params*/)
{
	TSharedPtr<FJsonObject> Result = MakeResult();

	Result->SetStringField(TEXT("instructions"),
		TEXT("execute_script runs your Python source after an injected preamble that exposes the helper `execute_tool(toolset, tool, json_input)`. Use it to chain any number of ECABridge command calls server-side, then `print(json.dumps(result))` your aggregated result so it shows up in log_output. Example: `assets = execute_tool('asset', 'find_assets', {'path_filter':'/Game/'}); print(len(assets['result']['assets']))`. The `toolset` first argument is informational - ECABridge uses flat command names. Result of execute_tool is a parsed dict {success, result|error}."));

	Result->SetStringField(TEXT("preamble"), GExecuteScriptPreamble);

	TArray<TSharedPtr<FJsonValue>> Helpers;
	Helpers.Add(MakeShared<FJsonValueString>(TEXT("execute_tool")));
	Result->SetArrayField(TEXT("helpers"), Helpers);

	Result->SetStringField(TEXT("allowed_modules"),
		TEXT("ECABridge does NOT sandbox imports. The script runs in UE's Python interpreter with full access to `unreal` and any installed Python module. There is no allow-list."));

	Result->SetStringField(TEXT("security_notice"),
		TEXT("execute_script is NOT a security sandbox. UE Python has full editor access - filesystem, asset registry, console commands. Run only scripts from trusted AI agents or trusted humans."));

	TArray<TSharedPtr<IECACommand>> All = FECACommandRegistry::Get().GetAllCommands();
	TArray<TSharedPtr<FJsonValue>> Names;
	Names.Reserve(All.Num());
	for (const TSharedPtr<IECACommand>& Cmd : All)
	{
		if (Cmd.IsValid())
		{
			Names.Add(MakeShared<FJsonValueString>(Cmd->GetName()));
		}
	}
	// Stable alphabetical order so the list is diffable.
	Names.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) {
		return A->AsString() < B->AsString();
	});
	Result->SetArrayField(TEXT("commands"), Names);
	Result->SetNumberField(TEXT("command_count"), Names.Num());

	return FECACommandResult::Success(Result);
}
