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
	
	UWidgetBlueprint* WidgetBlueprint = LoadUMGWidgetBlueprintByPath(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}
	
	// Find the widget in the tree
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
	
	// For now, just mark the widget as a variable so it can be bound in Blueprints
	TargetWidget->bIsVariable = true;
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("event_name"), EventName);
	Result->SetBoolField(TEXT("is_variable"), true);
	Result->SetStringField(TEXT("note"), TEXT("Widget marked as variable. Use Blueprint editor to complete event binding."));
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
