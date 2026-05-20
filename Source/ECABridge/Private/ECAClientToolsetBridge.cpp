// Copyright Epic Games, Inc. All Rights Reserved.

#include "ECAClientToolsetBridge.h"

#if WITH_ECA_NATIVE_MCP_INTEGRATION

#include "ECABridgeSettings.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
	const FName NativeMCPModuleName(TEXT("ModelContextProtocol"));
}

bool FECAClientToolsetBridge::IsNativeMCPModuleLoaded()
{
	return FModuleManager::Get().IsModuleLoaded(NativeMCPModuleName);
}

FString FECAClientToolsetBridge::GuessNativeMCPUrl()
{
	// Native plugin's default in current Experimental builds. Documented in
	// docs/EDA_INTEGRATION.md alongside the ECABridge URL. We do not probe.
	return TEXT("http://127.0.0.1:8000/mcp");
}

FString FECAClientToolsetBridge::GetECABridgeMCPUrl()
{
	int32 Port = 3000;
	if (const UECABridgeSettings* Settings = GetDefault<UECABridgeSettings>())
	{
		Port = Settings->ServerPort;
	}
	return FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), Port);
}

void FECAClientToolsetBridge::LogStartupDetection()
{
	const bool bNative = IsNativeMCPModuleLoaded();
	if (bNative)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[ECABridge] Native MCP plugin detected. Coexistence-ready. ECABridge URL: %s | Native URL (default): %s. ")
			TEXT("Register both in EDA's MCPToolsetSettings.json — see docs/EDA_INTEGRATION.md section 5."),
			*GetECABridgeMCPUrl(),
			*GuessNativeMCPUrl());
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("[ECABridge] Native MCP plugin NOT loaded. ECABridge runs solo at %s. ")
			TEXT("Enable Plugins -> Experimental -> ModelContextProtocol to get both surfaces in EDA."),
			*GetECABridgeMCPUrl());
	}
}

void FECAClientToolsetBridge::RegisterConsoleCommands()
{
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ECABridge.NativeMCPStatus"),
		TEXT("Report whether Epic's ModelContextProtocol plugin is loaded and show the EDA registration URLs for both servers."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const bool bNative = FECAClientToolsetBridge::IsNativeMCPModuleLoaded();
			UE_LOG(LogTemp, Display,
				TEXT("[ECABridge.NativeMCPStatus] native_loaded=%s | ecabridge_url=%s | native_url=%s"),
				bNative ? TEXT("true") : TEXT("false"),
				*FECAClientToolsetBridge::GetECABridgeMCPUrl(),
				*FECAClientToolsetBridge::GuessNativeMCPUrl());
			UE_LOG(LogTemp, Display,
				TEXT("[ECABridge.NativeMCPStatus] To register both servers in EDA, add entries for both URLs to ")
				TEXT("Saved/Config/<Platform>/MCPToolsetSettings.json. Reference snippet in docs/EDA_INTEGRATION.md section 5."));
		}),
		ECVF_Default);
}

#else // !WITH_ECA_NATIVE_MCP_INTEGRATION

bool FECAClientToolsetBridge::IsNativeMCPModuleLoaded()                  { return false; }
FString FECAClientToolsetBridge::GuessNativeMCPUrl()                     { return FString(); }
FString FECAClientToolsetBridge::GetECABridgeMCPUrl()                    { return FString(); }
void FECAClientToolsetBridge::LogStartupDetection()                      {}
void FECAClientToolsetBridge::RegisterConsoleCommands()                  {}

#endif
