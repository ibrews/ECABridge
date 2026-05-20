// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECABlueprintAnalysisCommands.h"
#include "Commands/ECACommand.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Variable.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_SwitchName.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Knot.h"
#include "K2Node_Composite.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"

// ============================================================================
// Register
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_SummarizeBlueprint)
REGISTER_ECA_COMMAND(FECACommand_FindBlueprintCallers)
REGISTER_ECA_COMMAND(FECACommand_DiffBlueprints)
REGISTER_ECA_COMMAND(FECACommand_FindUnusedAssets)

// ============================================================================
// Shared helpers
// ============================================================================

namespace
{
	/** Skip past pure-passthrough knot nodes to the "real" node on the other end of an exec link. */
	static UEdGraphPin* StepThroughKnot(UEdGraphPin* Pin)
	{
		while (Pin)
		{
			UEdGraphNode* Owner = Pin->GetOwningNode();
			UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Owner);
			if (!Knot)
			{
				return Pin;
			}
			// A knot is a 1-in / 1-out passthrough. Pick the pin opposite to the one we arrived on.
			UEdGraphPin* Next = (Pin->Direction == EGPD_Input) ? Knot->GetOutputPin() : Knot->GetInputPin();
			if (!Next || Next->LinkedTo.Num() == 0)
			{
				return Pin;
			}
			Pin = Next->LinkedTo[0];
		}
		return Pin;
	}

	/** First exec-output pin of a node, or nullptr if there isn't one. */
	static UEdGraphPin* FindExecOutPin(UEdGraphNode* Node, const FName& PreferredName = NAME_None)
	{
		if (!Node) return nullptr;
		if (PreferredName != NAME_None)
		{
			if (UEdGraphPin* Named = Node->FindPin(PreferredName, EGPD_Output))
			{
				if (Named->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) return Named;
			}
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/** Follow an exec-output pin to the next *real* (non-knot) node, or nullptr if disconnected. */
	static UEdGraphNode* FollowExec(UEdGraphPin* OutPin)
	{
		if (!OutPin || OutPin->LinkedTo.Num() == 0) return nullptr;
		UEdGraphPin* Stepped = StepThroughKnot(OutPin->LinkedTo[0]);
		return Stepped ? Stepped->GetOwningNode() : nullptr;
	}

	/** Short, human-readable label for a node ("Branch", "Cast to Character", "PrintString", ...). */
	static FString PrettyNodeTitle(UEdGraphNode* Node)
	{
		if (!Node) return TEXT("<null>");
		FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		// Compact multi-line titles ("Set\nMyVar" -> "Set MyVar")
		Title.ReplaceInline(TEXT("\r"), TEXT(""));
		Title.ReplaceInline(TEXT("\n"), TEXT(" "));
		Title.TrimStartAndEndInline();
		return Title;
	}

	/** Name of the value feeding a data input pin (a literal, variable, or upstream function call). */
	static FString DescribeDataInput(UEdGraphPin* InputPin)
	{
		if (!InputPin) return TEXT("?");
		if (InputPin->LinkedTo.Num() > 0)
		{
			UEdGraphPin* Source = StepThroughKnot(InputPin->LinkedTo[0]);
			if (Source)
			{
				if (UEdGraphNode* SrcNode = Source->GetOwningNode())
				{
					if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(SrcNode))
					{
						return VarGet->GetVarNameString();
					}
					if (UK2Node_CallFunction* CallSrc = Cast<UK2Node_CallFunction>(SrcNode))
					{
						return CallSrc->GetFunctionName().ToString() + TEXT("()");
					}
					return PrettyNodeTitle(SrcNode);
				}
			}
		}
		if (!InputPin->DefaultValue.IsEmpty()) return InputPin->DefaultValue;
		if (InputPin->DefaultObject)           return InputPin->DefaultObject->GetName();
		return TEXT("?");
	}

	/** Pretty name for an event entry node. */
	static FString DescribeEventEntry(UEdGraphNode* Node)
	{
		if (UK2Node_ComponentBoundEvent* CBE = Cast<UK2Node_ComponentBoundEvent>(Node))
		{
			return FString::Printf(TEXT("%s.%s"),
				*CBE->GetComponentPropertyName().ToString(),
				*CBE->GetFunctionName().ToString());
		}
		if (UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
		{
			return Ev->GetFunctionName().ToString();
		}
		if (UK2Node_FunctionEntry* FE = Cast<UK2Node_FunctionEntry>(Node))
		{
			const FName Custom = FE->CustomGeneratedFunctionName;
			if (!Custom.IsNone()) return Custom.ToString();
			const FName Sig = FE->FunctionReference.GetMemberName();
			return Sig.IsNone() ? FString(TEXT("FunctionEntry")) : Sig.ToString();
		}
		return PrettyNodeTitle(Node);
	}

	/** Indent helper for the pseudo-code output. */
	static FString Indent(int32 Depth)
	{
		FString Out;
		for (int32 i = 0; i < Depth; ++i) Out += TEXT("  ");
		return Out;
	}

	struct FSummaryContext
	{
		TSet<UEdGraphNode*> Visited;
		int32 MaxDepth = 6;
		int32 NodeBudget = 200; // Hard cap per graph to keep responses bounded.
		int32 NodesEmitted = 0;
	};

	static void WalkExec(FSummaryContext& Ctx, UEdGraphNode* Node, int32 Depth, TArray<FString>& OutLines);

	/** Format the line for a single exec-flow node and recurse into its outputs. */
	static void EmitNode(FSummaryContext& Ctx, UEdGraphNode* Node, int32 Depth, TArray<FString>& OutLines)
	{
		if (!Node) return;
		if (Ctx.NodesEmitted >= Ctx.NodeBudget)
		{
			OutLines.Add(Indent(Depth) + TEXT("... (node budget exceeded)"));
			return;
		}
		if (Depth > Ctx.MaxDepth)
		{
			OutLines.Add(Indent(Depth) + TEXT("..."));
			return;
		}
		if (Ctx.Visited.Contains(Node))
		{
			// Cycle / shared continuation — common when two branches re-merge.
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("-> (merge to %s)"), *PrettyNodeTitle(Node)));
			return;
		}
		Ctx.Visited.Add(Node);
		++Ctx.NodesEmitted;

		// Branch
		if (UK2Node_IfThenElse* BranchNode = Cast<UK2Node_IfThenElse>(Node))
		{
			UEdGraphPin* Cond = BranchNode->FindPin(UEdGraphSchema_K2::PN_Condition, EGPD_Input);
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Branch(%s)"), *DescribeDataInput(Cond)));

			UEdGraphPin* TruePin  = BranchNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
			UEdGraphPin* FalsePin = BranchNode->FindPin(UEdGraphSchema_K2::PN_Else, EGPD_Output);
			OutLines.Add(Indent(Depth + 1) + TEXT("True:"));
			WalkExec(Ctx, FollowExec(TruePin), Depth + 2, OutLines);
			OutLines.Add(Indent(Depth + 1) + TEXT("False:"));
			WalkExec(Ctx, FollowExec(FalsePin), Depth + 2, OutLines);
			return;
		}

		// Cast
		if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
		{
			const FString TargetName = CastNode->TargetType ? CastNode->TargetType->GetName() : TEXT("?");
			UEdGraphPin* SrcPin = CastNode->GetCastSourcePin();
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Cast<%s>(%s)"), *TargetName, *DescribeDataInput(SrcPin)));

			UEdGraphPin* OkPin   = CastNode->FindPin(UEdGraphSchema_K2::PN_CastSucceeded, EGPD_Output);
			UEdGraphPin* FailPin = CastNode->FindPin(UEdGraphSchema_K2::PN_CastFailed,   EGPD_Output);
			if (OkPin && OkPin->LinkedTo.Num() > 0)
			{
				OutLines.Add(Indent(Depth + 1) + TEXT("Success:"));
				WalkExec(Ctx, FollowExec(OkPin), Depth + 2, OutLines);
			}
			if (FailPin && FailPin->LinkedTo.Num() > 0)
			{
				OutLines.Add(Indent(Depth + 1) + TEXT("Failure:"));
				WalkExec(Ctx, FollowExec(FailPin), Depth + 2, OutLines);
			}
			return;
		}

		// Switch (enum / int / string / name — they all expose one exec-out pin per case)
		if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(Node))
		{
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Switch (%s)"), *PrettyNodeTitle(Node)));
			for (UEdGraphPin* Pin : SwitchNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;
				if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->LinkedTo.Num() == 0) continue;
				const FString CaseLabel = Pin->PinName.ToString();
				OutLines.Add(Indent(Depth + 1) + FString::Printf(TEXT("case %s:"), *CaseLabel));
				WalkExec(Ctx, FollowExec(Pin), Depth + 2, OutLines);
			}
			return;
		}

		// Variable set
		if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node))
		{
			const FString VarName = VarSet->GetVarNameString();
			UEdGraphPin* ValuePin = VarSet->FindPin(VarSet->GetVarName(), EGPD_Input);
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Set %s = %s"),
				*VarName, ValuePin ? *DescribeDataInput(ValuePin) : TEXT("?")));
			UEdGraphPin* NextPin = FindExecOutPin(Node, UEdGraphSchema_K2::PN_Then);
			WalkExec(Ctx, FollowExec(NextPin), Depth, OutLines);
			return;
		}

		// Macro instance — loops, IsValid, gates. Don't recurse into the macro graph; just label it.
		if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
		{
			UEdGraph* MGraph = MacroNode->GetMacroGraph();
			const FString MacroName = MGraph ? MGraph->GetName() : PrettyNodeTitle(Node);
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Macro<%s>"), *MacroName));
			// Macros expose their continuations as exec outputs (Loop/Then/Completed/etc).
			for (UEdGraphPin* Pin : MacroNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;
				if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->LinkedTo.Num() == 0) continue;
				const FString Label = Pin->PinName.IsNone() ? FString(TEXT("then")) : Pin->PinName.ToString();
				OutLines.Add(Indent(Depth + 1) + FString::Printf(TEXT("%s:"), *Label));
				WalkExec(Ctx, FollowExec(Pin), Depth + 2, OutLines);
			}
			return;
		}

		// Function call (and friends)
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			// Build a parenthesized arg list of the first few non-exec inputs.
			TArray<FString> ArgBits;
			for (UEdGraphPin* Pin : CallNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input) continue;
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->PinName == UEdGraphSchema_K2::PN_Self) continue;
				if (Pin->bHidden) continue;
				ArgBits.Add(DescribeDataInput(Pin));
				if (ArgBits.Num() >= 3) break;
			}
			const FString Fn = CallNode->GetFunctionName().ToString();
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("%s(%s)"), *Fn, *FString::Join(ArgBits, TEXT(", "))));
			WalkExec(Ctx, FollowExec(FindExecOutPin(Node, UEdGraphSchema_K2::PN_Then)), Depth, OutLines);
			return;
		}

		// Collapsed (composite) sub-graph — just label it; consumers can re-summarize on demand.
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(Node))
		{
			OutLines.Add(Indent(Depth) + FString::Printf(TEXT("Collapsed<%s>"), *PrettyNodeTitle(CompositeNode)));
			WalkExec(Ctx, FollowExec(FindExecOutPin(Node, UEdGraphSchema_K2::PN_Then)), Depth, OutLines);
			return;
		}

		// Default: print the node title, follow its single exec-out.
		OutLines.Add(Indent(Depth) + PrettyNodeTitle(Node));
		WalkExec(Ctx, FollowExec(FindExecOutPin(Node)), Depth, OutLines);
	}

	static void WalkExec(FSummaryContext& Ctx, UEdGraphNode* Node, int32 Depth, TArray<FString>& OutLines)
	{
		if (!Node)
		{
			OutLines.Add(Indent(Depth) + TEXT("<empty>"));
			return;
		}
		EmitNode(Ctx, Node, Depth, OutLines);
	}

	/** Build the per-event "-> ... -> ..." style summary line. Resets Visited per call. */
	static FString SummarizeEntry(UEdGraphNode* EntryNode, int32 MaxDepth)
	{
		FSummaryContext Ctx;
		Ctx.MaxDepth = MaxDepth;

		TArray<FString> Lines;
		UEdGraphPin* OutExec = FindExecOutPin(EntryNode);
		UEdGraphNode* First = FollowExec(OutExec);
		if (!First)
		{
			return TEXT("<empty>");
		}
		EmitNode(Ctx, First, 0, Lines);
		return FString::Join(Lines, TEXT("\n"));
	}

	/** Find all entry points in a graph (Events, CustomEvents, FunctionEntry). */
	static TArray<UEdGraphNode*> CollectEntryPoints(UEdGraph* Graph)
	{
		TArray<UEdGraphNode*> Out;
		if (!Graph) return Out;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (Cast<UK2Node_Event>(Node) || Cast<UK2Node_FunctionEntry>(Node))
			{
				Out.Add(Node);
			}
		}
		return Out;
	}
} // anonymous namespace

