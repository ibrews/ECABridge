// Copyright 2026 Agile Lens / ibrews. MIT.
//
// ECABridge auto-config writer for MCP-aware AI clients (Claude Code, Cursor,
// VSCode, Gemini CLI, Codex). Writes (or upserts into) the client-specific
// config files so an installed client picks up the local ECABridge server
// without manual setup.
//
// API-compatible with the equivalent behavior in Epic's ModelContextProtocol
// plugin (their `GenerateClientConfig` console command). Clean-room
// reimplementation: matches external behavior, written from scratch under MIT.
//
// Exposed via the `ECABridge.GenerateClientConfig <ClaudeCode|Cursor|VSCode|Gemini|Codex|All>`
// console command. Does NOT run at startup; user-invoked only.

#pragma once

#include "CoreMinimal.h"

enum class EECAMCPClient : uint8
{
	ClaudeCode,
	Cursor,
	VSCode,
	Gemini,
	Codex,
};

class ECABRIDGE_API FECAClientConfigWriter
{
public:
	/** Server name key used inside every config file. Differentiates us from
	 *  Epic's native MCP (which uses "unreal-mcp"). */
	static constexpr const TCHAR* ServerName = TEXT("ecabridge");

	/** Write the config for a single client. Returns true on success.
	 *  BaseDirectory: if empty, resolves to project dir on installed engines,
	 *  engine root on source builds. */
	static bool WriteClientConfig(EECAMCPClient Client, const FString& BaseDirectory = FString());

	/** Write configs for all clients. Returns the count of successful writes. */
	static int32 WriteAllClientConfigs(const FString& BaseDirectory = FString());

	/** Parse a case-insensitive client name with aliases.
	 *  Recognized: ClaudeCode (alias: Claude), Cursor, VSCode (alias: Copilot),
	 *  Gemini, Codex, All. Returns false on unknown input. */
	static bool ParseClientArg(const FString& Arg, EECAMCPClient& OutClient, bool& bOutAll);

	/** Resolve the default base directory per the platform rules. */
	static FString GetDefaultBaseDirectory();

	/** Build the server URL from the running settings. */
	static FString GetServerUrl();
};
