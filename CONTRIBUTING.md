# Contributing to ECABridge

Thanks for the interest. ECABridge is an internal Agile Lens project but
external patches are welcome — this file documents the workflow we use
ourselves so a contribution looks right on the first pass.

## Before you open a PR

1. **Read the wiki.** [Engine Compatibility][compat] and [Optional
   Dependencies][opt-deps] cover the two non-obvious constraints every
   change has to respect.
2. **Pick the right base branch.** All work targets `main`. We don't
   maintain release branches; the single `main` branch is verified to
   build and run on both UE 5.7 and UE 5.8.
3. **Open an issue first for anything non-trivial.** Bug fixes and small
   tweaks can go straight to PR; new commands, new build flags, or
   anything touching the registration core (`ECABridgeModule.cpp`,
   `ECACommand.cpp`) should be discussed first.

[compat]: https://github.com/ibrews/ECABridge/wiki/Engine-Compatibility
[opt-deps]: https://github.com/ibrews/ECABridge/wiki/Optional-Dependencies

## Coding style

We follow Epic's [C++ coding standard][ue-style] with the deltas
captured in `.clang-format` at the repo root. The short version:

- Tabs, width 4. Spaces only for alignment within a line.
- Allman braces (open brace on its own line).
- No column limit — break where the code reads best.
- `PointerAlignment: Left` (`FString* Foo`, not `FString *Foo`).
- UE macros (`UCLASS`, `UPROPERTY`, `REGISTER_ECA_COMMAND`) are listed
  under `StatementMacros` so clang-format doesn't mangle them.

Format touched files before committing:

```powershell
clang-format -i Source\ECABridge\Private\Commands\YourFile.cpp
```

Do **not** reformat files you didn't otherwise touch. The repo has
historical drift from the format; tightening that up is a separate
cleanup PR.

[ue-style]: https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/

## Adding a new command

The 500-command surface follows one shape: a class deriving from
`IECACommand` with `GetName() / GetDescription() / GetCategory() /
GetParameters() / Execute()`, registered at the bottom of the `.cpp`
file with `REGISTER_ECA_COMMAND(FECACommand_YourName)`.

Checklist:

- [ ] `GetName()` is snake_case, lowercase, and unique across the registry.
- [ ] Add an `outputSchema` via `GetOutputSchema()` when the response has
      stable structure — it lets `tools/list` consumers know what to
      expect. Aim for parity with siblings in the same category.
- [ ] If the command depends on an optional engine plugin (Mutable,
      Niagara, MetaSound, Movie Render Pipeline, etc.) wrap the
      `REGISTER_ECA_COMMAND` line in the matching `#if WITH_ECA_X` guard
      so the registry stays consistent with the build flags.
- [ ] If a 5.8-only API is touched, gate it with
      `#if !UE_VERSION_OLDER_THAN(5,8,0)` so the 5.7 build still passes.
- [ ] Run `scripts/lint-skill-tools.py --list` and confirm your command
      appears in the diff vs. the previous output.

## Testing locally

We don't ship a CI runner with UE installed yet (see
`.github/workflows/README.md`). The local smoke-test rhythm is:

```powershell
# Build + deploy + restart against UE 5.8
powershell -File scripts\build-deploy.ps1 -Engine 5.8

# Confirm the server is up
curl http://127.0.0.1:3000/health

# Smoke-test the surface
python scripts\smoke-test.py
```

If you can run the same on UE 5.7 with `-Engine 5.7`, please do — the
single-branch promise breaks if 5.7 regresses.

## Running CI checks locally

The hosted-Ubuntu lint workflow (`.github/workflows/lint.yml`) is
trivially runnable on Fort:

```powershell
# python compile-check every script
python -m py_compile (git ls-files 'scripts/*.py')

# json + .uplugin parse
git ls-files '*.uplugin' '*.json' | ForEach-Object { python -m json.tool $_ > $null }

# field-manual cross-check
python scripts\lint-skill-tools.py --skill docs\skill\SKILL.md --warn-only --ignore log_output
```

## Commit messages

Conventional-style prefix, imperative mood, lower-case subject:

```
feat: add dump_pcg_data for runtime data inspection
fix: handle missing UE_VERSION_OLDER_THAN gate on FJsonObject::Values
ci: bump shellcheck step to ubuntu-24.04
docs: clarify the Python sandbox return-value contract
```

A two-line body explaining *why* is almost always worth writing — the
diff already says *what*.

## Clean-room discipline (very important)

ECABridge is positioned as an **always-+ layer** on top of Epic's native
`ModelContextProtocol` plugin. We clean-room reimplement Epic's MCP
features so launcher users get them without compiling from source. This
only works if we keep our repository legally clean.

**No Epic source code in this repository. Ever.**

- You may **call** Epic's public C++ headers (anything in
  `ModelContextProtocol/Source/.../Public/`) — that's normal plugin
  development under the UE License.
- You may **read** Epic's source to understand behavior, but **do not
  paste or paraphrase** more than a few lines into our files. Write the
  behavior down in plain English in an internal spec note first, then
  implement against the spec.
- When porting a feature, the PR description should link to:
  - The behavioral spec we wrote (often a tool agent's output or a doc
    in our internal KB)
  - The native commit(s) the feature originated in, for traceability
- Do not include or modify any file that has Epic's copyright header.

If you find that something is much easier to do by copying Epic's code,
escalate to a maintainer before doing it. The right answer is usually
a thin wrapper that calls Epic's public API, not a copy.

For the full reasoning, see `intelligence/tools/ue5-eula-mcp-redistribution.md`
in our internal KB (summary: UE EULA §1(A)(b) restricts source
redistribution to fellow licensees on the matching version, which a
public GitHub repo can't enforce; clean-room API reimplementation is
explicitly allowed).

## What we can't accept

- **Any Epic source code copied into our repo, even with attribution.**
  Headers from `#include` are fine; literal source is not. See the
  clean-room discipline section above.
- Anything that breaks the 5.7 build without a `#if UE_VERSION_OLDER_THAN`
  guard around the change.
- New hard dependencies on engine plugins that aren't enabled by default
  in a stock UE install. Add them as optional via the `EngineHasPlugin`
  pattern instead.
- Reformatting passes that touch files unrelated to the change.

## Where to ask

Open a GitHub issue or post in the Agile Lens engineering channel. For
fast feedback on a UE API question, Alex is the fastest path.
