# ECABridge Next-Tier Roadmap — Post-Fanout Handoff, 2026-05-20

This doc supersedes the priority list in `docs/ecabridge-improvement-roadmap-2026-05-20.md` (v1).
The v1 doc surveyed competing plugins; this one is the work plan after the May 20 fanout
session, written for a fresh agent picking up where we left off.

---

## What just shipped on this branch

Seven parallel feature branches landed on `feature/big-wins-from-roadmap-2026-05-20`
during the May 20 fanout session. Each was authored by an isolated worktree agent
following a self-contained prompt; the bullet under each row names what the
*fresh* agent picking up this doc will see on disk.

| Branch | Commit | What landed |
|---|---|---|
| A1A2 | `d7dfabd` | Registry-level `FScopedTransaction` wrap (every mutating command becomes one undo step). `IECACommand::IsMutating()` virtual, default true; **161 explicit overrides to `false`** across read-only commands. `FECACommandResult::Warnings` field + `CollectUnknownParamKeys()` helper for silent-typo detection. Wired into `FECACommandRegistry::ExecuteCommand`. |
| B2   | `2d8b489` | `take_blueprint_editor_screenshot` — opens the Blueprint asset editor, frames the requested graph via `SGraphEditor::ZoomToFit`, captures the Slate widget with `FSlateApplication::TakeScreenshot`. Returns PNG inline as MCP image content or saved to disk. Documented fallback when SGraphEditor capture returns empty. |
| C1   | `e208c94` | `FECANodeNameRegistry` — session-scoped friendly-name → GUID map. 4 meta-tools (`name_blueprint_node`, `unname_blueprint_node`, `list_node_names`, `resolve_node_name`). `node_name` alias on 5 mutating BP-node commands. Optional `name` param on 11 `add_*` commands (auto-registers the new node's GUID). |
| C2   | `01451a0` | `MakeECADumpMeta(method, coverage, confidence, notes)` helper. `_meta` block added to `dump_blueprint_graph`, `find_blueprint_nodes`, `find_assets`, `dump_niagara_system`, `dump_metasound_graph`, `dump_animation_blueprint`, `dump_datatable`, `find_actors`, `search_blueprint_usage`, `list_textures`, `list_materials`. Convention documented in `CONTRIBUTING.md`. |
| C3   | `b7f6e0e` | 4 read-side analysis commands in a new category (`Blueprint Analysis`): `summarize_blueprint` (pseudo-code walk), `find_blueprint_callers` (reverse lookup over AssetRegistry), `diff_blueprints` (topology delta), `find_unused_assets` (zero-referrer scan). |
| D1   | (TBD)    | Headless build + test commands: `build_plugin` (UBT), `run_automation_tests` (`UnrealEditor-Cmd -nullrhi`), `compile_project_blueprints`. New category `Build & Test`. |
| D2   | (TBD)    | In-process Slate input injection — the "computer use" loop *inside* the editor. `find_slate_widgets`, `click_slate_widget`, `type_slate_text`, `slate_key_chord`, `take_slate_widget_screenshot`. New category `Slate Input`. |

---

## ⚠ Must-do BEFORE picking up anything else from this doc

These are the loose ends from the fanout. Fix them first.

### M1. Wire `warnings` into the MCP `tools/call` response path
**Why:** The A1A2 agent flagged that `ECAMCPServer.cpp:771` serializes `CommandResult.ResultData`
directly and bypasses `FECACommandResult::ToJsonString()`. As shipped, `warnings` only
surface via the legacy HTTP path — MCP clients never see them.

**Fix:** In `Source/ECABridge/Private/ECAMCPServer.cpp` around the `tools/call` response
builder, when `Result.Warnings.Num() > 0`:
  - Either include them under `_meta.warnings` of the MCP response (preferred — matches
    the MCP 2025-06-18 spec's `_meta` extension point)
  - Or prepend a single text-content block summarizing them before the result content

**Effort:** ~half day. Includes a smoke test that calls a command with a misspelled param.

### M2. B1 bulk pass — numbered error codes + next-step hints
**Why:** The v1 roadmap section 4. LLMs route themselves to recovery tools when error
messages are canonical and include a hint. Today every `FECACommandResult::Error(...)`
callsite uses an ad-hoc string.

**Approach:**
1. Add `FECABridgeErrors` in `Source/ECABridge/Public/Commands/ECABridgeErrors.h` with
   constexpr error codes:
   - `1xx` parameter parsing
   - `2xx` validation (out of range, bad format)
   - `3xx` not-found (asset/node/pin/function/graph)
   - `4xx` operation failed (compile/connect/save/transaction)
   - `5xx` context (editor not in expected state — no world, no selection, plugin not loaded)
2. Static factories: `ActorNotFound(name, hint)`, `BlueprintNotFound(path, hint)`,
   `PinNotFound(node, pin, hint)`, `ConnectionFailed(src, tgt, reason, hint)`,
   `CompilationFailed(bp, log_tail)`, `PluginNotLoaded(plugin, hint)`, etc. Each
   builds an `FECACommandResult` with a `code` integer alongside `error` string.
3. Mechanical replace across ~85 command files. Use `grep -rn "FECACommandResult::Error("`
   for the worklist; replace each with the appropriate factory. Many will be
   `BlueprintNotFound` / `ActorNotFound` / `GraphNotFound` — the long tail is
   one-offs that can stay as raw `Error()` calls with a code of 0 (uncategorized).

**Effort:** 2-3 days. Mostly mechanical. Risky areas: `ECABlueprintLispCommands.cpp` has
its own error format for Lisp parse errors — preserve that, just wrap.

**Test:** A new MCP smoke test that calls each error-producing path once and checks the
returned code is in the right range.

### M3. Update `ue5-mcp` SKILL.md
**Why:** [github.com/ibrews/ue5-mcp](https://github.com/ibrews/ue5-mcp) is the field manual
the agent loads on Archie/Fort. After this session it'll be stale on:
- Tool count (was "400+", now 510+ish)
- New commands that agents won't discover otherwise
- The semantics of `warnings`, `_meta`, and Ctrl+Z undo

**Add new sections / update:**
- New section "Reading warnings and `_meta`" — when an agent should retry vs. accept partial output
- New section "Slate input fallback" — when to use D2's commands instead of writing a bespoke MCP wrapper
- "Tool Strategy" decision matrix update — add `take_blueprint_editor_screenshot` for slide-ready BPs;
  add `summarize_blueprint` for "understand before editing"; add `run_automation_tests` for verification
- "Niagara" / "MetaSound" gotchas may be partly obsolete now that transactions wrap every command —
  re-test the documented crash patterns and update; some may now be cleanly reverted via undo

**Effort:** ~1 day. Co-locate with a re-test pass that confirms which v1 gotchas are now
moot under the transaction wrap.

---

## Tier 1 — High-impact, well-scoped (1-3 days each)

Pick any. Each fits in a single agent worktree without conflicts (assuming M1-M3 land first).

### T1a. PIE testing framework with assertions
**Inspired by:** [Flop / Flopperam](https://www.flopperam.com/) — they expose `pie_test_bp` +
`pie_test_scene` with 30+ assertion types and "vision + baseline pixel-diff".

**File:** new `Source/ECABridge/Private/Commands/ECAPIETestCommands.cpp` + header.

**Commands:**
- `pie_session_start` — begin PIE with optional level + player controller class
- `pie_session_end`
- `pie_advance_frames(n)` — tick the world N frames without realtime delay
- `pie_assert(kind, args, timeout_seconds?)` — single assertion. Kinds:
  - `actor_exists(class, name?)`, `actor_count(class, op, n)`, `actor_at(name, location, tolerance)`
  - `dispatcher_fired(actor, dispatcher_name, within_seconds)`
  - `anim_state(skeletal_mesh, expected_state_machine_state)`
  - `bt_task_ran(ai_controller, task_class)`
  - `niagara_activated(emitter_name)`
  - `overlap_detected(actor_a, actor_b)`
  - `widget_visible(widget_name)`, `widget_text_equals(widget_name, expected)`
  - `value_equals(variable_path, expected)`, `value_in_range(variable_path, min, max)`
  - `screenshot_diff(baseline_path, tolerance_percent)` — pixel-diff a captured frame
- `pie_run_test_script(steps[])` — composite: start, run each step, end, return pass/fail per step

**Why ECABridge needs this:** The agent currently has no way to verify gameplay
actually works. Authoring + verification asymmetry is our biggest gap vs. competitors.
Unblocks the "agent finds and fixes a bug" demo arc.

**Effort:** ~1 week. The assertion engine itself is small; building 12-15 high-value assertion
implementations is the bulk of the work. Use `FAutomationTestBase` patterns internally — UE's
own framework handles PIE lifecycle correctly and is well-tested.

**Dependencies:** None hard, but pairs naturally with M2's error codes. Test failures should
return code 4xx with the assertion name in the hint.

### T1b. `lookup_docs` — local UE API documentation search
**Inspired by:** [mcp-unreal](https://github.com/remiphilippe/mcp-unreal) ships a local Bleve
search index of UE 5.7 docs. We do the same but for both 5.7 and 5.8.

**What to build:**
1. **Index build** (Python script under `scripts/build-docs-index.py`):
   - Scrape `dev.epicgames.com/documentation/en-us/unreal-engine/5.7/` and `.../5.8/` into markdown
   - Tokenize and build a BM25 inverted index with class-name and function-name boosts
   - Store as a flat SQLite database under `Resources/docs/ue5.7.db` + `Resources/docs/ue5.8.db`
   - Add `Resources/docs/` to the `.uplugin`'s `CanContainContent` if needed
2. **MCP command** `lookup_docs(query, engine_version?, max_results?, context_chars?)`:
   - Loads the right SQLite db
   - Returns matching docs with relevance score + short excerpt + canonical URL
3. **Index refresh CI** — workflow that re-scrapes docs and updates the DBs monthly. The
   committed DBs make consumers' lives easy (no build step required).

**Why:** Agents constantly hallucinate UE API calls. Grounded doc lookup drops that to near zero.

**Effort:** 2-3 days. Python scrape + index is half a day; C++ query side is half a day;
license / EULA review for shipping Epic docs in our binary is ~half a day. The doc license
is permissive for redistribution but document the source in our LICENSE file.

**Risk:** Scraper fragility. Wrap each fetch in retries with exponential backoff; cache
intermediate markdown so a partial scrape can resume.

### T1c. MCP `resources/list` + `prompts/list` server endpoints
**Inspired by:** [StraySpark](https://forums.unrealengine.com/t/strayspark-unreal-mcp-server-200-ai-tools-for-ue5-editor-automation-via-mcp/2707474)
exposes 12 context resources + 10 reusable workflow prompts via the MCP spec endpoints.
ECABridge currently has zero `resources/` or `prompts/` — only `tools/`.

**What to build:**

*Resources* (in `Source/ECABridge/Private/ECAMCPServer.cpp`'s router):
- `ecabridge://project/info` — project name, engine version, enabled plugins
- `ecabridge://level/hierarchy` — current persistent level's actor tree (cheap version of dump_)
- `ecabridge://selection` — currently selected actors / assets
- `ecabridge://perf/stats` — frame time, draw calls, primitive count, memory
- `ecabridge://recent-errors` — last 50 output-log errors
- `ecabridge://open-editors` — names of asset editors currently open
- `ecabridge://session-history` — recent MCP tool calls (last 100, ring buffer)
- `ecabridge://node-names` — current `FECANodeNameRegistry` bindings (from C1)
- `ecabridge://transactions` — recent transactions stack (so agents can see undo history)

*Prompts:*
- `prompt://debug-actor-not-spawning` — preset workflow walking through the common causes
- `prompt://optimize-scene-for-vr` — checklist + targeted dump commands
- `prompt://set-up-niagara-burst` — canned recipe wrapping multiple Niagara commands
- `prompt://compose-slide-ready-blueprint` — wraps auto_layout + comment + screenshot
- `prompt://migrate-blueprint-to-c++` — guided refactor flow
- `prompt://write-pie-test-for-actor` — uses T1a's PIE test framework
- `prompt://check-blueprint-for-replication-correctness`
- `prompt://recreate-from-screenshot` — vision-prompted level recreation

**Why:** Resources let agents read state cheaply (no big tool-call payload, no
transaction wrap). Prompts give the agent canned workflows so we don't hard-code
every recipe as MCP tools.

**Effort:** 2 days for the server-side plumbing + ~3 hours per prompt to author the text.

**Dependency:** M1 should land first since both touch `ECAMCPServer.cpp`.

### T1d. Mass Entity (ECS) commands
**Inspired by:** quodsoler `ue-mass-entity` skill ([skills.sh](https://skills.sh/quodsoler/unreal-engine-skills)).
ECABridge has zero coverage of UE's Mass Entity system. Crowd / battle scenes (Spellrot,
the user's demo arc) live or die by Mass.

**File:** new `Source/ECABridge/Private/Commands/ECAMassEntityCommands.cpp`.

**Commands:**
- `spawn_mass_entities(config_asset, count, location_or_volume)` — create from `UMassEntityConfigAsset`
- `query_mass_entities(fragment_filter[], tag_filter[], max_results)`
- `set_mass_entity_fragment(entity_handle, fragment_class, json_value)`
- `destroy_mass_entity(entity_handle)`
- `list_mass_processors` / `enable_mass_processor(name, bool)` / `set_mass_processor_tick_group(name, group)`
- `get_mass_perf_stats` — entity count, processor times, fragment memory

**Build.cs gate:** `EngineHasPluginWithBinaries("MassEntity")` → `WITH_ECA_MASS_ENTITY=1`
and add `MassEntity`, `MassActors`, `MassSpawner`, `MassRepresentation` deps. Follow the
existing Niagara / MetaHumanCharacter optional-block pattern (Build.cs lines 187-225).

**Effort:** 3 days. Bounded — Mass API is well-documented and stable.

### T1e. C2 _meta backfill — the deferred ~30 commands
The C2 agent shipped the must-do + stretch (~10 commands). Long tail of dump_/find_/list_
commands in DMX, MetaHuman, IK Retargeter, NDisplay, PCG, MovieRender, ColorGrading, LiveLink,
USD, etc. still lack `_meta`.

**Approach:** Mechanical — open each, count parsed/skipped during the existing loop, attach
`MakeECADumpMeta(...)` at the end.

**Effort:** 2 days. Pure follow-through on C2's pattern.

---

## Tier 2 — Subsystem coverage gaps (each a self-contained file)

Compared against the quodsoler 27-skill UE5 pack, ECABridge is missing whole subsystems.
Each row below is one new commands file. Effort tags are loose estimates.

| # | Subsystem | New commands | Effort | Notes |
|---|---|---|---|---|
| T2a | **StateTree** | spawn/run/dump/edit StateTree assets, blackboard ops | 2-3 days | UE 5.4+. Newer than BTs; agents will look here first. |
| T2b | **Game Features** | enable/disable/list/dump GF plugins, query actions | 1-2 days | Editor-only ops are easy; runtime activation needs care. |
| T2c | **SaveGame** | save/load/list/delete slots, get_save_data(slot, key) | 1 day  | Easy + high agent demand. Do this early. |
| T2d | **Replication** | set_actor_replicates, replication_condition, dump_replicated, sim_lag, start_dedi_pie | 2-3 days | Some commands require PIE (dep on T1a). |
| T2e | **Character Movement** | get/set CMC config, apply preset, simulate jump arc | 1 day | Mostly property bag manipulation. |
| T2f | **AI Nav + BT + EQS** | generate_navmesh, set_nav_modifier, run_eqs_query, set_bt_blackboard, dump_blackboard | 3 days | NavigationSystem dep already in Build.cs but unused. |
| T2g | **Audio spatial placement** | spawn_attenuated_audio, set_attenuation_curve, spatial_audio_debug_overlay | 1 day | MetaSound covers graph editing; this is the missing actor-side. |
| T2h | **VR optimization** | toggle instanced_stereo, set_foveated_rendering, dump_draw_call_stats | 2 days | Reverses-only against next-gen.md gap list. |
| T2i | **Packaging** | package_project(platform, config), cook_content, list_targets | 2 days | Shells out to UAT/UBT like D1. |

If you want one big-win Tier 2 bundle for an overnight fan-out: **T2c (SaveGame) + T2e
(CMC) + T2g (Audio spatial) + T2b (Game Features)** — each ~1-2 days, all isolated files,
no conflicts. Four agents in parallel, two days of wall clock, ~5 days of work landed.

---

## Tier 3 — Speculative or higher-cost

### T3a. PIE input recorder + replay
**Source:** ue-llm-toolkit's `PIEInputRecorder` / `PIESequenceRunner`. Always-on 60fps
deterministic input capture during PIE; replay-as-test sequence.

**Why:** Powers the "diagnose then fix" workflow — record a bug repro once, replay against
new builds.

**Effort:** 1 week. Self-contained subsystem, but engine-level hookery (input system
interception) is fiddly.

### T3b. Domain prompt packs
Markdown context files auto-loaded by tool-name regex match. Saves agent tokens on
repeated discovery patterns. Pairs with T1c's prompts/list. **Effort:** Half-day.

### T3c. Event subscriptions via MCP `notifications/*`
Emit `notifications/ecabridge/asset_changed`, `actor_spawned`, `blueprint_compiled`, etc.
Enables long-lived agent loops that react to editor state without polling.
**Effort:** 3-5 days. Requires hooking UE's `FAssetRegistry::Get().OnAssetAdded` etc.
and bridging to the SSE stream.

### T3d. Arbitrary object-path property access
Generic `get_object_property(path, field)` / `set_object_property(path, field, value)` via
UE reflection. ECABridge currently has typed get/set per subsystem; a generic escape hatch
reduces the "we don't have a command for that yet" surface area.
**Effort:** 2 days. Risk: needs careful transaction wrap + property-type coverage tests.

### T3e. Authenticated MCP sessions
Bearer token in requests. Currently any localhost client can connect. Matters when
ECABridge binds to non-localhost (Tailscale fleet usage).
**Effort:** 1 day. Add to `ECABridgeSettings` + check in `ECAMCPServer` request handler.

### T3f. NVIDIA-style AST-grounded code retrieval for the project's C++
**Source:** [NVIDIA's blog on reliable UE AI coding](https://developer.nvidia.com/blog/reliable-ai-coding-for-unreal-engine-improving-accuracy-and-reducing-token-costs/).
AST-based syntax-aware chunking + hybrid (dense + lexical) search over UE engine source +
the consumer project's C++.

**Why:** Combines with T1b — `lookup_docs` is for the public API; this is for the
implementation. When an agent asks "how is X implemented" or "what calls Y", it should hit
this index.

**Effort:** 1 week+. Higher complexity (embedder choice, vector store, index refresh). Defer
until T1b lands and we know whether it's enough.

### T3g. `get_output_log` with cursor + filtering
`since`, `categories`, `min_verbosity`, `compact`, `strip_timestamps`. ECABridge log
tools today are firehose-shaped. **Effort:** 1 day. Self-contained.

### T3h. `find_blueprints` with structured filters
Parent class, component, interface, query language. **Effort:** 2 days.

### T3i. `list_files` with .gitignore + ripgrep semantics
**Source:** AIK pattern. Cleaner than raw Python sandbox FS reads. **Effort:** 1 day.

---

## Explicitly NOT doing — and why

### Pixel Streaming MCP commands

`projects/ecabridge-next-gen.md` (2026-04-15) called pixel streaming "THE EYES — highest
priority infrastructure". After this session's analysis, we're scoping it out.

**The four documented motivations and how we cover each without pixel streaming:**

1. **Dialog dismissal / viewport navigation / debugging (same-machine)** — D2's in-process
   `FSlateApplication` injection has microsecond latency vs. WebRTC's tens of ms. No
   render-thread dependency. Strictly better for the same-machine case.

2. **Cross-machine MCP control** — Already solved by binding ECABridge's HTTP server to
   `0.0.0.0` (not just `127.0.0.1`) and using Tailscale. Agents on Sam (Mac Mini gateway)
   call commands on Archie/Fort's ECABridge directly. The MCP transport already crosses
   machines.

3. **Visual verification (periodic screenshots)** — Covered by existing screenshot commands
   that return inline PNG via MCP image content blocks (`take_gameplay_screenshot`,
   `take_camera_screenshot`, B2's `take_blueprint_editor_screenshot`, and D2's
   `take_slate_widget_screenshot`). Bandwidth and latency are far better than a continuous
   WebRTC stream.

4. **Overnight autonomous operation** — Covered by D1's `run_automation_tests`,
   T1a's PIE testing, and the existing screenshot tools. The agent's loop is "command →
   verify with screenshot/test → next command", not "watch the stream and react in real
   time".

**The residual case** — continuous high-frame-rate visual feedback for an interactive
WebRTC viewer — is a niche need not served by ECABridge's MCP-tool model. Anyone who
needs it can launch PixelStreaming2 via UE's existing CLI flags directly; we don't
gain anything by wrapping that in an MCP command.

If a future requirement emerges (e.g., the demo arc explicitly requires watching a
running gameplay session from a remote viewer), revisit. The PixelStreaming2 plugin is
still optionally available — we just don't wrap it.

### `execute_script` mega-tool collapse
From v1 roadmap. ECABridge's ~510 named tools beat a single Lua/Python-dispatched
mega-tool on discovery. Keep.

### Embedded chat UI / Svelte WebUI / Lua VM
From v1 roadmap. The agent is the chat host. The plugin is not. Don't add a second
runtime to the plugin.

### UE 5.7-only or 5.8-only pin
Our dual-engine support is a moat (every other plugin pins to one version). Don't drop it
for any new feature unless absolutely necessary, and document the trade-off if so.

### Hardcoded "precision" magic numbers (per v1)
`LUA_MAX_TRACE_ENTRIES = 7341`, `ChunkTokenLimit = 4173`, `bonus = 0.1473f`. Cute, but
unmaintainable. Use named constants with rationale comments.

---

## How to pick what's next

| You have... | Pick... | Why |
|---|---|---|
| ½ day | T2c (SaveGame) | High demand, low effort, ships fast |
| 1 day | M1 (warnings → MCP path) then M3 (SKILL.md update) | Loose ends — block other agents otherwise |
| 2 days | M2 (B1 error codes, bulk) | Touches everything; do it once, never again |
| 2-3 days | T1b (docs lookup), T1c (resources/prompts), T1d (Mass Entity), T2a (StateTree) | Pick by what you're stronger at — server protocol, UE subsystem, or doc tooling |
| 1 week | T1a (PIE testing) | The biggest single-feature win. Unblocks the demo arc. |
| Overnight parallel fan-out | T2c + T2e + T2g + T2b (four self-contained subsystem files) | All isolated, all 1-2 days, no merge conflicts |

**Recommended single-session pick:** M1 + M3 + T1c. Half day of cleanup, then ship MCP
resources + prompts in one day. Sets up future agents to use the new resource path
instead of re-reading state via tool calls.

---

## References

### In this repo
- [`docs/ecabridge-improvement-roadmap-2026-05-20.md`](ecabridge-improvement-roadmap-2026-05-20.md) — v1 survey (UnrealClaude / ue-llm-toolkit / gdep / AIK)
- [`docs/new-layout-commands-2026-05-20.md`](new-layout-commands-2026-05-20.md) — the BP layout / comment work that motivated the May 20 fanout
- [`CONTRIBUTING.md`](../CONTRIBUTING.md) — house style + the new "Confidence metadata" convention from C2

### In the team KB
- `projects/ecabridge-next-gen.md` (2026-04-15) — original "THE EYES" pixel streaming plan, now superseded by the §"Explicitly NOT doing" rationale above
- `projects/ecabridge-roadmap.md` — the broader fleet-level project plan
- `intelligence/decisions/2026-04-28-skills-audit.md` — what UE5 skills are loaded per machine
- `intelligence/techniques/skills-quodsoler-unreal-engine.md` — the 27-skill UE5 reference

### Competing plugins surveyed
- [Flop / Flopperam](https://www.flopperam.com/) — 700+ ops, PIE assertions, vision pixel-diff
- [StraySpark MCP Server](https://forums.unrealengine.com/t/strayspark-unreal-mcp-server-200-ai-tools-for-ue5-editor-automation-via-mcp/2707474) — `resources/list` + `prompts/list` + 207 tools
- [mcp-unreal (remiphilippe)](https://github.com/remiphilippe/mcp-unreal) — headless build/test + local docs index
- [UnrealClaude (Natfii)](https://github.com/Natfii/UnrealClaude) — 25 tools, MCP 2025-06-18 spec
- [ue-llm-toolkit (ColtonWilley)](https://github.com/ColtonWilley/ue-llm-toolkit) — PIE input recorder, 37 composite tools
- [UnrealGenAISupport (prajwalshettydev)](https://github.com/prajwalshettydev/UnrealGenAISupport) — model integration, 3D-gen connectors (orthogonal to our scope)
- [Aura](https://forums.unrealengine.com/t/aura-ai-agent-for-unreal-editor/2689209) — Jan 2026 commercial; no public source to borrow from
- [quodsoler/unreal-engine-skills](https://skills.sh/quodsoler/unreal-engine-skills) — the 27-skill UE5 pack used as a subsystem-coverage checklist

### External technical references
- [NVIDIA: Reliable AI Coding for Unreal Engine](https://developer.nvidia.com/blog/reliable-ai-coding-for-unreal-engine-improving-accuracy-and-reducing-token-costs/) — AST chunking, hybrid retrieval (T3f)
- [Epic: Pixel Streaming in Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/pixel-streaming-in-editor) — for the "NOT doing" reasoning
- [MCP spec 2025-06-18](https://modelcontextprotocol.io/) — for resources/prompts endpoint shapes (T1c)

---

*This roadmap was written by Claude Opus 4.7 (1M context) on Archie/FRIDGE during the
May 20 fanout session. Refresh when the next batch of fanout work merges.*
