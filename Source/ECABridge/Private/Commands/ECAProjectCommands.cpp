// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAProjectCommands.h"
#include "Commands/ECACommand.h"
#include "GameFramework/InputSettings.h"
#include "GameMapsSettings.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

// Register all project commands
REGISTER_ECA_COMMAND(FECACommand_CreateInputMapping)
REGISTER_ECA_COMMAND(FECACommand_GetInputMappings)
REGISTER_ECA_COMMAND(FECACommand_RemoveInputMapping)
REGISTER_ECA_COMMAND(FECACommand_GetProjectSettings)
REGISTER_ECA_COMMAND(FECACommand_SetProjectSetting)

//------------------------------------------------------------------------------
// Helper: Parse key name to FKey
//------------------------------------------------------------------------------

static FKey ParseKeyName(const FString& KeyName)
{
	// Try direct parsing first
	FKey Key = FKey(*KeyName);
	if (Key.IsValid())
	{
		return Key;
	}
	
	// Try common aliases
	static TMap<FString, FKey> KeyAliases = {
		// Mouse
		{ TEXT("LeftMouse"), EKeys::LeftMouseButton },
		{ TEXT("RightMouse"), EKeys::RightMouseButton },
		{ TEXT("MiddleMouse"), EKeys::MiddleMouseButton },
		{ TEXT("MouseX"), EKeys::MouseX },
		{ TEXT("MouseY"), EKeys::MouseY },
		{ TEXT("MouseWheelUp"), EKeys::MouseScrollUp },
		{ TEXT("MouseWheelDown"), EKeys::MouseScrollDown },
		
		// Keyboard shortcuts
		{ TEXT("Space"), EKeys::SpaceBar },
		{ TEXT("Enter"), EKeys::Enter },
		{ TEXT("Escape"), EKeys::Escape },
		{ TEXT("Tab"), EKeys::Tab },
		{ TEXT("Backspace"), EKeys::BackSpace },
		
		// Gamepad
		{ TEXT("GamepadA"), EKeys::Gamepad_FaceButton_Bottom },
		{ TEXT("GamepadB"), EKeys::Gamepad_FaceButton_Right },
		{ TEXT("GamepadX"), EKeys::Gamepad_FaceButton_Left },
		{ TEXT("GamepadY"), EKeys::Gamepad_FaceButton_Top },
		{ TEXT("GamepadLeftStickX"), EKeys::Gamepad_LeftX },
		{ TEXT("GamepadLeftStickY"), EKeys::Gamepad_LeftY },
		{ TEXT("GamepadRightStickX"), EKeys::Gamepad_RightX },
		{ TEXT("GamepadRightStickY"), EKeys::Gamepad_RightY },
	};
	
	const FKey* Found = KeyAliases.Find(KeyName);
	if (Found)
	{
		return *Found;
	}
	
	// Return the parsed key even if not fully valid - UE will handle it
	return FKey(*KeyName);
}

//------------------------------------------------------------------------------
// CreateInputMapping
//------------------------------------------------------------------------------

