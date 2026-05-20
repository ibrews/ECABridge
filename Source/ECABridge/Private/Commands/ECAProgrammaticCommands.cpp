// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAProgrammaticCommands.h"
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"

REGISTER_ECA_COMMAND(FECACommand_ExecuteScript)
REGISTER_ECA_COMMAND(FECACommand_GetExecutionEnvironment)
REGISTER_ECA_COMMAND(FECACommand_SetUPropertyChecked)

// ---------------------------------------------------------------------------
// UECABridgeProgrammatic - the bridge between Python and the command registry.
// Exposed to Python as `unreal.ECABridgeProgrammatic.execute_tool_json(name, args_json)`.
// ---------------------------------------------------------------------------

namespace
{
	// Per-call slot for the JSON-serialized return value of a user's run() function.
	// The preamble footer writes to it; FECACommand_ExecuteScript clears it before
	// invocation and consumes it afterward. Lives in process-static storage because
	// the UFUNCTION shape must be a free static; execute_script calls are serialized
	// through the editor's main thread (Python plugin requirement) so there's no
	// concurrency risk here in practice.
	static FString GLastCommandResultJson;
}

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

void UECABridgeProgrammatic::SetCommandResult(const FString& JsonValue)
{
	GLastCommandResultJson = JsonValue;
}

FString UECABridgeProgrammatic::ConsumeCommandResult()
{
	FString Value = MoveTemp(GLastCommandResultJson);
	GLastCommandResultJson.Empty();
	return Value;
}

// ---------------------------------------------------------------------------
// FECACommand_ExecuteScript
// ---------------------------------------------------------------------------

// The Python preamble injected before every user script. Defines `execute_tool`
// in terms of the UFUNCTION exposed on UECABridgeProgrammatic above. Python
// auto-translates `ExecuteToolJson` to `execute_tool_json` on the class.
static const TCHAR* GExecuteScriptPreamble = TEXT(R"PY(import json as _eca_json
import unreal as _eca_unreal

# IPythonScriptPlugin reuses the interpreter's module-level namespace across calls,
# so prior scripts' `run` definitions would leak into a follow-up script that
# doesn't define one. Wipe it on entry so the footer only acts on a fresh run().
try:
    del run  # noqa: F821
except NameError:
    pass

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

// Footer appended after the user script. If the user defined a `run` callable at
// module scope, invoke it and JSON-serialize the return value into the C++ side
// so command_result carries structured data. Failures are caught and logged so
// they don't break the existing print()-based fallback. Skipped silently if no
// `run` is defined - print() scripts keep working unchanged.
static const TCHAR* GExecuteScriptFooter = TEXT(R"PY(
# ---- ECABridge: capture run() return value, if defined ----
try:
    _eca_run_fn = run  # type: ignore[name-defined]
except NameError:
    _eca_run_fn = None

if _eca_run_fn is not None and callable(_eca_run_fn):
    try:
        _eca_run_result = _eca_run_fn()
    except Exception as _eca_err:
        import traceback as _eca_tb
        print("[ECABridge] run() raised: " + repr(_eca_err))
        _eca_tb.print_exc()
    else:
        if _eca_run_result is not None:
            try:
                _eca_payload = _eca_json.dumps(_eca_run_result, default=str)
                _eca_unreal.ECABridgeProgrammatic.set_command_result(_eca_payload)
            except Exception as _eca_dump_err:
                print("[ECABridge] failed to JSON-serialize run() return: " + repr(_eca_dump_err))
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

	// Clear any prior run() return value before invoking — otherwise a script
	// that doesn't define run() would inherit the last one's payload.
	UECABridgeProgrammatic::ConsumeCommandResult();

	const FString FullScript = FString::Printf(TEXT("%s\n%s\n%s"), GExecuteScriptPreamble, *Script, GExecuteScriptFooter);

	FPythonCommandEx Cmd;
	Cmd.Command = FullScript;
	Cmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Cmd.Flags = EPythonCommandFlags::None;

	const bool bOk = Py->ExecPythonCommandEx(Cmd);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), bOk);

	// Prefer the structured run() return value when present; fall back to whatever
	// ExecPythonCommandEx put into Cmd.CommandResult (typically the value of the
	// last expression statement, or an empty string for ExecuteFile mode).
	const FString RunReturnJson = UECABridgeProgrammatic::ConsumeCommandResult();
	if (!RunReturnJson.IsEmpty())
	{
		TSharedPtr<FJsonValue> Parsed;
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(RunReturnJson);
		if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
		{
			Result->SetField(TEXT("command_result"), Parsed);
		}
		else
		{
			// Failed to parse — surface the raw string so the caller can still see it.
			Result->SetStringField(TEXT("command_result"), RunReturnJson);
		}
	}
	else
	{
		Result->SetStringField(TEXT("command_result"), Cmd.CommandResult);
	}

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
		TEXT("execute_script runs your Python source after a preamble that exposes `execute_tool(toolset, tool, json_input)`. Use it to chain any number of ECABridge command calls server-side. PREFERRED return path: define `def run() -> dict:` at module scope; the preamble's footer will call it and JSON-serialize the return value into the response's `command_result` field. Example: `def run(): assets = execute_tool('asset', 'find_assets', {'path_filter':'/Game/'}); return {'count': len(assets['result']['assets'])}`. FALLBACK: any `print()` output is captured in `log_output` (use `print(json.dumps(result))` for structured fallback). The `toolset` first argument is informational - ECABridge uses flat command names. Result of execute_tool is a parsed dict {success, result|error}."));

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

