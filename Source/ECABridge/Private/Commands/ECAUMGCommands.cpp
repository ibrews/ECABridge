// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAUMGCommands.h"
#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "UObject/UObjectIterator.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"

// Register all UMG commands
REGISTER_ECA_COMMAND(FECACommand_CreateUMGWidgetBlueprint)
REGISTER_ECA_COMMAND(FECACommand_AddTextBlockToWidget)
REGISTER_ECA_COMMAND(FECACommand_AddButtonToWidget)
REGISTER_ECA_COMMAND(FECACommand_AddWidgetToViewport)
REGISTER_ECA_COMMAND(FECACommand_BindWidgetEvent)
REGISTER_ECA_COMMAND(FECACommand_SetTextBlockBinding)
REGISTER_ECA_COMMAND(FECACommand_AddImageToWidget)
REGISTER_ECA_COMMAND(FECACommand_AddWidgetElement)
REGISTER_ECA_COMMAND(FECACommand_ListWidgetClasses)
REGISTER_ECA_COMMAND(FECACommand_GetWidgetInfo)

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

static UWidgetBlueprint* LoadUMGWidgetBlueprintByPath(const FString& WidgetPath)
{
	return LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
}

static FVector2D GetPositionFromParams(const TSharedPtr<FJsonObject>& Params)
{
	FVector2D Position(0, 0);
	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj))
	{
		Position.X = (*PosObj)->GetNumberField(TEXT("x"));
		Position.Y = (*PosObj)->GetNumberField(TEXT("y"));
	}
	return Position;
}

static FVector2D GetSizeFromParams(const TSharedPtr<FJsonObject>& Params, FVector2D DefaultSize = FVector2D(200, 50))
{
	FVector2D Size = DefaultSize;
	const TSharedPtr<FJsonObject>* SizeObj;
	if (Params->TryGetObjectField(TEXT("size"), SizeObj))
	{
		Size.X = (*SizeObj)->GetNumberField(TEXT("width"));
		Size.Y = (*SizeObj)->GetNumberField(TEXT("height"));
	}
	return Size;
}

static FLinearColor GetColorFromParams(const TSharedPtr<FJsonObject>& Params, const FString& ParamName, FLinearColor DefaultColor = FLinearColor::White)
{
	FLinearColor Color = DefaultColor;
	const TSharedPtr<FJsonObject>* ColorObj;
	if (Params->TryGetObjectField(*ParamName, ColorObj))
	{
		Color.R = (*ColorObj)->GetNumberField(TEXT("r"));
		Color.G = (*ColorObj)->GetNumberField(TEXT("g"));
		Color.B = (*ColorObj)->GetNumberField(TEXT("b"));
		double Alpha = 1.0;
		(*ColorObj)->TryGetNumberField(TEXT("a"), Alpha);
		Color.A = Alpha;
	}
	return Color;
}

