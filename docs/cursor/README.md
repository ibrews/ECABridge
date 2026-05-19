# Cursor integration

Cursor reads MCP servers from `~/.cursor/mcp.json` (global) or `<project>/.cursor/mcp.json` (per-workspace). The format is identical to Claude Desktop's `claude_desktop_config.json` but lives in Cursor's own settings directory.

## Setup

### Global (every Cursor workspace)

```bash
# macOS / Linux
mkdir -p ~/.cursor
cp /path/to/ECABridge/docs/cursor/mcp.json ~/.cursor/mcp.json

# Windows (PowerShell)
New-Item -ItemType Directory -Force "$env:USERPROFILE\.cursor" | Out-Null
Copy-Item .\docs\cursor\mcp.json "$env:USERPROFILE\.cursor\mcp.json"
```

### Per-project

```bash
mkdir -p .cursor
cp /path/to/ECABridge/docs/cursor/mcp.json .cursor/mcp.json
```

Then in Cursor: **Settings → Features → MCP**. ECABridge should appear in the server list. Toggle it on. If you're using Cursor's agent / Composer, the toolset is available immediately.

## Config format reference

Cursor's MCP config uses HTTP transport via the top-level `url` field — same shape as Claude Desktop:

```json
{
  "mcpServers": {
    "ecabridge": {
      "url": "http://127.0.0.1:3000/mcp",
      "headers": {
        "Accept": "application/json, text/event-stream"
      }
    }
  }
}
```

| Field | Required | Notes |
|---|---|---|
| `url` | yes | ECABridge's MCP endpoint. Change port if you've overridden it in Project Settings. |
| `headers.Accept` | yes | ECABridge enforces `application/json, text/event-stream` on POST `/mcp`. |
| `headers.Authorization` | no | ECABridge has no auth by default — local-only loopback. Set if you've added a reverse proxy. |

## Stdio variant (for older Cursor versions)

If your Cursor build predates HTTP MCP support, wrap the HTTP server in a stdio bridge:

```json
{
  "mcpServers": {
    "ecabridge": {
      "command": "npx",
      "args": ["-y", "mcp-remote", "http://127.0.0.1:3000/mcp"]
    }
  }
}
```

`mcp-remote` is a published npm package that bridges Cursor's stdio MCP client to an HTTP-transport server. Slower than native HTTP, but works on every Cursor version.

## Coexistence with native UE 5.8 MCP

Register both Epic's native MCP and ECABridge — Cursor merges toolsets and the agent picks the right one per request:

```json
{
  "mcpServers": {
    "ecabridge":     { "url": "http://127.0.0.1:3000/mcp", "headers": { "Accept": "application/json, text/event-stream" } },
    "unreal-native": { "url": "http://127.0.0.1:8000/mcp", "headers": { "Accept": "application/json, text/event-stream" } }
  }
}
```

## Verifying

1. Start UE with ECABridge enabled. Confirm `curl http://127.0.0.1:3000/health` returns a JSON with `commands` > 0.
2. Open Cursor, hit `Cmd/Ctrl-K` for the agent prompt, and ask: *"What's the actor count in the current UE level?"* Cursor should call ECABridge's `find_actors` or `get_scene_stats`.
3. If you don't see ECABridge in the MCP server list, restart Cursor — config is read on startup.

## Troubleshooting

- **"Connection refused"** — the editor isn't running or ECABridge failed to load. Check `Saved/Logs/<Project>.log` for `ECABridge: MCP server listening on :3000`.
- **"Schema validation error"** — Cursor sometimes rewrites tool inputs. ECABridge returns the full JSON Schema in the error response — paste it back to the agent and ask it to retry with corrected arguments.
- **Cursor hangs on first call** — Cursor caches the tool list on first connect. Large surfaces (~400 tools, ~330 KB) can take ~1 s. Subsequent calls are fast.
