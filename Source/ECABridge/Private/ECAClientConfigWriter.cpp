// Copyright 2026 Agile Lens / ibrews. MIT.
//
// Clean-room reimplementation of an MCP-client config writer. API-compatible
// with Epic's ModelContextProtocol.GenerateClientConfig command; written from
// scratch under MIT.

#include "ECAClientConfigWriter.h"
#include "ECABridgeSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogECAClientConfig, Log, All);

// ===== Per-client descriptor =====
//
// Each client uses a slightly different file path + root key + entry shape.
// Native MCP's behavior (per agent-derived spec, 2026-05-20):
//   - ClaudeCode: .mcp.json, root "mcpServers", entry has {type:"http", url}
//   - Cursor:     .cursor/mcp.json, root "mcpServers", entry has {url} only
//   - VSCode:     .vscode/mcp.json, root "servers" (NOT mcpServers), entry has {type,url}
//   - Gemini:     .gemini/settings.json, root "mcpServers", entry has {httpUrl}
//   - Codex:      .codex/config.toml, write-once, no merge
//
// We mirror the file paths and root keys (so existing entries from native or
// other tools are preserved on upsert) but use ServerName = "ecabridge" so our
// entry does not collide with native's "unreal-mcp" key.

namespace
{
	struct FClientDescriptor
	{
		const TCHAR* RelativePath;
		const TCHAR* RootKey;       // "mcpServers" or "servers"
		bool         bIncludeType;  // include "type":"http"
		bool         bUseHttpUrl;   // use "httpUrl" instead of "url"
		bool         bIsToml;       // codex
	};

	const FClientDescriptor& GetDescriptor(EECAMCPClient Client)
	{
		static const FClientDescriptor Descriptors[] = {
			/* ClaudeCode */ { TEXT(".mcp.json"),              TEXT("mcpServers"), true,  false, false },
			/* Cursor     */ { TEXT(".cursor/mcp.json"),       TEXT("mcpServers"), false, false, false },
			/* VSCode     */ { TEXT(".vscode/mcp.json"),       TEXT("servers"),    true,  false, false },
			/* Gemini     */ { TEXT(".gemini/settings.json"),  TEXT("mcpServers"), false, true,  false },
			/* Codex      */ { TEXT(".codex/config.toml"),     TEXT(""),           false, false, true  },
		};
		return Descriptors[(uint8)Client];
	}

	const TCHAR* ClientName(EECAMCPClient Client)
	{
		switch (Client)
		{
		case EECAMCPClient::ClaudeCode: return TEXT("ClaudeCode");
		case EECAMCPClient::Cursor:     return TEXT("Cursor");
		case EECAMCPClient::VSCode:     return TEXT("VSCode");
		case EECAMCPClient::Gemini:     return TEXT("Gemini");
		case EECAMCPClient::Codex:      return TEXT("Codex");
		}
		return TEXT("Unknown");
	}

