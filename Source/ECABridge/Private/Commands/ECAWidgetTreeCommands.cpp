// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAWidgetTreeCommands.h"
#include "Commands/ECACommand.h"

// Engine - Widget Blueprint
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"

// Engine - UMG Components
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/StackBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

// Engine - UMG Slots (SlateWrapperTypes for FSlateChildSize; individual slot headers
// are NOT needed since we use UE reflection to set slot properties uniformly)
#include "Components/PanelSlot.h"
#include "Components/SlateWrapperTypes.h"

// Engine - Blueprint utilities
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SavePackage.h"

// Register all widget tree commands
REGISTER_ECA_COMMAND(FECACommand_WrapWidget)
REGISTER_ECA_COMMAND(FECACommand_UnwrapWidget)
REGISTER_ECA_COMMAND(FECACommand_ReparentWidget)
REGISTER_ECA_COMMAND(FECACommand_SetSlotProperty)
REGISTER_ECA_COMMAND(FECACommand_SetWidgetProperty)
REGISTER_ECA_COMMAND(FECACommand_AddChildWidget)
REGISTER_ECA_COMMAND(FECACommand_CompileWidgetBlueprint)
REGISTER_ECA_COMMAND(FECACommand_RemoveWidget)

//------------------------------------------------------------------------------
// Shared Helper Functions
//------------------------------------------------------------------------------

static UWidgetBlueprint* LoadWidgetBlueprintFromPath(const FString& WidgetPath)
{
	// Try loading directly
	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
	if (WBP)
	{
		return WBP;
	}

	// Try with common prefix patterns for short paths
	if (!WidgetPath.StartsWith(TEXT("/")))
	{
		WBP = LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("/Game/%s"), *WidgetPath));
		if (WBP)
		{
			return WBP;
		}
	}

	return nullptr;
}

static EHorizontalAlignment ParseHorizontalAlignment(const FString& Value)
{
	if (Value.Equals(TEXT("Left"), ESearchCase::IgnoreCase)) return HAlign_Left;
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return HAlign_Center;
	if (Value.Equals(TEXT("Right"), ESearchCase::IgnoreCase)) return HAlign_Right;
	if (Value.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return HAlign_Fill;
	return HAlign_Fill; // Default
}

static EVerticalAlignment ParseVerticalAlignment(const FString& Value)
{
	if (Value.Equals(TEXT("Top"), ESearchCase::IgnoreCase)) return VAlign_Top;
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return VAlign_Center;
	if (Value.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) return VAlign_Bottom;
	if (Value.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return VAlign_Fill;
	return VAlign_Fill; // Default
}

static ESlateVisibility ParseVisibility(const FString& Value)
{
	if (Value.Equals(TEXT("Visible"), ESearchCase::IgnoreCase)) return ESlateVisibility::Visible;
	if (Value.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase)) return ESlateVisibility::Collapsed;
	if (Value.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase)) return ESlateVisibility::Hidden;
	if (Value.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase)) return ESlateVisibility::HitTestInvisible;
	if (Value.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase)) return ESlateVisibility::SelfHitTestInvisible;
	return ESlateVisibility::Visible; // Default
}

static FMargin ParsePadding(const TSharedPtr<FJsonObject>& PaddingObj)
{
	FMargin Padding(0);
	if (PaddingObj.IsValid())
	{
		double Left = 0, Top = 0, Right = 0, Bottom = 0;
		PaddingObj->TryGetNumberField(TEXT("left"), Left);
		PaddingObj->TryGetNumberField(TEXT("top"), Top);
		PaddingObj->TryGetNumberField(TEXT("right"), Right);
		PaddingObj->TryGetNumberField(TEXT("bottom"), Bottom);
		Padding = FMargin(Left, Top, Right, Bottom);
	}
	return Padding;
}

/**
 * Ensure all widgets in the tree have GUIDs registered with the WidgetBlueprint.
 * The compiler's ValidateAndFixUpVariableGuids will ensure-fail on any widget
 * that was added to the tree without a corresponding GUID entry.
 * Call this before MarkBlueprintAsStructurallyModified.
 */
static void EnsureAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return;
	}

	WidgetBlueprint->WidgetTree->ForEachWidget([WidgetBlueprint](UWidget* Widget)
	{
		if (Widget && !WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			WidgetBlueprint->OnVariableAdded(Widget->GetFName());
		}
	});
}

