# ECABridge — AI-Powered Unreal Engine 5 MCP Plugin

340+ MCP tools for UE5 editor automation via Claude, ChatGPT, or any MCP-compatible AI agent.

## Features

- **340+ MCP tools** organized by category
- **12 Rosetta Stone commands** — full JSON dumps of assets, blueprints, levels, materials, Niagara, sequencer, widgets, animation, MetaSound, and DataTables
- **HTTP/SSE MCP server** on localhost:3000 (Streamable HTTP transport)
- **UE 5.7 compatible** (built and tested against 5.7.4)
- **No engine modifications** — drop-in plugin

## Rosetta Stone: Deep Introspection

LLMs can't read binary `.uasset` or `.umap` files. These commands serialize UE5 assets into structured JSON, making any asset fully legible in a single MCP call:

| Command | What it does |
|---------|-------------|
| `dump_asset` | Full JSON of any asset — all UPROPERTYs, sub-objects, references, metadata, optional thumbnail |
| `dump_blueprint_graph` | Complete Blueprint — all graphs, nodes, pins, connections, variables, components |
| `dump_level` | Full level state — all actors with transforms, components, tags. Lightweight or deep mode |
| `find_assets` | Search the asset registry by class, path, or name wildcard |
| `get_asset_references` | Dependency graph — what references what, with recursive depth |
| `dump_material_graph` | Complete material — all expression nodes, connections, material properties, compilation errors |
| `dump_niagara_system` | Full Niagara system — emitter stacks per stage, renderers, module inputs, parameters |
| `dump_level_sequence` | Complete sequencer — all bindings, tracks, sections, keyframe channel data |
| `dump_widget_tree` | Hierarchical widget tree — parent-child structure, slot properties, visibility |
| `dump_animation_blueprint` | AnimBP — state machines, states, transitions, AnimGraph nodes, variables |
| `dump_metasound_graph` | Full MetaSound — all nodes, pins, connections, source inputs/outputs |
| `dump_datatable` | Complete DataTable — schema with types + all row data in one call |

```bash
# See everything about a material
dump_asset(asset_path="/Game/Materials/M_Hero")

# Search for all blueprints in a folder
find_assets(class_filter="Blueprint", path_filter="/Game/Blueprints/")

# Read the full node graph of a blueprint
dump_blueprint_graph(blueprint_path="/Game/Blueprints/BP_Enemy")

# Get all actors in the current level (lightweight mode)
dump_level(max_actors=100)

# What would break if I changed this texture?
get_asset_references(asset_path="/Game/Textures/T_Hero_D", direction="referencers")

# Full Niagara system with module stacks and renderers
dump_niagara_system(system_path="/Game/Effects/NS_Fire")

# Read a complete sequencer timeline with keyframes
dump_level_sequence(sequence_path="/Game/Cinematics/MySeq")

# Inspect a widget tree hierarchy
dump_widget_tree(widget_path="/Game/UI/WBP_MainMenu")
```

## Command Categories

| Category | Commands | Highlights |
|----------|----------|------------|
| **Actor** | 35+ | Spawn, delete, transform, find, duplicate, select, describe, hierarchy, tags, folders, visibility, mobility |
| **Blueprint** | 25+ | Create, compile, add nodes/variables/components, wire graphs, Blueprint Lisp DSL |
| **Blueprint Node** | 34+ | Events, functions, macros, casts, flow control, batch ops, auto-layout, **full graph dump** |
| **Material** | 16+ | Create materials/instances, edit node graphs, set parameters |
| **Material Node** | 20+ | Add/delete/connect nodes, set properties, auto-layout, error checking |
| **Mesh** | 40+ | Primitives, booleans, extrude, subdivide, simplify, UVs, import/export |
| **Niagara** | 20+ | Create systems/emitters, add modules/renderers, set parameters |
| **MetaSound** | 15+ | Create sources, add/connect nodes, set inputs, preview |
| **MVVM** | 7 | ViewModel binding, text/visibility/image bindings |
| **Widget/UMG** | 15+ | Create widgets, add text/buttons/images, bind events |
| **Mutable/CO** | 16 | Character customization: create/edit CO graphs, compile, runtime params, spawn actors, MetaHuman creation |
| **Sequencer** | 8 | Create sequences, bind actors, keyframe transforms, spawn cameras, camera properties (FOV/aperture), float key animation |
| **Animation** | 6 | Play/stop animations, list compatible anims, set AnimBP, skeleton info, **create animations programmatically from bone keyframes** |
| **Lighting** | 4 | Set light properties, get light info, one-command 3-point rig, post-process settings |
| **Movie Render Queue** | 2 | Render sequences to PNG/JPG/MP4 with configurable resolution, check render progress |
| **Environment/PCG** | 28+ | Generate grids/circles, walls, fog, sky, splines, gravity, batch spawn/set/destroy, physics simulation, impulse, visibility, teleport, scene stats, camera screenshots, decals, 3D text, describe actor, clone arrays, audio, triggers, align/distribute, scene snapshots, time dilation, measurements, scatter |
| **AI/Navigation** | 4 | Build navmesh, find paths, move actors, navmesh info |
| **Data Table** | 4 | Schema, rows, CRUD |
| **Editor** | 10+ | Viewport control, screenshots, PIE, console commands, save/open levels, **full level dump** |
| **View** | 3 | Camera control, scene description, frustum queries |
| **Asset** | 15+ | Import/export, create materials, thumbnails, **dump_asset, find_assets, get_asset_references** |
| **Component** | 5 | Properties, transforms, physics, static mesh |
| **Project** | 3+ | Settings, input mappings |

## Quick Start

1. Copy the plugin folder to your project's `Plugins/` directory
2. Add to your `.uproject`:
   ```json
   {"Name": "ECABridge", "Enabled": true}
   ```
3. Build and launch the editor
4. ECABridge starts automatically on `localhost:3000`

### Claude Desktop Config

```json
{
  "mcpServers": {
    "ue5": {
      "command": "npx",
      "args": ["-y", "mcp-remote", "http://localhost:3000/mcp"]
    }
  }
}
```

### Test with curl

```bash
# Initialize MCP session
curl -X POST http://localhost:3000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'

# List all tools
curl -X POST http://localhost:3000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'

# Spawn a cube
curl -X POST http://localhost:3000/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"create_actor","arguments":{"actor_type":"StaticMeshActor","name":"MyCube","location":{"x":0,"y":0,"z":100},"mesh":"/Engine/BasicShapes/Cube"}}}'
```

## Mutable Character Customization

The plugin includes 15 commands for Epic's Mutable (Customizable Object) system, enabling AI-driven character creation:

```bash
# Create a customizable object
create_co → add_co_node → set_co_node_property → connect_co_nodes → compile_co

# Modify characters at runtime
set_co_instance_param(actor_name="CharacterActor", param_name="Body Type 1", param_value="Strong")
set_co_instance_param(actor_name="CharacterActor", param_name="Skin Tone", param_value=0.5)
set_co_instance_param(actor_name="CharacterActor", param_name="Head Accessories", param_value="Helmet")
```

## Level Sequencer

6 commands for cinematic creation:

```bash
create_level_sequence → add_sequence_camera → add_sequence_transform_key → play_sequence
```

## Requirements

- Unreal Engine 5.7+
- Visual Studio 2022 (for building)
- For Mutable commands: `Mutable` plugin enabled
- For MetaHuman commands: `MetaHumanCharacter` plugin enabled

## License

Internal use — Agile Lens / ibrews
