# Blueprint Layout & Comment Commands — 2026-05-20

Three improvements to ECABridge's Blueprint authoring surface, motivated by a
Spellrot prototype session where the existing tools couldn't produce slide-ready
Blueprints without hand-editing in the Unreal Editor.

## New commands

### `set_blueprint_node_position`

Move one or more existing nodes to specified `(x, y)` coordinates. Single-node
and batch forms both supported.

```jsonc
// single-node form
{
  "blueprint_path": "/Game/ThirdPerson/Blueprints/BP_WaveSpawner",
  "graph_name": "EventGraph",
  "node_id": "A950175C0A412A29B1F97A9B9859215E",
  "x": 0, "y": 0
}

// batch form
{
  "blueprint_path": "/Game/ThirdPerson/Blueprints/BP_WaveSpawner",
  "graph_name": "EventGraph",
  "positions": [
    {"node_id": "A950175C...", "x":   0, "y":   0},
    {"node_id": "B72CD8F5...", "x": 400, "y":   0},
    {"node_id": "0FF6D9C6...", "x":   0, "y": 800}
  ]
}
```

**Why this exists**: agent callers sometimes need to override the auto-layout
output for slide-ready arrangements. Doing this via `unreal.execute_python`
with `set_editor_property('NodePosX', x)` has crashed UE 5.7/5.8 hard in
field testing — the Slate UI thread hangs and the next editor launch
gets stuck at "Initializing Python 87%" indefinitely. This MCP command
routes the change through `Modify()` + `MarkBlueprintAsStructurallyModified`
so the editor's notification path runs and the graph view refreshes.

### `add_blueprint_comment_node`

Add a labelled comment box to the graph. UE Python doesn't expose
`UEdGraphNode_Comment` (and there's no other MCP command to add one),
which previously forced AI-generated Blueprints to be self-documenting
only via external markdown.

```jsonc
{
  "blueprint_path": "/Game/ThirdPerson/Blueprints/BP_WaveSpawner",
  "comment": "Setup — timer registration on BeginPlay",
  "graph_name": "EventGraph",
  "x": -40, "y": -40,
  "width": 700, "height": 280,
  "color": "#5B8CFF"
}
```

**Auto-wrap form**: pass an array of node GUIDs in `wrap_node_ids` and the
comment auto-sizes around them with the specified margin (default 40):

```jsonc
{
  "blueprint_path": "/Game/ThirdPerson/Blueprints/BP_WaveSpawner",
  "comment": "Spawn Loop — fire every SpawnInterval seconds",
  "wrap_node_ids": [
    "0FF6D9C6B94970EFACB6348A5FB0B2BB",
    "2811C88EDC47F2764C7D329850CCD530",
    "4E2D1E4B9F42F1258BBB35B89CC7F1DE",
    "0C544BF755499C402BF84DAC5CC08D05"
  ],
  "color": "#FFCC55",
  "margin": 60
}
```

Color accepts hex (`#RRGGBB` or `#RRGGBBAA`) or comma-separated `r,g,b` (either
0-255 ints or 0.0-1.0 floats — the parser picks based on whether any value
exceeds 1.5).

Comments default to `ECommentBoxMode::GroupMovement` so dragging the comment
in the editor moves the wrapped nodes with it — which is what you want for
labelled sections in slide screenshots.

### Improved `auto_layout_blueprint_graph` pure-node spacing

`PositionPureNodesForConsumer` previously used `Config.DefaultNodeHeight`
(constant 100) for Y spacing between pure nodes in a column. Tall pure nodes
(e.g. variable_set with many pins, or a function call with default-expanded
struct pins) would overlap.

Fixed: each pure node now consumes its actual `NodeHeight + PureNodePaddingY`
in the Y axis, so taller nodes get more room.

## Test recipe

```python
# Set up a BP with overlapping pure nodes (e.g. spawn chain with Get Actor Transform)
auto_layout_blueprint_graph(blueprint_path="...", strategy="horizontal")

# Dump and verify no two nodes share bounding boxes:
dump = dump_blueprint_graph(...)
for n in dump.graphs[0].nodes:
    # Each node should have a unique (x, y, w, h) rectangle that doesn't overlap others

# Wrap the spawn chain in a labelled comment for slide-ready output:
add_blueprint_comment_node(
    blueprint_path="...",
    comment="Spawn Loop",
    wrap_node_ids=[spawn_event_id, begin_deferred_id, finish_spawn_id, transform_id],
    color="#FFCC55"
)
```

## File-level changes

```
Source/ECABridge/Public/Commands/ECABlueprintNodeCommands.h   +57 lines (2 new class decls)
Source/ECABridge/Private/Commands/ECABlueprintNodeCommands.cpp +338 lines (2 new Execute impls + helper)
Source/ECABridge/Private/Commands/BlueprintAutoLayout.cpp     +23 / -14 lines (pure-node Y fix)
```

No new module dependencies — `UnrealEd` was already in `ECABridge.Build.cs` and
`EdGraphNode_Comment.h` lives there.

## Field-tested motivation

This work was provoked by a Spellrot Unreal Fest demo build (2026-05-20):
the existing `auto_layout` placed `Get Actor Transform` (pure) directly on
top of `SpawnWave` (custom event), and there was no way to label "Setup"
vs "Spawn Loop" regions of the graph for the slide deck. The agent's
Python-based workaround (set `NodePosX`/`NodePosY` via reflection) crashed
the editor twice and left it stuck on relaunch for 18+ minutes.

These three additions close that loop: layout now respects actual node
heights, agents can fine-tune positions when auto-layout isn't ideal, and
labelled comments give AI-authored Blueprints the same readability as
hand-authored ones.
