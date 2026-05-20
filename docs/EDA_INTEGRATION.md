# EDA (Epic Developer Assistant) integration

UE 5.8 ships an AI assistant panel called the **Epic Developer Assistant** (EDA), powered by the `ModelContextProtocol` plugin. EDA talks to MCP toolset servers — Epic's built-in `:8000` server out of the box, plus any third-party server you register, including ECABridge.

This is the full walkthrough: register ECABridge with EDA, verify the connection, run a few canonical prompts, and combine ECABridge with Epic's native toolsets in the same panel.

---

## 1. Prerequisites

| | |
|---|---|
| UE version | 5.8 Preview or newer (EDA does not exist on 5.7) |
| Plugins enabled | `ModelContextProtocol` (Epic), `ECABridge` (this plugin) |
| Native MCP port | `:8000` (`http://127.0.0.1:8000/mcp`) — auto-started by Epic's plugin |
| ECABridge port | `:3000` (`http://127.0.0.1:3000/mcp`) — auto-started by this plugin |

Confirm both are reachable:

```bash
curl -s http://127.0.0.1:8000/health       # Epic native
curl -s http://127.0.0.1:3000/health       # ECABridge — returns "commands": 402
```

---

## 2. Register ECABridge in `MCPToolsetSettings.json`

EDA reads MCP toolset servers from `Saved/Config/<Platform>/MCPToolsetSettings.json` in your project (or `Config/DefaultMCPToolsetSettings.json` for source builds that want the registration version-controlled).

### Minimum entry

```json
{
  "Toolsets": [
    {
      "Name": "ECABridge",
      "ServerUrl": "http://127.0.0.1:3000/mcp",
      "Transport": "StreamableHTTP"
    }
  ]
}
```

| Field | Value | Notes |
|---|---|---|
| `Name` | `ECABridge` | Free-form label shown in the EDA tool picker. |
| `ServerUrl` | `http://127.0.0.1:3000/mcp` | ECABridge's MCP endpoint. Change the port if you've overridden it in **Project Settings → Plugins → ECABridge**. |
| `Transport` | `StreamableHTTP` | ECABridge uses Streamable HTTP — POST `/mcp` with `Accept: application/json, text/event-stream`. |