/**
 * Find a UClass for any UWidget subclass by name.
 * Searches engine UMG classes, then project classes via UClass iterator.
 */
static UClass* FindWidgetClass(const FString& ClassName)
{
	// Try common engine classes first (fast path)
	static const TMap<FString, UClass*> EngineWidgets = []() {
		TMap<FString, UClass*> Map;
		Map.Add(TEXT("TextBlock"), UTextBlock::StaticClass());
		Map.Add(TEXT("Image"), UImage::StaticClass());
		Map.Add(TEXT("SizeBox"), USizeBox::StaticClass());
		Map.Add(TEXT("Overlay"), UOverlay::StaticClass());
		Map.Add(TEXT("Border"), UBorder::StaticClass());
		Map.Add(TEXT("ScaleBox"), UScaleBox::StaticClass());
		Map.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
		Map.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
		Map.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
		Map.Add(TEXT("StackBox"), UStackBox::StaticClass());
		return Map;
	}();

	// Case-insensitive engine class lookup
	for (const auto& Pair : EngineWidgets)
	{
		if (Pair.Key.Equals(ClassName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}

	// Strip 'U' prefix if present for engine lookup
	FString SearchName = ClassName;
	if (SearchName.StartsWith(TEXT("U")))
	{
		SearchName = SearchName.RightChop(1);
		for (const auto& Pair : EngineWidgets)
		{
			if (Pair.Key.Equals(SearchName, ESearchCase::IgnoreCase))
			{
				return Pair.Value;
			}
		}
	}

	// Search all loaded UWidget subclasses (covers project/plugin types like CommonTextBlock, WBP_*_C)
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UWidget::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			if (Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
	}

	return nullptr;
}

/**
 * Find a UClass for a panel widget by name.
 * Searches common UMG panel/content widget classes.
 */
static UClass* FindPanelWidgetClass(const FString& ClassName)
{
	// Direct lookup table for common wrapper classes
	static const TMap<FString, UClass*> ClassMap = []() {
		TMap<FString, UClass*> Map;
		Map.Add(TEXT("SizeBox"), USizeBox::StaticClass());
		Map.Add(TEXT("Overlay"), UOverlay::StaticClass());
		Map.Add(TEXT("Border"), UBorder::StaticClass());
		Map.Add(TEXT("ScaleBox"), UScaleBox::StaticClass());
		Map.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
		Map.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
		Map.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
		Map.Add(TEXT("StackBox"), UStackBox::StaticClass());
		return Map;
	}();

	// Case-insensitive search
	for (const auto& Pair : ClassMap)
	{
		if (Pair.Key.Equals(ClassName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}

	// Try with 'U' prefix stripped/added
	FString SearchName = ClassName;
	if (SearchName.StartsWith(TEXT("U")))
	{
		SearchName = SearchName.RightChop(1);
	}
	for (const auto& Pair : ClassMap)
	{
		if (Pair.Key.Equals(SearchName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}

	// Fallback: try FindObject for any UPanelWidget subclass
	UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *ClassName));
	if (FoundClass && FoundClass->IsChildOf(UPanelWidget::StaticClass()))
	{
		return FoundClass;
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// WrapWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_WrapWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	FString WrapperClassName;
	if (!GetStringParam(Params, TEXT("wrapper_class"), WrapperClassName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: wrapper_class"));
	}

	FString WrapperName;
	GetStringParam(Params, TEXT("wrapper_name"), WrapperName, false);

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the target widget
	UWidget* TargetWidget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!TargetWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	// Find the wrapper class
	UClass* WrapperClass = FindPanelWidgetClass(WrapperClassName);
	if (!WrapperClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget class not found: %s. Valid classes: SizeBox, Overlay, Border, ScaleBox, CanvasPanel, VerticalBox, HorizontalBox, StackBox"), *WrapperClassName));
	}

	// Auto-generate wrapper name if not provided
	if (WrapperName.IsEmpty())
	{
		WrapperName = FString::Printf(TEXT("%s_%s"), *WrapperClass->GetName(), *WidgetName);
	}

	// Check for name collision
	if (WidgetTree->FindWidget(FName(*WrapperName)))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("A widget named '%s' already exists in the tree. Provide a unique wrapper_name."), *WrapperName));
	}

	// Find the target's parent and index
	int32 ChildIndex = INDEX_NONE;
	UPanelWidget* ParentWidget = WidgetTree->FindWidgetParent(TargetWidget, ChildIndex);

	// Construct the wrapper widget
	UPanelWidget* WrapperWidget = WidgetTree->ConstructWidget<UPanelWidget>(WrapperClass, FName(*WrapperName));
	if (!WrapperWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to construct wrapper widget of class: %s"), *WrapperClassName));
	}

	WrapperWidget->SetFlags(RF_Transactional);

	// Perform the wrap operation
	FString ParentName;
	if (ParentWidget)
	{
		// Case 1: Widget has a parent - replace the widget at its index with the wrapper,
		// then add the widget as a child of the wrapper
		ParentName = ParentWidget->GetName();
		ParentWidget->ReplaceChildAt(ChildIndex, WrapperWidget);
		WrapperWidget->AddChild(TargetWidget);
	}
	else if (TargetWidget == WidgetTree->RootWidget)
	{
		// Case 2: Widget is the root - make the wrapper the new root
		ParentName = TEXT("[root]");
		WidgetTree->RootWidget = WrapperWidget;
		WrapperWidget->AddChild(TargetWidget);
	}
	else
	{
		// Clean up the orphaned wrapper before returning
		WidgetTree->RemoveWidget(WrapperWidget);
		return FECACommandResult::Error(TEXT("Widget has no parent and is not the root widget. Cannot determine where to insert wrapper."));
	}

	// Ensure all widgets have GUIDs, then mark blueprint as modified (dirty + recompile)
	EnsureAllWidgetGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("wrapper_name"), WrapperName);
	Result->SetStringField(TEXT("wrapper_class"), WrapperClass->GetName());
	Result->SetStringField(TEXT("wrapped_widget"), WidgetName);
	Result->SetStringField(TEXT("parent"), ParentName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// UnwrapWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_UnwrapWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the wrapper widget
	UWidget* WrapperAsWidget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!WrapperAsWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	UPanelWidget* WrapperWidget = Cast<UPanelWidget>(WrapperAsWidget);
	if (!WrapperWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget '%s' is not a panel widget and cannot be unwrapped"), *WidgetName));
	}

	// Handle based on child count
	int32 ChildCount = WrapperWidget->GetChildrenCount();
	if (ChildCount > 1)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget '%s' has %d children. Can only unwrap widgets with 0 or 1 children."), *WidgetName, ChildCount));
	}

	// Find the wrapper's parent
	int32 WrapperIndex = INDEX_NONE;
	UPanelWidget* ParentWidget = WidgetTree->FindWidgetParent(WrapperAsWidget, WrapperIndex);

	FString ParentName;
	FString ChildName = TEXT("[none]");

	if (ChildCount == 1)
	{
		// Has one child — promote it to take the wrapper's place
		UWidget* ChildWidget = WrapperWidget->GetChildAt(0);
		if (!ChildWidget)
		{
			return FECACommandResult::Error(TEXT("Child widget is null"));
		}

		ChildName = ChildWidget->GetName();
		ChildWidget->RemoveFromParent();

		if (ParentWidget)
		{
			ParentName = ParentWidget->GetName();
			ParentWidget->ReplaceChildAt(WrapperIndex, ChildWidget);
		}
		else if (WrapperAsWidget == WidgetTree->RootWidget)
		{
			ParentName = TEXT("[root]");
			WidgetTree->RootWidget = ChildWidget;
		}
		else
		{
			WrapperWidget->AddChild(ChildWidget);
			return FECACommandResult::Error(TEXT("Wrapper has no parent and is not the root widget. Cannot determine where to place the promoted child."));
		}
	}
	else
	{
		// Empty panel (0 children) — just remove it
		if (ParentWidget)
		{
			ParentName = ParentWidget->GetName();
			ParentWidget->RemoveChild(WrapperAsWidget);
		}
		else if (WrapperAsWidget == WidgetTree->RootWidget)
		{
			return FECACommandResult::Error(TEXT("Cannot remove the root widget when it has no children. Add a child first, or use wrap_widget to replace the root."));
		}
		else
		{
			return FECACommandResult::Error(TEXT("Wrapper has no parent and is not the root widget."));
		}
	}

	// Remove the wrapper from the tree
	WidgetTree->RemoveWidget(WrapperAsWidget);

	// Ensure all widgets have GUIDs, then mark blueprint as modified
	EnsureAllWidgetGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("removed_wrapper"), WidgetName);
	Result->SetStringField(TEXT("promoted_child"), ChildName);
	Result->SetStringField(TEXT("new_parent"), ParentName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// ReparentWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_ReparentWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	FString NewParentName;
	if (!GetStringParam(Params, TEXT("new_parent_name"), NewParentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: new_parent_name"));
	}

	int32 InsertIndex = -1;
	GetIntParam(Params, TEXT("insert_index"), InsertIndex, false);

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find both widgets
	UWidget* Widget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	UWidget* NewParentAsWidget = WidgetTree->FindWidget(FName(*NewParentName));
	if (!NewParentAsWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Target parent widget not found: %s"), *NewParentName));
	}

	UPanelWidget* NewParent = Cast<UPanelWidget>(NewParentAsWidget);
	if (!NewParent)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Target '%s' is not a panel widget and cannot accept children"), *NewParentName));
	}

	// Self-reparent guard
	if (Widget == NewParentAsWidget)
	{
		return FECACommandResult::Error(TEXT("Cannot reparent a widget to itself."));
	}

	// Root widget guard - reparenting root corrupts the tree
	if (Widget == WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Cannot reparent the root widget. Use wrap_widget to insert a new root, or restructure the hierarchy."));
	}

	// Check if the new parent can accept more children
	if (!NewParent->CanAddMoreChildren())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Target '%s' cannot accept more children (it is a %s which already has a child)"), *NewParentName, *NewParent->GetClass()->GetName()));
	}

	// Circular reparent check: ensure new parent is not a descendant of the widget
	if (UPanelWidget* WidgetAsPanel = Cast<UPanelWidget>(Widget))
	{
		int32 DescendantIndex = WidgetTree->FindChildIndex(WidgetAsPanel, NewParentAsWidget);
		if (DescendantIndex != INDEX_NONE)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Circular reparenting detected: '%s' is a descendant of '%s'"), *NewParentName, *WidgetName));
		}
	}

	// Capture old parent info for the response
	int32 OldIndex = INDEX_NONE;
	UPanelWidget* OldParent = WidgetTree->FindWidgetParent(Widget, OldIndex);
	FString OldParentName = OldParent ? OldParent->GetName() : TEXT("[root]");

	// Perform the reparent
	// AddChild internally calls RemoveFromParent, creating a new slot
	UPanelSlot* NewSlot = nullptr;
	if (InsertIndex >= 0)
	{
		NewSlot = NewParent->InsertChildAt(InsertIndex, Widget);
	}
	else
	{
		NewSlot = NewParent->AddChild(Widget);
	}

	if (!NewSlot)
	{
		return FECACommandResult::Error(TEXT("Failed to add widget to new parent. The widget may have been left in a detached state."));
	}

	int32 NewIndex = NewParent->GetChildIndex(Widget);

	// Ensure all widgets have GUIDs, then mark blueprint as modified
	EnsureAllWidgetGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget"), WidgetName);
	Result->SetStringField(TEXT("old_parent"), OldParentName);
	Result->SetNumberField(TEXT("old_index"), OldIndex);
	Result->SetStringField(TEXT("new_parent"), NewParentName);
	Result->SetNumberField(TEXT("new_index"), NewIndex);
	Result->SetStringField(TEXT("warning"), TEXT("Slot properties (padding, alignment, size rule) were reset to defaults. Use set_slot_property to re-apply if needed."));
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetSlotProperty
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetSlotProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the widget
	UWidget* Widget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	// Get the widget's slot
	UPanelSlot* Slot = Widget->Slot;
	if (!Slot)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget '%s' has no slot (it may be the root widget or detached)"), *WidgetName));
	}

	// Track what we set
	TArray<FString> PropertiesSet;
	TArray<FString> Warnings;
	FString SlotTypeName = Slot->GetClass()->GetName();

	// Use UE reflection to set slot properties uniformly across all slot types.
	// This replaces per-type cast chains for UOverlaySlot, UVerticalBoxSlot,
	// UHorizontalBoxSlot, USizeBoxSlot, UBorderSlot, UStackBoxSlot, etc.
	// All these slots expose Padding, HorizontalAlignment, and VerticalAlignment
	// as UPROPERTYs with the same names and types.
	//
	// Ideally UPanelSlot itself would expose virtual setters for these common
	// properties, but until that engine change happens, reflection works reliably.

	// Parse and apply padding
	const TSharedPtr<FJsonObject>* PaddingObj = nullptr;
	if (Params->TryGetObjectField(TEXT("padding"), PaddingObj) && PaddingObj)
	{
		FMargin Padding = ParsePadding(*PaddingObj);
		FStructProperty* Prop = CastField<FStructProperty>(Slot->GetClass()->FindPropertyByName(TEXT("Padding")));
		if (Prop && Prop->Struct == TBaseStructure<FMargin>::Get())
		{
			*Prop->ContainerPtrToValuePtr<FMargin>(Slot) = Padding;
			Slot->SynchronizeProperties();
			PropertiesSet.Add(TEXT("padding"));
		}
		else
		{
			if (Prop)
			{
				UE_LOG(LogTemp, Warning, TEXT("ECABridge: Slot '%s' has a Padding property but it is not FMargin (struct: %s). Skipping."), *SlotTypeName, *Prop->Struct->GetName());
			}
			Warnings.Add(FString::Printf(TEXT("padding is not supported on slot type: %s"), *SlotTypeName));
		}
	}

	// Parse and apply horizontal alignment
	FString HAlignStr;
	if (GetStringParam(Params, TEXT("horizontal_alignment"), HAlignStr, false))
	{
		EHorizontalAlignment HAlign = ParseHorizontalAlignment(HAlignStr);
		FProperty* Prop = Slot->GetClass()->FindPropertyByName(TEXT("HorizontalAlignment"));
		if (Prop && Prop->GetSize() == sizeof(TEnumAsByte<EHorizontalAlignment>))
		{
			*Prop->ContainerPtrToValuePtr<TEnumAsByte<EHorizontalAlignment>>(Slot) = HAlign;
			Slot->SynchronizeProperties();
			PropertiesSet.Add(TEXT("horizontal_alignment"));
		}
		else
		{
			if (Prop)
			{
				UE_LOG(LogTemp, Warning, TEXT("ECABridge: Slot '%s' has HorizontalAlignment property but unexpected size %d (expected %d). Skipping."), *SlotTypeName, Prop->GetSize(), (int32)sizeof(TEnumAsByte<EHorizontalAlignment>));
			}
			Warnings.Add(FString::Printf(TEXT("horizontal_alignment is not supported on slot type: %s"), *SlotTypeName));
		}
	}

	// Parse and apply vertical alignment
	FString VAlignStr;
	if (GetStringParam(Params, TEXT("vertical_alignment"), VAlignStr, false))
	{
		EVerticalAlignment VAlign = ParseVerticalAlignment(VAlignStr);
		FProperty* Prop = Slot->GetClass()->FindPropertyByName(TEXT("VerticalAlignment"));
		if (Prop && Prop->GetSize() == sizeof(TEnumAsByte<EVerticalAlignment>))
		{
			*Prop->ContainerPtrToValuePtr<TEnumAsByte<EVerticalAlignment>>(Slot) = VAlign;
			Slot->SynchronizeProperties();
			PropertiesSet.Add(TEXT("vertical_alignment"));
		}
		else
		{
			if (Prop)
			{
				UE_LOG(LogTemp, Warning, TEXT("ECABridge: Slot '%s' has VerticalAlignment property but unexpected size %d (expected %d). Skipping."), *SlotTypeName, Prop->GetSize(), (int32)sizeof(TEnumAsByte<EVerticalAlignment>));
			}
			Warnings.Add(FString::Printf(TEXT("vertical_alignment is not supported on slot type: %s"), *SlotTypeName));
		}
	}

	// Parse and apply size rule (only VBox/HBox/StackBox slots have a Size property)
	FString SizeRuleStr;
	if (GetStringParam(Params, TEXT("size_rule"), SizeRuleStr, false))
	{
		double SizeValue = 1.0;
		GetFloatParam(Params, TEXT("size_value"), SizeValue, false);

		bool bValidSizeRule = true;
		FSlateChildSize SlateSize;
		if (SizeRuleStr.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
		{
			SlateSize = FSlateChildSize(ESlateSizeRule::Automatic);
		}
		else if (SizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
		{
			SlateSize = FSlateChildSize(ESlateSizeRule::Fill);
			SlateSize.Value = SizeValue;
		}
		else
		{
			Warnings.Add(FString::Printf(TEXT("Unknown size_rule: '%s'. Use 'Auto' or 'Fill'."), *SizeRuleStr));
			bValidSizeRule = false;
		}

		if (bValidSizeRule)
		{
			FStructProperty* Prop = CastField<FStructProperty>(Slot->GetClass()->FindPropertyByName(TEXT("Size")));
			if (Prop && Prop->Struct == FSlateChildSize::StaticStruct())
			{
				*Prop->ContainerPtrToValuePtr<FSlateChildSize>(Slot) = SlateSize;
				Slot->SynchronizeProperties();
				PropertiesSet.Add(TEXT("size_rule"));
			}
			else
			{
				if (Prop)
				{
					UE_LOG(LogTemp, Warning, TEXT("ECABridge: Slot '%s' has a Size property but it is not FSlateChildSize (struct: %s). Skipping."), *SlotTypeName, *Prop->Struct->GetName());
				}
				Warnings.Add(FString::Printf(TEXT("size_rule is not supported on slot type: %s"), *SlotTypeName));
			}
		}
	}

	if (PropertiesSet.Num() == 0 && Warnings.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("No slot properties were specified. Provide at least one of: padding, horizontal_alignment, vertical_alignment, size_rule"));
	}

	// Mark blueprint as modified (not structurally - no tree changes, just property values)
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget"), WidgetName);
	Result->SetStringField(TEXT("slot_type"), SlotTypeName);

	TArray<TSharedPtr<FJsonValue>> SetArray;
	for (const FString& Prop : PropertiesSet)
	{
		SetArray.Add(MakeShared<FJsonValueString>(Prop));
	}
	Result->SetArrayField(TEXT("properties_set"), SetArray);

	if (Warnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArray;
		for (const FString& Warn : Warnings)
		{
			WarnArray.Add(MakeShared<FJsonValueString>(Warn));
		}
		Result->SetArrayField(TEXT("warnings"), WarnArray);
	}

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetWidgetProperty
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetWidgetProperty::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	const TSharedPtr<FJsonObject>* PropertiesObjPtr = nullptr;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObjPtr) || !PropertiesObjPtr || !PropertiesObjPtr->IsValid())
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: properties (must be a JSON object)"));
	}
	const TSharedPtr<FJsonObject>& Properties = *PropertiesObjPtr;

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the widget
	UWidget* Widget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	TArray<FString> PropertiesSet;
	TArray<FString> PropertiesSkipped;

	// Process each property
	for (const auto& Pair : Properties->Values)
	{
		const FString PropName(*Pair.Key);
		const TSharedPtr<FJsonValue>& PropValue = Pair.Value;

		bool bHandled = false;

		// --- Universal UWidget properties ---

		if (PropName.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
		{
			FString VisStr;
			if (PropValue->TryGetString(VisStr))
			{
				Widget->SetVisibility(ParseVisibility(VisStr));
				bHandled = true;
			}
		}
		else if (PropName.Equals(TEXT("RenderOpacity"), ESearchCase::IgnoreCase))
		{
			double Opacity = 1.0;
			if (PropValue->TryGetNumber(Opacity))
			{
				Widget->SetRenderOpacity(Opacity);
				bHandled = true;
			}
		}
		else if (PropName.Equals(TEXT("IsEnabled"), ESearchCase::IgnoreCase))
		{
			bool bEnabled = true;
			if (PropValue->TryGetBool(bEnabled))
			{
				Widget->SetIsEnabled(bEnabled);
				bHandled = true;
			}
		}
		else if (PropName.Equals(TEXT("ToolTipText"), ESearchCase::IgnoreCase))
		{
			FString TooltipStr;
			if (PropValue->TryGetString(TooltipStr))
			{
				Widget->SetToolTipText(FText::FromString(TooltipStr));
				bHandled = true;
			}
		}
		else if (PropName.Equals(TEXT("bIsVariable"), ESearchCase::IgnoreCase))
		{
			bool bIsVariable = false;
			if (PropValue->TryGetBool(bIsVariable))
			{
				Widget->bIsVariable = bIsVariable;
				bHandled = true;
			}
		}

		// --- USizeBox properties ---
		else if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
		{
			if (PropName.Equals(TEXT("WidthOverride"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetWidthOverride(Val);
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("HeightOverride"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetHeightOverride(Val);
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("MinDesiredWidth"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetMinDesiredWidth(Val);
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("MinDesiredHeight"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetMinDesiredHeight(Val);
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("MaxDesiredWidth"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetMaxDesiredWidth(Val);
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("MaxDesiredHeight"), ESearchCase::IgnoreCase))
			{
				double Val = 0;
				if (PropValue->TryGetNumber(Val))
				{
					SizeBox->SetMaxDesiredHeight(Val);
					bHandled = true;
				}
			}
		}

		// --- UTextBlock properties ---
		else if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			if (PropName.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
			{
				FString TextStr;
				if (PropValue->TryGetString(TextStr))
				{
					TextBlock->SetText(FText::FromString(TextStr));
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("ColorAndOpacity"), ESearchCase::IgnoreCase))
			{
				const TSharedPtr<FJsonObject>* ColorObj = nullptr;
				if (PropValue->TryGetObject(ColorObj) && ColorObj)
				{
					double R = 1, G = 1, B = 1, A = 1;
					(*ColorObj)->TryGetNumberField(TEXT("r"), R);
					(*ColorObj)->TryGetNumberField(TEXT("g"), G);
					(*ColorObj)->TryGetNumberField(TEXT("b"), B);
					(*ColorObj)->TryGetNumberField(TEXT("a"), A);
					TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(R, G, B, A)));
					bHandled = true;
				}
			}
		}

		// --- UImage properties ---
		else if (UImage* Image = Cast<UImage>(Widget))
		{
			if (PropName.Equals(TEXT("ColorAndOpacity"), ESearchCase::IgnoreCase))
			{
				const TSharedPtr<FJsonObject>* ColorObj = nullptr;
				if (PropValue->TryGetObject(ColorObj) && ColorObj)
				{
					double R = 1, G = 1, B = 1, A = 1;
					(*ColorObj)->TryGetNumberField(TEXT("r"), R);
					(*ColorObj)->TryGetNumberField(TEXT("g"), G);
					(*ColorObj)->TryGetNumberField(TEXT("b"), B);
					(*ColorObj)->TryGetNumberField(TEXT("a"), A);
					Image->SetColorAndOpacity(FLinearColor(R, G, B, A));
					bHandled = true;
				}
			}
			else if (PropName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase))
			{
				double Opacity = 1.0;
				if (PropValue->TryGetNumber(Opacity))
				{
					Image->SetOpacity(Opacity);
					bHandled = true;
				}
			}
		}

		if (bHandled)
		{
			PropertiesSet.Add(PropName);
		}
		else
		{
			FString WidgetClass = Widget->GetClass()->GetName();
			PropertiesSkipped.Add(FString::Printf(TEXT("%s (not supported on %s)"), *PropName, *WidgetClass));
		}
	}

	if (PropertiesSet.Num() == 0)
	{
		FString SkippedList;
		for (const FString& S : PropertiesSkipped)
		{
			if (!SkippedList.IsEmpty()) SkippedList += TEXT(", ");
			SkippedList += S;
		}
		return FECACommandResult::Error(FString::Printf(TEXT("No properties were applied. Skipped: %s"), *SkippedList));
	}

	// Mark blueprint as modified (not structurally - no tree changes, just property values)
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget"), WidgetName);
	Result->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetName());

	TArray<TSharedPtr<FJsonValue>> SetArray;
	for (const FString& Prop : PropertiesSet)
	{
		SetArray.Add(MakeShared<FJsonValueString>(Prop));
	}
	Result->SetArrayField(TEXT("properties_set"), SetArray);

	if (PropertiesSkipped.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> SkipArray;
		for (const FString& Skip : PropertiesSkipped)
		{
			SkipArray.Add(MakeShared<FJsonValueString>(Skip));
		}
		Result->SetArrayField(TEXT("properties_skipped"), SkipArray);
	}

	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddChildWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddChildWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString ParentName;
	if (!GetStringParam(Params, TEXT("parent_name"), ParentName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: parent_name"));
	}

	FString WidgetClassName;
	if (!GetStringParam(Params, TEXT("widget_class"), WidgetClassName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_class"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	int32 InsertIndex = -1;
	GetIntParam(Params, TEXT("insert_index"), InsertIndex, false);

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the parent widget
	UWidget* ParentAsWidget = WidgetTree->FindWidget(FName(*ParentName));
	if (!ParentAsWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Parent widget not found: %s"), *ParentName));
	}

	UPanelWidget* Parent = Cast<UPanelWidget>(ParentAsWidget);
	if (!Parent)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Parent '%s' is not a panel widget and cannot accept children"), *ParentName));
	}

	if (!Parent->CanAddMoreChildren())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Parent '%s' cannot accept more children (it is a %s which already has a child)"), *ParentName, *Parent->GetClass()->GetName()));
	}

	// Check for name collision
	if (WidgetTree->FindWidget(FName(*WidgetName)))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("A widget named '%s' already exists in the tree"), *WidgetName));
	}

	// Find the widget class
	UClass* WidgetClass = FindWidgetClass(WidgetClassName);
	if (!WidgetClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget class not found: %s"), *WidgetClassName));
	}

	if (!WidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Class '%s' is not a UWidget subclass"), *WidgetClassName));
	}

	// Construct the widget
	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to construct widget of class: %s"), *WidgetClassName));
	}

	NewWidget->SetFlags(RF_Transactional);

	// Add to parent
	UPanelSlot* NewSlot = nullptr;
	if (InsertIndex >= 0)
	{
		NewSlot = Parent->InsertChildAt(InsertIndex, NewWidget);
	}
	else
	{
		NewSlot = Parent->AddChild(NewWidget);
	}

	if (!NewSlot)
	{
		WidgetTree->RemoveWidget(NewWidget);
		return FECACommandResult::Error(TEXT("Failed to add widget to parent"));
	}

	int32 ActualIndex = Parent->GetChildIndex(NewWidget);

	// Unless skip_compile is set, ensure GUIDs and recompile
	bool bSkipCompile = false;
	GetBoolParam(Params, TEXT("skip_compile"), bSkipCompile, false);
	if (!bSkipCompile)
	{
		EnsureAllWidgetGuids(WidgetBlueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
	else
	{
		WidgetBlueprint->MarkPackageDirty();
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("widget_class"), WidgetClass->GetName());
	Result->SetStringField(TEXT("parent"), ParentName);
	Result->SetNumberField(TEXT("index"), ActualIndex);
	return FECACommandResult::Success(Result);
}


//------------------------------------------------------------------------------
// CompileWidgetBlueprint
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CompileWidgetBlueprint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	EnsureAllWidgetGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Save the package to disk so changes aren't lost on editor close/crash
	bool bSaved = false;
	UPackage* Package = WidgetBlueprint->GetPackage();
	if (Package)
	{
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		bSaved = UPackage::SavePackage(Package, WidgetBlueprint, *PackageFileName, SaveArgs);
		if (bSaved)
		{
			UE_LOG(LogTemp, Log, TEXT("ECABridge: Saved %s"), *PackageFileName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ECABridge: Failed to save %s"), *PackageFileName);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetBoolField(TEXT("compiled"), true);
	Result->SetBoolField(TEXT("saved"), bSaved);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// RemoveWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_RemoveWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	// Parse required parameters
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprintFromPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Find the widget
	UWidget* Widget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	// Don't allow removing the root widget
	if (Widget == WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Cannot remove the root widget. Use wrap_widget to replace the root, or clear the entire tree."));
	}

	// Count descendants for the response
	int32 DescendantCount = 0;
	FString WidgetClass = Widget->GetClass()->GetName();
	UPanelWidget* Parent = Cast<UPanelWidget>(Widget->GetParent());
	FString ParentName = Parent ? Parent->GetName() : TEXT("[unknown]");

	if (UPanelWidget* WidgetAsPanel = Cast<UPanelWidget>(Widget))
	{
		TArray<UWidget*> Descendants;
		WidgetTree->GetChildWidgets(Widget, Descendants);
		DescendantCount = Descendants.Num();
	}

	// RemoveWidget handles detaching from parent and recursive cleanup
	WidgetTree->RemoveWidget(Widget);

	// Ensure all remaining widgets have GUIDs, then mark blueprint as modified
	EnsureAllWidgetGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("removed_widget"), WidgetName);
	Result->SetStringField(TEXT("widget_class"), WidgetClass);
	Result->SetStringField(TEXT("parent"), ParentName);
	Result->SetNumberField(TEXT("descendants_removed"), DescendantCount);
	return FECACommandResult::Success(Result);
}
