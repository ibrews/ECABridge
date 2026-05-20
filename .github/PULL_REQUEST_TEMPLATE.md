<!--
  Thanks for the patch. The checklist below mirrors CONTRIBUTING.md — if
  any item doesn't apply, strike it through or delete it instead of
  leaving it unchecked.
-->

## What this changes



## Why



## Tested on

- [ ] UE 5.7 (Test57.uproject, `build-deploy.ps1 -Engine 5.7`)
- [ ] UE 5.8 (Test58.uproject, `build-deploy.ps1 -Engine 5.8`)
- [ ] `scripts/smoke-test.py` passes against the affected categories
- [ ] `scripts/lint-skill-tools.py --skill docs/skill/SKILL.md --warn-only --ignore log_output` runs clean

## Checklist

- [ ] No new hard dep on an engine plugin without an `EngineHasPlugin` /
      `WITH_ECA_*` guard
- [ ] Any 5.8-only API is wrapped in `#if !UE_VERSION_OLDER_THAN(5,8,0)`
- [ ] `clang-format -i` run on touched files only (no drive-by reformat
      of unrelated files)
- [ ] New commands have `outputSchema` parity with siblings in the
      same category
- [ ] CONTRIBUTING.md, README.md, or the wiki updated if behavior or
      install steps change

## Notes for reviewers