// ============================================================================
// summarize_blueprint
// ============================================================================

FECACommandResult FECACommand_SummarizeBlueprint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path"));
	}

	bool bIncludeFunctions = true;
	GetBoolParam(Params, TEXT("include_functions"), bIncludeFunctions, false);

	int32 MaxDepth = 6;
	GetIntParam(Params, TEXT("max_depth"), MaxDepth, false);
	MaxDepth = FMath::Clamp(MaxDepth, 1, 32);

	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	TArray<TSharedPtr<FJsonValue>> Graphs;

	// Event graphs (UbergraphPages) — one entry per event handler inside.
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());
		GraphObj->SetStringField(TEXT("kind"), TEXT("ubergraph"));

		TArray<TSharedPtr<FJsonValue>> Events;
		for (UEdGraphNode* Entry : CollectEntryPoints(Graph))
		{
			// Event graphs hold events, not function entries — but a function may also be
			// embedded as a tunnel-style entry in some rare cases. Skip non-event entries here.
			if (!Cast<UK2Node_Event>(Entry)) continue;

			TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
			EventObj->SetStringField(TEXT("name"), DescribeEventEntry(Entry));
			EventObj->SetStringField(TEXT("class"), Entry->GetClass()->GetName());
			EventObj->SetStringField(TEXT("summary"), SummarizeEntry(Entry, MaxDepth));
			Events.Add(MakeShared<FJsonValueObject>(EventObj));
		}
		GraphObj->SetArrayField(TEXT("events"), Events);
		GraphObj->SetNumberField(TEXT("event_count"), Events.Num());

		Graphs.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	// Function graphs — one entry node per function, summarized as a single body.
	if (bIncludeFunctions)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (!Graph) continue;
			// Skip the auto-generated "UserConstructionScript" if it's empty.
			TArray<UEdGraphNode*> Entries = CollectEntryPoints(Graph);
			if (Entries.Num() == 0) continue;

			TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
			GraphObj->SetStringField(TEXT("name"), Graph->GetName());
			GraphObj->SetStringField(TEXT("kind"), TEXT("function"));

			// Use the first FunctionEntry as the canonical body root.
			UEdGraphNode* Root = nullptr;
			for (UEdGraphNode* N : Entries)
			{
				if (Cast<UK2Node_FunctionEntry>(N)) { Root = N; break; }
			}
			if (!Root) Root = Entries[0];

			GraphObj->SetStringField(TEXT("summary"), SummarizeEntry(Root, MaxDepth));
			Graphs.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}

	Result->SetArrayField(TEXT("graphs"), Graphs);
	Result->SetNumberField(TEXT("graph_count"), Graphs.Num());
	return FECACommandResult::Success(Result);
}

