# ECABridge — UE MCP, plus everything Epic hasn't shipped to the launcher yet

![ECABridge demo — building a scene from scratch with MCP commands](docs/media/demo.gif)

ECABridge is the **+features layer on top of Epic's native `ModelContextProtocol` plugin**, built for the ~90% of UE developers who use the Epic Games Launcher and don't compile UE from source. We track Epic's `ue5-main` MCP plugin weekly and clean-room port the protocol changes and new toolsets within 1-2 weeks, so launcher users get bleeding-edge MCP capabilities without waiting for the next 5.X point release.

On top of that, we own the surfaces Epic doesn't ship: **MetaHuman, Mutable, MetaSound, Movie Render Queue, the `dump_*` Rosetta Stone serializers, refactoring safety nets, DMX/nDisplay/XR mutation, Movie Render Graph, World Partition mutation.** Those aren't going to land in Epic's core toolsets soon — they're too domain-specific. So we maintain them.

**570+ MCP tools**, **one branch supports UE 5.7 and 5.8**, embedded **Python sandbox** for server-side multi-tool chaining, **MCP 2025-03-26 protocol completeness** (sessions, cancellation, SSE progress, `resources/`, `notifications/tools/list_changed`), and **inline base64 PNG screenshots**.

> **Using this with an AI agent?** Pair it with **[ue5-mcp](https://github.com/ibrews/ue5-mcp)** — a Claude Code / Cowork skill that loads the hard-won knowledge your agent needs to use these tools without crashing the editor. ECABridge is the plugin (what tools exist); ue5-mcp is the field manual (which calls actually work, which crash, and the workarounds). Install both.

## How we relate to Epic's native MCP plugin

UE 5.8 ships an experimental `ModelContextProtocol` plugin from Epic. ECABridge is **not** a competitor or replacement. We deliberately:

- **Track Epic's `ue5-main` weekly** (see `automation/native-mcp-watcher/`) and port new protocol features and toolsets so launcher users see them ahead of the next binary release
- **Use a non-overlapping config key** (`ecabridge` vs native's `unreal-mcp`) so both can register in the same `.mcp.json`
- **Run on a non-overlapping port** (`:3000` vs native's `:8000`) so both can listen simultaneously
- **Reimplement clean-room** under MIT — no Epic source in our repo. See `intelligence/tools/ue5-eula-mcp-redistribution.md` in our KB for the license discipline

So if you have UE 5.8 Preview, run both. If you're on UE 5.7, ECABridge is your only option. If you're on a 5.8.1 launcher build but Epic just shipped a protocol upgrade in `ue5-main`, ECABridge gets it to you in 1-2 weeks instead of months.

## Quick start

1. Copy the plugin folder to your project's `Plugins/` directory.
2. Add to your `.uproject`:
   ```json
   {"Name": "ECABridge", "Enabled": true}
   ```
3. Build and launch the editor — ECABridge starts automatically on `localhost:3000`.
4. Auto-generate the MCP client config for your AI tool:
   ```
   ECABridge.GenerateClientConfig All
   ```
   That writes `.mcp.json`, `.cursor/mcp.json`, `.vscode/mcp.json`, `.gemini/settings.json`, and `.codex/config.toml` at your project root with the right per-client schemas. (Or pass `ClaudeCode`, `Cursor`, `VSCode`, `Gemini`, or `Codex` to target just one.) Coexists with native's MCP entry — we upsert, we don't overwrite.
5. Or register manually with Claude Code:
   ```bash
   claude mcp add --transport http --scope user unreal-ecabridge http://127.0.0.1:3000/mcp
   ```

Full setup details, Claude Desktop config, and port-collision notes in **[Installation](https://github.com/ibrews/ECABridge/wiki/Installation)**.

## Documentation

The full docs live in the **[ECABridge Wiki](https://github.com/ibrews/ECABridge/wiki)**. Start there for:

| | |
|---|---|
| [Installation](https://github.com/ibrews/ECABridge/wiki/Installation) | Drop-in setup, Claude / Cursor config, curl smoke check |
| [Command Categories](https://github.com/ibrews/ECABridge/wiki/Command-Categories) | All ~500 tools broken down by area |
| [Recipes](https://github.com/ibrews/ECABridge/wiki/Recipes) | Concrete prompts and call sequences |
| [Rosetta Stone](https://github.com/ibrews/ECABridge/wiki/Rosetta-Stone) | The `dump_*` family — full JSON serialization of any UE5 asset |
| [MetaHuman Pipeline](https://github.com/ibrews/ECABridge/wiki/MetaHuman-Pipeline) | 22 commands for procedural MetaHuman creation, photo→character |
| [Engine Compatibility](https://github.com/ibrews/ECABridge/wiki/Engine-Compatibility) | UE 5.7 + 5.8 from one branch, deprecation backlog |
| [Coexistence with Epic's Native MCP](https://github.com/ibrews/ECABridge/wiki/Coexistence-with-Epic-MCP) | Running alongside the UE 5.8 `ModelContextProtocol` plugin, EDA panel |
| [Optional Dependencies](https://github.com/ibrews/ECABridge/wiki/Optional-Dependencies) | Structurally optional Mutable / MRQ / MetaHuman / Niagara / MetaSound / ControlRig / GAS |
| [Python Sandbox](https://github.com/ibrews/ECABridge/wiki/Python-Sandbox) | `execute_script` for multi-tool chaining in one MCP round trip |
| [What's New](https://github.com/ibrews/ECABridge/wiki/Whats-New) | Recent changes |

## What's inside (the +layer Epic isn't shipping)

These are the things we maintain ourselves because Epic's core toolsets don't cover them:

- **12 Rosetta Stone commands** — full JSON dumps of assets, blueprints, levels, materials, Niagara systems, sequencer, widgets, animation, MetaSound, DataTables, PCG, Control Rig, landscape
- **22 MetaHuman commands** — end-to-end procedural pipeline: cloud texture/rig, groom attachment, outfit tinting, body constraints, makeup
- **15 Mutable commands** — Customizable Object graphs + runtime parameter control
- **15 MetaSound commands** — graph authoring, node introspection, parameter routing
- **Movie Render Queue / Graph** (MRG 5.8-gated) — deployment-grade rendering
- **14 Sequencer commands** — cinematic creation, keyframes, camera control, dumps
- **36 UMG / Widget Tree / MVVM commands** — widget authoring (type-specific + polymorphic `add_widget`), hierarchy dumps, named-slot ops, tree mutation (move/rename/reparent), ViewModel binding, compile
- **Source control (13 commands)** — pre-submit validation, dry-run reverts, landscape locking
- **Performance / diagnostics (19 commands)** — Insights traces, stat groups, CVar profiles, memory snapshots, frame captures, diagnostic bundles
- **PCG (10 commands)** — authoring, runtime, data inspection, settings asset, full graph dump
- **Virtual production / Stage (18 commands)** — DMX library + patch authoring, LiveLink, nDisplay cluster lifecycle + ICVFX inner-frustum, OpenXR session lifecycle, USD, Stage Actor
- **World Partition mutation** — `force_load_wp_region`, `pin_wp_actors`, `unpin_wp_actors`
- **Refactoring safety nets** — `replace_asset_references`, `bulk_rename_assets`, dry-run modes
- **Blueprint graph editing (35 commands)** — `batch_edit_blueprint_nodes` creates and wires multiple nodes atomically; `add_blueprint_variable_get/set` with optional `variable_class` enables cross-BP variable access (read/write a variable on a cast object from a different Blueprint); `add_blueprint_flow_control_node`, `connect_blueprint_nodes`, `break_pin_connection`, `auto_layout_blueprint_graph`

## What we've absorbed from native MCP (clean-room)

These are features Epic shipped to `ue5-main` that we've ported under MIT so launcher users get them without compiling from source:

- **MCP 2025-03-26 protocol completeness** — per-session state via `Mcp-Session-Id`, request cancellation, streaming `notifications/progress` SSE events, `resources/list` + `resources/read`, `notifications/tools/list_changed` (Batch E, 2026-05-20)
- **Auto-config writer** for 5 MCP clients via `ECABridge.GenerateClientConfig <client|All>` (Batch T, 2026-05-20)
- **Physics asset (PhAT-style) authoring (17 commands)** — `create_physics_asset_from_mesh`, body/shape/constraint CRUD with upsert semantics (sphere/capsule/box), `set_body_physics_mode`, `set_body_mass_scale`, constraint limits via swing1/swing2/twist motions (Batch X, 2026-05-20)
- **UMG verb convergence (9 commands)** — polymorphic `add_widget` complementing our type-specific add verbs, named-slot ops, tree mutation (move/rename/reparent), `set_widget_as_variable`, `compile_widget_blueprint` with FCompilerResultsLog capture (Batch W, 2026-05-20)
- **Niagara API convergence (23 commands)** — schemas (system/emitter/renderer/data-interface/module-from-asset), topology getters (system/script-stack/emitter/module/stack-input), partial-update setters, user variable CRUD, compile-state + stack-issues diagnostics. `apply_niagara_stack_issue_fix` is scaffold-only (needs `NiagaraStackEditor` UI module for full impl) — flagged for follow-up (Batch V, 2026-05-20)
- **Schema-in-error responses** — every validation failure returns the full input JSON Schema inline so LLMs can self-correct
- **Inline base64 PNG screenshots** — no file I/O required
- **Lazy registration** (`load_category`, `list_categories`, `describe_category`) with payload caps and continuation tokens
- **Python sandbox** — `execute_script` chains N MCP calls in one round-trip; mirrors Epic's `ProgrammaticToolset`

## Engine compatibility

Single branch supports UE 5.7 and 5.8. Cross-version API divergences are handled with `UE_VERSION_OLDER_THAN(5,8,0)` guards.

- **Verified on Fort 2026-05-19:** clean UAT `BuildPlugin`, loads in a 5.8 project, server starts on `:3000`, 500+ commands available with `Mutable`+`MovieRenderPipeline` enabled, build correctly omits the optional-dep commands when the upstream plugins aren't reachable, runs side-by-side with Epic's native `ModelContextProtocol` plugin on `:8000`.
- **Verified on Theseus 2026-05-20:** clean `Test57Editor` + `Test58Editor` builds against both engines from the same `main` branch. UE 5.7 editor launches with `:3000` reporting `commands: 489, bridge_ready: true, sessions: 0`. `resources/list` returns a well-formed (empty for an empty test project) response. `ECABridge.GenerateClientConfig All` writes all 5 client configs with correct per-client schemas.
- **Verified on alex-mbp (macOS) 2026-05-20:** clean build against UE 5.7 from a standard Epic Games Launcher install (Apple M1 Max). Optional plugins without Mac binaries (nDisplay, etc.) are now correctly detected and skipped via `EngineHasPluginWithBinaries()` — `EngineHasPlugin()` alone returned true for source-only plugins causing linker failures. `PublicDelayLoadDLLs` entries are now Mac-guarded via `AddDelayLoadDLL()` wrapper; on Mac, module linking is handled by UBT automatically through `PrivateDependencyModuleNames`.

## Requirements

- Unreal Engine 5.7 or 5.8 (installed via Epic Games Launcher — no source build required)
- Visual Studio 2022 (for building the plugin)
- For Mutable commands: `Mutable` plugin enabled
- For MetaHuman commands: `MetaHumanCharacter` plugin enabled

## Strategy & cadence (for contributors)

ECABridge is built around an **"always +" model** — we lag Epic's `ue5-main` by ~1-2 weeks and stay current by clean-room porting changes from a behavioral spec (never copying Epic source). The mechanics:

1. A weekly watcher (`automation/native-mcp-watcher/` in our internal KB) diffs `ue5-main` paths for MCP and toolsets
2. Each commit gets triaged: **port** (clean-room implement), **mirror-API** (internal pattern only), or **ignore**
3. Port decisions become ECABridge issues or batches
4. Quarterly we audit whether we're still ahead of Epic's launcher cadence

Contributors: please read [CONTRIBUTING.md](CONTRIBUTING.md) for the clean-room discipline (no Epic source in this repo, ever — APIs only, written from English specs).

## License

MIT. Note: clean-room interop with Epic's native MCP plugin and the UE engine itself. No Epic source is included in this repository.
