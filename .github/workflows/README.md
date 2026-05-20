# Workflows

Current CI is intentionally lightweight — every check here runs on a
hosted Ubuntu runner without needing Unreal Engine installed.

| Workflow | What it does | When it runs |
|---|---|---|
| `lint.yml` | `py_compile` every helper script, validate `.uplugin` + JSON, run the field-manual linter, shellcheck any tracked `*.sh` | push to `main`, every PR |

## Planned (Batch D, second pass)

The roadmap calls for a full UAT `BuildPlugin` job against UE 5.7 and
5.8 plus the headless smoke test (`-NoSplash -NullRHI -ExecCmds=...`).
Both need a self-hosted Windows runner with the engine pre-installed —
work blocked on Alex's runner provisioning decision. The slot is
reserved as `build-deploy.yml`; see `scripts/SETUP.md` for the local
build cycle the runner will mirror.

## Local equivalents

Everything in `lint.yml` can be run locally:

```powershell
# python helpers compile
python -m py_compile (git ls-files 'scripts/*.py')

# json validation
git ls-files '*.uplugin' '*.json' | ForEach-Object { python -m json.tool $_ > $null }

# command-surface listing + skill cross-check
python scripts/lint-skill-tools.py --list
python scripts/lint-skill-tools.py --skill docs/skill/SKILL.md --warn-only --ignore log_output
```
