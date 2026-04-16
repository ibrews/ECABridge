# ECABridge — AI-Powered Unreal Engine 5 MCP Plugin

300+ MCP tools for UE5 editor automation via Claude, ChatGPT, or any MCP-compatible AI agent.

## Features

- **300+ MCP tools** organized by category
- **HTTP/SSE MCP server** on localhost:3000 (Streamable HTTP transport)
- **UE 5.7 compatible** (built and tested against 5.7.4)
- **No engine modifications** — drop-in plugin

## Command Categories

| Category | Commands | Highlights |
|----------|----------|------------|
| **Actor** | 8 | Spawn, delete, transform, find, duplicate, select |
| **Blueprint** | 25+ | Create, compile, add nodes/variables/components, wire graphs, Blueprint Lisp DSL |
| **Material** | 16+ | Create materials/instances, edit node graphs, set parameters |
| **Mesh** | 40+ | Primitives, booleans, extrude, subdivide, simplify, UVs, import/export |
| **Niagara** | 20+ | Create systems/emitters, add modules/renderers, set parameters |
| **MetaSound** | 15+ | Create sources, add/connect nodes, set inputs, preview |
| **MVVM** | 7 | ViewModel binding, text/visibility/image bindings |
| **Widget/UMG** | 15+ | Create widgets, add text/buttons/images, bind events |
| **Mutable/CO** | 16 | Character customization: create/edit CO graphs, compile, runtime params, spawn actors, MetaHuman creation |
| **Sequencer** | 8 | Create sequences, bind actors, keyframe transforms, spawn cameras, camera properties (FOV/aperture), float key animation |
| **Animation** | 6 | Play/stop animations, list compatible anims, set AnimBP, skeleton info, **create animations programmatically from bone keyframes** |
| **Lighting** | 4 | Set light properties, get light info, one-command 3-point rig, post-process settings |
| **Movie Render Queue** | 2 | Render sequences to PNG/JPG with configurable resolution, check render progress |
| **Environment/PCG** | 28+ | Generate grids/circles, walls, fog, sky, splines, gravity, batch spawn/set/destroy, physics simulation, impulse, visibility, teleport, scene stats, camera screenshots, decals, 3D text, describe actor, clone arrays, audio, triggers, align/distribute, scene snapshots, time dilation, measurements, scatter |
| **AI/Navigation** | 4 | Build navmesh, find paths, move actors, navmesh info |
| **Data Table** | 4 | Schema, rows, CRUD |
| **Editor** | 10+ | Viewport control, screenshots, PIE, console commands, save/open levels |
| **View** | 3 | Camera control, scene description, frustum queries |
| **Asset** | 10+ | Import/export textures/meshes, create materials, thumbnails |
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
