# ECABridge — AI-Powered Unreal Engine 5 MCP Plugin

390+ MCP tools for UE5 editor automation via Claude, ChatGPT, or any MCP-compatible AI agent.

> **Using this with an AI agent?** Pair it with **[ue5-mcp](https://github.com/ibrews/ue5-mcp)** — a Claude Code / Cowork skill that loads the hard-won knowledge your agent needs to use these tools without crashing the editor or wasting hours on silent-fail APIs. ECABridge is the plugin (what tools exist); ue5-mcp is the field manual (which calls actually work, which crash, and the workarounds). Install both.

## Features

- **390+ MCP tools** organized by category
- **12 Rosetta Stone commands** — full JSON dumps of assets, blueprints, levels, materials, Niagara, sequencer, widgets, animation, MetaSound, and DataTables
- **6 workflow commands** — project overview, cross-blueprint search, asset validation, snapshot/diff, undo-batching, class hierarchy
- **5 refactoring commands** — replace references, bulk rename, search-and-replace properties, world settings
- **5 advanced system commands** — landscape, source control, PCG graphs, Control Rig, Gameplay Ability System
- **22 MetaHuman commands** — full procedural pipeline from asset creation through cloud texture/rig through groom attachment and outfit tinting
- **5 Enhanced Input commands** — actions, mapping contexts, key bindings
- **2 Niagara Data Channel commands** — create and dump cross-system data channels
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
| **Mutable/CO** | 16 | Character customization: create/edit CO graphs, compile, runtime params, spawn actors |
| **MetaHuman** | 22 | Full procedural pipeline: create, describe (NL), face presets, body constraints, groom attach, makeup, cloud textures/rig, tint outfit, editor screenshot |
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

## MetaHuman: Zero-Touch Procedural Character Creation

22 commands that drive Epic's MetaHuman Character pipeline end-to-end from natural-language prompts. No UI clicks required (after Epic sign-in).

**The full pipeline:**

```bash
create_metahuman(package_path="/Game/Characters", asset_name="MH_Hero")

describe_metahuman(character_path=".../MH_Hero",
  description="tall muscular with vivid purple hair and green eyes and light freckled skin")
  # matches: body, skin, eyes, hair keywords → sets 7+ properties

set_metahuman_face_preset(character_path=".../MH_Hero",
  preset_path="/MetaHumanCharacter/Optional/Presets/Grace")
  # 29 preset faces: Ada, Bruce, Celeste, Dominic, Grace, Isaiah, Jelena, Kelvin, Lorenzo, ...

set_metahuman_body_type(character_path=".../MH_Hero", body_type="f_tal_nrw")
  # 18 body presets: {gender}_{height}_{weight}, e.g. m_tal_ovw, f_med_nrw

set_metahuman_body_constraints(character_path=".../MH_Hero", constraints=[
  {name: "Height", target_measurement: 178, active: true},
  {name: "Chest",  target_measurement: 94,  active: true},
  {name: "Waist",  target_measurement: 72,  active: true}
])  # 30 parametric body dimensions in cm

set_metahuman_makeup(character_path=".../MH_Hero",
  lip_type="Natural", lip_color={r:0.5,g:0.15,b:0.2,a:1},
  eye_type="SoftSmokey", blush_type="Apple")

set_metahuman_preview_mode(character_path=".../MH_Hero", mode="skin")
open_metahuman_editor(character_path=".../MH_Hero")
download_metahuman_textures(character_path=".../MH_Hero")  # cloud — needs Epic sign-in
rig_metahuman(character_path=".../MH_Hero", rig_type="full") # cloud

attach_metahuman_groom(slot_name="Hair",
  wardrobe_item_path="/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_L_Straight")
attach_metahuman_groom(slot_name="Eyebrows",
  wardrobe_item_path="/MetaHumanCharacter/Optional/Grooms/Bindings/Eyebrows/WI_Eyebrows_M_Dense")
# 96 hair/beard/eyebrow/eyelash grooms ship with the engine

refresh_metahuman_preview(character_path=".../MH_Hero")
spawn_metahuman_actor(character_path=".../MH_Hero")
tint_metahuman_outfit(actor_name="MetaHumanDefaultEditorPipelineActor0",
  shirt_color={r:0.6,g:0.1,b:0.1,a:1})

take_metahuman_editor_screenshot(character_path=".../MH_Hero",
  file_path="D:/Screenshots/Hero.png")
```

**Photo → MetaHuman workflow:** pair Claude's vision with the MetaHuman commands. The AI analyzes a photo, extracts features (skin tone, hair style, face shape, beard, etc.), picks the closest preset from Epic's 29, attaches matching grooms, and builds a photo-inspired character. Not a perfect likeness (that needs Epic's Mesh-to-MetaHuman scan pipeline) but genuinely readable.

**Commands:** create, dump, describe, set_property, preview_mode, open_editor, build, download_textures, rig, spawn_actor, get/set_body_constraints, set_body_type, attach_groom, list_grooms, list_presets, set_face_preset, set_makeup, refresh_preview, take_editor_screenshot, tint_outfit.

**Requires:** Epic sign-in (click the person icon in the MetaHuman editor toolbar) for cloud operations — texture synthesis and auto-rigging both hit Epic's MetaHuman service.

## Things to Try

Once the editor is running with ECABridge loaded, point your AI agent at it and try these:

**See your whole level as JSON for the first time:**
```
dump_level(max_actors=200)
```

**Reverse-engineer any Blueprint without opening the editor:**
```
dump_blueprint_graph(blueprint_path="/Game/Blueprints/BP_PlayerCharacter")
```

**Understand a material's full node graph:**
```
dump_material_graph(material_path="/Game/Materials/M_Landscape")
```

**Find every asset of a type and then deep-dive one:**
```
find_assets(class_filter="NiagaraSystem", path_filter="/Game/Effects/")
dump_niagara_system(system_path="/Game/Effects/NS_Explosion")
```

**Check what would break before you change something:**
```
get_asset_references(asset_path="/Game/Textures/T_BaseColor", direction="referencers", depth=2)
```

**Read back a cinematic you built, keyframes and all:**
```
dump_level_sequence(sequence_path="/Game/Cinematics/Intro", include_keyframes=true)
```

**Inspect a UI widget tree hierarchy:**
```
dump_widget_tree(widget_path="/Game/UI/WBP_HUD")
```

**See an Animation Blueprint's state machine structure:**
```
dump_animation_blueprint(anim_bp_path="/Game/Characters/ABP_Mannequin")
```

**Dump a DataTable — schema and all rows at once:**
```
dump_datatable(asset_path="/Game/Data/DT_WeaponStats")
```

**Serialize any asset you can't otherwise read:**
```
dump_asset(asset_path="/Game/SomeOpaqueAsset", include_thumbnail=true)
```

**Get a bird's-eye view of an unfamiliar project:**
```
get_project_overview(path="/Game/", max_depth=3)
```

**Find every Blueprint that calls a specific function:**
```
search_blueprint_usage(search_term="SetActorLocation", path_filter="/Game/Blueprints/")
```

**Understand a class before you use it:**
```
get_class_hierarchy(class_name="Character", include_functions=true)
```

**Validate your work after making changes:**
```
validate_asset(asset_path="/Game/Blueprints/BP_Enemy")
```

**Track what changed — snapshot before, diff after:**
```
snapshot_asset(asset_path="/Game/Materials/M_Hero")
# ...make changes...
diff_asset(asset_path="/Game/Materials/M_Hero", snapshot_id="<id from snapshot>")
```

**Batch multiple operations into a single undo step:**
```
batch_operation(
  description="Setup enemy spawner",
  commands=[
    {name: "create_actor", arguments: {actor_type: "StaticMeshActor", name: "Spawner", ...}},
    {name: "set_actor_material", arguments: {actor_name: "Spawner", material_path: "..."}},
    {name: "add_actor_tag", arguments: {actor_name: "Spawner", tag: "EnemySpawn"}}
  ]
)
```

**Swap all references to one asset with another (safe refactoring):**
```
replace_asset_references(old_asset_path="/Game/Materials/M_Old", new_asset_path="/Game/Materials/M_New", dry_run=true)
```

**Bulk rename assets with find/replace:**
```
bulk_rename_assets(path_filter="/Game/Textures/", find="T_Old", replace="T_New", dry_run=true)
```

**Change a property on every actor of a class at once:**
```
search_and_replace_property(class_filter="PointLight", property_name="Intensity", find_value="5000", replace_value="3000")
```

**Read or change world settings:**
```
get_world_settings()
set_world_settings(property="KillZ", value="-10000")
```

**Understand a class hierarchy before working with it:**
```
get_class_hierarchy(class_name="Character", include_functions=true)
```

**Inspect landscape terrain — components, layers, material:**
```
dump_landscape()
```

**Check source control status for pending changes:**
```
get_source_control_status(path_filter="/Game/", include_unchanged=false)
```

**Dump a PCG graph — all nodes, edges, parameters:**
```
dump_pcg_graph(graph_path="/Game/PCG/PCGG_ForestSpawner")
```

**Read a Control Rig's hierarchy (bones, controls, nulls):**
```
dump_control_rig(rig_path="/Game/Characters/CR_Mannequin")
```

**Inspect a Gameplay Ability / Effect / AttributeSet:**
```
dump_gameplay_ability(asset_path="/Game/GAS/GA_Fireball")
```

**Create a MetaHuman procedurally from natural language:**
```
create_metahuman(package_path="/Game/Characters", asset_name="MH_Kai")
describe_metahuman(character_path="/Game/Characters/MH_Kai",
  description="tall muscular dark-skinned man with brown eyes and black hair")
set_metahuman_face_preset(character_path="/Game/Characters/MH_Kai",
  preset_path="/MetaHumanCharacter/Optional/Presets/Kelvin")
set_metahuman_body_type(character_path="/Game/Characters/MH_Kai", body_type="m_tal_ovw")
open_metahuman_editor(character_path="/Game/Characters/MH_Kai")
download_metahuman_textures(character_path="/Game/Characters/MH_Kai")  # Epic cloud
rig_metahuman(character_path="/Game/Characters/MH_Kai", rig_type="full") # Epic cloud
attach_metahuman_groom(character_path="/Game/Characters/MH_Kai", slot_name="Hair",
  wardrobe_item_path="/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_CurlyFade")
attach_metahuman_groom(character_path="/Game/Characters/MH_Kai", slot_name="Beard",
  wardrobe_item_path="/MetaHumanCharacter/Optional/Grooms/Bindings/Beards/WI_Beard_L_Full")
refresh_metahuman_preview(character_path="/Game/Characters/MH_Kai")
```

**Discover what groom assets you have available:**
```
list_metahuman_grooms()  # Groups 96 engine-shipped wardrobe items by slot
list_metahuman_presets() # Lists the 29 preset faces
```

**Morph a MetaHuman's body with 30 parametric dimensions:**
```
get_metahuman_body_constraints(character_path="/Game/Characters/MH_Hero")
set_metahuman_body_constraints(character_path="/Game/Characters/MH_Hero", constraints=[
  {name: "Height", target_measurement: 185, active: true},
  {name: "Chest",  target_measurement: 108, active: true},
  {name: "Thigh",  target_measurement: 62,  active: true}
])
```

**Apply Hollywood makeup in one call:**
```
set_metahuman_makeup(character_path="/Game/Characters/MH_Hero",
  lip_type="Hollywood", lip_color={r:0.5,g:0.1,b:0.15,a:1},
  eye_type="SoftSmokey", blush_type="Apple", foundation=true)
```

**Drop a MetaHuman into the level and tint their outfit per-actor:**
```
spawn_metahuman_actor(character_path="/Game/Characters/MH_Hero")
tint_metahuman_outfit(actor_name="MetaHumanDefaultEditorPipelineActor0",
  shirt_color={r:0.1,g:0.3,b:0.8,a:1})
```

**Capture the MetaHuman editor tab remotely:**
```
take_metahuman_editor_screenshot(character_path="/Game/Characters/MH_Hero",
  file_path="D:/Shots/hero.png")
```

**Photo → MetaHuman (with Claude's vision in the loop):** Share a photo with your AI agent. It analyzes the image (skin tone, hair, face shape, beard, etc.), picks the closest face preset, attaches matching grooms, and builds a photo-inspired MetaHuman. Not a perfect likeness — that requires Epic's Mesh-to-MetaHuman scan tool — but genuinely recognizable.

**Build an input system from scratch:**
```
create_input_action(asset_path="/Game/Input/IA_Jump", value_type="Digital")
create_input_mapping_context(asset_path="/Game/Input/IMC_Gameplay")
add_input_mapping(context_path="/Game/Input/IMC_Gameplay",
                  action_path="/Game/Input/IA_Jump", key="SpaceBar")
dump_input_mapping_context(context_path="/Game/Input/IMC_Gameplay")
```

**Create a Niagara Data Channel for cross-system communication:**
```
create_niagara_data_channel(
  asset_path="/Game/FX/NDC_Hits",
  variables=[{name: "Position", type: "Vector"}, {name: "Damage", type: "Float"}]
)
```

## Requirements

- Unreal Engine 5.7+
- Visual Studio 2022 (for building)
- For Mutable commands: `Mutable` plugin enabled
- For MetaHuman commands: `MetaHumanCharacter` plugin enabled

## License

Internal use — Agile Lens / ibrews