// ============================================================================
// find_blueprint_callers
// ============================================================================

namespace
{
	/** Returns true if either Member or its parent class matches the user's filter. */
	static bool MatchesMember(const FMemberReference& Ref, const FString& MemberName, const FString& TargetClass)
	{
		if (!Ref.GetMemberName().ToString().Equals(MemberName, ESearchCase::IgnoreCase))
		{
			return false;
		}
		if (TargetClass.IsEmpty()) return true;
		UClass* ParentClass = Ref.GetMemberParentClass();
		if (!ParentClass) return true; // self-member; user can't disambiguate further
		return ParentClass->GetName().Equals(TargetClass, ESearchCase::IgnoreCase) ||
		       ParentClass->GetName().EndsWith(TargetClass, ESearchCase::IgnoreCase);
	}
}

FECACommandResult FECACommand_FindBlueprintCallers::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionName, VariableName, EventName, TargetClass;
	GetStringParam(Params, TEXT("function_name"), FunctionName, false);
	GetStringParam(Params, TEXT("variable_name"), VariableName, false);
	GetStringParam(Params, TEXT("event_name"),    EventName,    false);
	GetStringParam(Params, TEXT("target_class"),  TargetClass,  false);

	FString SearchPath = TEXT("/Game");
	GetStringParam(Params, TEXT("search_path"), SearchPath, false);

	int32 MaxResults = 100;
	GetIntParam(Params, TEXT("max_results"), MaxResults, false);
	MaxResults = FMath::Clamp(MaxResults, 1, 10000);

	FString SearchTerm;
	FString SearchKind;
	if (!FunctionName.IsEmpty())      { SearchTerm = FunctionName; SearchKind = TEXT("function"); }
	else if (!VariableName.IsEmpty()) { SearchTerm = VariableName; SearchKind = TEXT("variable"); }
	else if (!EventName.IsEmpty())    { SearchTerm = EventName;    SearchKind = TEXT("event"); }
	else
	{
		return FECACommandResult::ValidationError(this,
			TEXT("Must provide at least one of: function_name, variable_name, event_name."));
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*SearchPath));
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> Callers;
	bool bTruncated = false;
	int32 Scanned = 0;

	for (const FAssetData& AssetData : Assets)
	{
		if (Callers.Num() >= MaxResults) { bTruncated = true; break; }

		const FString BPPath = AssetData.GetObjectPathString();
		// LoadObject matches the SearchBlueprintUsage pattern; FAssetData::GetAsset() does
		// the same loading under the hood but with extra tag-processing we don't need here.
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BPPath);
		if (!Blueprint) continue;
		++Scanned;

		TArray<UEdGraph*> AllGraphs;
		AllGraphs.Append(Blueprint->UbergraphPages);
		AllGraphs.Append(Blueprint->FunctionGraphs);
		AllGraphs.Append(Blueprint->MacroGraphs);

		for (UEdGraph* Graph : AllGraphs)
		{
			if (Callers.Num() >= MaxResults) { bTruncated = true; break; }
			if (!Graph) continue;

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Callers.Num() >= MaxResults) { bTruncated = true; break; }
				if (!Node) continue;

				bool bHit = false;
				FString Context;

				if (SearchKind == TEXT("function"))
				{
					if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
					{
						if (MatchesMember(Call->FunctionReference, SearchTerm, TargetClass))
						{
							bHit = true;
							UClass* PC = Call->FunctionReference.GetMemberParentClass();
							Context = FString::Printf(TEXT("Call %s%s%s"),
								PC ? *PC->GetName() : TEXT(""),
								PC ? TEXT("::") : TEXT(""),
								*SearchTerm);
						}
					}
				}
				else if (SearchKind == TEXT("variable"))
				{
					if (UK2Node_Variable* Var = Cast<UK2Node_Variable>(Node))
					{
						if (MatchesMember(Var->VariableReference, SearchTerm, TargetClass))
						{
							bHit = true;
							const bool bIsSet = (Cast<UK2Node_VariableSet>(Node) != nullptr);
							Context = FString::Printf(TEXT("%s %s"),
								bIsSet ? TEXT("Set") : TEXT("Get"), *SearchTerm);
						}
					}
				}
				else if (SearchKind == TEXT("event"))
				{
					// Direct event nodes (custom or override) that declare this name.
					if (UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
					{
						if (Ev->GetFunctionName().ToString().Equals(SearchTerm, ESearchCase::IgnoreCase))
						{
							bHit = true;
							Context = FString::Printf(TEXT("Event %s declared"), *SearchTerm);
						}
					}
					// Calls that target an event by name (function calls on Self for event dispatchers).
					if (!bHit)
					{
						if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
						{
							if (Call->FunctionReference.GetMemberName().ToString().Equals(SearchTerm, ESearchCase::IgnoreCase))
							{
								bHit = true;
								Context = FString::Printf(TEXT("Call %s"), *SearchTerm);
							}
						}
					}
				}

				if (bHit)
				{
					TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
					Hit->SetStringField(TEXT("blueprint_path"), BPPath);
					Hit->SetStringField(TEXT("graph_name"),     Graph->GetName());
					Hit->SetStringField(TEXT("node_id"),        Node->NodeGuid.ToString());
					Hit->SetStringField(TEXT("node_title"),     PrettyNodeTitle(Node));
					Hit->SetStringField(TEXT("node_class"),     Node->GetClass()->GetName());
					Hit->SetStringField(TEXT("context"),        Context);
					Callers.Add(MakeShared<FJsonValueObject>(Hit));
				}
			}
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("search_term"), SearchTerm);
	Result->SetStringField(TEXT("search_kind"), SearchKind);
	if (!TargetClass.IsEmpty()) Result->SetStringField(TEXT("target_class"), TargetClass);
	Result->SetStringField(TEXT("search_path"), SearchPath);
	Result->SetArrayField(TEXT("callers"), Callers);
	Result->SetNumberField(TEXT("count"), Callers.Num());
	Result->SetBoolField(TEXT("truncated"), bTruncated);
	Result->SetNumberField(TEXT("scanned"), Scanned);
	return FECACommandResult::Success(Result);
}

