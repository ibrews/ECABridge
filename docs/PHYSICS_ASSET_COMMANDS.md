# PhysicsAsset Commands (Batch X)

ECABridge surface for editing skeletal `UPhysicsAsset` documents from MCP. Category string is `PhysicsAsset` (distinct from the existing `Physics` category which covers dump + physical-material + actor-level constraint spawning).

All 17 commands live in `Source/ECABridge/Private/Commands/ECAPhysicsAssetCommands.cpp` and run on the editor thread. No new module dependencies — `PhysicsCore` is already a hard dep.

## Tools

### Lifecycle
- `create_physics_asset_from_mesh` — walk the bound skeleton of a `USkeletalMesh` and seed one default `USkeletalBodySetup` per bone. Returns `{ path, created, body_count, constraint_count }`. `assign_to_mesh=true` also sets the mesh's `PhysicsAsset`. `overwrite=true` allows replacement.
- `add_body` — empty `USkeletalBodySetup` for a bone that has none. Errors if the bone already has a body.
- `remove_body` — drop the body for a bone and any constraint that touches it (either ordering). Returns the names of removed constraints.

### Introspection
- `get_body_names` — list of bones that have a body.
- `get_body_shapes` — per-shape `ShapeInfo` (`shape_type` in {`Sphere`, `Capsule`, `Box`}, center, rotation, radius/length/extent depending on type). Capsules map to UE's `FKSphylElem` internally.

### Shape authoring (all upsert by `shape_name`)
- `set_body_sphere` — `{center, radius}`. Radius > 0.
- `set_body_capsule` — `{center, rotation, radius, length}`. Long axis is local Z after rotation. Total height = `length + 2*radius`. radius > 0, length >= 0.
- `set_body_box` — `{center, rotation, extent_x, extent_y, extent_z}`. All extents > 0.
- `remove_body_shape` — find by `shape_name` across all primitive arrays; returns `{removed: bool}`.

Identity is `FName` set via `FKSphereElem::SetName` / `FKBoxElem::SetName` / `FKSphylElem::SetName`. The upsert pass scans sphere → capsule → box arrays for a name hit and removes the first match before adding the new element. This means a shape can change primitive type by re-using the same `shape_name`.

### Body properties
- `get/set_body_physics_mode` — `mode` is `"Default"` | `"Kinematic"` | `"Simulated"`, mapping onto `EPhysicsType::PhysType_*` declared in `PhysicsCore/Public/BodySetupEnums.h`.
- `get/set_body_mass_scale` — `mass_scale` multiplier on `Body->DefaultInstance.MassScale`. Must be > 0.

### Constraints (angular only — linear is out of scope for v1)
- `get_constraints` — array of `{bone1 (child), bone2 (parent), swing1/swing2/twist motion + limit_degrees}`.
- `add_constraint` — `{bone1_name (child), bone2_name (parent)}`. Both bones must have bodies. Errors if a constraint already exists between this pair (either ordering).
- `set_constraint_limits` — partial update: any of the six axis params (`swing1_motion`, `swing1_limit_degrees`, `swing2_motion`, `swing2_limit_degrees`, `twist_motion`, `twist_limit_degrees`) can be omitted to leave the existing value untouched. Motion strings map to `EAngularConstraintMotion::ACM_Free`/`ACM_Limited`/`ACM_Locked`.
- `remove_constraint` — looks up the pair in either ordering (`FindConstraintIndex(B1,B2)` then `(B2,B1)`).

## After every mutation
The implementations always:
1. Call `PA->Modify()` for undo/redo + dirty marking.
2. Call `Body->InvalidatePhysicsData()` + `Body->CreatePhysicsMeshes()` on shape mutations (so the next sim run sees the new geometry).
3. Call `Body->MarkPackageDirty()` and `PA->MarkPackageDirty()`.
4. Call `PA->UpdateBodySetupIndexMap()` + `PA->UpdateBoundsBodiesArray()` + `PA->RefreshPhysicsAssetChange()` (the last is `WITH_EDITOR`-gated, both engines).

The new-asset path (`create_physics_asset_from_mesh`) additionally calls `FAssetRegistryModule::AssetCreated(PA)` so the content browser refreshes.

## Out of scope (for follow-up batches)
- Convex hull authoring (no `add_body_convex`).
- Tapered capsules (we dump them via `dump_physics_asset` but there's no setter).
- Per-shape physical material assignment.
- Linear constraint motions + linear limits (we expose angular only here).
- PhAT viewport / preview commands.
- Cloth asset authoring.

## Clean-room note
All command names, parameter shapes, output shapes, and error semantics are derived from the spec at `intelligence/tools/ecabridge-physics-asset-port.md`, which describes the externally-observable behavior in our own words. Implementations call only public engine headers (`PhysicsEngine/PhysicsAsset.h`, `SkeletalBodySetup.h`, `BodySetup.h`, `AggregateGeom.h`, `SphereElem.h`, `BoxElem.h`, `SphylElem.h`, `ShapeElem.h`, `ConstraintInstance.h`, `PhysicsConstraintTemplate.h`). No Epic MCP-plugin source is referenced.