//------------------------------------------------------------------------------
// CreateUMGWidgetBlueprint
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateUMGWidgetBlueprint::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!GetStringParam(Params, TEXT("widget_name"), WidgetName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_name"));
	}
	
	FString ParentClassStr = TEXT("UserWidget");
	GetStringParam(Params, TEXT("parent_class"), ParentClassStr, false);
	
	FString Path = TEXT("/Game/UI/");
	GetStringParam(Params, TEXT("path"), Path, false);
	
	if (!Path.EndsWith(TEXT("/")))
	{
		Path += TEXT("/");
	}
	
	// Get parent class
	UClass* ParentClass = UUserWidget::StaticClass();
	if (!ParentClassStr.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
	{
		UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *ParentClassStr));
		if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
		{
			ParentClass = FoundClass;
		}
	}
	
	// Create package path
	FString PackagePath = Path + WidgetName;
	
	// Check if asset already exists at this path
	if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *PackagePath))
	{
		// Asset already exists - return info about it instead of crashing
		if (UWidgetBlueprint* ExistingWidget = Cast<UWidgetBlueprint>(ExistingAsset))
		{
			TSharedPtr<FJsonObject> Result = MakeResult();
			Result->SetStringField(TEXT("widget_path"), PackagePath);
			Result->SetStringField(TEXT("widget_name"), WidgetName);
			Result->SetStringField(TEXT("parent_class"), ExistingWidget->ParentClass ? ExistingWidget->ParentClass->GetName() : TEXT("Unknown"));
			Result->SetBoolField(TEXT("already_exists"), true);
			Result->SetStringField(TEXT("message"), TEXT("Widget Blueprint already exists at this path"));
			return FECACommandResult::Success(Result);
		}
		else
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at path '%s' but is not a Widget Blueprint (it's a %s)"), *PackagePath, *ExistingAsset->GetClass()->GetName()));
		}
	}
	
	// Create package
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return FECACommandResult::Error(TEXT("Failed to create package"));
	}
	
	// Create Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = CastChecked<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			*WidgetName,
			BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass()
		)
	);
	
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(TEXT("Failed to create Widget Blueprint"));
	}
	
	// Add a default Canvas Panel as root
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
	}
	
	// Notify asset registry
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);
	Package->MarkPackageDirty();
	
	// Compile
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), PackagePath);
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddTextBlockToWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddTextBlockToWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	FString TextBlockName;
	if (!GetStringParam(Params, TEXT("text_block_name"), TextBlockName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: text_block_name"));
	}
	
	FString Text = TEXT("Text");
	GetStringParam(Params, TEXT("text"), Text, false);
	
	double FontSize = 12;
	GetFloatParam(Params, TEXT("font_size"), FontSize, false);
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no root widget"));
	}
	
	// Create Text Block
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*TextBlockName));
	if (!TextBlock)
	{
		return FECACommandResult::Error(TEXT("Failed to create Text Block"));
	}
	
	// Set text
	TextBlock->SetText(FText::FromString(Text));
	
	// Set color if specified
	FLinearColor Color = GetColorFromParams(Params, TEXT("color"), FLinearColor::White);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	
	// Add to canvas panel
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (Canvas)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(TextBlock);
		if (Slot)
		{
			FVector2D Position = GetPositionFromParams(Params);
			FVector2D Size = GetSizeFromParams(Params, FVector2D(200, 50));
			
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetAutoSize(true);
		}
	}
	else
	{
		WidgetTree->RootWidget = TextBlock;
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("text_block_name"), TextBlockName);
	Result->SetStringField(TEXT("text"), Text);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddButtonToWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddButtonToWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	FString ButtonName;
	if (!GetStringParam(Params, TEXT("button_name"), ButtonName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: button_name"));
	}
	
	FString ButtonText = TEXT("Button");
	GetStringParam(Params, TEXT("text"), ButtonText, false);
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no root widget"));
	}
	
	// Create Button
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*ButtonName));
	if (!Button)
	{
		return FECACommandResult::Error(TEXT("Failed to create Button"));
	}
	
	// Create Text Block for button content
	FString TextBlockName = ButtonName + TEXT("_Text");
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*TextBlockName));
	if (TextBlock)
	{
		TextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(TextBlock);
	}
	
	// Add to canvas panel
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (Canvas)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Button);
		if (Slot)
		{
			FVector2D Position = GetPositionFromParams(Params);
			FVector2D Size = GetSizeFromParams(Params, FVector2D(200, 50));
			
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("button_name"), ButtonName);
	Result->SetStringField(TEXT("text"), ButtonText);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddWidgetToViewport
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddWidgetToViewport::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	int32 ZOrder = 0;
	GetIntParam(Params, TEXT("z_order"), ZOrder, false);
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	if (!WidgetBlueprint->GeneratedClass)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no generated class. Try compiling it first."));
	}
	
	// This command only works during PIE
	UWorld* PlayWorld = GEditor->PlayWorld;
	if (!PlayWorld)
	{
		return FECACommandResult::Error(TEXT("This command requires an active Play In Editor session"));
	}
	
	// Get the player controller
	APlayerController* PC = PlayWorld->GetFirstPlayerController();
	if (!PC)
	{
		return FECACommandResult::Error(TEXT("No player controller found"));
	}
	
	// Create and add widget to viewport
	TSubclassOf<UUserWidget> WidgetClass = Cast<UClass>(WidgetBlueprint->GeneratedClass);
	if (!WidgetClass)
	{
		return FECACommandResult::Error(TEXT("Widget blueprint does not have a valid generated class"));
	}
	
	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!Widget)
	{
		return FECACommandResult::Error(TEXT("Failed to create widget instance"));
	}
	
	Widget->AddToViewport(ZOrder);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetNumberField(TEXT("z_order"), ZOrder);
	Result->SetBoolField(TEXT("added_to_viewport"), true);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// BindWidgetEvent
//------------------------------------------------------------------------------