// ---------------------------------------------------------------------------
// FECACommand_SetUPropertyChecked
// ---------------------------------------------------------------------------
//
// Write a UPROPERTY on a class's CDO, then read it back and compare against
// what was requested. The point isn't the simple-string compare: it's the
// canonical round-trip via FProperty::ImportText_Direct + ExportTextItem_Direct
// against a scratch buffer. That gives us an apples-to-apples "what should
// the value be after import?" vs "what does the property say now?" which holds
// for enums (Foo::Bar vs EFoo::Bar), structs ((X=1,Y=2) vs (X=1.0,Y=2.0)), and
// object refs (path vs qualified path), where naive string compare fails.

FECACommandResult FECACommand_SetUPropertyChecked::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassPath;
	if (!GetStringParam(Params, TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: class_path"));
	}

	FString PropertyName;
	if (!GetStringParam(Params, TEXT("property_name"), PropertyName) || PropertyName.IsEmpty())
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: property_name"));
	}

	FString PropertyValue;
	if (!GetStringParam(Params, TEXT("property_value"), PropertyValue))
	{
		PropertyValue = TEXT("");
	}

	UClass* TargetClass = LoadObject<UClass>(nullptr, *ClassPath);
	if (!TargetClass)
	{
		// Common LLM mistake: passing a /Game/X path without the trailing `.X_C`.
		// Try the asset-path → generated-class expansion that StaticLoadClass does.
		if (!ClassPath.Contains(TEXT(".")))
		{
			int32 LastSlash;
			if (ClassPath.FindLastChar('/', LastSlash))
			{
				const FString AssetName = ClassPath.Mid(LastSlash + 1);
				const FString Expanded = ClassPath + TEXT(".") + AssetName + TEXT("_C");
				TargetClass = LoadObject<UClass>(nullptr, *Expanded);
			}
		}
		if (!TargetClass)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Class not found: %s. For Blueprints, use the form '/Game/Path/BP_Foo.BP_Foo_C'."),
				*ClassPath));
		}
	}

	UObject* CDO = TargetClass->GetDefaultObject();
	if (!CDO)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Cannot get CDO for class: %s"), *ClassPath));
	}

	FProperty* Property = TargetClass->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// The exact case that bites Python: snake_case name silently no-ops on
		// attribute SET. Here we error loudly with the fix written out.
		return FECACommandResult::Error(FString::Printf(
			TEXT("Property '%s' not found on class %s. ")
			TEXT("Hint: UPROPERTY names are PascalCase (e.g., 'AutoPossessAI', not 'auto_possess_ai'). ")
			TEXT("snake_case Python attribute names silently no-op; use PascalCase via set_editor_property."),
			*PropertyName, *ClassPath));
	}

	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(CDO);
	if (!PropertyAddr)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Cannot get address for property '%s'"), *PropertyName));
	}

	// ---- Build the canonical "expected" value in a scratch buffer ----
	// Allocate scratch storage aligned to the property's natural alignment,
	// initialize it (constructs the typed value at its default), then ImportText
	// the user's input into it. Whatever shows up via ExportTextItem on this
	// scratch is the canonical form of "what the user asked for", in the same
	// representation we'll get when we export from the actual CDO slot.
	const int32 Size = Property->GetSize();
	const int32 ScratchAlign = Property->GetMinAlignment();
	TArray<uint8> ScratchBuf;
	ScratchBuf.SetNumZeroed(Size + ScratchAlign);
	uint8* Scratch = reinterpret_cast<uint8*>(Align(reinterpret_cast<UPTRINT>(ScratchBuf.GetData()), (UPTRINT)ScratchAlign));
	Property->InitializeValue(Scratch);

	const TCHAR* ImportEnd = Property->ImportText_Direct(*PropertyValue, Scratch, CDO, PPF_None);
	if (!ImportEnd)
	{
		Property->DestroyValue(Scratch);
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not parse '%s' as a value for property '%s' of type %s. ")
			TEXT("Hint: pass the Unreal text-import form for structs/enums/object refs (e.g., '(X=1,Y=2,Z=3)', 'EAutoPossessAI::PlacedInWorld', '/Game/.../Asset.Asset_C')."),
			*PropertyValue, *PropertyName, *Property->GetClass()->GetName()));
	}

	FString ExpectedText;
	Property->ExportTextItem_Direct(ExpectedText, Scratch, nullptr, CDO, PPF_None);

	// ---- Apply scratch → CDO ----
	Property->CopyCompleteValue(PropertyAddr, Scratch);
	Property->DestroyValue(Scratch);

	// Notify the object that an edit happened so any PostEditChangeProperty hooks
	// re-derive dependent state. Mirrors what Details panel writes do.
	FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ValueSet);
	CDO->PostEditChangeProperty(ChangeEvent);

	// ---- Read back ----
	FString ActualText;
	Property->ExportTextItem_Direct(ActualText, PropertyAddr, nullptr, CDO, PPF_None);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("class_path"), ClassPath);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetStringField(TEXT("property_type"), Property->GetClass()->GetName());
	Result->SetStringField(TEXT("expected"), ExpectedText);
	Result->SetStringField(TEXT("actual"), ActualText);

	if (ActualText.Equals(ExpectedText, ESearchCase::CaseSensitive))
	{
		Result->SetBoolField(TEXT("verified"), true);
		return FECACommandResult::Success(Result);
	}

	Result->SetBoolField(TEXT("verified"), false);
	const FString ErrorMsg = FString::Printf(
		TEXT("Property write verification failed for %s.%s. Expected: '%s', Actual: '%s'. ")
		TEXT("Hint: snake_case Python attribute names silently no-op; use PascalCase via set_editor_property."),
		*ClassPath, *PropertyName, *ExpectedText, *ActualText);
	FECACommandResult Out;
	Out.bSuccess = false;
	Out.ErrorMessage = ErrorMsg;
	Out.ResultData = Result;
	return Out;
}
