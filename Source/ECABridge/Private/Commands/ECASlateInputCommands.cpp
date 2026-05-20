// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECASlateInputCommands.h"

#include "Commands/ECACommand.h"

// Slate
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Layout/Children.h"
#include "Layout/Geometry.h"
#include "Layout/Visibility.h"

// Slate events
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericApplication.h"   // FModifierKeysState
#include "GenericPlatform/GenericWindow.h"        // FGenericWindow

// Misc
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

// Forward registration
REGISTER_ECA_COMMAND(FECACommand_FindSlateWidgets)
REGISTER_ECA_COMMAND(FECACommand_ClickSlateWidget)
REGISTER_ECA_COMMAND(FECACommand_TypeSlateText)
REGISTER_ECA_COMMAND(FECACommand_SlateKeyChord)
REGISTER_ECA_COMMAND(FECACommand_TakeSlateWidgetScreenshot)

//------------------------------------------------------------------------------
// Widget registry (transient session-scoped widget_id -> TWeakPtr<SWidget>)
//------------------------------------------------------------------------------
//
// Lives in this TU so it's shared across all five Slate-input commands but
// not exported elsewhere. widget_id is a monotonically increasing int32 the
// registry hands out on Register(); Resolve() returns nullptr if the widget
// was destroyed or the Slate tree rebuilt (TWeakPtr expired).
//
// This is intentionally NOT a persistent identity — see the note in
// find_slate_widgets' description: agents must call find_slate_widgets
// immediately before consuming the id.
class FECASlateWidgetRegistry
{
public:
	static FECASlateWidgetRegistry& Get()
	{
		static FECASlateWidgetRegistry Instance;
		return Instance;
	}

	int32 Register(TSharedPtr<SWidget> Widget)
	{
		if (!Widget.IsValid())
		{
			return 0;
		}
		FScopeLock Locker(&Lock);
		const int32 Id = NextId++;
		Bindings.Add(Id, TWeakPtr<SWidget>(Widget));
		return Id;
	}

	TSharedPtr<SWidget> Resolve(int32 Id)
	{
		FScopeLock Locker(&Lock);
		if (TWeakPtr<SWidget>* Weak = Bindings.Find(Id))
		{
			TSharedPtr<SWidget> Pinned = Weak->Pin();
			if (!Pinned.IsValid())
			{
				// Opportunistic cleanup of expired entry.
				Bindings.Remove(Id);
			}
			return Pinned;
		}
		return nullptr;
	}

	void Clear()
	{
		FScopeLock Locker(&Lock);
		Bindings.Empty();
	}

	// Drop any entries whose weak pointers have expired. Called opportunistically
	// during full tree walks to keep the map from growing unboundedly across
	// many find_slate_widgets calls in a session.
	void PruneExpired()
	{
		FScopeLock Locker(&Lock);
		TArray<int32> Stale;
		for (const TPair<int32, TWeakPtr<SWidget>>& Pair : Bindings)
		{
			if (!Pair.Value.IsValid())
			{
				Stale.Add(Pair.Key);
			}
		}
		for (int32 Id : Stale)
		{
			Bindings.Remove(Id);
		}
	}

private:
	int32 NextId = 1;
	TMap<int32, TWeakPtr<SWidget>> Bindings;
	FCriticalSection Lock;
};

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

namespace
{
	// Pick the SWindow we should operate against. If WindowTitle is non-empty,
	// search FSlateApplication::GetInteractiveTopLevelWindows() for a window
	// whose title contains the substring (case-insensitive). Falls back to the
	// active top-level window, then to the LevelEditor's parent window, in that
	// order. Returns nullptr if no window can be found.
	TSharedPtr<SWindow> ResolveTargetWindow(const FString& WindowTitle)
	{
		FSlateApplication& App = FSlateApplication::Get();

		if (!WindowTitle.IsEmpty())
		{
			TArray<TSharedRef<SWindow>> Windows = App.GetInteractiveTopLevelWindows();
			for (const TSharedRef<SWindow>& Window : Windows)
			{
				if (Window->GetTitle().ToString().Contains(WindowTitle, ESearchCase::IgnoreCase))
				{
					return Window;
				}
			}
			// No match — fall through to defaults.
		}

		if (TSharedPtr<SWindow> Active = App.GetActiveTopLevelWindow())
		{
			return Active;
		}

		// Last resort: the main editor frame via LevelEditor.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			TWeakPtr<SDockTab> LevelEditorTab = LevelEditorModule.GetLevelEditorInstanceTab();
			if (LevelEditorTab.IsValid())
			{
				if (TSharedPtr<SWindow> ParentWindow = LevelEditorTab.Pin()->GetParentWindow())
				{
					return ParentWindow;
				}
			}
		}