// ============================================================================
// diff_blueprints
// ============================================================================

namespace
{
	/** Stable identity for a node when matching across two graphs. */
	static FString NodeIdentity(UEdGraphNode* Node)
	{
		if (!Node) return FString();
		return FString::Printf(TEXT("%s|%s"),
			*Node->GetClass()->GetName(),
			*PrettyNodeTitle(Node));
	}

	/** Stable connection identity (source node + pin -> target node + pin), all by identity. */
	static FString ConnectionIdentity(UEdGraphPin* Out, UEdGraphPin* In)
	{
		if (!Out || !In || !Out->GetOwningNode() || !In->GetOwningNode()) return FString();
		return FString::Printf(TEXT("%s::%s --> %s::%s"),
			*NodeIdentity(Out->GetOwningNode()), *Out->PinName.ToString(),
			*NodeIdentity(In->GetOwningNode()),  *In->PinName.ToString());
	}

	struct FGraphSnapshot
	{
		TMap<FString, UEdGraphNode*> Nodes;            // identity -> node
		TSet<FString> Connections;                     // identity strings
		TMap<FString, TMap<FString, FString>> PinDefaults; // node identity -> (pin name -> default value)
	};

	static FGraphSnapshot SnapshotGraph(UEdGraph* Graph)
	{
		FGraphSnapshot Snap;
		if (!Graph) return Snap;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const FString Id = NodeIdentity(Node);
			Snap.Nodes.Add(Id, Node);

			TMap<FString, FString> Defaults;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				if (Pin->Direction != EGPD_Input) continue;
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->LinkedTo.Num() > 0) continue; // only literals
				FString Val = Pin->DefaultValue;
				if (Val.IsEmpty() && Pin->DefaultObject) Val = Pin->DefaultObject->GetPathName();
				if (Val.IsEmpty()) continue;
				Defaults.Add(Pin->PinName.ToString(), Val);
			}
			if (Defaults.Num() > 0) Snap.PinDefaults.Add(Id, MoveTemp(Defaults));

			// Outgoing connections only — directed pairs are symmetric.
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					const FString Id2 = ConnectionIdentity(Pin, Linked);
					if (!Id2.IsEmpty()) Snap.Connections.Add(Id2);
				}
			}
		}
		return Snap;
	}

	static TArray<TSharedPtr<FJsonValue>> ToStringArray(const TArray<FString>& Strings)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		Out.Reserve(Strings.Num());
		for (const FString& S : Strings) Out.Add(MakeShared<FJsonValueString>(S));
		return Out;
	}
}

