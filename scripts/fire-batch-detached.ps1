# Fires a batch goal file as a fully-detached background process. Returns
# immediately with a PID. Use when you want claude -p to keep running after
# the launching shell (or SSH session) disconnects.
#
# Logs go to <repo>/scripts/.batch-logs/batch-<X>-<timestamp>-{stdout,stderr,fire}.{log,ps1}
# Tail the stdout log to monitor progress.
#
# Usage:
#   .\fire-batch-detached.ps1 -GoalFile C:\GH\ECABridge\scripts\batch-w-goal.txt
#   .\fire-batch-detached.ps1 -GoalFile D:\GH\ECABridge\scripts\batch-v-goal.txt -Model claude-opus-4-7

param(
    [Parameter(Mandatory=$true)] [string]$GoalFile,
    [string]$Model = "claude-opus-4-7"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $GoalFile)) { throw "Goal file not found: $GoalFile" }

$base = Split-Path $GoalFile -Leaf
$batchTag = $base -replace '^batch-','' -replace '-goal\.txt$',''
$scriptsDir = Split-Path $GoalFile -Parent
$logDir = Join-Path $scriptsDir ".batch-logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

$ts = Get-Date -Format "yyyyMMdd-HHmmss"
$stdout = Join-Path $logDir "batch-$batchTag-$ts-stdout.log"
$stderr = Join-Path $logDir "batch-$batchTag-$ts-stderr.log"
$inner  = Join-Path $logDir "batch-$batchTag-$ts-fire.ps1"

# Inner script body — pipes goal to claude via stdin (avoids argv parser
# issues with goal text containing dashes / commas / quotes).
$innerBody = @"
`$goal = Get-Content -Raw '$($GoalFile -replace "'","''")'
`$goal | claude --model $Model -p
"@

Set-Content -Path $inner -Value $innerBody -NoNewline -Encoding UTF8

# Spawn detached. -WindowStyle Hidden + redirects = truly background.
$proc = Start-Process -FilePath powershell `
    -ArgumentList "-NoProfile","-WindowStyle","Hidden","-File",$inner `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -WindowStyle Hidden `
    -PassThru

Write-Host "Fired batch '$batchTag' (pid=$($proc.Id), parent-pid=$PID)"
Write-Host "  goal:   $GoalFile"
Write-Host "  stdout: $stdout"
Write-Host "  stderr: $stderr"
Write-Host "  inner:  $inner"
Write-Host "Monitor: Get-Content -Tail 20 -Wait '$stdout'"