Save the file. Restart the editor (or reload EDA via the panel's gear menu if your build supports hot-reload).

### Path reference

| Build type | Config path |
|---|---|
| Packaged project / installed engine | `<Project>/Saved/Config/Windows/MCPToolsetSettings.json` |
| Source build | `<Project>/Config/DefaultMCPToolsetSettings.json` (committed) |

---

## 3. Verify the registration

Open the EDA panel: **Window → Epic Developer Assistant** (or the AI button on the main toolbar in newer 5.8 builds).

1. Click the **Toolsets** gear (top-right of the EDA panel).
2. ECABridge should appear in the list alongside Epic's native toolsets (Actor, Asset, Blueprint, Mesh, Object, Primitive, Programmatic, Scene, Texture, DataTable).
3. Each ECABridge command shows up under its registered category — about 400 entries total. Expand `ECABridge / Asset` and you'll see `dump_asset`, `find_assets`, `bulk_rename_assets`, etc.

If the entry doesn't appear:

- Check the **Output Log** for `LogModelContextProtocol` warnings — connection errors land there with the raw HTTP response.
- Confirm ECABridge is actually listening: `curl -s http://127.0.0.1:3000/health` should return JSON.
- File path typos in `MCPToolsetSettings.json` silently produce an empty toolset list — re-validate JSON.

---

## 4. Canonical prompts to try

Once EDA sees ECABridge, the following prompts exercise the most distinctive parts of the surface. Each one is the kind of thing the native toolsets can't do (or do less completely).

### a. Rosetta Stone — dump an asset to JSON

> *"Dump the blueprint at `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` so I can see its components, variables, and event graph nodes."*

EDA picks `dump_blueprint_graph`. The JSON includes 4 graphs, variables, all components, and every node with its connections. Useful for asking the agent to *reason about* a blueprint before making changes.

### b. MetaHuman creation

> *"Create a new MetaHuman, set the body type to athletic, attach the long-hair groom preset, and tint the outfit teal."*

EDA chains `create_metahuman` → `set_metahuman_body_type` → `attach_metahuman_groom` → `tint_metahuman_outfit`. All four are ECABridge-only — the native toolset doesn't touch MetaHuman.

### c. Python sandbox

> *"Use the Python sandbox to find every actor in the current level whose class name starts with `BP_`, then return their world transforms."*

EDA hits `execute_script` with a multi-line script:

```python
def run():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    out = []
    for a in actors:
        cls = a.get_class().get_name()
        if cls.startswith("BP_"):
            t = a.get_actor_location()
            out.append({"name": a.get_actor_label(), "class": cls,
                        "x": t.x, "y": t.y, "z": t.z})
    return out
```

Result comes back as a single JSON response with `result.content[0].text` containing the JSON-serialized return value of `run()`.

### d. Validation-error self-correction

> *"Set the MetaHuman's body type to `xyzzy`."*

ECABridge returns `isError: true` with the full JSON Schema for the `body_type` parameter inline. EDA reads the schema, picks one of the valid enum values, and retries automatically. This is the schema-in-error-response behavior — designed precisely for this self-correction loop.

### e. Combined call (cross-server)

> *"Find every `StaticMeshActor` in the level [native MCP], then use ECABridge to dump the material slots of the first three."*

EDA picks `find_actors` from Epic's native toolset, then `get_actor_materials` from ECABridge for each. Demonstrates the two servers cooperating — the agent doesn't have to know which one owns which capability.

---

## 5. Coexistence with native UE 5.8 MCP

Both plugins run side-by-side. EDA can target either or both. Register both:

```json
{
  "Toolsets": [
    { "Name": "EpicNative", "ServerUrl": "http://127.0.0.1:8000/mcp", "Transport": "StreamableHTTP" },
    { "Name": "ECABridge", "ServerUrl": "http://127.0.0.1:3000/mcp", "Transport": "StreamableHTTP" }
  ]
}
```

### Who owns what

| Capability | Native (`:8000`) | ECABridge (`:3000`) |
|---|---|---|
| Actor spawn / transform | yes | yes (more variants — `clone_actor_array`, `scatter_actors_on_surface`, etc.) |
| Asset find / rename / move | yes | yes (plus `bulk_rename_assets`, `replace_asset_references`) |
| Blueprint inspection | partial | full Rosetta Stone (`dump_blueprint_graph` returns every graph, variable, node) |
| MetaHuman pipeline | no | 22 commands, end-to-end |
| Mutable | no | 16 commands |
| Niagara | no | full system + emitter + module surface |
| MetaSound | no | yes |
| MVVM / UMG | no | yes |
| Sequencer | partial | full |
| PCG | no | yes (Batch P) |
| Python sandbox | yes | yes — both expose `execute_script` |
| Screenshots | yes | yes (ECABridge returns inline base64) |
| Source control | no | yes (Batch R) |

There's no port collision and no toolset-name collision (ECABridge prefixes its categories; Epic's are bare names). If both servers expose a command with the same name (e.g. both have `find_assets`), EDA uses the `Toolset` prefix in the call to disambiguate — the agent picks one based on the prompt.

---

## 6. Cross-server smoke check

A one-liner to verify both servers are answering MCP correctly:

```bash
for port in 3000 8000; do
  echo "=== :$port ==="
  curl -s -X POST "http://127.0.0.1:$port/mcp" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -d '{"jsonrpc":"2.0","method":"tools/list","id":1,"params":{}}' \
  | head -c 500
  echo
done
```