FECACommandResult FECACommand_DiffBlueprints::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PathA, PathB;
	if (!GetStringParam(Params, TEXT("blueprint_a"), PathA))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_a"));
	}
	if (!GetStringParam(Params, TEXT("blueprint_b"), PathB))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_b"));
	}

	bool bIncludePinValues = false;
	GetBoolParam(Params, TEXT("include_pin_values"), bIncludePinValues, false);

	UBlueprint* A = LoadBlueprintByPath(PathA);
	if (!A) return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *PathA));
	UBlueprint* B = LoadBlueprintByPath(PathB);
	if (!B) return FECACommandResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *PathB));

	// Index graphs by name on both sides.
	TMap<FString, UEdGraph*> GraphsA, GraphsB;
	auto Collect = [](UBlueprint* BP, TMap<FString, UEdGraph*>& Out)
	{
		auto Add = [&](const TArray<UEdGraph*>& Source)
		{
			for (UEdGraph* G : Source) if (G) Out.Add(G->GetName(), G);
		};
		Add(BP->UbergraphPages);
		Add(BP->FunctionGraphs);
		Add(BP->MacroGraphs);
	};
	Collect(A, GraphsA);
	Collect(B, GraphsB);

	TArray<TSharedPtr<FJsonValue>> GraphDiffs;
	TArray<FString> OnlyInA, OnlyInB;

	TSet<FString> CommonNames;
	for (const auto& KV : GraphsA)
	{
		if (GraphsB.Contains(KV.Key)) CommonNames.Add(KV.Key);
		else OnlyInA.Add(KV.Key);
	}
	for (const auto& KV : GraphsB)
	{
		if (!GraphsA.Contains(KV.Key)) OnlyInB.Add(KV.Key);
	}

	OnlyInA.Sort();
	OnlyInB.Sort();

	TArray<FString> CommonSorted = CommonNames.Array();
	CommonSorted.Sort();

	for (const FString& Name : CommonSorted)
	{
		FGraphSnapshot SA = SnapshotGraph(GraphsA[Name]);
		FGraphSnapshot SB = SnapshotGraph(GraphsB[Name]);

		TArray<FString> AddedNodes, RemovedNodes;
		for (const auto& KV : SB.Nodes) if (!SA.Nodes.Contains(KV.Key)) AddedNodes.Add(KV.Key);
		for (const auto& KV : SA.Nodes) if (!SB.Nodes.Contains(KV.Key)) RemovedNodes.Add(KV.Key);

		TArray<FString> AddedConns, RemovedConns;
		for (const FString& C : SB.Connections) if (!SA.Connections.Contains(C)) AddedConns.Add(C);
		for (const FString& C : SA.Connections) if (!SB.Connections.Contains(C)) RemovedConns.Add(C);

		AddedNodes.Sort();   RemovedNodes.Sort();
		AddedConns.Sort();   RemovedConns.Sort();

		TSharedPtr<FJsonObject> GD = MakeShared<FJsonObject>();
		GD->SetStringField(TEXT("name"), Name);
		GD->SetArrayField(TEXT("added_nodes"),         ToStringArray(AddedNodes));
		GD->SetArrayField(TEXT("removed_nodes"),       ToStringArray(RemovedNodes));
		GD->SetArrayField(TEXT("added_connections"),   ToStringArray(AddedConns));
		GD->SetArrayField(TEXT("removed_connections"), ToStringArray(RemovedConns));

		if (bIncludePinValues)
		{
			TArray<TSharedPtr<FJsonValue>> Changes;
			// Walk nodes common to both sides; compare per-pin defaults.
			for (const auto& KV : SA.Nodes)
			{
				if (!SB.Nodes.Contains(KV.Key)) continue;
				const TMap<FString, FString>* PA = SA.PinDefaults.Find(KV.Key);
				const TMap<FString, FString>* PB = SB.PinDefaults.Find(KV.Key);

				TSet<FString> PinNames;
				if (PA) for (const auto& P : *PA) PinNames.Add(P.Key);
				if (PB) for (const auto& P : *PB) PinNames.Add(P.Key);

				for (const FString& PN : PinNames)
				{
					const FString* VA = PA ? PA->Find(PN) : nullptr;
					const FString* VB = PB ? PB->Find(PN) : nullptr;
					const FString StrA = VA ? *VA : FString();
					const FString StrB = VB ? *VB : FString();
					if (StrA == StrB) continue;
					TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
					Change->SetStringField(TEXT("node"),  KV.Key);
					Change->SetStringField(TEXT("pin"),   PN);
					Change->SetStringField(TEXT("a"),     StrA);
					Change->SetStringField(TEXT("b"),     StrB);
					Changes.Add(MakeShared<FJsonValueObject>(Change));
				}
			}
			GD->SetArrayField(TEXT("changed_pin_values"), Changes);
		}

		GraphDiffs.Add(MakeShared<FJsonValueObject>(GD));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("blueprint_a"), PathA);
	Result->SetStringField(TEXT("blueprint_b"), PathB);
	Result->SetArrayField(TEXT("graphs"), GraphDiffs);
	Result->SetArrayField(TEXT("graphs_only_in_a"), ToStringArray(OnlyInA));
	Result->SetArrayField(TEXT("graphs_only_in_b"), ToStringArray(OnlyInB));
	return FECACommandResult::Success(Result);
}