	// Build the JSON object that goes under ServerName for one client.
	TSharedPtr<FJsonObject> BuildEntry(const FClientDescriptor& Desc, const FString& Url)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		if (Desc.bIncludeType)
		{
			Entry->SetStringField(TEXT("type"), TEXT("http"));
		}
		Entry->SetStringField(Desc.bUseHttpUrl ? TEXT("httpUrl") : TEXT("url"), Url);
		return Entry;
	}

	// Upsert ServerName into the existing JSON file, preserving sibling entries
	// and unrelated top-level keys. Overwrites on malformed-JSON with a warning.
	bool WriteJsonClient(EECAMCPClient Client, const FString& AbsPath, const FString& Url)
	{
		const FClientDescriptor& Desc = GetDescriptor(Client);

		TSharedPtr<FJsonObject> Root;
		FString Existing;
		bool bExistingValid = false;
		if (FFileHelper::LoadFileToString(Existing, *AbsPath))
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Existing);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				bExistingValid = true;
			}
			else
			{
				UE_LOG(LogECAClientConfig, Warning,
					TEXT("Existing file at %s is malformed JSON; backing up to .bak and overwriting"),
					*AbsPath);
				// Preserve the broken file so the user doesn't lose hand-edits.
				const FString BackupPath = AbsPath + TEXT(".bak");
				IFileManager::Get().Copy(*BackupPath, *AbsPath, true, true);
			}
		}
		if (!bExistingValid || !Root.IsValid())
		{
			Root = MakeShared<FJsonObject>();
		}

		// Find or create the servers map at the descriptor's root key.
		TSharedPtr<FJsonObject> ServersMap;
		const TSharedPtr<FJsonValue> RootVal = Root->TryGetField(Desc.RootKey);
		if (RootVal.IsValid() && RootVal->Type == EJson::Object)
		{
			ServersMap = RootVal->AsObject();
		}
		else
		{
			ServersMap = MakeShared<FJsonObject>();
		}

		// Upsert our entry.
		ServersMap->SetObjectField(FECAClientConfigWriter::ServerName, BuildEntry(Desc, Url));
		Root->SetObjectField(Desc.RootKey, ServersMap);

		// Ensure parent dir exists.
		const FString ParentDir = FPaths::GetPath(AbsPath);
		IFileManager::Get().MakeDirectory(*ParentDir, /*Tree=*/true);

		// Serialize pretty.
		FString Output;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
		{
			UE_LOG(LogECAClientConfig, Error, TEXT("Failed to serialize JSON for %s"), *AbsPath);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(Output, *AbsPath))
		{
			UE_LOG(LogECAClientConfig, Error, TEXT("Failed to write %s"), *AbsPath);
			return false;
		}

		UE_LOG(LogECAClientConfig, Log, TEXT("Wrote %s (%s)"), *AbsPath, ClientName(Client));
		return true;
	}

	// Codex: write-once TOML. Refuse to overwrite an existing file.
	bool WriteCodexClient(const FString& AbsPath, const FString& Url)
	{
		if (FPaths::FileExists(AbsPath))
		{
			UE_LOG(LogECAClientConfig, Warning,
				TEXT("%s already exists; refusing to merge TOML. Edit manually to add the ecabridge entry."),
				*AbsPath);
			return false;
		}

		const FString ParentDir = FPaths::GetPath(AbsPath);
		IFileManager::Get().MakeDirectory(*ParentDir, /*Tree=*/true);

		const FString Content = FString::Printf(
			TEXT("[mcp_servers.%s]\nurl = \"%s\"\n"),
			FECAClientConfigWriter::ServerName, *Url);

		if (!FFileHelper::SaveStringToFile(Content, *AbsPath))
		{
			UE_LOG(LogECAClientConfig, Error, TEXT("Failed to write %s"), *AbsPath);
			return false;
		}

		UE_LOG(LogECAClientConfig, Log, TEXT("Wrote %s (Codex)"), *AbsPath);
		return true;
	}
}

FString FECAClientConfigWriter::GetServerUrl()
{
	const UECABridgeSettings* Settings = UECABridgeSettings::Get();
	const int32 Port = Settings ? Settings->ServerPort : 3000;
	return FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), Port);
}

FString FECAClientConfigWriter::GetDefaultBaseDirectory()
{
	// Installed/launcher builds: project dir.
	// Source builds: engine root dir (workspace root containing Engine/ + .uproject).
	if (FApp::IsEngineInstalled())
	{
		return FPaths::ProjectDir();
	}
	return FPaths::RootDir();
}

bool FECAClientConfigWriter::WriteClientConfig(EECAMCPClient Client, const FString& BaseDirectory)
{
	const FString Base = BaseDirectory.IsEmpty() ? GetDefaultBaseDirectory() : BaseDirectory;
	const FClientDescriptor& Desc = GetDescriptor(Client);
	const FString AbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Base, Desc.RelativePath));
	const FString Url = GetServerUrl();

	if (Desc.bIsToml)
	{
		return WriteCodexClient(AbsPath, Url);
	}
	return WriteJsonClient(Client, AbsPath, Url);
}

int32 FECAClientConfigWriter::WriteAllClientConfigs(const FString& BaseDirectory)
{
	int32 Succeeded = 0;
	const EECAMCPClient All[] = {
		EECAMCPClient::ClaudeCode,
		EECAMCPClient::Cursor,
		EECAMCPClient::VSCode,
		EECAMCPClient::Gemini,
		EECAMCPClient::Codex,
	};
	for (EECAMCPClient C : All)
	{
		if (WriteClientConfig(C, BaseDirectory)) { ++Succeeded; }
	}
	return Succeeded;
}

bool FECAClientConfigWriter::ParseClientArg(const FString& Arg, EECAMCPClient& OutClient, bool& bOutAll)
{
	bOutAll = false;
	const FString A = Arg.ToLower();
	if (A == TEXT("all"))                       { bOutAll = true;                       return true; }
	if (A == TEXT("claudecode") || A == TEXT("claude"))  { OutClient = EECAMCPClient::ClaudeCode; return true; }
	if (A == TEXT("cursor"))                    { OutClient = EECAMCPClient::Cursor;     return true; }
	if (A == TEXT("vscode")    || A == TEXT("copilot"))  { OutClient = EECAMCPClient::VSCode;     return true; }
	if (A == TEXT("gemini"))                    { OutClient = EECAMCPClient::Gemini;     return true; }
	if (A == TEXT("codex"))                     { OutClient = EECAMCPClient::Codex;      return true; }
	return false;
}
