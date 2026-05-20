# Refreshes the "ahead of <hash>" baseline in each batch-*-goal.txt to match
# current origin/main of the specified ECABridge repo. Run right before firing.
#
# Usage:
#   .\refresh-batch-baselines.ps1                          # Theseus default
#   .\refresh-batch-baselines.ps1 -RepoPath D:\GH\ECABridge # Fort
#
# Idempotent. Reports "already current" if no update needed.

param(
    [string]$RepoPath = "C:\GH\ECABridge",
    [string[]]$Batches = @('w','v','x','y','z')
)

$ErrorActionPreference = "Stop"
$ScriptsDir = Join-Path $RepoPath 'scripts'

git -C $RepoPath fetch --all 2>$null
$head = git -C $RepoPath rev-parse origin/main
if (-not $head) { throw "Could not resolve origin/main HEAD in $RepoPath" }
$short = $head.Substring(0,7)
Write-Host "Target baseline: $short ($head)"

foreach ($b in $Batches) {
    $p = Join-Path $ScriptsDir "batch-$b-goal.txt"
    if (-not (Test-Path $p)) { Write-Host "$p : missing, skipping"; continue }
    $c = Get-Content -Raw $p
    $m = [regex]::Match($c, 'ahead of ([0-9a-f]{40})')
    if (-not $m.Success) { Write-Host "batch-$b : no full hash found, skipping"; continue }
    $oldFull = $m.Groups[1].Value
    if ($oldFull -eq $head) {
        Write-Host "batch-$b : already current ($short)"
        continue
    }
    $oldShort = $oldFull.Substring(0,7)
    $new = $c -replace $oldFull, $head -replace "ahead of $oldShort", "ahead of $short"
    Set-Content -Path $p -Value $new -NoNewline
    Write-Host "batch-$b : refreshed $oldShort -> $short"
}
