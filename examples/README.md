# Examples

Drop-in demo projects for trying ECABridge in under a minute.

## HelloECA — the 60-second demo

A minimal UE 5.8 project that has ECABridge wired up and nothing else. Open it, wait for the editor to load, and the MCP server is live on `:3000`. Perfect for verifying a fresh install, running smoke tests, or showing a teammate what the plugin does.

### Setup

```bash
# Clone the ECABridge repo
git clone https://github.com/ibrews/ECABridge.git
cd ECABridge

# Symlink (or copy) the plugin into HelloECA's Plugins/ folder
mkdir -p examples/HelloECA/Plugins

# Windows (PowerShell, as admin or developer mode):
New-Item -ItemType SymbolicLink -Path examples\HelloECA\Plugins\ECABridge -Target $PWD

# macOS / Linux:
ln -s "$(pwd)" examples/HelloECA/Plugins/ECABridge
```

The symlink approach means HelloECA always points at the live plugin source — edit the plugin, rebuild, and HelloECA picks it up. If you'd rather copy:

```bash
cp -r . examples/HelloECA/Plugins/ECABridge
# (then ignore Binaries/ and Intermediate/ — they're rebuilt per project)
```

### Launching

```bash
# Windows
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" examples\HelloECA\HelloECA.uproject

# macOS
/Applications/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor examples/HelloECA/HelloECA.uproject
```

First launch will prompt to compile ECABridge — say yes. ~60-90s on a warm cache.

### Verifying

While the editor is loading, in another shell:

```bash
# Wait for the MCP server to come up
while ! curl -sf http://127.0.0.1:3000/health >/dev/null; do sleep 1; done
echo "ECABridge is live."

# List tools — should be ~340+ (more if you enabled Mutable/MetaHuman/etc.)
curl -s -X POST http://127.0.0.1:3000/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"tools/list","id":1,"params":{}}' \
  | head -c 500
```

### What's in HelloECA

| | |
|---|---|
| Engine | UE 5.8 (set in `HelloECA.uproject` `EngineAssociation`) |
| Map | `/Engine/Maps/Templates/Template_Default` (Epic's empty template — no custom content) |
| Plugins enabled | `ECABridge`, `PythonScriptPlugin`, `EditorScriptingUtilities`, `ModelViewViewModel` |
| Content/ | empty — add your own to play with |

The intent is **zero distraction**: the project is the bare minimum required to load ECABridge cleanly. No custom assets to confuse a new user, no template gameplay to wade through. Just the plugin and the empty world.

### Recommended next steps

1. Open the Output Log (Window → Output Log) and filter for `ECABridge` — you'll see the server bind to `:3000`.
2. From an MCP client (Claude Desktop, Cursor, VS Code, or the EDA panel), register `http://127.0.0.1:3000/mcp` — see [`../docs/vscode/README.md`](../docs/vscode/README.md), [`../docs/cursor/README.md`](../docs/cursor/README.md), or [`../docs/EDA_INTEGRATION.md`](../docs/EDA_INTEGRATION.md).
3. Ask the agent: *"Find all actors in this level."* — it should call `find_actors` and return Epic's default `WorldDataLayers`, `WorldPartition`, etc.
4. Drop a static mesh into the level, then ask: *"Dump the actor list with their world transforms."*

### Why ship this?

Without a known-good demo project, new users have to figure out:

- Which UE version (the plugin supports 5.7 + 5.8)
- Which baseline plugins to enable (`PythonScriptPlugin` is required for the sandbox)
- How to wire the Plugins/ folder up
- What MCP server URL to register

HelloECA collapses all four into "open this `.uproject`". The friction-to-first-tool-call drops from ~30 minutes to ~3 minutes (most of which is the first plugin compile).

### Coexistence with UE 5.7

The uproject's `EngineAssociation` is `5.8`. To open it on 5.7:

```bash
# Right-click the .uproject in Explorer → "Switch Unreal Engine version" → pick 5.7
# Or edit EngineAssociation in the .uproject directly.
```

ECABridge supports both engines from the same plugin source.
