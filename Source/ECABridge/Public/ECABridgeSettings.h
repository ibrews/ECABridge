// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ECABridgeSettings.generated.h"

/**
 * ECA Bridge Settings
 * 
 * Configure the MCP server that allows AI assistants (Claude Desktop, Cursor, etc.)
 * to control Unreal Editor.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="ECA Bridge"))
class ECABRIDGE_API UECABridgeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UECABridgeSettings();

	/** Port for the MCP HTTP server (for Claude Desktop, Cursor, etc.) */
	UPROPERTY(config, EditAnywhere, Category="Server", meta=(ClampMin=1024, ClampMax=65535))
	int32 ServerPort = 3000;
	
	/** Whether to start the server automatically when the editor launches */
	UPROPERTY(config, EditAnywhere, Category="Server")
	bool bAutoStart = true;
	
	/** Maximum time (in seconds) to wait for a command to complete */
	UPROPERTY(config, EditAnywhere, Category="Server", meta=(ClampMin=1, ClampMax=300))
	int32 CommandTimeoutSeconds = 30;
	
	/** Enable verbose logging for debugging */
	UPROPERTY(config, EditAnywhere, Category="Debugging")
	bool bVerboseLogging = false;
	
	/** Log all incoming commands (may contain sensitive data) */
	UPROPERTY(config, EditAnywhere, Category="Debugging")
	bool bLogCommands = false;
	
	/** Allow connections from external IPs (security risk - use with caution) */
	UPROPERTY(config, EditAnywhere, Category="Security", AdvancedDisplay)
	bool bAllowRemoteConnections = false;
	
	/** Get the singleton settings object */
	static const UECABridgeSettings* Get()
	{
		return GetDefault<UECABridgeSettings>();
	}
	
	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FText GetSectionText() const override { return NSLOCTEXT("ECABridge", "SettingsSection", "ECA Bridge"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("ECABridge", "SettingsDescription", "Configure Epic Code Assistant MCP server for AI-assisted development"); }
};
