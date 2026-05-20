// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAUMGVerbsCommands.h"
#include "Commands/ECAUMGVerbsHelpers.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"

REGISTER_ECA_COMMAND(FECACommand_AddWidget)

//------------------------------------------------------------------------------
// add_widget — polymorphic widget construction + attachment.
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintPath;
	if (!GetStringParam(Params, TEXT("widget_blueprint_path"), WidgetBlueprintPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: widget_blueprint_path"));
	}

	FString WidgetClassPath;
	if (!GetStringParam(Params, TEXT("widget_class"), WidgetClassPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: widget_class"));
	}

	FString NewWidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), NewWidgetName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: widget_name"));
	}

	FString ParentName;
	const bool bHasParentArg = GetStringParam(Params, TEXT("parent_widget_name"), ParentName, false) && !ParentName.IsEmpty();

	int32 ChildIndex = -1;
	GetIntParam(Params, TEXT("child_index"), ChildIndex, false);

	UWidgetBlueprint* WidgetBP = ECAUMGVerbs::LoadAssetTolerant<UWidgetBlueprint>(WidgetBlueprintPath);
	if (!WidgetBP)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetBlueprintPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	UClass* WidgetClass = ECAUMGVerbs::ResolveWidgetClass(WidgetClassPath);
	if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("widget_class '%s' is not a UWidget subclass"), *WidgetClassPath));
	}
	if (WidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("widget_class '%s' is abstract"), *WidgetClass->GetName()));
	}

	if (WidgetTree->FindWidget(FName(*NewWidgetName)))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("A widget named '%s' already exists in the tree"), *NewWidgetName));
	}

	// Locate parent (explicit, or fall back to root).
	UPanelWidget* ParentPanel = nullptr;
	if (bHasParentArg)
	{
		UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentName));
		if (!ParentWidget)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("parent_widget_name '%s' not found"), *ParentName));
		}
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("parent_widget_name '%s' is not a panel widget"), *ParentName));
		}
	}
	else if (WidgetTree->RootWidget)
	{
		ParentPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
		if (!ParentPanel)
		{
			return FECACommandResult::Error(TEXT("Tree root is not a panel widget; pass an explicit parent_widget_name"));
		}
	}

	WidgetTree->Modify();

	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*NewWidgetName));
	if (!NewWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to construct widget of class %s"), *WidgetClass->GetName()));
	}

	bool bIsRoot = false;
	UPanelSlot* AddedSlot = nullptr;

	if (!ParentPanel)
	{
		WidgetTree->RootWidget = NewWidget;
		bIsRoot = true;
	}
	else
	{
		AddedSlot = ParentPanel->AddChild(NewWidget);
		if (!AddedSlot)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Parent '%s' refused the new child (slot limit or type mismatch)"), *ParentPanel->GetName()));
		}

		if (ChildIndex >= 0 && ChildIndex < ParentPanel->GetChildrenCount())
		{
			ParentPanel->ShiftChild(ChildIndex, NewWidget);
		}
	}

	WidgetBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_name"), NewWidgetName);
	Result->SetStringField(TEXT("widget_class"), WidgetClass->GetName());
	Result->SetStringField(TEXT("parent_name"), ParentPanel ? ParentPanel->GetName() : FString());
	Result->SetStringField(TEXT("slot_class"), AddedSlot ? AddedSlot->GetClass()->GetName() : FString());
	Result->SetBoolField(TEXT("is_root"), bIsRoot);
	Result->SetBoolField(TEXT("is_variable"), NewWidget->bIsVariable);
	return FECACommandResult::Success(Result);
}
