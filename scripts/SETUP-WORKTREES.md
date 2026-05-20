# ECABridge — Parallel-Batch Worktree Pattern

When running multiple autonomous `claude -p` batches on the same machine, each batch must work in its own git worktree to avoid clobbering each other's source files. This doc captures the pattern.

## The two singletons

Two singletons constrain parallelism on any one machine:

1. **The plugin junction.** `<UE-base>\Test57\Plugins\ECABridge` and `\Test58\Plugins\ECABridge` are NTFS junctions that the build script follows to find ECABridge source. Only one target can be active at a time.
2. **The Test57 / Test58 build artifacts.** `C:\UE\Test57\Binaries\` and `\Intermediate\` accumulate compiled output from whichever source the junction currently points at. Concurrent builds against the same Test project will collide.

So even with separate worktrees, **only one batch can be building at any given moment per machine.** For true parallel work, fan out across machines (Theseus + Fort + ...).

## Per-machine setup

### Pre-stage 5 worktrees (one per active batch)

```powershell
# Theseus — C:\GH\ECABridge is canonical, worktrees branch off main
cd C:\GH\ECABridge
git fetch --all
git worktree add C:\GH\ECABridge-x feature/batch-x   # if resuming an existing branch
git worktree add C:\GH\ECABridge-w -b feature/batch-w main
git worktree add C:\GH\ECABridge-y -b feature/batch-y main
git worktree add C:\GH\ECABridge-z -b feature/batch-z main
```

Fort uses `D:\GH\ECABridge` and the same pattern. Worktree paths are arbitrary — just match what your goal files reference.

### Junction helper

`scripts/point-junctions.ps1` redirects both engines' plugin junctions to a specified worktree:

```powershell
# Point junctions at a worktree before building
.\scripts\point-junctions.ps1 -Target C:\GH\ECABridge-w

# Fort uses D:\UE as the test-project base
.\scripts\point-junctions.ps1 -Target D:\GH\ECABridge-v -JunctionBase D:\UE

# Restore canonical state after the batch
.\scripts\point-junctions.ps1 -Target C:\GH\ECABridge
```

Each batch's goal file calls this twice — once at Setup, once at Teardown. If the agent crashes mid-batch, the junction is left pointing at its worktree; the next batch's Setup will redirect, or you can restore manually.

### Baseline-freshness helper

`scripts/refresh-batch-baselines.ps1` updates the "ahead of `<hash>`" baseline in each `batch-*-goal.txt` to match the current `origin/main`. Run right before firing each batch (see [[claude-code-goal-command]] Rule 1 — Baseline freshness).

```powershell
# Theseus
.\scripts\refresh-batch-baselines.ps1

# Fort
.\scripts\refresh-batch-baselines.ps1 -RepoPath D:\GH\ECABridge
```

Idempotent. Reports "already current" if no update needed.

## Fire pattern (per batch, per machine)

1. Pre-stage worktree (above, one-time per batch).
2. Refresh baselines: `.\scripts\refresh-batch-baselines.ps1`.
3. Goal text already has the Setup step that calls `point-junctions.ps1` with the worktree path.
4. Fire from anywhere: `Get-Content -Raw scripts\batch-<x>-goal.txt | claude --model claude-opus-4-7 -p`.
5. Agent does its work in the worktree, commits, pushes, then calls `point-junctions.ps1` to restore canonical state.

Run only one batch at a time per machine. Wait for one to land + verify before firing the next.

## Cleanup after merge

After a batch is merged into `main`:

```powershell
git worktree remove C:\GH\ECABridge-w
git branch -d feature/batch-w
```

Restore junction to canonical if needed:

```powershell
.\scripts\point-junctions.ps1 -Target C:\GH\ECABridge
```

## Why this matters — postmortem

On 2026-05-20 a parallel-fire on Theseus without worktree isolation produced a race condition disaster: 4 agents shared one worktree, each ran `git checkout -b feature/batch-<x>` in turn, clobbering whoever's in-progress files were on disk. Net result: only 2 of an expected ~5×5 = 25 commits landed, and only because Batch X had finished its first 2 commits before the others arrived. Every other batch produced zero commits. The worktree pattern documented above prevents this.

See `intelligence/tools/claude-code-goal-command.md` Rule 1+2+3 (in the KB) for the full discipline checklist.
