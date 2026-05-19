# EDA (Epic Developer Assistant) integration

ECABridge speaks MCP over Streamable HTTP on `http://127.0.0.1:3000/mcp` (default port). To plug it into the Epic Developer Assistant — the built-in AI panel in UE 5.8's `ModelContextProtocol` plugin — add ECABridge as a toolset server in EDA's `MCPToolsetSettings.json`.

## Configuration

EDA reads MCP toolset servers from a JSON config in the project's `Saved/Config/` (or `Config/` for source builds). Add ECABridge alongside the built-in toolsets:

```json
{
  "Toolsets": [
    {
      "Name": "ECABridge",
      "ServerUrl": "http://127.0.0.1:3000/mcp",
      "Transport": "StreamableHTTP"
    }
  ]
}
```

Field reference:

| Field | Value | Notes |
|---|---|---|
| `Name` | `ECABridge` | Free-form label shown in the EDA UI |
| `ServerUrl` | `http://127.0.0.1:3000/mcp` | ECABridge's MCP endpoint. Change the port if you've overridden `ECABridgeSettings`. |
| `Transport` | `StreamableHTTP` | The MCP transport. ECABridge uses Streamable HTTP (POST `/mcp` with `Accept: application/json, text/event-stream`). |

Once saved, restart the editor (or whatever EDA's reload mechanism is in your build) and ECABridge's ~400 tools should show up in EDA's tool list. They coexist with the native Epic `ModelContextProtocol` toolsets — ECABridge is on `:3000`, Epic's native server is on `:8000`, so there is no port collision.

## Verifying

After EDA picks up the entry, ask it to list tools or describe one of ECABridge's commands (`dump_blueprint_graph`, `dump_level`, `find_assets`, etc.). EDA's tool catalog should include them. You can also sanity-check ECABridge directly via curl:

```bash
curl -s -X POST http://127.0.0.1:3000/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"tools/list","id":1,"params":{}}' | head -c 2000
```

If the server responds with a list of tools, the EDA registration just needs the JSON above to surface them in the assistant.

## Coexistence with Epic's native MCP plugin

ECABridge and the native UE 5.8 `ModelContextProtocol` plugin run on different ports and do not conflict. Configure both in `MCPToolsetSettings.json` and they will appear together:

```json
{
  "Toolsets": [
    { "Name": "EpicNative", "ServerUrl": "http://127.0.0.1:8000/mcp", "Transport": "StreamableHTTP" },
    { "Name": "ECABridge", "ServerUrl": "http://127.0.0.1:3000/mcp", "Transport": "StreamableHTTP" }
  ]
}
```

The native plugin covers core editor toolsets (actor / mesh / blueprint / scene / object / primitive / texture / data tables) plus a Python sandbox. ECABridge adds Niagara, MetaSound, MetaHuman, Mutable, Sequencer, the `dump_*` "Rosetta Stone" JSON serializers, refactoring tools, and an MVVM / UMG surface. Use both side-by-side and let the assistant pick the right tool per task.
