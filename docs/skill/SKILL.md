---
name: ue5-mcp
description: Field manual for using ECABridge MCP tools against Unreal Engine 5.7/5.8 without crashing the editor. Covers schema-in-error self-correction, Python sandbox chaining, optional-dependency gating, output schemas, and the ~50 known-good call patterns that have survived real-world use.
version: 2.0.0
date: 2026-05-19
plugin: ECABridge (github.com/ibrews/ECABridge)
plugin_version_min: af30649
---

# ue5-mcp — Field manual for ECABridge

ECABridge exposes ~400 MCP tools over Streamable HTTP on `:3000/mcp`. This skill is the survival guide an LLM needs to use them without faceplanting on UE's silent-fail edges.

> **Skill major version 2.0** — refreshed for ECABridge `af30649` (Batches L/M/N/O/P/Q/R shipped). Breaking changes vs 1.x: schema-in-error response format, Python sandbox returns `run()` directly, optional-dep gating means some commands are missing on minimal projects, `outputSchema` is now authoritative for response shape.

---

## 1. Connection — the only setup you need

```bash
curl -s -X POST http://127.0.0.1:3000/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"tools/list","id":1,"params":{}}'
```

Required headers — **omitting `Accept: application/json, text/event-stream` returns 406**. ECABridge enforces this on every POST `/mcp`. Most MCP clients (Claude Desktop, Cursor, VS Code, EDA) set it automatically; raw curl does not.

Health probe: `GET /health` returns `{ "commands": N, ... }` where N is the live command count. Use it to confirm the plugin loaded.

---

## 2. Schema-in-error self-correction

**Every validation error includes the tool's full JSON Schema inline.** If you pass a bad argument, the response is:

```json
{
  "jsonrpc": "2.0",
  "id": 17,
  "result": {
    "isError": true,
    "content": [{
      "type": "text",
      "text": "Validation failed for argument 'body_type'. Expected one of [Skinny, Athletic, Heavyset]. Schema follows.\n\n<full inputSchema JSON here>"
    }]
  }
}
```

**Loop:** read the schema → fix the argument → retry. Do not give up after one validation error; the schema tells you exactly what shape is expected. This applies to all ~548 validation sites in ECABridge.

---

## 3. Output schemas — trust them

Every `dump_*`, `find_*`, `get_*`, and many `list_*` commands now ship an `outputSchema` in `tools/list`. The schema is the authoritative response shape — fields not in the schema may exist (additive) but fields IN the schema are guaranteed.

```json
{
  "name": "dump_blueprint_graph",
  "inputSchema": { ... },
  "outputSchema": {
    "type": "object",
    "properties": {
      "blueprint_path": { "type": "string" },
      "graphs": { "type": "array", "items": { ... } },
      "variables": { "type": "array", "items": { ... } },
      "components": { "type": "array", "items": { ... } }
    },
    "required": ["blueprint_path", "graphs"]
  }
}
```

**Implication for skills:** when chaining tools, parse the upstream response using its `outputSchema` rather than guessing field names. Schemas are loaded with `tools/list` once at startup — cache them.

---

## 4. Python sandbox — chained calls in one round-trip

`execute_script` runs Python inside the editor with access to `unreal.*` and ECABridge's command registry. **The return value of `run()` becomes the tool result** — no more `print(json.dumps(...))` ceremony.

```python
# Submit this as the 'script' argument to execute_script:
def run():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    return [
        {
            "name": a.get_actor_label(),
            "class": a.get_class().get_name(),
            "location": [a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z],
        }
        for a in actors
        if a.get_class().get_name().startswith("BP_")
    ]
```

Response shape:
```json
{
  "isError": false,
  "content": [{
    "type": "text",
    "text": "<JSON-serialized return value of run()>"
  }]
}
```

**Backwards-compat:** if `run()` is not defined or returns `None`, ECABridge falls back to capturing `print()` output in `log_output`. Skills written for the 1.x convention keep working; new skills should use the return-value form.

**When to reach for the sandbox:** any time you'd otherwise issue 3+ MCP calls in sequence where each depends on the prior result. One Python call → one round-trip → faster, cheaper, no race conditions.

