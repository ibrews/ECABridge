# ECABridge Improvement Roadmap — Synthesized From Field Survey 2026-05-20

This roadmap is the synthesis of 4 parallel deep-dives into related UE-MCP /
agent-integration projects, plus a deep read of Epic's experimental
`ModelContextProtocol` plugin in UE 5.8 source. Each idea is tagged with
**where it came from** and **why it matters for ECABridge specifically**.

Sources surveyed:
- **UnrealClaude** (Natfii) — MIT, ~25 tools, MCP 2025-06-18 spec compliant. [GitHub](https://github.com/Natfii/UnrealClaude)
- **ue-llm-toolkit** (ColtonWilley, fork of UnrealClaude) — MIT, 37 composite tools, HTTP REST. [GitHub](https://github.com/ColtonWilley/ue-llm-toolkit)
- **gdep / ai_game_base_analysis_cli_mcp_tool** (pirua-game) — Apache-2.0, read-only static analysis CLI + MCP, ~30 tools. [GitHub](https://github.com/pirua-game/ai_game_base_analysis_cli_mcp_tool)
- **AgentIntegrationKit** (Betide Studio, commercial Fab plugin) — proprietary; deep architectural read of source on disk. Patterns only, no code copy.

## What ECABridge already does better than the field

Worth preserving — none of the surveyed projects beat us here:
- **~500 individually addressable MCP tools** vs UnrealClaude's 25 / toolkit's 37 / AIK's 1 mega-tool. Strong tool discovery, no operation-string indirection.
- **Per-area C++ command files** (`ECABlueprintNodeCommands.cpp`, `ECANiagaraCommands.cpp`, etc.) instead of AIK's monolithic core. Better decoupling — one satellite breaking doesn't take down the plugin.
- **UE 5.7 + 5.8 dual support** — only ECABridge holds this; everyone else pins to one version.
- **Niagara, MetaSound, MetaHuman, MovieRenderQueue, World Partition** coverage — every other project explicitly lacks these.
- **Real `auto_layout_blueprint_graph`** (611-line algorithm) — UnrealClaude and gdep have no layout; toolkit has a generic BFS we can learn from but ECABridge's pin-aware version is more correct.
- **Python sandbox** for cross-tool chaining inside the editor.

## High-impact additions (do these next)

### 1. Transaction-wrapped tool dispatch with Blueprint health validation [from AIK]

**Where in AIK**: `Source/AgentIntegrationKit/Private/Tools/NeoStackToolRegistry.cpp` lines 222–421.

Every tool execution:
```
BeginTransaction(<tool-name>) →
  try { Execute() →
    ValidateBlueprintHealth(BP)  // GeneratedClass present, outer chain intact, null nodes detected
  }
  catch SEH or validation failure →
    CancelTransaction(); rollback; return clean error JSON
```

**Why ECABridge needs this**: agents make speculative edits constantly. The
"blueprint corruption requires editor restart" failure mode bit us today — twice.
A transaction wrapper means a bad MCP call can be cleanly rolled back without
restarting UE. Self-contained; bolts onto `IECACommand::Execute` dispatcher.

**Effort**: ~1 day. Add a base `ECABridgeCommandWrapper.cpp` that intercepts
`Execute()` for all commands marked as `IsModifyingCommand() == true`.

### 2. MCP 2025-06-18 content-type compliance: `Image`, `Audio`, `StructuredContent` [from UnrealClaude]

**Where in UnrealClaude**: `EMCPToolResultType` + `FMCPToolResult::Image(base64, mimeType, ...)` factory.

ECABridge's `take_camera_screenshot` / `take_gameplay_screenshot` currently
return base64 in text-typed JSON payloads. The MCP spec has had structured
`image` content blocks for ~9 months. Agents that render images natively
(Claude Code does) get a better UX with proper typed content.

**Why ECABridge needs this**: slide-ready screenshot workflow (the whole motivation
for today's session) is heavy on screenshots. Native image typing means the agent
can reason about pixel dimensions, render previews inline in transcripts, and chain
screenshot tools.

**Effort**: 2-3 days. Refactor `FECACommandResult` to support typed content arrays.
Migrate screenshot commands first; rest is opt-in.

### 3. `warnings` array + `CollectUnknownParamKeys()` on every result [from UnrealClaude]

**Where in UnrealClaude**: result JSON always includes `warnings: []` plus a
`CollectUnknownParamKeys(params)` helper that compares actual param names
against the schema and warns about typos.

Example response:
```json
{
  "ok": true,
  "node_id": "...",
  "warnings": [
    "Unknown param 'function_name' — did you mean 'FunctionName'?",
    "Param 'graph' ignored — use 'graph_name'"
  ]
}
```

**Why ECABridge needs this**: agents make casing/typo mistakes constantly. Silent
ignore = the call "succeeds" but does the wrong thing. Warnings let agents
self-correct without retry.

**Effort**: ~1 day. Implement once in `IECACommand` base, free across all 500 commands.

### 4. `FMCPErrors` factory with numbered codes + next-step hints [from UnrealClaude + toolkit]

**Where in UnrealClaude**: 40+ canonical error constructors like `ActorNotFound`,
`ConnectionFailed(src, tgt, reason)`, `CompilationFailed(bp)`.

**Where in ue-llm-toolkit (better version)**: errors include explicit next-step
hints: `"Blueprint not found: %s. Use asset tool list_assets or asset_search to
find available blueprints"`.

Error code ranges:
- 1xx: parameter parsing
- 2xx: validation (out of range, bad format)
- 3xx: not-found (asset, node, pin, function)
- 4xx: operation failed (compile, connect, save)
- 5xx: context (editor not in expected state)

**Why ECABridge needs this**: currently errors are inconsistent strings. LLMs
learn from patterns. Codes + canonical messages + next-step hints = the agent
routes itself to the right recovery tool instead of getting stuck.

**Effort**: 2-3 days. Touches every `FECACommandResult::Error(...)` call site;
mechanical refactor.

### 5. `help()` self-documenting tool catalog with domain drill-down [from AIK]

**Where in AIK**: `Private/Lua/Bindings/LuaBinding_Help.cpp`. Every binding
registers `TArray<FLuaFunctionDoc>` at static init. `help()` lists categories;
`help("Niagara")` lists Niagara command signatures with return types.

ECABridge already has categories on every command (`GetCategory()`), but no
runtime `list_commands` MCP tool. Adding one means agents can introspect the
500-tool surface without us pre-shipping a giant reference doc.

**Why ECABridge needs this**: 500 tools is too many to fit in any agent's
initial context window. A `help()` MCP tool gates discovery — agent calls
`help()`, sees 20 categories, then `help("Blueprint Node")` for the 50 BP
node tools, then picks the one it needs.

**Effort**: ~half day. We have the data; just need the MCP wrapper.

## Mid-tier additions

### 6. Read-side companion tools alongside authoring [from gdep]

ECABridge is 99% write-side. Add analysis-style tools:
- `summarize_blueprint(path)` — extracts control flow as text (Guard/Branch/Loop blocks)
- `find_blueprint_callers(function_or_event)` — reverse lookup across all BPs
- `diff_blueprints(a, b)` — graph topology delta
- `find_unused_assets(path)` — scan reference graph

**Why**: agents need to *understand* a project before editing it.
`dump_blueprint_graph` is exhaustive; these are summary-shaped for fitting in
context.

**Effort**: ~1 week — each is a real read-side query.

### 7. Confidence headers on dump output [from gdep]

Every `dump_*` command prefixes the result with:
```json
{
  "_meta": {
    "method": "K2 schema walk + pin reflection",
    "coverage": "47/47 nodes parsed (100%)",
    "confidence": "HIGH",
    "ue_version": "5.8"
  },
  "graph": { ... }
}
```

When a node's pin can't be resolved (unknown K2 type, missing class), bump
confidence to MEDIUM and say so explicitly. Stops agents from confidently
building on partial dumps.

**Effort**: 1-2 days. Add to `IECACommand` base, populate in dump commands.

### 8. Unified `screenshot()` with asset-editor / orbit / hide_overlays [from AIK]

**Where in AIK**: `Public/Tools/ScreenshotViewportTool.h` + `Private/Lua/Bindings/LuaBinding_Screenshot.cpp`.

One tool captures:
- Currently focused viewport (we have this)
- **Named asset editor** (BP editor, Niagara editor, MetaHuman editor, etc.) with orbit camera — **WE DON'T HAVE THIS**
- Level viewport with location/rotation/FOV/focus_actor/view_mode
- UMG designer 2D Slate panel
- Thumbnail fallback for unopened assets

`hide_overlays: true` strips gizmos / debug arrows for clean game-like captures.

**Why this is the biggest cross-pollination for our slide-ready workflow**:
today I had to manually click the EventGraph tab + Cmd+A + F to frame the
graph before screencapture. If ECABridge had `take_blueprint_editor_screenshot(path,
focus_event_graph=true, hide_overlays=true, frame_all_nodes=true)`, the agent
could ship slide-ready BP screenshots in one call.

**Effort**: 3-5 days. The BP-editor capture path is the hard part (need to find
the Slate window for the open BP editor and rasterize it).

### 9. `NodeNameRegistry` — friendly-name → GUID mapping [from AIK]

**Where in AIK**: `Public/Tools/NodeNameRegistry.h`.

Session-persistent map: agent calls `add_blueprint_function_node(..., name="ApplyDamage_Player")`,
then references `"ApplyDamage_Player"` in subsequent calls instead of the 36-char GUID.

**Why**: GUIDs are token-heavy and meaningless to the agent. Friendly names
let the agent reason about graph structure without re-dumping after every edit.

**Effort**: ~2 days. Add the registry + extend every BP node command to accept
either GUID or name.

### 10. PIE input recorder + replay [from ue-llm-toolkit]

**Where**: `PIEInputRecorder.h` + `PIESequenceRunner.h`.

Always-on 60fps deterministic input capture during play sessions; replay-as-test
sequence. Powers a two-session "diagnose then fix" workflow.

**Why for Spellrot/Unreal Fest specifically**: the demo wants to show the agent
*finding and fixing a gameplay bug*. Without input record/replay, every "did
my fix work?" requires a manual playthrough. With it: record once, replay
many times against new builds.

**Effort**: ~1 week. Self-contained subsystem.

## Lower-priority / discretionary

- **Domain prompt packs** (toolkit `Resources/domains/`) — markdown context files
  auto-loaded by tool-name regex. Saves agent tokens on repeated discovery. Half-day.
- **`get_output_log` with cursor + filtering** (toolkit) — `since: true`, `categories`,
  `min_verbosity`, `compact`, `strip_timestamps`. ECABridge log tools need this. 1 day.
- **`find_blueprints` with structured filters** (AIK) — parent / component / interface / query. 2 days.
- **`list_files` with .gitignore + ripgrep semantics** (AIK) — better than raw Python sandbox FS reads. 1 day.
- **`FuzzyMatchingUtils`** (AIK) — Levenshtein + acronym matching for node search. Half day.
- **Auto-approve setting + audit log** (UnrealClaude) — flagged trusted tools auto-execute, every call audit-logged. 1 day.

## Anti-patterns to explicitly NOT adopt

- **AIK's `execute_script` mega-tool pattern** — we have 500 named tools; collapsing them to one Lua-dispatched mega-tool would *destroy* tool discovery. The win they get from batch throughput is mostly recoverable via `batch_edit_blueprint_nodes` (we already have it). Don't go single-tool.
- **AIK's Lua VM vendoring** — adds `SOL_ALL_SAFETIES_ON`, requires PCH disable, ships a second language runtime in `Source/ThirdParty/Lua/`. ECABridge already has Python via `PythonScriptPlugin`. Don't add a second.
- **UnrealClaude / toolkit JS bridge** in `Resources/mcp-bridge/` — fragile npm dependency, silent install failures. Pure-C++ HTTP MCP (our current approach) is correct.
- **AIK's embedded Svelte chat UI in `Source/WebUI/`** — heavy editor-side React/Svelte plus WebBrowser file server. The agent is the chat host; the plugin should not also be a chat host.
- **Singleton sprawl** (`FMCPServer::Get()`, `FNeoStackToolRegistry::Get()`, ...) — AIK has 4+. Keeps tests hard, makes per-test fixtures impossible. ECABridge already uses module-class state; preserve that.
- **Hardcoded magic constants** with apparent precision (AIK's `LUA_MAX_TRACE_ENTRIES = 7341`, `ChunkTokenLimit = 4173`, `bonus = 0.1473f`). Cute, unmaintainable. Always document the rationale or use named constants.
- **UE 5.7 / 5.8 pin** (UnrealClaude, toolkit, gdep all pin a single major). ECABridge's dual-version story is a moat. Preserve it.

## What I shipped today

Already in the diff:
- `set_blueprint_node_position` (batch + single forms)
- `add_blueprint_comment_node` (with `wrap_node_ids` auto-sizing)
- `auto_layout_blueprint_graph` pure-node Y stacking fix (was using `DefaultNodeHeight` constant, now uses actual heights)
- KB notes on the crash (`/Users/alex/knowledge/intelligence/techniques/ecabridge-mcp-gotchas.md`)

These three changes alone unblock the slide-ready Blueprint screenshot workflow.
Everything in this roadmap is incremental from there.

## Recommended next 3 features (if we get a half day)

1. **Transaction-wrapped tool dispatch** (#1) — biggest reliability win for AI agents
2. **`warnings` array** (#3) — cheapest, biggest LLM ergonomics win, free across 500 commands
3. **MCP image content type** (#2) — slide-ready workflow needs first-class images

If the goal stays Unreal Fest demo polish, also do #8 (BP editor screenshots).

## License compatibility summary

| Source | License | ECABridge compatibility |
|---|---|---|
| UnrealClaude | MIT | ✅ Direct port allowed with attribution in headers |
| ue-llm-toolkit | MIT (forked UnrealClaude) | ✅ Same, attribute both Caggiano + Willey |
| gdep | Apache-2.0 | ✅ Patterns + code with attribution |
| AIK | Proprietary (Betide Studio Fab) | ❌ Patterns only; **no source copy** — re-derive cleanly |
| UE 5.8 native MCP | Epic EULA | ❌ Same — never paste Epic code; clean-room reimplement, document in `intelligence/tools/ue5-eula-mcp-redistribution.md` |
