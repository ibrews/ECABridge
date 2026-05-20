// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECABlueprintScreenshotCommands.h"
#include "Commands/ECACommand.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Subsystems/AssetEditorSubsystem.h"

// FBlueprintEditor + SGraphEditor live in the Kismet / UnrealEd modules respectively;
// both are already PrivateDependencyModuleNames in ECABridge.Build.cs.
#include "BlueprintEditor.h"
#include "GraphEditor.h"

#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
// IPlatformFile::GetPlatformPhysical comes in transitively via CoreMinimal.h
// (matches ECAEditorCommands.cpp which also doesn't include PlatformFileManager directly).

REGISTER_ECA_COMMAND(FECACommand_TakeBlueprintEditorScreenshot)

//------------------------------------------------------------------------------
// TakeBlueprintEditorScreenshot
//------------------------------------------------------------------------------

FECACommandResult FECACommand_TakeBlueprintEditorScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// ── Parameter extraction ───────────────────────────────────────────────
	FString BlueprintPath;
	if (!GetStringParam(Params, TEXT("blueprint_path"), BlueprintPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: blueprint_path"));
	}

	FString GraphName = TEXT("EventGraph");
	GetStringParam(Params, TEXT("graph_name"), GraphName, /*bRequired=*/false);

	bool bFrameAllNodes = true;
	GetBoolParam(Params, TEXT("frame_all_nodes"), bFrameAllNodes, /*bRequired=*/false);

	bool bHideOverlays = true;
	GetBoolParam(Params, TEXT("hide_overlays"), bHideOverlays, /*bRequired=*/false);

	FString FilePath;
	const bool bSaveToFile = GetStringParam(Params, TEXT("file_path"), FilePath, /*bRequired=*/false) && !FilePath.IsEmpty();

	// Width/height are advisory. We don't actually resize the SGraphEditor widget
	// — its size is dictated by the BP editor's docked layout. We accept these for
	// API symmetry with take_gameplay_screenshot and surface the actual size in
	// the result so callers can detect mismatches.
	double WidthD = 1920.0;
	double HeightD = 1080.0;
	GetFloatParam(Params, TEXT("width"), WidthD, /*bRequired=*/false);
	GetFloatParam(Params, TEXT("height"), HeightD, /*bRequired=*/false);

	// ── Load the Blueprint ─────────────────────────────────────────────────
	UBlueprint* Blueprint = LoadBlueprintByPath(BlueprintPath);
	if (!Blueprint)
	{
		return FECACommandResult::ValidationError(this,
			FString::Printf(TEXT("Blueprint not found at path: %s"), *BlueprintPath));
	}

	// ── Open the asset editor ──────────────────────────────────────────────
	if (!GEditor)
	{
		return FECACommandResult::Error(TEXT("GEditor not available — this command requires the editor to be running."));
	}

	UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!EditorSubsystem)
	{
		return FECACommandResult::Error(TEXT("UAssetEditorSubsystem not available."));
	}

	const bool bOpened = EditorSubsystem->OpenEditorForAsset(Blueprint);
	if (!bOpened)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Failed to open Blueprint editor for '%s'. The Blueprint may have a compilation error — try running compile_blueprint first to diagnose, then retry."),
			*BlueprintPath));
	}

	// Recover the editor instance and cast to FBlueprintEditor. The canonical
	// pattern (see AssetTypeActions_Blueprint.cpp:118) is a static_cast on the
	// IAssetEditorInstance pointer returned by FindEditorForAsset.
	IAssetEditorInstance* EditorInstance = EditorSubsystem->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/true);
	if (!EditorInstance)
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("OpenEditorForAsset succeeded for '%s' but FindEditorForAsset returned null — the editor toolkit may be torn-down or not yet fully constructed."),
			*BlueprintPath));
	}

	FBlueprintEditor* BPEditor = static_cast<FBlueprintEditor*>(EditorInstance);

	// Make sure the editor's window is foregrounded so its Slate widgets are
	// actually realized for capture. FocusWindow is on the base IAssetEditorInstance.
	BPEditor->FocusWindow();

	// ── Resolve the target graph ───────────────────────────────────────────
	UEdGraph* TargetGraph = nullptr;
	const FName GraphFName(*GraphName);

	for (UEdGraph* G : Blueprint->UbergraphPages)
	{
		if (G && G->GetFName() == GraphFName)
		{
			TargetGraph = G;
			break;
		}
	}
	if (!TargetGraph)
	{
		for (UEdGraph* G : Blueprint->FunctionGraphs)
		{
			if (G && G->GetFName() == GraphFName)
			{
				TargetGraph = G;
				break;
			}
		}
	}

	// EventGraph fallback: when the user (or the default) asks for "EventGraph"
	// but the Blueprint's UbergraphPage is named something else (the engine
	// historically names it "EventGraph" but custom Blueprints can rename), pick
	// the first UbergraphPage as a best-effort match.
	FString GraphNameUsed = GraphName;
	if (!TargetGraph && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase) && Blueprint->UbergraphPages.Num() > 0)
	{
		TargetGraph = Blueprint->UbergraphPages[0];
		if (TargetGraph)
		{
			GraphNameUsed = TargetGraph->GetName();
		}
	}

	if (!TargetGraph)
	{
		// Build a hint listing available graphs so the caller knows what to ask for next.
		TArray<FString> AvailableNames;
		for (UEdGraph* G : Blueprint->UbergraphPages)
		{
			if (G) AvailableNames.Add(G->GetName());
		}
		for (UEdGraph* G : Blueprint->FunctionGraphs)
		{
			if (G) AvailableNames.Add(G->GetName());
		}
		return FECACommandResult::Error(FString::Printf(
			TEXT("Graph '%s' not found on Blueprint '%s'. Available graphs: %s"),
			*GraphName, *BlueprintPath,
			AvailableNames.Num() > 0 ? *FString::Join(AvailableNames, TEXT(", ")) : TEXT("(none)")));
	}

	// ── Bring the graph to front and grab its SGraphEditor widget ──────────
	TSharedPtr<SGraphEditor> GraphEditor = BPEditor->OpenGraphAndBringToFront(TargetGraph, /*bSetFocus=*/true);
	if (!GraphEditor.IsValid())
	{
		// Fall back to the currently-focused graph editor — if the requested graph
		// resolved but couldn't be promoted to a dedicated panel (some BP modes
		// reuse a single graph slot), GetFocusedGraph + a manual lookup is the
		// only public API we have.
		UEdGraph* FocusedGraph = BPEditor->GetFocusedGraph();
		if (FocusedGraph)
		{
			GraphEditor = BPEditor->OpenGraphAndBringToFront(FocusedGraph, /*bSetFocus=*/true);
		}
	}

	if (!GraphEditor.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Could not obtain SGraphEditor widget for graph '%s' on '%s'. The BP editor may not have completed its initial tab construction yet — retry once or open the editor manually first."),
			*GraphNameUsed, *BlueprintPath));
	}

	// ── Layout pass: frame nodes, hide overlays ────────────────────────────
	if (bHideOverlays)
	{
		// Best-effort: clearing selection removes the orange selection rectangles
		// around nodes and pins. The BP editor's pinned watch values, debug
		// breakpoint glyphs, and PIE debug-arrow overlays are NOT togglable via
		// the public 5.7/5.8 SGraphEditor API — they're owned by per-node Slate
		// children and FKismetDebugUtilities globals. If you need a totally
		// clean shot, stop PIE and remove watches before calling this command.
		GraphEditor->ClearSelectionSet();
	}

	if (bFrameAllNodes)
	{
		// SGraphEditor::ZoomToFit(bOnlySelection=false) zooms-to-fit on all nodes
		// in the current graph. Empty graphs (no nodes) become a no-op.
		GraphEditor->ZoomToFit(/*bOnlySelection=*/false);
	}

	// ── Let Slate settle ───────────────────────────────────────────────────
	// Two ticks: one to apply the ZoomToFit's deferred view-target interpolation,
	// one to let the resulting Paint pass actually run. Without this we routinely
	// catch the previous frame's layout and the captured PNG shows mid-animation
	// graph state.
	FSlateApplication::Get().Tick();
	FSlateApplication::Get().Tick();

	// ── Capture the SGraphEditor widget ────────────────────────────────────
	TArray<FColor> Bitmap;
	FIntVector Size(0, 0, 0);
	FString Source = TEXT("blueprint_graph");
	FString CaptureNote;

	// SGraphEditor derives from SCompoundWidget → SWidget; TSharedRef upcasts implicitly.
	const bool bShot = FSlateApplication::Get().TakeScreenshot(
		GraphEditor.ToSharedRef(),
		Bitmap, Size);

	if (!bShot || Bitmap.Num() == 0)
	{
		// Slate-widget capture failed; degrade gracefully by capturing the parent
		// window. This still gives the caller a usable image (with editor chrome)
		// and is strictly better than returning nothing.
		Bitmap.Reset();
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(GraphEditor.ToSharedRef());
		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().TakeScreenshot(ParentWindow.ToSharedRef(), Bitmap, Size);
			Source = TEXT("blueprint_editor_window_fallback");
			CaptureNote = TEXT("SGraphEditor capture failed; returned the parent BP-editor window instead. Image includes editor chrome.");
		}

		if (Bitmap.Num() == 0)
		{
			return FECACommandResult::Error(TEXT("Failed to capture the Blueprint graph via Slate, and the parent-window fallback also returned an empty bitmap. The BP editor window may be hidden, minimized, or behind another window."));
		}
	}

	const int32 Width = Size.X;
	const int32 Height = Size.Y;
	if (Width == 0 || Height == 0)
	{
		return FECACommandResult::Error(TEXT("Captured bitmap has zero dimensions — the graph editor widget may not yet be allocated. Try opening the BP editor manually and re-running."));
	}

	// Force alpha to 255 (opaque). Slate widget captures often have alpha=0 in
	// transparent regions which makes PNGs appear black in most image viewers.
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	// ── Encode PNG ─────────────────────────────────────────────────────────
	TArray64<uint8> CompressedData;
	FImageUtils::PNGCompressImageArray(Width, Height,
		TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedData);

	if (CompressedData.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to compress Blueprint graph screenshot to PNG."));
	}

	// ── Build result + return inline OR write to file ──────────────────────
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Result->SetStringField(TEXT("graph_name"), GraphNameUsed);
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetStringField(TEXT("source"), Source);
	Result->SetBoolField(TEXT("framed_all_nodes"), bFrameAllNodes);
	Result->SetBoolField(TEXT("cleared_selection"), bHideOverlays);
	if (!CaptureNote.IsEmpty())
	{
		Result->SetStringField(TEXT("note"), CaptureNote);
	}

	// Surface advisory width/height mismatches so callers can detect the gap.
	const int32 RequestedW = FMath::Clamp(FMath::RoundToInt(WidthD), 1, 16384);
	const int32 RequestedH = FMath::Clamp(FMath::RoundToInt(HeightD), 1, 16384);
	if (RequestedW != Width || RequestedH != Height)
	{
		Result->SetStringField(TEXT("size_note"), FString::Printf(
			TEXT("Requested %dx%d but graph editor widget rendered at %dx%d. width/height are advisory; actual size is dictated by the BP editor's docked layout."),
			RequestedW, RequestedH, Width, Height));
	}

	if (bSaveToFile)
	{
		FString Directory = FPaths::GetPath(FilePath);
		IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Directory);
		if (!FFileHelper::SaveArrayToFile(
			TArrayView<const uint8>(CompressedData.GetData(), CompressedData.Num()), *FilePath))
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to write PNG to: %s"), *FilePath));
		}
		Result->SetStringField(TEXT("file_path"), FilePath);
		return FECACommandResult::Success(Result);
	}

	FECACommandResult Out = FECACommandResult::Success(Result);
	Out.McpContent.Add(FECACommandResult::MakeImageContent(CompressedData));
	return Out;
}