**When NOT to:** single-tool calls (overhead isn't worth it). Long-running operations the agent might want to cancel — those should be plain tool calls so MCP cancellation works.

See `get_execution_environment` for the live list of available `unreal.*` modules and ECABridge command bindings in the sandbox namespace.

---

## 5. Optional-dependency gating — which commands exist on which project

ECABridge structurally gates commands behind their engine plugins. On a minimal project, the following categories may be absent from `tools/list`:

| Category | Required engine plugin | `WITH_ECA_*` macro |
|---|---|---|
| Mutable | `Mutable` (Customizable Objects) | `WITH_ECA_MUTABLE` |
| MovieRenderQueue / MovieRenderGraph | `MovieRenderPipeline` | `WITH_ECA_MRQ`, `WITH_ECA_MRG` |
| MetaHuman | `MetaHumanCharacter` | `WITH_ECA_METAHUMAN` |
| Niagara | `Niagara` (usually on by default) | `WITH_ECA_NIAGARA` |
| MetaSound | `MetaSound` | `WITH_ECA_METASOUND` |
| ControlRig | `ControlRig` | `WITH_ECA_CONTROLRIG` |
| GameplayAbilities | `GameplayAbilities` (GAS) | `WITH_ECA_GAS` |
| USD import/export | `USDImporter` | `WITH_ECA_USD` |
| nDisplay | `nDisplay` | `WITH_ECA_NDISPLAY` |
| DMX | `DMXEngine` | `WITH_ECA_DMX` |
| Data validation | `DataValidation` | `WITH_ECA_DATAVALIDATION` |
| RenderDoc capture | `RenderDocPlugin` | `WITH_ECA_RENDERDOC` |

**Skills should not assume any optional command exists.** Probe `tools/list` first, or wrap the call in a try-catch and degrade gracefully. ECABridge's `DelayLoad` + `UnregisterByCategory` machinery (commit `35feb94`) ensures the plugin loads cleanly even when an upstream plugin is missing — the command simply isn't registered.

**Command-count baseline (2026-05-19, FirstPerson template):**
- UE 5.7 minimal: ~340 commands
- UE 5.7 + Mutable + MRQ + MetaHuman + Niagara + MetaSound: ~376
- UE 5.8 full: ~462 (5.8-only commands: MovieRenderGraph, some PathTracer, some PCG)

---

## 6. Image / screenshot content blocks

Five commands return MCP `image` content blocks instead of `text`:

| Command | Returns |
|---|---|
| `take_camera_screenshot` | Editor viewport snapshot |
| `take_gameplay_screenshot` | Active PIE session capture |
| `take_depth_screenshot` | Depth buffer as 8-bit PNG |
| `take_metahuman_editor_screenshot` | MetaHuman editor viewport |
| `take_screenshots_sweep` | Multi-angle batch (array of images) |

Response shape:
```json
{
  "content": [{
    "type": "image",
    "data": "<base64-encoded PNG>",
    "mimeType": "image/png"
  }]
}
```

Inline base64 — no file I/O. Decoded bytes start with `\x89PNG\r\n\x1a\n`. Most MCP clients render the image directly in the chat UI.

---

## 7. The 5.7 vs 5.8 surface

A single ECABridge branch supports both engine versions via `UE_VERSION_OLDER_THAN(5,8,0)` guards. Five things to know:

1. **MovieRenderGraph** (`create_mrg_graph`, `add_mrg_node`, `connect_mrg_nodes`, `execute_mrg_graph`) is 5.8-only. Tools/list omits them on 5.7.
2. **Some PathTracer settings** are 5.8-only on the C++ surface; the wrapper command falls back to 5.7-compatible CVar manipulation.
3. **`FJsonObject::Values`** key type differs (`FString` on 5.7, `UE::FSharedString` on 5.8). Affects raw native code, not skills.
4. **Native MCP** (`ModelContextProtocol` plugin) is 5.8-only. ECABridge works on both.
5. **Output is identical across engines** — same command names, same JSON shapes, except where a command is genuinely missing on 5.7.

---

## 8. Known-good call patterns

These have survived months of real-world use. Use them as templates.

### a. Find all `StaticMeshActor`s, dump materials of the first 5

```
1. find_actors(class_filter="StaticMeshActor")
   → returns array of {name, path, location, ...}
2. For first 5: get_actor_materials(actor_path=<path>)
   → returns array of material slots with material asset paths
```

Or in one Python sandbox call:

```python
def run():
    actors = unreal.EditorActorSubsystem().get_all_level_actors()
    smas = [a for a in actors if a.get_class().get_name() == "StaticMeshActor"][:5]
    out = []
    for a in smas:
        sm_comp = a.static_mesh_component
        mats = sm_comp.get_materials()
        out.append({
            "name": a.get_actor_label(),
            "materials": [m.get_path_name() if m else None for m in mats],
        })
    return out
```

### b. Create a MetaHuman, customize, screenshot

```
1. create_metahuman(name="Aria", body_type="Athletic")
2. set_metahuman_face_preset(actor_path=<from step 1>, preset="default_face_01")
3. attach_metahuman_groom(actor_path=<step 1>, groom_path="/Game/Hair/long_brown")
4. tint_metahuman_outfit(actor_path=<step 1>, color={"r":0.1,"g":0.4,"b":0.5,"a":1})
5. take_metahuman_editor_screenshot()
   → inline PNG of the result
```

### c. Source-controlled bulk edit

```
1. find_assets(asset_class="Material", path_filter="/Game/Materials/Legacy/")
2. create_changelist(description="Bulk move legacy materials")
3. For each asset: check_out_asset(asset_path=<path>)
4. bulk_rename_assets(operations=[{from: "/Game/Materials/Legacy/X", to: "/Game/Materials/X"} ...])
5. validate_before_submit(changelist_id=<from step 2>)
6. submit_changelist(changelist_id=<step 2>)
```

### d. Niagara — spawn + tune emitter

```
1. create_niagara_system(name="Sparks", path="/Game/VFX/")
2. create_niagara_emitter(system_path=<step 1>, template="EmberShower")
3. set_niagara_emitter_property(emitter_path=<step 2>, property="SpawnRate", value=200)
4. spawn_niagara_effect(system_path=<step 1>, location={x:0,y:0,z:300})
```

### e. PCG graph authoring (Batch P)

```
1. create_pcg_graph(name="Trees", path="/Game/PCG/")
   → use add_pcg_node + connect_pcg_nodes to build
2. dump_pcg_graph(graph_path=<step 1>)
   → verify topology before execution
```

---

## 9. Anti-patterns — what NOT to do

- **Don't poll `tools/list` mid-session.** Tool surface is stable. Cache once on connect.
- **Don't pass actor labels as identifiers** unless the command explicitly accepts them. Use the actor's full path (`/Game/Maps/Lvl_FirstPerson.Lvl_FirstPerson:PersistentLevel.BP_FirstPersonCharacter_C_1`) for stable references.
- **Don't try to `execute_python` (deprecated)** — use `execute_script` with the `run()` convention instead.
- **Don't chain a command on an asset before `save_asset`** unless you're explicitly testing in-memory state — many `dump_*` commands serialize from disk.
- **Don't ignore `isError: true`.** The schema is in there; use it.
- **Don't assume `Mutable` / `MetaHuman` / `MRG` commands exist** on every project. Probe.

---

## 10. Quick reference — commands by category

(Counts approximate; varies by project plugin set.)

| Category | Count | Most-used |
|---|---|---|
| Actor | ~45 | `find_actors`, `create_actor`, `set_actor_transform`, `clone_actor_array`, `scatter_actors_on_surface` |
| Asset | ~35 | `find_assets`, `dump_asset`, `bulk_rename_assets`, `replace_asset_references`, `validate_asset` |
| Blueprint | ~40 | `dump_blueprint_graph`, `create_blueprint`, `add_blueprint_*_node`, `connect_blueprint_nodes` |
| Material | ~25 | `dump_material_graph`, `create_material`, `add_material_node`, `set_material_node_property` |
| MetaHuman | 22 | `create_metahuman`, `build_metahuman`, `attach_metahuman_groom`, `tint_metahuman_outfit` |
| Mutable | 16 | `create_co`, `add_co_node`, `compile_co`, `spawn_co_actor` |
| Niagara | ~20 | `create_niagara_system`, `add_niagara_emitter`, `spawn_niagara_effect` |
| MetaSound | ~15 | `create_metasound_source`, `add_metasound_node`, `preview_metasound` |
| Sequencer | ~15 | `create_level_sequence`, `add_sequence_*`, `play_sequence`, `render_sequence` |
| Widget / MVVM / UMG | ~30 | `create_umg_widget_blueprint`, `add_widget_element`, `bind_*_to_viewmodel` |
| PCG | ~20 | `create_pcg_graph`, `dump_pcg_graph`, `add_pcg_node` |
| Landscape / Mesh | ~30 | `create_mesh`, `mesh_*`, `create_landscape` |
| Source control | 11 | `check_out_asset`, `create_changelist`, `submit_changelist` |
| Observability | 8 | `start_insights_trace`, `dump_stat_values`, `capture_diagnostic_bundle` |
| Editor UX | ~20 | `get_cvar`, `set_cvar`, viewport bookmarks, workspace layouts |
| Rendering | ~25 | `set_post_process_setting`, `enable_path_tracer`, `set_sun_position` |
| Asset pipeline | ~15 | `import_usd`, `import_gltf`, `import_fbx_advanced`, `purge_ddc` |
| Stage / XR / nDisplay | ~10 | `get_xr_runtime_info`, `get_ndisplay_status`, `list_livelink_subjects` |

Run `tools/list` against your editor for the authoritative live count.

---

## 11. Version history

| Skill version | Date | Plugin baseline | Changes |
|---|---|---|---|
| 2.0.0 | 2026-05-19 | `af30649` (post-P) | Schema-in-error inline; Python sandbox return-value; optional-dep gating; output schemas; 5.7+5.8 single branch; ~80 new commands from Batches L–R |
| 1.x | (prior) | UE 5.7-only era | Original surface (~310 commands), `print()`-based sandbox, no optional gating |

---

## 12. Where to read more

- `docs/EDA_INTEGRATION.md` — Epic Developer Assistant panel walkthrough
- `docs/vscode/README.md` — VS Code MCP UI setup
- `docs/cursor/README.md` — Cursor MCP setup
- `docs/METAHUMAN.md` — deep dive on the MetaHuman pipeline
- ECABridge wiki — full Command Categories, Rosetta Stone, Recipes