FECACommandResult FECACommand_BindWidgetEvent::Execute(const TSharedPtr<FJsonObject>& Params)
{
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

	FString EventName;
	if (!GetStringParam(Params, TEXT("event_name"), EventName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: event_name"));
	}

	FString FunctionName;
	const bool bHasFunction = GetStringParam(Params, TEXT("function_name"), FunctionName, false) && !FunctionName.IsEmpty();

	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	UWidget* TargetWidget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!TargetWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	TargetWidget->bIsVariable = true;

	bool bAddedBinding = false;
	FString ResolvedDelegateName;
	if (bHasFunction)
	{
		// Resolve the delegate property name. Conventions:
		//   - User passes "OnClicked" — actual property is "OnClickedEvent"
		//   - User passes "OnHovered" — actual property is "OnHovered"
		//   - User passes "OnClickedEvent" — use as-is.
		// Try the literal name first, then with "Event" suffix.
		FProperty* DelegateProp = TargetWidget->GetClass()->FindPropertyByName(FName(*EventName));
		if (!DelegateProp)
		{
			DelegateProp = TargetWidget->GetClass()->FindPropertyByName(FName(*(EventName + TEXT("Event"))));
		}
		if (!DelegateProp)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Event '%s' not found on widget class '%s'. Try the actual delegate property name (e.g., 'OnClickedEvent' on Button) or omit function_name to fall back to mark-as-variable mode."),
				*EventName, *TargetWidget->GetClass()->GetName()));
		}

		// Verify the function exists on the BP. We look at SkeletonGeneratedClass since
		// the function might not be in the runtime class until next compile.
		UClass* BPClass = WidgetBlueprint->SkeletonGeneratedClass
			? WidgetBlueprint->SkeletonGeneratedClass.Get()
			: WidgetBlueprint->GeneratedClass.Get();
		if (BPClass && !BPClass->FindFunctionByName(FName(*FunctionName)))
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Function '%s' does not exist on Widget Blueprint '%s'. Create the function first (e.g., add_blueprint_function), then call this with function_name set."),
				*FunctionName, *WidgetBlueprint->GetName()));
		}

		// Build the binding. Avoid duplicating an existing entry for the same widget+event.
		FDelegateEditorBinding NewBinding;
		NewBinding.ObjectName = WidgetName;
		NewBinding.PropertyName = DelegateProp->GetFName();
		NewBinding.FunctionName = FName(*FunctionName);
		NewBinding.Kind = EBindingKind::Function;

		const int32 ExistingIdx = WidgetBlueprint->Bindings.IndexOfByKey(NewBinding);
		if (ExistingIdx == INDEX_NONE)
		{
			WidgetBlueprint->Bindings.Add(NewBinding);
			bAddedBinding = true;
		}
		else
		{
			// Update the FunctionName to match — operator== compares only ObjectName + PropertyName.
			WidgetBlueprint->Bindings[ExistingIdx].FunctionName = FName(*FunctionName);
			WidgetBlueprint->Bindings[ExistingIdx].Kind = EBindingKind::Function;
			bAddedBinding = true;
		}

		ResolvedDelegateName = DelegateProp->GetName();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("event_name"), EventName);
	Result->SetBoolField(TEXT("is_variable"), true);
	if (bHasFunction)
	{
		Result->SetStringField(TEXT("function_name"), FunctionName);
		Result->SetStringField(TEXT("delegate_property"), ResolvedDelegateName);
		Result->SetBoolField(TEXT("binding_added"), bAddedBinding);
		Result->SetStringField(TEXT("note"), TEXT("Binding stored in WidgetBlueprint->Bindings. compile_blueprint to bake it into the runtime class."));
	}
	else
	{
		Result->SetStringField(TEXT("note"), TEXT("Widget marked as variable. Pass function_name to also wire a delegate binding (function must already exist on the Widget Blueprint)."));
	}
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetTextBlockBinding
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetTextBlockBinding::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	FString TextBlockName;
	if (!GetStringParam(Params, TEXT("text_block_name"), TextBlockName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: text_block_name"));
	}
	
	FString BindingFunction;
	if (!GetStringParam(Params, TEXT("binding_function"), BindingFunction))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: binding_function"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}
	
	UWidget* Widget = WidgetTree->FindWidget(FName(*TextBlockName));
	UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
	if (!TextBlock)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Text Block not found: %s"), *TextBlockName));
	}
	
	// Mark as variable for binding
	TextBlock->bIsVariable = true;
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("text_block_name"), TextBlockName);
	Result->SetStringField(TEXT("binding_function"), BindingFunction);
	Result->SetBoolField(TEXT("is_variable"), true);
	Result->SetStringField(TEXT("note"), TEXT("Text Block marked as variable. Create binding function in Blueprint editor."));
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// ListWidgetClasses
//------------------------------------------------------------------------------