FECACommandResult FECACommand_CreateInputMapping::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActionName;
	if (!GetStringParam(Params, TEXT("action_name"), ActionName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: action_name"));
	}
	
	FString KeyName;
	if (!GetStringParam(Params, TEXT("key"), KeyName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: key"));
	}
	
	FString InputType = TEXT("Action");
	GetStringParam(Params, TEXT("input_type"), InputType, false);
	
	double Scale = 1.0;
	GetFloatParam(Params, TEXT("scale"), Scale, false);
	
	bool bShift = false, bCtrl = false, bAlt = false, bCmd = false;
	GetBoolParam(Params, TEXT("shift"), bShift, false);
	GetBoolParam(Params, TEXT("ctrl"), bCtrl, false);
	GetBoolParam(Params, TEXT("alt"), bAlt, false);
	GetBoolParam(Params, TEXT("cmd"), bCmd, false);
	
	UInputSettings* InputSettings = UInputSettings::GetInputSettings();
	if (!InputSettings)
	{
		return FECACommandResult::Error(TEXT("Failed to get Input Settings"));
	}
	
	FKey Key = ParseKeyName(KeyName);
	
	if (InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase))
	{
		// Create action mapping
		FInputActionKeyMapping ActionMapping;
		ActionMapping.ActionName = FName(*ActionName);
		ActionMapping.Key = Key;
		ActionMapping.bShift = bShift;
		ActionMapping.bCtrl = bCtrl;
		ActionMapping.bAlt = bAlt;
		ActionMapping.bCmd = bCmd;
		
		InputSettings->AddActionMapping(ActionMapping);
	}
	else if (InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase))
	{
		// Create axis mapping
		FInputAxisKeyMapping AxisMapping;
		AxisMapping.AxisName = FName(*ActionName);
		AxisMapping.Key = Key;
		AxisMapping.Scale = Scale;
		
		InputSettings->AddAxisMapping(AxisMapping);
	}
	else
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Invalid input_type: %s (must be Action or Axis)"), *InputType));
	}
	
	// Save settings
	InputSettings->SaveKeyMappings();
	InputSettings->TryUpdateDefaultConfigFile();
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("action_name"), ActionName);
	Result->SetStringField(TEXT("key"), Key.ToString());
	Result->SetStringField(TEXT("input_type"), InputType);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetInputMappings
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetInputMappings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString InputType = TEXT("All");
	GetStringParam(Params, TEXT("input_type"), InputType, false);
	
	UInputSettings* InputSettings = UInputSettings::GetInputSettings();
	if (!InputSettings)
	{
		return FECACommandResult::Error(TEXT("Failed to get Input Settings"));
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	// Action mappings
	if (InputType.Equals(TEXT("All"), ESearchCase::IgnoreCase) || InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase))
	{
		TArray<TSharedPtr<FJsonValue>> ActionMappingsArray;
		
		const TArray<FInputActionKeyMapping>& ActionMappings = InputSettings->GetActionMappings();
		for (const FInputActionKeyMapping& Mapping : ActionMappings)
		{
			TSharedPtr<FJsonObject> MappingJson = MakeShared<FJsonObject>();
			MappingJson->SetStringField(TEXT("action_name"), Mapping.ActionName.ToString());
			MappingJson->SetStringField(TEXT("key"), Mapping.Key.ToString());
			MappingJson->SetBoolField(TEXT("shift"), Mapping.bShift);
			MappingJson->SetBoolField(TEXT("ctrl"), Mapping.bCtrl);
			MappingJson->SetBoolField(TEXT("alt"), Mapping.bAlt);
			MappingJson->SetBoolField(TEXT("cmd"), Mapping.bCmd);
			ActionMappingsArray.Add(MakeShared<FJsonValueObject>(MappingJson));
		}
		
		Result->SetArrayField(TEXT("action_mappings"), ActionMappingsArray);
	}
	
	// Axis mappings
	if (InputType.Equals(TEXT("All"), ESearchCase::IgnoreCase) || InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase))
	{
		TArray<TSharedPtr<FJsonValue>> AxisMappingsArray;
		
		const TArray<FInputAxisKeyMapping>& AxisMappings = InputSettings->GetAxisMappings();
		for (const FInputAxisKeyMapping& Mapping : AxisMappings)
		{
			TSharedPtr<FJsonObject> MappingJson = MakeShared<FJsonObject>();
			MappingJson->SetStringField(TEXT("axis_name"), Mapping.AxisName.ToString());
			MappingJson->SetStringField(TEXT("key"), Mapping.Key.ToString());
			MappingJson->SetNumberField(TEXT("scale"), Mapping.Scale);
			AxisMappingsArray.Add(MakeShared<FJsonValueObject>(MappingJson));
		}
		
		Result->SetArrayField(TEXT("axis_mappings"), AxisMappingsArray);
	}
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// RemoveInputMapping
//------------------------------------------------------------------------------

