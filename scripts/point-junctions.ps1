# Repoint the Test{57,58} plugin junctions at a specified ECABridge worktree.
# Used by batch agents to isolate builds when multiple worktrees exist for parallel
# feature/batch-* branches. After the batch finishes, call again with the canonical
# repo path to restore the default state.
#
# Usage:
#   .\point-junctions.ps1 -Target C:\GH\ECABridge-w
#   .\point-junctions.ps1 -Target C:\GH\ECABridge          # restore canonical
#   .\point-junctions.ps1 -Target D:\GH\ECABridge-v -JunctionBase D:\UE   # Fort
#
# Each call removes any existing junction at <JunctionBase>\Test<ver>\Plugins\ECABridge
# and creates a new one. The target path must exist and contain the .uplugin.

param(
    [Parameter(Mandatory=$true)] [string]$Target,
    [string]$JunctionBase = "C:\UE",
    [string[]]$Engines = @('5.7','5.8')
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path "$Target\ECABridge.uplugin")) {
    throw "Target $Target does not contain ECABridge.uplugin"
}

foreach ($e in $Engines) {
    $ver = $e.Replace('.','')
    $junctionDir = "$JunctionBase\Test$ver\Plugins"
    $junction = "$junctionDir\ECABridge"
    if (-not (Test-Path $junctionDir)) {
        Write-Warning "$junctionDir does not exist - skipping engine $e"
        continue
    }
    if (Test-Path $junction) {
        Remove-Item $junction -Force -Recurse
    }
    New-Item -ItemType Junction -Path $junction -Target $Target | Out-Null
    $resolved = (Get-Item $junction).Target
    Write-Host "$junction -> $resolved"
}
