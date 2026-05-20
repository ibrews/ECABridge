// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Thin-wrapper integration with Epic's native ModelContextProtocol plugin.
 *
 * The "always +" positioning (see knowledge/intelligence/tools/native-mcp-always-plus-strategy.md)
 * calls for ECABridge and the native plugin to coexist: both servers register
 * with EDA, the agent picks whichever has the right capability for a prompt,
 * and the two surfaces do not conflict.
 *
 * This bridge implements the lightest-weight coexistence layer that is still
 * useful:
 *
 *   - Detects whether the native ModelContextProtocol plugin is loaded
 *     (FModuleManager::IsModuleLoaded). No header dependency on Epic's plugin;
 *     no link-time coupling; license-clean even though the native plugin is
 *     NoRedist.
 *
 *   - Logs the detection state at module startup so EDA users get an obvious
 *     "yes, both are running" signal in the Output Log.
 *
 *   - Registers an `ECABridge.NativeMCPStatus` console command that reports
 *     detection state + the ECABridge URL the user should drop into
 *     MCPToolsetSettings.json for EDA round-trip.
 *
 * Gated on WITH_ECA_NATIVE_MCP_INTEGRATION which Build.cs sets to 1 when the
 * engine ships the ModelContextProtocol plugin (5.8+). Compiled out entirely
 * on 5.7 where the native plugin does not exist.
 */
struct ECABRIDGE_API FECAClientToolsetBridge
{
	/** True if Epic's ModelContextProtocol plugin module is loaded in this process. */
	static bool IsNativeMCPModuleLoaded();

	/** Best-effort guess at the native MCP server's HTTP URL. The default in
	 *  the current native plugin is http://127.0.0.1:8000/mcp. Returned as a
	 *  hint string only — we do not probe the port. */
	static FString GuessNativeMCPUrl();

	/** ECABridge's own MCP URL (port from ECABridgeSettings). */
	static FString GetECABridgeMCPUrl();

	/** Register the `ECABridge.NativeMCPStatus` console command. Called from
	 *  ECABridgeModule::StartupModule when WITH_ECA_NATIVE_MCP_INTEGRATION=1. */
	static void RegisterConsoleCommands();

	/** Log detection state at startup. Called once from StartupModule. */
	static void LogStartupDetection();
};