FECACommandResult FECACommand_ListWidgetClasses::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	bool bIncludeAbstract = false;
	GetBoolParam(Params, TEXT("include_abstract"), bIncludeAbstract, false);

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* C = *It;
		if (!C || !C->IsChildOf(UWidget::StaticClass())) continue;
		if (!bIncludeAbstract && C->HasAnyClassFlags(CLASS_Abstract)) continue;

		const FString ClassName = C->GetName();
		if (!NameFilter.IsEmpty() && !ClassName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), ClassName);
		Entry->SetStringField(TEXT("class_path"), C->GetPathName());
		Entry->SetBoolField(TEXT("is_panel"), C->IsChildOf(UPanelWidget::StaticClass()));
		Entry->SetBoolField(TEXT("is_content"), C->IsChildOf(UContentWidget::StaticClass()));
		ClassesArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Sort by name for stable output.
	ClassesArray.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) {
		return A->AsObject()->GetStringField(TEXT("name")) < B->AsObject()->GetStringField(TEXT("name"));
	});

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("classes"), ClassesArray);
	Result->SetNumberField(TEXT("count"), ClassesArray.Num());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddWidgetElement (generic)
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddWidgetElement::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetType;
	if (!GetStringParam(Params, TEXT("widget_type"), WidgetType))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_type"));
	}

	FString ElementName;
	if (!GetStringParam(Params, TEXT("element_name"), ElementName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: element_name"));
	}

	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Resolve widget class. Try common locations: /Script/UMG.X, fully qualified path, then LoadObject.
	UClass* WidgetClass = nullptr;
	if (!WidgetType.Contains(TEXT(".")) && !WidgetType.Contains(TEXT("/")))
	{
		WidgetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *WidgetType));
	}
	if (!WidgetClass)
	{
		WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
	}
	if (!WidgetClass)
	{
		WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
	}
	if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return FECACommandResult::Error(FString::Printf(
			TEXT("Widget class '%s' not found or is not a UWidget subclass. Try the unqualified UMG name (e.g., 'ProgressBar', 'VerticalBox')."),
			*WidgetType));
	}
	if (WidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget class '%s' is abstract and cannot be instantiated"), *WidgetClass->GetName()));
	}

	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*ElementName));
	if (!NewWidget)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to construct widget of class %s"), *WidgetClass->GetName()));
	}

	// Find the parent panel.
	UPanelWidget* ParentPanel = nullptr;
	FString ParentName;
	if (GetStringParam(Params, TEXT("parent_name"), ParentName, false) && !ParentName.IsEmpty())
	{
		UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentName));
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Parent '%s' not found or not a panel widget"), *ParentName));
		}
	}
	else
	{
		ParentPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	}

	bool bMadeRoot = false;
	UPanelSlot* AddedSlot = nullptr;
	if (!ParentPanel)
	{
		// Tree has no root yet (or root isn't a panel) — promote this widget to root if it's a panel.
		if (UPanelWidget* AsPanel = Cast<UPanelWidget>(NewWidget))
		{
			WidgetTree->RootWidget = AsPanel;
			bMadeRoot = true;
		}
		else
		{
			return FECACommandResult::Error(TEXT("Widget Blueprint has no panel root to attach to. Add a CanvasPanel/VerticalBox/etc. first, or pass a non-panel widget as parent_name."));
		}
	}
	else
	{
		AddedSlot = ParentPanel->AddChild(NewWidget);
		if (!AddedSlot)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to add widget to parent '%s' (the parent may have a fixed slot count or refuse this child)"), *ParentPanel->GetName()));
		}
	}

	// If parent is a CanvasPanel, apply optional position and size to the slot.
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		const TSharedPtr<FJsonObject>* PosObj = nullptr;
		if (Params->TryGetObjectField(TEXT("position"), PosObj) && PosObj && PosObj->IsValid())
		{
			FVector2D Position(0, 0);
			(*PosObj)->TryGetNumberField(TEXT("x"), Position.X);
			(*PosObj)->TryGetNumberField(TEXT("y"), Position.Y);
			CanvasSlot->SetPosition(Position);
		}

		const TSharedPtr<FJsonObject>* SizeObj = nullptr;
		if (Params->TryGetObjectField(TEXT("size"), SizeObj) && SizeObj && SizeObj->IsValid())
		{
			FVector2D Size(200, 50);
			(*SizeObj)->TryGetNumberField(TEXT("width"), Size.X);
			(*SizeObj)->TryGetNumberField(TEXT("height"), Size.Y);
			CanvasSlot->SetSize(Size);
		}
	}

	// Apply optional properties via reflection.
	int32 PropertiesSet = 0;
	TArray<FString> FailedProperties;
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj && PropsObj->IsValid())
	{
		for (const auto& KV : (*PropsObj)->Values)
		{
			FProperty* Prop = WidgetClass->FindPropertyByName(FName(*KV.Key));
			if (!Prop)
			{
				FailedProperties.Add(FString::Printf(TEXT("%s: not found"), *KV.Key));
				continue;
			}

			void* ValPtr = Prop->ContainerPtrToValuePtr<void>(NewWidget);
			FString ValStr;

			if (KV.Value->Type == EJson::String)
			{
				ValStr = KV.Value->AsString();
			}
			else if (KV.Value->Type == EJson::Number)
			{
				ValStr = FString::SanitizeFloat(KV.Value->AsNumber());
			}
			else if (KV.Value->Type == EJson::Boolean)
			{
				ValStr = KV.Value->AsBool() ? TEXT("True") : TEXT("False");
			}
			else
			{
				FailedProperties.Add(FString::Printf(TEXT("%s: unsupported JSON value type"), *KV.Key));
				continue;
			}

			if (Prop->ImportText_Direct(*ValStr, ValPtr, NewWidget, PPF_None))
			{
				PropertiesSet++;
			}
			else
			{
				FailedProperties.Add(FString::Printf(TEXT("%s: import failed for value '%s'"), *KV.Key, *ValStr));
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("element_name"), ElementName);
	Result->SetStringField(TEXT("widget_type"), WidgetClass->GetName());
	Result->SetStringField(TEXT("widget_class_path"), WidgetClass->GetPathName());
	Result->SetBoolField(TEXT("is_root"), bMadeRoot);
	if (ParentPanel)
	{
		Result->SetStringField(TEXT("parent"), ParentPanel->GetName());
	}
	Result->SetNumberField(TEXT("properties_set"), PropertiesSet);
	if (FailedProperties.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArr;
		for (const FString& F : FailedProperties)
		{
			FailedArr.Add(MakeShared<FJsonValueString>(F));
		}
		Result->SetArrayField(TEXT("properties_failed"), FailedArr);
	}
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// AddImageToWidget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_AddImageToWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	FString ImageName;
	if (!GetStringParam(Params, TEXT("image_name"), ImageName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: image_name"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no root widget"));
	}
	
	// Create Image widget
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*ImageName));
	if (!Image)
	{
		return FECACommandResult::Error(TEXT("Failed to create Image widget"));
	}
	
	// Set texture if specified
	FString TexturePath;
	if (GetStringParam(Params, TEXT("texture_path"), TexturePath, false))
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (Texture)
		{
			Image->SetBrushFromTexture(Texture);
		}
	}
	
	// Set color and opacity
	FLinearColor ColorAndOpacity = GetColorFromParams(Params, TEXT("color_and_opacity"), FLinearColor::White);
	Image->SetColorAndOpacity(ColorAndOpacity);
	
	// Add to canvas panel
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (Canvas)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
		if (Slot)
		{
			FVector2D Position = GetPositionFromParams(Params);
			FVector2D Size = GetSizeFromParams(Params, FVector2D(100, 100));
			
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("image_name"), ImageName);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetWidgetInfo
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetWidgetInfo::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("name"), WidgetBlueprint->GetName());
	Result->SetStringField(TEXT("path"), WidgetBlueprint->GetPathName());
	Result->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("None"));
	
	// Get widgets in tree
	TArray<TSharedPtr<FJsonValue>> WidgetsArray;
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&WidgetsArray](UWidget* Widget)
		{
			TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
			WidgetJson->SetStringField(TEXT("name"), Widget->GetName());
			WidgetJson->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
			WidgetJson->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
			WidgetsArray.Add(MakeShared<FJsonValueObject>(WidgetJson));
		});
	}
	Result->SetArrayField(TEXT("widgets"), WidgetsArray);
	Result->SetNumberField(TEXT("widget_count"), WidgetsArray.Num());

	return FECACommandResult::Success(Result);
}