FECACommandResult FECACommand_RemoveInputMapping::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActionName;
	if (!GetStringParam(Params, TEXT("action_name"), ActionName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: action_name"));
	}
	
	FString InputType = TEXT("Action");
	GetStringParam(Params, TEXT("input_type"), InputType, false);
	
	UInputSettings* InputSettings = UInputSettings::GetInputSettings();
	if (!InputSettings)
	{
		return FECACommandResult::Error(TEXT("Failed to get Input Settings"));
	}
	
	int32 RemovedCount = 0;
	
	if (InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase))
	{
		// Get current mappings
		TArray<FInputActionKeyMapping> Mappings;
		InputSettings->GetActionMappingByName(FName(*ActionName), Mappings);
		
		for (const FInputActionKeyMapping& Mapping : Mappings)
		{
			InputSettings->RemoveActionMapping(Mapping);
			RemovedCount++;
		}
	}
	else if (InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase))
	{
		TArray<FInputAxisKeyMapping> Mappings;
		InputSettings->GetAxisMappingByName(FName(*ActionName), Mappings);
		
		for (const FInputAxisKeyMapping& Mapping : Mappings)
		{
			InputSettings->RemoveAxisMapping(Mapping);
			RemovedCount++;
		}
	}
	
	if (RemovedCount > 0)
	{
		InputSettings->SaveKeyMappings();
		InputSettings->TryUpdateDefaultConfigFile();
	}
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("action_name"), ActionName);
	Result->SetNumberField(TEXT("removed_count"), RemovedCount);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// GetProjectSettings
//------------------------------------------------------------------------------

FECACommandResult FECACommand_GetProjectSettings::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeResult();
	
	// Game Maps settings
	UGameMapsSettings* GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
	if (GameMapsSettings)
	{
		Result->SetStringField(TEXT("game_default_map"), GameMapsSettings->GetGameDefaultMap());
		Result->SetStringField(TEXT("editor_startup_map"), GameMapsSettings->EditorStartupMap.ToString());
		Result->SetStringField(TEXT("game_instance_class"), GameMapsSettings->GameInstanceClass.ToString());
		Result->SetStringField(TEXT("global_default_game_mode"), GameMapsSettings->GetGlobalDefaultGameMode());
	}
	
	// General project info
	Result->SetStringField(TEXT("project_name"), FApp::GetProjectName());
	Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// SetProjectSetting
//------------------------------------------------------------------------------

FECACommandResult FECACommand_SetProjectSetting::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString SettingName;
	if (!GetStringParam(Params, TEXT("setting_name"), SettingName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: setting_name"));
	}
	
	FString SettingValue;
	if (!GetStringParam(Params, TEXT("setting_value"), SettingValue))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: setting_value"));
	}
	
	UGameMapsSettings* GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
	if (!GameMapsSettings)
	{
		return FECACommandResult::Error(TEXT("Failed to get Game Maps Settings"));
	}
	
	bool bSettingFound = false;
	
	if (SettingName.Equals(TEXT("GameDefaultMap"), ESearchCase::IgnoreCase))
	{
		GameMapsSettings->SetGameDefaultMap(SettingValue);
		bSettingFound = true;
	}
	else if (SettingName.Equals(TEXT("EditorStartupMap"), ESearchCase::IgnoreCase))
	{
		GameMapsSettings->EditorStartupMap = FSoftObjectPath(SettingValue);
		bSettingFound = true;
	}
	else if (SettingName.Equals(TEXT("GlobalDefaultGameMode"), ESearchCase::IgnoreCase))
	{
		GameMapsSettings->SetGlobalDefaultGameMode(SettingValue);
		bSettingFound = true;
	}
	else if (SettingName.Equals(TEXT("GameInstanceClass"), ESearchCase::IgnoreCase))
	{
		GameMapsSettings->GameInstanceClass = FSoftClassPath(SettingValue);
		bSettingFound = true;
	}
	
	if (!bSettingFound)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unknown setting: %s"), *SettingName));
	}
	
	// Save config
	GameMapsSettings->TryUpdateDefaultConfigFile();
	
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("setting_name"), SettingName);
	Result->SetStringField(TEXT("setting_value"), SettingValue);
	return FECACommandResult::Success(Result);
}