		return nullptr;
	}

	// Build FModifierKeysState from a JSON modifier-name array. Accepts any of
	// "ctrl", "control", "shift", "alt", "cmd", "command", "meta". Case-insensitive.
	FModifierKeysState BuildModifierKeys(const TArray<TSharedPtr<FJsonValue>>* Modifiers)
	{
		bool bShift = false, bCtrl = false, bAlt = false, bCmd = false;
		if (Modifiers)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Modifiers)
			{
				if (!Value.IsValid()) continue;
				FString Name = Value->AsString().ToLower();
				if (Name == TEXT("shift")) bShift = true;
				else if (Name == TEXT("ctrl") || Name == TEXT("control")) bCtrl = true;
				else if (Name == TEXT("alt")) bAlt = true;
				else if (Name == TEXT("cmd") || Name == TEXT("command") || Name == TEXT("meta")) bCmd = true;
			}
		}
		// Map "X is held" to "left-X is held" — Slate widgets only check
		// IsShiftDown()/IsControlDown()/etc which OR the two sides.
		return FModifierKeysState(
			/*bShift*/ bShift, /*bRightShift*/ false,
			/*bCtrl*/ bCtrl,  /*bRightCtrl*/ false,
			/*bAlt*/ bAlt,    /*bRightAlt*/ false,
			/*bCmd*/ bCmd,    /*bRightCmd*/ false,
			/*bCapsLocked*/ false);
	}

	// Resolve a button-name string to an EKeys mouse button. Returns
	// EKeys::LeftMouseButton on unknown input (silent default).
	FKey ResolveMouseButton(const FString& ButtonName)
	{
		const FString Lower = ButtonName.ToLower();
		if (Lower == TEXT("right"))  return EKeys::RightMouseButton;
		if (Lower == TEXT("middle")) return EKeys::MiddleMouseButton;
		return EKeys::LeftMouseButton;
	}

	// Resolve a key-name string to an FKey. Accepts UE canonical names and a
	// small set of common aliases. Returns FKey() (invalid) on unknown input.
	FKey ResolveKey(const FString& KeyName)
	{
		const FString Trimmed = KeyName.TrimStartAndEnd();
		const FString Lower = Trimmed.ToLower();

		// Aliases for common names that don't quite match UE's canonical FName.
		if (Lower == TEXT("return") || Lower == TEXT("enter")) return EKeys::Enter;
		if (Lower == TEXT("esc")     || Lower == TEXT("escape")) return EKeys::Escape;
		if (Lower == TEXT("space")   || Lower == TEXT("spacebar")) return EKeys::SpaceBar;
		if (Lower == TEXT("tab"))    return EKeys::Tab;
		if (Lower == TEXT("backspace") || Lower == TEXT("bksp")) return EKeys::BackSpace;
		if (Lower == TEXT("delete")  || Lower == TEXT("del")) return EKeys::Delete;
		if (Lower == TEXT("up"))     return EKeys::Up;
		if (Lower == TEXT("down"))   return EKeys::Down;
		if (Lower == TEXT("left"))   return EKeys::Left;
		if (Lower == TEXT("right"))  return EKeys::Right;
		if (Lower == TEXT("home"))   return EKeys::Home;
		if (Lower == TEXT("end"))    return EKeys::End;
		if (Lower == TEXT("pageup")) return EKeys::PageUp;
		if (Lower == TEXT("pagedown")) return EKeys::PageDown;

		// UE canonical key lookup — FName("S") -> EKeys::S, FName("F5") -> EKeys::F5, etc.
		// FKey doesn't validate at construction; IsValid() checks the registry.
		FKey Key(FName(*Trimmed));
		if (Key.IsValid())
		{
			return Key;
		}

		// Try a capitalized fallback ("s" -> "S").
		if (Trimmed.Len() == 1)
		{
			FString Upper = Trimmed.ToUpper();
			FKey Upcase(FName(*Upper));
			if (Upcase.IsValid())
			{
				return Upcase;
			}
		}

		return FKey();
	}

	// Recursive walk the Slate tree rooted at Widget. Visitor receives each
	// SWidget; returning false from Visitor stops the traversal entirely.
	// Stops descending further than 1024 deep (defensive cap).
	void WalkSlateTree(const TSharedPtr<SWidget>& Widget, TFunctionRef<bool(const TSharedPtr<SWidget>&)> Visitor, int32 Depth = 0)
	{
		if (!Widget.IsValid() || Depth > 1024)
		{
			return;
		}
		if (!Visitor(Widget))
		{
			return;
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children) return;
		const int32 NumChildren = Children->Num();
		for (int32 i = 0; i < NumChildren; ++i)
		{
			TSharedRef<SWidget> Child = Children->GetChildAt(i);
			WalkSlateTree(Child, Visitor, Depth + 1);
		}
	}

	// Return widget-relative window position from cached geometry (used by
	// click/screenshot routines).
	FVector2D GetWidgetAbsolutePosition(const TSharedPtr<SWidget>& Widget)
	{
		return FVector2D(Widget->GetCachedGeometry().GetAbsolutePosition());
	}

	FVector2D GetWidgetAbsoluteSize(const TSharedPtr<SWidget>& Widget)
	{
		return FVector2D(Widget->GetCachedGeometry().GetAbsoluteSize());
	}

	// Find the first widget anywhere under Root whose accessible text matches
	// the given string exactly (case-sensitive). Returns nullptr if no match.
	TSharedPtr<SWidget> FindWidgetByAccessibleText(const TSharedPtr<SWidget>& Root, const FString& Text)
	{
		TSharedPtr<SWidget> Found;
		WalkSlateTree(Root, [&](const TSharedPtr<SWidget>& W) -> bool {
			if (!W.IsValid()) return true;
			if (W->GetAccessibleText().ToString() == Text)
			{
				Found = W;
				return false; // stop
			}
			return true;
		});
		return Found;
	}

	// Resolve a widget from EITHER widget_id OR accessible_text. Used by
	// click/type/screenshot. If neither param is set, returns nullptr (caller
	// reports a validation error). Reads widget_id directly from the JSON
	// object since IECACommand::GetIntParam is protected.
	TSharedPtr<SWidget> ResolveTargetWidget(const TSharedPtr<FJsonObject>& Params, FString& OutHowResolved)
	{
		if (!Params.IsValid())
		{
			return nullptr;
		}

		double WidgetIdNum = 0.0;
		if (Params->HasField(TEXT("widget_id")) && Params->TryGetNumberField(TEXT("widget_id"), WidgetIdNum))
		{
			const int32 WidgetId = static_cast<int32>(WidgetIdNum);
			if (TSharedPtr<SWidget> W = FECASlateWidgetRegistry::Get().Resolve(WidgetId))
			{
				OutHowResolved = TEXT("widget_id");
				return W;
			}
		}

		FString AccText;
		if (Params->TryGetStringField(TEXT("accessible_text"), AccText) && !AccText.IsEmpty())
		{
			TSharedPtr<SWindow> Window = ResolveTargetWindow(FString());
			if (Window.IsValid())
			{
				if (TSharedPtr<SWidget> W = FindWidgetByAccessibleText(Window, AccText))
				{
					OutHowResolved = TEXT("accessible_text");
					return W;
				}
			}
		}
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// find_slate_widgets
//------------------------------------------------------------------------------

FECACommandResult FECACommand_FindSlateWidgets::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString TextContains;
	GetStringParam(Params, TEXT("text_contains"), TextContains, /*required*/false);

	FString WidgetType;
	GetStringParam(Params, TEXT("widget_type"), WidgetType, /*required*/false);

	FString WindowTitle;
	GetStringParam(Params, TEXT("window_title"), WindowTitle, /*required*/false);

	int32 MaxResults = 50;
	GetIntParam(Params, TEXT("max_results"), MaxResults, /*required*/false);
	if (MaxResults <= 0) MaxResults = 50;

	if (!FSlateApplication::IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Slate application is not initialized."));
	}

	TSharedPtr<SWindow> Window = ResolveTargetWindow(WindowTitle);
	if (!Window.IsValid())
	{
		return FECACommandResult::Error(TEXT("No editor window found to walk."));
	}

	// Opportunistic GC of stale registry entries.
	FECASlateWidgetRegistry::Get().PruneExpired();

	TArray<TSharedPtr<FJsonValue>> WidgetsArray;
	bool bTruncated = false;

	// We capture by ref so the lambda can stop early when the cap is hit.
	WalkSlateTree(Window, [&](const TSharedPtr<SWidget>& W) -> bool {
		if (!W.IsValid()) return true;

		const FString TypeStr = W->GetType().ToString();
		const FString AccText = W->GetAccessibleText().ToString();

		// Filter by widget_type (exact match).
		if (!WidgetType.IsEmpty() && TypeStr != WidgetType)
		{
			return true;
		}

		// Filter by text_contains (substring match against text OR type name).
		if (!TextContains.IsEmpty())
		{
			const bool bMatchesText = AccText.Contains(TextContains, ESearchCase::IgnoreCase);
			const bool bMatchesType = TypeStr.Contains(TextContains, ESearchCase::IgnoreCase);
			if (!bMatchesText && !bMatchesType)
			{
				return true;
			}
		}

		if (WidgetsArray.Num() >= MaxResults)
		{
			bTruncated = true;
			return false; // stop the walk entirely
		}

		const int32 Id = FECASlateWidgetRegistry::Get().Register(W);

		// Geometry in window-relative coordinates. AbsolutePosition is in
		// "Slate absolute" space (window origin = window screen origin), so
		// we subtract the window's screen position to convert.
		FVector2D AbsPos = FVector2D(W->GetCachedGeometry().GetAbsolutePosition());
		FVector2D AbsSize = FVector2D(W->GetCachedGeometry().GetAbsoluteSize());
		FVector2D WindowScreenPos = FVector2D(Window->GetPositionInScreen());

		const double X = AbsPos.X - WindowScreenPos.X;
		const double Y = AbsPos.Y - WindowScreenPos.Y;

		TSharedPtr<FJsonObject> Geom = MakeShared<FJsonObject>();
		Geom->SetNumberField(TEXT("x"), X);
		Geom->SetNumberField(TEXT("y"), Y);
		Geom->SetNumberField(TEXT("w"), AbsSize.X);
		Geom->SetNumberField(TEXT("h"), AbsSize.Y);

		TSharedPtr<FJsonObject> WJson = MakeShared<FJsonObject>();
		WJson->SetNumberField(TEXT("widget_id"), Id);
		WJson->SetStringField(TEXT("type"), TypeStr);
		WJson->SetStringField(TEXT("accessible_text"), AccText);
		WJson->SetObjectField(TEXT("geometry"), Geom);
		WJson->SetBoolField(TEXT("is_enabled"), W->IsEnabled());
		WJson->SetBoolField(TEXT("is_visible"), W->GetVisibility().IsVisible());

		WidgetsArray.Add(MakeShared<FJsonValueObject>(WJson));
		return true;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("window_title"), Window->GetTitle().ToString());
	Result->SetArrayField(TEXT("widgets"), WidgetsArray);
	Result->SetNumberField(TEXT("count"), WidgetsArray.Num());
	Result->SetBoolField(TEXT("truncated"), bTruncated);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// click_slate_widget
//------------------------------------------------------------------------------

FECACommandResult FECACommand_ClickSlateWidget::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!FSlateApplication::IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Slate application is not initialized."));
	}

	FString ButtonName = TEXT("left");
	GetStringParam(Params, TEXT("button"), ButtonName, /*required*/false);
	const FKey MouseKey = ResolveMouseButton(ButtonName);

	const TArray<TSharedPtr<FJsonValue>>* Modifiers = nullptr;
	GetArrayParam(Params, TEXT("modifiers"), Modifiers, /*required*/false);
	const FModifierKeysState ModKeys = BuildModifierKeys(Modifiers);

	bool bDoubleClick = false;
	GetBoolParam(Params, TEXT("double_click"), bDoubleClick, /*required*/false);

	// Determine click position in absolute (screen) space and which window
	// the click is targeting.
	FVector2D AbsoluteClickPos = FVector2D::ZeroVector;
	TSharedPtr<SWindow> TargetWindow;
	FString HowResolved;

	// Case 1: explicit window_pixel.
	const TSharedPtr<FJsonObject>* WindowPixelObj = nullptr;
	if (GetObjectParam(Params, TEXT("window_pixel"), WindowPixelObj, /*required*/false) && WindowPixelObj && WindowPixelObj->IsValid())
	{
		double Px = 0.0, Py = 0.0;
		(*WindowPixelObj)->TryGetNumberField(TEXT("x"), Px);
		(*WindowPixelObj)->TryGetNumberField(TEXT("y"), Py);
		TargetWindow = ResolveTargetWindow(FString());
		if (!TargetWindow.IsValid())
		{
			return FECACommandResult::Error(TEXT("No active editor window to anchor window_pixel against."));
		}
		FVector2D WindowScreenPos = FVector2D(TargetWindow->GetPositionInScreen());
		AbsoluteClickPos = WindowScreenPos + FVector2D(Px, Py);
		HowResolved = TEXT("window_pixel");
	}
	else
	{
		// Case 2/3: widget_id or accessible_text.
		TSharedPtr<SWidget> Widget = ResolveTargetWidget(Params, HowResolved);
		if (!Widget.IsValid())
		{
			return FECACommandResult::ValidationError(this, TEXT("Must provide one of: widget_id (valid), accessible_text (matching a current widget), or window_pixel {x, y}."));
		}
		FVector2D Pos = GetWidgetAbsolutePosition(Widget);
		FVector2D Size = GetWidgetAbsoluteSize(Widget);
		AbsoluteClickPos = Pos + (Size * 0.5);
		TargetWindow = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
		if (!TargetWindow.IsValid())
		{
			TargetWindow = ResolveTargetWindow(FString());
		}
	}

	FSlateApplication& App = FSlateApplication::Get();
	TSharedPtr<FGenericWindow> NativeWindow = TargetWindow.IsValid() ? TargetWindow->GetNativeWindow() : TSharedPtr<FGenericWindow>();

	// Build a single pointer event for this position. We reuse it for move,
	// down, and up — Slate copies what it needs out of the event, so this is
	// safe and matches the AnalogCursor synthetic-input pattern.
	auto MakePointerEvent = [&]() {
		return FPointerEvent(
			FSlateApplication::CursorUserIndex,
			FSlateApplication::CursorPointerIndex,
			AbsoluteClickPos,
			AbsoluteClickPos,
			App.GetPressedMouseButtons(),
			MouseKey,
			/*WheelDelta*/ 0.0f,
			ModKeys);
	};

	// Slate is single-threaded; all input processing must happen on the game
	// thread. ECA commands already run there, but assert defensively.
	check(IsInGameThread());

	const int32 NumClicks = bDoubleClick ? 2 : 1;
	bool bAnyDownHandled = false;
	bool bAnyUpHandled = false;

	for (int32 i = 0; i < NumClicks; ++i)
	{
		// Move the synthetic pointer to the target first so hover-only widgets
		// (tooltips, combobox arrows) latch correctly before the click.
		FPointerEvent MoveEvent = MakePointerEvent();
		App.ProcessMouseMoveEvent(MoveEvent, /*bIsSynthetic*/ true);

		FPointerEvent DownEvent = MakePointerEvent();
		bAnyDownHandled |= App.ProcessMouseButtonDownEvent(NativeWindow, DownEvent);

		FPointerEvent UpEvent = MakePointerEvent();
		bAnyUpHandled |= App.ProcessMouseButtonUpEvent(UpEvent);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("resolved_via"), HowResolved);
	Result->SetNumberField(TEXT("abs_x"), AbsoluteClickPos.X);
	Result->SetNumberField(TEXT("abs_y"), AbsoluteClickPos.Y);
	Result->SetStringField(TEXT("button"), ButtonName.ToLower());
	Result->SetBoolField(TEXT("double_click"), bDoubleClick);
	Result->SetBoolField(TEXT("down_handled"), bAnyDownHandled);
	Result->SetBoolField(TEXT("up_handled"), bAnyUpHandled);
	if (TargetWindow.IsValid())
	{
		Result->SetStringField(TEXT("window_title"), TargetWindow->GetTitle().ToString());
	}
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// type_slate_text
//------------------------------------------------------------------------------

FECACommandResult FECACommand_TypeSlateText::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!FSlateApplication::IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Slate application is not initialized."));
	}

	FString Text;
	if (!GetStringParam(Params, TEXT("text"), Text, /*required*/true))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: text"));
	}

	bool bIntoFocused = true;
	GetBoolParam(Params, TEXT("into_focused"), bIntoFocused, /*required*/false);

	// Optional widget focus before typing.
	FString HowResolved;
	TSharedPtr<SWidget> Target = ResolveTargetWidget(Params, HowResolved);
	if (Target.IsValid())
	{
		FSlateApplication::Get().SetUserFocus(0, Target, EFocusCause::SetDirectly);
	}
	else if (!bIntoFocused)
	{
		return FECACommandResult::ValidationError(this, TEXT("into_focused=false but neither widget_id nor accessible_text resolved a widget."));
	}

	FSlateApplication& App = FSlateApplication::Get();
	check(IsInGameThread());

	const FModifierKeysState NoMods;
	int32 CharsSent = 0;
	int32 KeysSent = 0;

	// Escape handling: \n -> Enter (via key down/up), \t -> Tab (via key down/up).
	// JSON arrives with literal backslash-n in the FString.
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR Ch = Text[i];

		// Backslash escape: examine next char.
		if (Ch == TEXT('\\') && i + 1 < Text.Len())
		{
			const TCHAR Next = Text[i + 1];
			if (Next == TEXT('n'))
			{
				FKeyEvent KE(EKeys::Enter, NoMods, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
				App.ProcessKeyDownEvent(KE);
				App.ProcessKeyUpEvent(KE);
				++KeysSent;
				++i;
				continue;
			}
			if (Next == TEXT('t'))
			{
				FKeyEvent KE(EKeys::Tab, NoMods, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
				App.ProcessKeyDownEvent(KE);
				App.ProcessKeyUpEvent(KE);
				++KeysSent;
				++i;
				continue;
			}
		}

		// Raw control chars: literal \n inside the FString (no backslash).
		if (Ch == TEXT('\n'))
		{
			FKeyEvent KE(EKeys::Enter, NoMods, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
			App.ProcessKeyDownEvent(KE);
			App.ProcessKeyUpEvent(KE);
			++KeysSent;
			continue;
		}
		if (Ch == TEXT('\t'))
		{
			FKeyEvent KE(EKeys::Tab, NoMods, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
			App.ProcessKeyDownEvent(KE);
			App.ProcessKeyUpEvent(KE);
			++KeysSent;
			continue;
		}

		FCharacterEvent CE(Ch, NoMods, /*UserIndex*/0, /*bIsRepeat*/false);
		App.ProcessKeyCharEvent(CE);
		++CharsSent;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("chars_sent"), CharsSent);
	Result->SetNumberField(TEXT("special_keys_sent"), KeysSent);
	Result->SetStringField(TEXT("resolved_via"), HowResolved);
	Result->SetBoolField(TEXT("focused_target"), Target.IsValid());
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// slate_key_chord
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SlateKeyChord::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!FSlateApplication::IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Slate application is not initialized."));
	}

	FString KeyName;
	if (!GetStringParam(Params, TEXT("key"), KeyName, /*required*/true))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: key"));
	}

	const FKey ResolvedKey = ResolveKey(KeyName);
	if (!ResolvedKey.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unknown key name '%s'. Use UE canonical names (S, F5, Tab, Enter, Escape, ...)."), *KeyName));
	}

	const TArray<TSharedPtr<FJsonValue>>* Modifiers = nullptr;
	GetArrayParam(Params, TEXT("modifiers"), Modifiers, /*required*/false);
	const FModifierKeysState ModKeys = BuildModifierKeys(Modifiers);

	FSlateApplication& App = FSlateApplication::Get();
	check(IsInGameThread());

	FKeyEvent KeyDown(ResolvedKey, ModKeys, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
	const bool bDownHandled = App.ProcessKeyDownEvent(KeyDown);

	FKeyEvent KeyUp(ResolvedKey, ModKeys, /*UserIndex*/0, /*bIsRepeat*/false, /*CharCode*/0, /*KeyCode*/0);
	const bool bUpHandled = App.ProcessKeyUpEvent(KeyUp);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("key"), ResolvedKey.ToString());
	Result->SetBoolField(TEXT("down_handled"), bDownHandled);
	Result->SetBoolField(TEXT("up_handled"), bUpHandled);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// take_slate_widget_screenshot
//------------------------------------------------------------------------------

FECACommandResult FECACommand_TakeSlateWidgetScreenshot::Execute(const TSharedPtr<FJsonObject>& Params)
{
	if (!FSlateApplication::IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Slate application is not initialized."));
	}

	FString HowResolved;
	TSharedPtr<SWidget> Widget = ResolveTargetWidget(Params, HowResolved);
	if (!Widget.IsValid())
	{
		return FECACommandResult::ValidationError(this, TEXT("Must provide widget_id (valid) or accessible_text (matching a current widget)."));
	}

	int32 Padding = 8;
	GetIntParam(Params, TEXT("padding"), Padding, /*required*/false);
	if (Padding < 0) Padding = 0;

	FString FilePath;
	const bool bSaveToFile = GetStringParam(Params, TEXT("file_path"), FilePath, /*required*/false) && !FilePath.IsEmpty();

	FSlateApplication& App = FSlateApplication::Get();

	// Strategy: pass the WINDOW to TakeScreenshot with a window-local
	// InnerWidgetArea covering the widget's bounds + padding. Per
	// TakeScreenshotCommon in SlateApplication.cpp, when the widget arg is
	// the window itself, the inner rect is interpreted as window-local pixels
	// directly. This lets us inflate the bounds without re-implementing the
	// shift math.
	TSharedPtr<SWindow> Window = App.FindWidgetWindow(Widget.ToSharedRef());
	if (!Window.IsValid())
	{
		return FECACommandResult::Error(TEXT("Widget has no parent window — cannot screenshot."));
	}

	FVector2D WidgetAbsPos = FVector2D(Widget->GetCachedGeometry().GetAbsolutePosition());
	FVector2D WidgetAbsSize = FVector2D(Widget->GetCachedGeometry().GetAbsoluteSize());
	FVector2D WindowScreenPos = FVector2D(Window->GetPositionInScreen());

	const int32 MinX = FMath::Max(0, FMath::FloorToInt(WidgetAbsPos.X - WindowScreenPos.X) - Padding);
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(WidgetAbsPos.Y - WindowScreenPos.Y) - Padding);
	const int32 MaxX = FMath::CeilToInt(WidgetAbsPos.X - WindowScreenPos.X + WidgetAbsSize.X) + Padding;
	const int32 MaxY = FMath::CeilToInt(WidgetAbsPos.Y - WindowScreenPos.Y + WidgetAbsSize.Y) + Padding;

	if (MaxX <= MinX || MaxY <= MinY)
	{
		return FECACommandResult::Error(TEXT("Widget has zero or negative bounds; nothing to capture."));
	}

	FIntRect InnerWidgetArea(MinX, MinY, MaxX, MaxY);

	TArray<FColor> Bitmap;
	FIntVector OutSize;
	// Pass the WINDOW so the inner rect is interpreted as window-local pixels.
	const bool bOk = App.TakeScreenshot(Window.ToSharedRef(), InnerWidgetArea, Bitmap, OutSize);

	if (!bOk || Bitmap.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("FSlateApplication::TakeScreenshot returned empty bitmap."));
	}

	const int32 Width = OutSize.X;
	const int32 Height = OutSize.Y;

	// Force alpha = 255; render-target alpha is often 0 which makes PNGs
	// appear transparent in most viewers. Same fix as TakeGameplayScreenshot.
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	TArray64<uint8> CompressedData;
	FImageUtils::PNGCompressImageArray(Width, Height,
		TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedData);

	if (CompressedData.Num() == 0)
	{
		return FECACommandResult::Error(TEXT("Failed to compress widget screenshot to PNG."));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetStringField(TEXT("resolved_via"), HowResolved);
	Result->SetNumberField(TEXT("padding"), Padding);

	if (bSaveToFile)
	{
		FString Directory = FPaths::GetPath(FilePath);
		IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Directory);

		if (!FFileHelper::SaveArrayToFile(
			TArrayView<const uint8>(CompressedData.GetData(), CompressedData.Num()), *FilePath))
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Failed to save widget screenshot to: %s"), *FilePath));
		}
		Result->SetStringField(TEXT("file_path"), FilePath);
		return FECACommandResult::Success(Result);
	}

	FECACommandResult Out = FECACommandResult::Success(Result);
	Out.McpContent.Add(FECACommandResult::MakeImageContent(CompressedData));
	return Out;
}