// ============================================================================
// find_unused_assets
// ============================================================================

FECACommandResult FECACommand_FindUnusedAssets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = TEXT("/Game");
	GetStringParam(Params, TEXT("search_path"), SearchPath, false);

	FString AssetClassFilter;
	GetStringParam(Params, TEXT("asset_class"), AssetClassFilter, false);

	int32 MaxResults = 200;
	GetIntParam(Params, TEXT("max_results"), MaxResults, false);
	MaxResults = FMath::Clamp(MaxResults, 1, 10000);

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(*SearchPath, Assets, /*bRecursive=*/true);

	TArray<TSharedPtr<FJsonValue>> Unused;
	bool bTruncated = false;
	int32 Scanned = 0;

	for (const FAssetData& AD : Assets)
	{
		if (Unused.Num() >= MaxResults) { bTruncated = true; break; }

		const FString ClassName = AD.AssetClassPath.GetAssetName().ToString();

		// Skip Worlds/Levels — they are roots, never "unused".
		if (ClassName == TEXT("World") || ClassName == TEXT("Level")) continue;

		// Skip the auto-generated class-default suffix (rare to surface here, but cheap to guard).
		if (AD.AssetName.ToString().EndsWith(TEXT("_C"))) continue;

		// Optional class filter.
		if (!AssetClassFilter.IsEmpty() && !ClassName.Equals(AssetClassFilter, ESearchCase::IgnoreCase)) continue;

		++Scanned;

		TArray<FName> Referencers;
		AR.GetReferencers(AD.PackageName, Referencers);

		// "No referencers other than itself" — package may show up in its own deps, drop that.
		Referencers.Remove(AD.PackageName);

		if (Referencers.Num() > 0) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"),  AD.GetObjectPathString());
		Obj->SetStringField(TEXT("class"), ClassName);

		// Best-effort on-disk size (skipped silently when filename resolution fails).
		FString PackageFilename;
		if (FPackageName::DoesPackageExist(AD.PackageName.ToString(), &PackageFilename))
		{
			const int64 SizeBytes = IFileManager::Get().FileSize(*PackageFilename);
			if (SizeBytes > 0)
			{
				Obj->SetNumberField(TEXT("size_kb"), static_cast<double>(SizeBytes) / 1024.0);
			}
		}

		Unused.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("search_path"), SearchPath);
	if (!AssetClassFilter.IsEmpty()) Result->SetStringField(TEXT("asset_class"), AssetClassFilter);
	Result->SetArrayField(TEXT("unused"), Unused);
	Result->SetNumberField(TEXT("count"), Unused.Num());
	Result->SetBoolField(TEXT("truncated"), bTruncated);
	Result->SetNumberField(TEXT("scanned"), Scanned);
	return FECACommandResult::Success(Result);
}