// ============================================================================
// Rosetta Stone: dump_widget_tree
// ============================================================================

REGISTER_ECA_COMMAND(FECACommand_DumpWidgetTree)

static TSharedPtr<FJsonObject> DumpWidgetRecursive(UWidget* Widget, bool bIncludeSlotProps)
{
	if (!Widget) return nullptr;

	TSharedPtr<FJsonObject> WidgetObj = MakeShared<FJsonObject>();
	WidgetObj->SetStringField(TEXT("name"), Widget->GetName());
	WidgetObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	WidgetObj->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);

	// Visibility
	ESlateVisibility Vis = Widget->GetVisibility();
	switch (Vis)
	{
	case ESlateVisibility::Visible: WidgetObj->SetStringField(TEXT("visibility"), TEXT("Visible")); break;
	case ESlateVisibility::Collapsed: WidgetObj->SetStringField(TEXT("visibility"), TEXT("Collapsed")); break;
	case ESlateVisibility::Hidden: WidgetObj->SetStringField(TEXT("visibility"), TEXT("Hidden")); break;
	case ESlateVisibility::HitTestInvisible: WidgetObj->SetStringField(TEXT("visibility"), TEXT("HitTestInvisible")); break;
	case ESlateVisibility::SelfHitTestInvisible: WidgetObj->SetStringField(TEXT("visibility"), TEXT("SelfHitTestInvisible")); break;
	}

	// Slot properties
	if (bIncludeSlotProps && Widget->Slot)
	{
		TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetName());

		// Use reflection to read common slot properties
		UObject* SlotAsObj = Widget->Slot;
		for (TFieldIterator<FProperty> PropIt(SlotAsObj->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit)) continue;

			FString StringValue;
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(SlotAsObj);
			Property->ExportTextItem_Direct(StringValue, ValuePtr, nullptr, SlotAsObj, PPF_None);
			if (!StringValue.IsEmpty())
			{
				SlotObj->SetStringField(Property->GetName(), StringValue);
			}
		}
		WidgetObj->SetObjectField(TEXT("slot"), SlotObj);
	}

	// Recurse into children if panel widget
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 i = 0; i < Panel->GetChildrenCount(); i++)
		{
			UWidget* Child = Panel->GetChildAt(i);
			TSharedPtr<FJsonObject> ChildObj = DumpWidgetRecursive(Child, bIncludeSlotProps);
			if (ChildObj)
			{
				ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildObj));
			}
		}
		if (ChildrenArray.Num() > 0)
		{
			WidgetObj->SetArrayField(TEXT("children"), ChildrenArray);
		}
	}
	// ContentWidget (Border, SizeBox, etc.) — single child
	else if (UContentWidget* Content = Cast<UContentWidget>(Widget))
	{
		UWidget* Child = Content->GetContent();
		if (Child)
		{
			TSharedPtr<FJsonObject> ChildObj = DumpWidgetRecursive(Child, bIncludeSlotProps);
			if (ChildObj)
			{
				TArray<TSharedPtr<FJsonValue>> ChildrenArray;
				ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildObj));
				WidgetObj->SetArrayField(TEXT("children"), ChildrenArray);
			}
		}
	}

	return WidgetObj;
}

