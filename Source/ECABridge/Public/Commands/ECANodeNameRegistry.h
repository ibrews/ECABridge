// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"

/**
 * Session-scoped friendly-name -> Blueprint node GUID registry.
 *
 * Blueprint authoring commands return node identities as 36-char GUIDs
 * (e.g. A950175C0A412A29B1F97A9B9859215E) which cost a lot of tokens to
 * pass back through MCP. This registry lets an agent bind a short friendly
 * name (e.g. "ApplyDamage_Player") to the (Blueprint, Graph, NodeGuid)
 * triple at create-time and then reference the name in subsequent calls
 * via the `node_name` parameter alias accepted by every BP-node command.
 *
 * Lifetime: process-singleton, lives until the editor (or commandlet)
 * shuts down. Bindings do NOT persist across editor restarts — they're
 * intentionally session-scoped, matching the AgentIntegrationKit prior art.
 *
 * Lookup is case-sensitive. Agents already deal with case-sensitive command
 * names (add_blueprint_function_node, not Add_Blueprint_Function_Node) so
 * matching that contract here avoids the "did you mean MyNode vs mynode?"
 * surprise. If a binding is registered twice with the same Name the latter
 * overwrites the former.
 *
 * Thread safety: a single FCriticalSection guards Bindings. Register/
 * Resolve/Unregister/Clear/Snapshot may be called from any thread but the
 * commands in practice run on the game thread because they touch UObjects.
 */
class ECABRIDGE_API FECANodeNameRegistry
{
public:
	/** Process-singleton accessor. */
	static FECANodeNameRegistry& Get();

	/**
	 * Register a Name -> (BlueprintPath, GraphName, NodeGuid) binding.
	 * Replaces any existing entry for the same Name (no error, no warning).
	 * Returns false when Name is empty (we never store empty keys). NodeGuid
	 * may be the zero GUID but Resolve() will then refuse to return it.
	 */
	bool Register(
		const FString& Name,
		const FString& BlueprintPath,
		const FString& GraphName,
		const FGuid& NodeGuid);

	/**
	 * Look up a binding. Returns false when Name is not bound OR when the
	 * stored NodeGuid is invalid (zero). Output params are only written when
	 * the method returns true.
	 */
	bool Resolve(
		const FString& Name,
		FString& OutBlueprintPath,
		FString& OutGraphName,
		FGuid& OutNodeGuid) const;

	/**
	 * Drop a binding. Returns true if it existed (and was removed), false
	 * if Name was unknown or empty.
	 */
	bool Unregister(const FString& Name);

	/**
	 * Wipe all bindings. Used by the clear_node_names MCP tool and on
	 * editor shutdown.
	 */
	void Clear();

	/** Single binding entry, used by Snapshot(). */
	struct FEntry
	{
		FString Name;
		FString BlueprintPath;
		FString GraphName;
		FGuid NodeGuid;
	};

	/**
	 * Snapshot every current binding into an array. Order is not guaranteed
	 * (TMap iteration). Cheap copy — used by the list_node_names MCP tool.
	 */
	TArray<FEntry> Snapshot() const;

private:
	FECANodeNameRegistry() = default;

	mutable FCriticalSection Lock;
	TMap<FString, FEntry> Bindings;
};