Each port should return a JSON object with a `result.tools[]` array. If one of them returns an HTML error page or `Connection refused`, that plugin isn't loaded.

For a more thorough cross-server check, `scripts/smoke-test.py --include-native` runs the full ECABridge smoke test then probes the native MCP server's `tools/list` and asserts that the two tool-name sets are disjoint (or reports the overlap that EDA must disambiguate via toolset prefix):

```bash
python scripts/smoke-test.py --include-native
# ...full smoke test against :3000...
# --- cross-server probe: native MCP at http://127.0.0.1:8000/mcp ---
#   ok   native: tools/list returned N tools
#   ok   native + ECABridge expose disjoint tool name sets
```

The native probe is soft-skipped (exit code 0) when the native plugin isn't loaded, so the same command works on engines with or without it enabled.

---

## 6a. Schema conventions (Batch K — convergence with native)

Where ECABridge and the native `ModelContextProtocol` plugin overlap, we try to
emit schemas in the same shape so EDA renders results consistently regardless
of which server answered. The conventions:

**Input schemas (`inputSchema` on `tools/list`)**

- `$schema: http://json-schema.org/draft-07/schema#` on every tool.
- `type: "object"` with `additionalProperties: false` — unknown args are a
  client error, not a silent drop.
- `title` at the top level (TitleCased command name) and on every property.
- `description` at the top level (= the command's `GetDescription()`) and on
  every property.
- `required: [...]` for non-optional args, omitted entirely when no args are
  required (vs. an empty array).

**Output schemas (`outputSchema` on `tools/list`)**

- `type: "object"` with `properties: {...}`. We do not currently advertise
  `required` on output schemas — the agent should treat any absent field as
  "not present in this response" rather than an error.
- UObject references are nested objects with the shape
  `{path: string<uobject-path>, class: string}`. Build via
  `MakeECAObjectRefSchema(Description)`.
- Asset path strings (the kind a command takes as an arg) use
  `type: "string", format: "uobject-path"`. Build via
  `MakeECAAssetPathSchema(Description)`.
- Error structures travel via JSON-RPC `isError: true` + a text content block
  whose payload is the validation message + embedded input schema. We do not
  use `outputSchema` for error shapes.

**Divergences from native we intentionally keep**

- ECABridge attaches an `example` field on each tool (sourced from
  `Resources/command-examples.json`). Native does not. EDA renders it in the
  tool inspector; clients that don't understand the field ignore it per the
  JSON Schema additionalProperties rule on the *tool descriptor*, which is
  open (only the *inputSchema* is strict).
- ECABridge uses MCP `notifications/tools/list_changed` when the lazy registry
  loads a category. Native ships its full surface up front.

---

## 7. Customizing ECABridge for EDA

Two settings in **Project Settings → Plugins → ECABridge** matter for EDA integration:

| Setting | Default | When to change |
|---|---|---|
| MCP port | `3000` | If `:3000` is taken on your machine. Update both the project setting and `MCPToolsetSettings.json`. |
| Auto-start server | enabled | Leave on for editor sessions. Disable only for CI / headless builds. |

ECABridge does not currently support per-toolset filtering from EDA's side — the entire ~400-command surface is exposed. Batch C (lazy registration, on the forward roadmap) will reduce the baseline `tools/list` size; until then the full surface is what EDA sees on first connect.

---

## 8. What to read next

- [`docs/vscode/README.md`](vscode/README.md) — same plugin via VS Code's MCP UI
- [`docs/cursor/README.md`](cursor/README.md) — same plugin via Cursor
- [ue5-mcp](https://github.com/ibrews/ue5-mcp) — Claude Code / Cowork skill with the hard-won field-manual knowledge for using these tools without crashing the editor
- ECABridge wiki [Coexistence with Epic's Native MCP](https://github.com/ibrews/ECABridge/wiki/Coexistence-with-Epic-MCP) — the deeper compatibility notes