FECACommandResult FECACommand_DumpWidgetTree::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	bool bIncludeSlotProps = true;
	GetBoolParam(Params, TEXT("include_slot_properties"), bIncludeSlotProps, false);

	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
	if (!WidgetBP)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetStringField(TEXT("widget_name"), WidgetBP->GetName());

	// Root widget
	if (WidgetTree->RootWidget)
	{
		Result->SetObjectField(TEXT("tree"), DumpWidgetRecursive(WidgetTree->RootWidget, bIncludeSlotProps));
	}

	// Count total widgets
	int32 TotalWidgets = 0;
	WidgetTree->ForEachWidget([&TotalWidgets](UWidget*) { TotalWidgets++; });
	Result->SetNumberField(TEXT("total_widgets"), TotalWidgets);

	// Blueprint variables (for data bindings)
	UBlueprint* BP = Cast<UBlueprint>(WidgetBP);
	if (BP)
	{
		TArray<TSharedPtr<FJsonValue>> VarsArray;
		for (const FBPVariableDescription& Var : BP->NewVariables)
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
			VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
			if (Var.VarType.PinSubCategoryObject.IsValid())
			{
				VarObj->SetStringField(TEXT("sub_type"), Var.VarType.PinSubCategoryObject->GetName());
			}
			VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
		}
		Result->SetArrayField(TEXT("variables"), VarsArray);
	}

	return FECACommandResult::Success(Result);
}
