# MetaHuman Commands — Technical Reference

This directory is a pointer. The **canonical** technical reference for working on MetaHuman commands in ECABridge lives in the Agile Lens knowledge base, because it's shared across the fleet and git-synced everywhere an Agile Lens developer would pick up this plugin:

**Canonical reference:** `C:/Users/Sam/knowledge/departments/engineering/metahuman-mcp-reference.md`
(on any Agile Lens fleet machine — path is the same on Fort, Lenovo, etc.)

## What's in the reference

- **Build environment** — OpenClawTestBed (source) vs MutableSample (build target) dichotomy, required plugin deps, `.gitattributes`, 5.7 header gotchas
- **Class/subsystem discovery paths** — exact `/Script/Module.ClassName` strings that work
- **Canonical helpers** — `LoadMHCharacter`, `GetMHEditorSubsystem`, `OpenCharacterEditor` (copy verbatim)
- **The reflection pattern** — the annotated `InitializeValue_InContainer` → `ProcessEvent` → `DestroyValue_InContainer` template, and why each step matters
- **The out-param capture/replay pattern** — `CopyScriptStruct` to pass struct out-params between reflected calls
- **Groom attachment canonical flow** — Collection+Instance, not `WardrobeIndividualAssets`
- **Enum inversion gotcha** — `EMetaHumanCharacterSkinPreviewMaterial::Default` is UI's "Topology", `Editable` is UI's "Skin"
- **Slate screenshot pattern** — window walk + 5.7 PNG encode API
- **Epic sign-in requirement** — which commands need it, crash signature, defensive pattern
- **Spawned actor vs built MetaHuman** — what components each has, why `tint_metahuman_outfit` needs a body_outfit_proxy fallback
- **Data discovery** — how to enumerate available presets/grooms/constraints at runtime
- **5.7 API pitfalls** — table of bug → cause → fix for every 5.7 API surprise hit on 2026-04-16
- **UI → API mapping cheatsheet** — which subsystem function corresponds to which MetaHuman editor UI action
- **Adding a new MetaHuman command** — step-by-step checklist
- **Debugging** — common symptoms and root causes

## Ground truth

When the reference and the code disagree, the code wins. The reference is maintained against:
- `Source/ECABridge/Private/Commands/ECAMetaHumanCommands.cpp`
- `Source/ECABridge/Public/Commands/ECAMetaHumanCommands.h`

Update the KB reference when you ship new patterns.
