# detect-ecabridge.ps1 — probe a running ECABridge server and write a VS Code MCP config.
#
# Usage:
#   powershell -File detect-ecabridge.ps1 [-Port 3000] [-OutFile .vscode/mcp.json]
#
# Exit codes:
#   0 — server detected, config written
#   1 — server not reachable on the given port
#   2 — config write failed

param(
    [int]$Port = 3000,
    [string]$OutFile = ".vscode/mcp.json"
)

$ErrorActionPreference = 'Stop'
$healthUrl = "http://127.0.0.1:$Port/health"

try {
    $resp = Invoke-RestMethod -Uri $healthUrl -TimeoutSec 3 -Method Get
    if (-not $resp.commands) {
        Write-Error "Server on :$Port responded but is not ECABridge (no 'commands' field in /health)."
        exit 1
    }
    Write-Host "Detected ECABridge on :$Port with $($resp.commands) commands."
} catch {
    Write-Host "No ECABridge server on :$Port — start the UE editor with the plugin loaded first." -ForegroundColor Yellow
    exit 1
}

$config = @{
    '$schema' = 'https://aka.ms/vscode-mcp-schema'
    servers = @{
        ecabridge = @{
            type = 'http'
            url = "http://127.0.0.1:$Port/mcp"
            headers = @{
                Accept = 'application/json, text/event-stream'
            }
        }
    }
}

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

try {
    $config | ConvertTo-Json -Depth 6 | Out-File -FilePath $OutFile -Encoding utf8
    Write-Host "Wrote $OutFile — reload the VS Code window to pick up the new MCP server."
} catch {
    Write-Error "Failed to write $OutFile : $_"
    exit 2
}
