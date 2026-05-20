# ue5-mcp skill — changelog

## 2.0.0 — 2026-05-19

**Plugin baseline:** ECABridge `af30649` (post-Batch P).

### Breaking changes

- **Schema-in-error response format.** Validation errors now embed the full input JSON Schema in `content[0].text`. Old skill assumed plain error string; new skill must parse the schema and retry. Backwards-compat: agents that ignore the schema still see a human-readable failure message before it.
- **Python sandbox semantics.** `execute_script` now captures `run()`'s return value directly into the result. The 1.x `print(json.dumps(run()))` pattern still works (falls back to `log_output`), but new scripts should `return` instead.
- **Optional-dependency gating.** Commands for 7+ engine plugins (Mutable, MovieRenderPipeline, MetaHumanCharacter, Niagara, MetaSound, ControlRig, GameplayAbilities, USD, nDisplay, DMX, DataValidation, RenderDoc) are conditionally registered. Skills must not assume a fixed command surface.

### Additive

- **`outputSchema`** declared on every `dump_*`, `find_*`, `get_*`, and many `list_*` commands. Skills should trust these for response shape.
- **Inline base64 PNG screenshots** via MCP `image` content blocks — no file I/O.
- **~80 new commands** across Batches L (Stage/XR/nDisplay/DMX), M (MovieRenderGraph + rendering), N (observability), O (asset pipeline), P (PCG), Q (editor UX, CVars, viewport bookmarks), R (source control + P4 changelists).
- **UE 5.7 + 5.8** from a single branch — five engine-version-guarded call sites; everything else identical.
- **`get_execution_environment`** documents what's available inside the Python sandbox.

### Removed / deprecated

- `execute_python` (1.x) — superseded by `execute_script`. Still works but emits a deprecation hint.
- Bare command-name assumptions — every skill probe of "command X exists" must be checked against the live `tools/list` rather than a hardcoded list.

### Notes for skill authors

- This skill major bump pairs with ECABridge plugin commit `af30649`. Earlier plugin commits won't have all the optional-dep gating; check `/health` for the live command count to detect version mismatches.
- The skill no longer ships specific UE Python recipes — the field manual section uses templates that the agent fills in. This is intentional: pre-canned recipes become wrong as APIs drift, and the LLM is better at generating them on demand than the skill is at maintaining them.

## 1.x — prior history

See the ue5-mcp repo's own changelog: https://github.com/ibrews/ue5-mcp
