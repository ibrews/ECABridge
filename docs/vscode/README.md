# VS Code integration

ECABridge plugs straight into VS Code's MCP UI (GitHub Copilot Chat + the built-in MCP client introduced in VS Code 1.94+). No extension to install — VS Code reads MCP servers from a JSON config file, the same way it reads launch configurations.

## One-step setup (workspace-scoped)

Drop `mcp.json` into your project's `.vscode/` directory:

```bash
mkdir -p .vscode
cp /path/to/ECABridge/docs/vscode/mcp.json .vscode/mcp.json
```

Open the Copilot Chat panel, switch to **Agent** mode, and you should see ECABridge's tools listed in the tool picker. The first connection will probe `http://127.0.0.1:3000/mcp` — if the UE editor is running with the plugin loaded, you're done.

## User-scoped (every workspace)

If you want ECABridge available in every project without per-repo config, put the same content into your user-level MCP file:

| OS | Path |
|---|---|
| Windows | `%APPDATA%\Code\User\mcp.json` |
| macOS | `~/Library/Application Support/Code/User/mcp.json` |
| Linux | `~/.config/Code/User/mcp.json` |

VS Code merges user and workspace MCP servers — workspace entries win on a name collision.

## Verifying

1. Launch UE 5.7 or 5.8 with ECABridge enabled. Confirm the server is up:
   ```bash
   curl -s http://127.0.0.1:3000/health
   ```
   The response includes the live command count (`"commands": 402` on a fully-loaded 5.8 project).
2. In VS Code, open Copilot Chat, switch to **Agent** mode, and pick the `ecabridge` server from the tool picker.
3. Ask: *"List the actors in the current level."* Copilot will call `dump_level` or `find_actors` and stream back JSON.

## Coexistence with native UE 5.8 MCP

If you're also running Epic's native `ModelContextProtocol` plugin on `:8000`, register both:

```json
{
  "servers": {
    "ecabridge": { "type": "http", "url": "http://127.0.0.1:3000/mcp" },
    "unreal-native": { "type": "http", "url": "http://127.0.0.1:8000/mcp" }
  }
}
```

VS Code will surface both toolsets; Copilot picks the right tool per request. ECABridge adds the `dump_*` Rosetta Stone, MetaHuman, Niagara, MetaSound, MVVM, PCG, and refactoring surfaces that the native plugin doesn't cover.

## Troubleshooting

- **"No tools found"** — confirm the editor is running and `curl http://127.0.0.1:3000/health` returns 200. VS Code only probes on chat start; reload the window after starting the editor.
- **Port collision** — change ECABridge's port via `Project Settings → Plugins → ECABridge → MCP Port`, then update the `url` field above.
- **`Accept` header errors** — VS Code's built-in client sends the right headers; if you're proxying through a custom transport, ECABridge requires `Accept: application/json, text/event-stream` on POST `/mcp`.

## Auto-detect helper (optional)

For a zero-config experience across multiple machines, ship the snippet above as a workspace recommendation. The `docs/vscode/detect-ecabridge.ps1` helper (Windows) pings `:3000/health` and, on success, writes `.vscode/mcp.json` into the current project. Useful as a `postCreateCommand` in dev-containers or a one-shot bootstrap:

```powershell
powershell -File docs/vscode/detect-ecabridge.ps1 -Port 3000 -OutFile .vscode/mcp.json
```
