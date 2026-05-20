"""Lightweight Python client for ECABridge.

A thin, stdlib-only wrapper over the JSON-RPC-over-HTTP MCP endpoint
(`/mcp` on port 3000 by default). Designed for tutorials, smoke tests,
and quick scripting from the command line — not a high-throughput
library.

Quick start:
    from scripts.eca_client import ECAClient

    client = ECAClient()                      # defaults to :3000
    health = client.health()
    print(f"{health['commands']} commands available")

    # Generic call — works for every registered tool:
    actors = client.call("get_actors_in_level")
    print(actors["count"])

    # Category accessors mirror the canonical command groups:
    print(client.actors.find("*").count)      # via .find
    bp = client.blueprints.dump_graph("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")

The category accessors are sugar over `.call()`; nothing is hardcoded —
they forward to whichever tool name is registered. New batches that
add commands work without client changes.
"""
from __future__ import annotations

import json
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any


DEFAULT_URL = "http://127.0.0.1:3000/mcp"


class ECAError(RuntimeError):
    """Raised when an ECABridge call returns `isError: true` or transport fails.

    Attributes:
        tool: The tool name that was called (or None for transport errors).
        payload: The decoded MCP `result` dict, when available.
    """

    def __init__(self, message: str, *, tool: str | None = None,
                 payload: dict[str, Any] | None = None) -> None:
        super().__init__(message)
        self.tool = tool
        self.payload = payload


@dataclass
class _Result(dict):
    """Dict that also allows attribute access for the most-used keys.

    Returned by ECAClient.call() — a thin sugar layer so callers can write
    `actors.count` instead of `actors["count"]`. Full dict semantics still
    work, and unknown keys raise AttributeError (not KeyError) for clarity.
    """

    def __init__(self, data: dict[str, Any]) -> None:
        super().__init__(data)

    def __getattr__(self, name: str) -> Any:
        try:
            return self[name]
        except KeyError as e:
            raise AttributeError(name) from e


class _CategoryProxy:
    """Tiny accessor that turns `client.actors.find("*")` into
    `client.call("find_actors", name_pattern="*")` — convention-driven."""

    def __init__(self, client: "ECAClient", verb_suffix: str) -> None:
        self._client = client
        self._suffix = verb_suffix

    def __getattr__(self, attr: str) -> Any:
        # Map e.g. (.actors).find(...) -> find_actors(...)
        tool = f"{attr}_{self._suffix}"
        def _invoke(**kwargs: Any) -> _Result:
            return self._client.call(tool, **kwargs)
        _invoke.__name__ = f"{self._suffix}.{attr}"
        return _invoke


class ECAClient:
    """ECABridge HTTP client. One instance = one base URL.

    Construct with the MCP endpoint (default :3000) and a per-request
    timeout in seconds. The client is stateless — safe to share across
    threads, no connection pooling.
    """

    def __init__(self, url: str = DEFAULT_URL, *, timeout: float = 60.0) -> None:
        self.url = url
        self.timeout = timeout
        self._next_id = 1
        # Convention accessors. Add more as new categories emerge.
        self.actors = _CategoryProxy(self, "actors")
        self.assets = _CategoryProxy(self, "assets")
        self.blueprints = _CategoryProxy(self, "blueprint")
        self.materials = _CategoryProxy(self, "materials")

    def _post(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        request_id = self._next_id
        self._next_id += 1
        body = json.dumps({
            "jsonrpc": "2.0",
            "method": method,
            "id": request_id,
            "params": params,
        }).encode("utf-8")
        req = urllib.request.Request(
            self.url, data=body,
            headers={
                "Content-Type": "application/json",
                "Accept": "application/json, text/event-stream",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.URLError as e:
            raise ECAError(f"transport error to {self.url}: {e}") from e

    def health(self) -> dict[str, Any]:
        """Hit the `/health` endpoint (NOT `/mcp`). Returns parsed JSON."""
        base = self.url.rsplit("/", 1)[0] + "/health"
        with urllib.request.urlopen(base, timeout=self.timeout) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def tools(self) -> list[dict[str, Any]]:
        """Return the full `tools/list` array. Each entry has `name`,
        `description`, `inputSchema`, optional `outputSchema`."""
        resp = self._post("tools/list", {})
        return resp.get("result", {}).get("tools", []) or []

    def call(self, tool: str, **arguments: Any) -> _Result:
        """Invoke a tool by name with keyword arguments. Returns the parsed
        `result` payload as a _Result (dict + attribute access).

        Raises ECAError if the server returns `isError: true` or the response
        text isn't JSON-decodable as `{success: true, result: {...}}`.
        """
        resp = self._post("tools/call",
                          {"name": tool, "arguments": arguments})
        result = resp.get("result")
        if not isinstance(result, dict):
            raise ECAError(f"{tool}: no result object: {resp!r}", tool=tool)
        if result.get("isError"):
            content = result.get("content", [])
            msg = "; ".join(b.get("text", "") for b in content
                            if isinstance(b, dict) and b.get("type") == "text")
            raise ECAError(f"{tool}: {msg or 'isError=True'}", tool=tool, payload=result)

        content = result.get("content", [])
        # Pull the first text block — that's where ECABridge serializes the
        # structured response. Image-only responses (e.g. screenshots) return
        # the raw result dict so callers can inspect mcpContent themselves.
        for block in content:
            if isinstance(block, dict) and block.get("type") == "text":
                try:
                    parsed = json.loads(block.get("text", ""))
                except json.JSONDecodeError:
                    return _Result({"raw": block.get("text", "")})
                if parsed.get("success") is False:
                    raise ECAError(f"{tool}: {parsed.get('error')}",
                                   tool=tool, payload=result)
                inner = parsed.get("result", parsed)
                if isinstance(inner, dict):
                    return _Result(inner)
                return _Result({"value": inner})
        return _Result(result)


def main() -> int:
    """When run directly: print a health summary. Use as a smoke check."""
    import sys
    client = ECAClient()
    try:
        h = client.health()
    except (urllib.error.URLError, OSError) as e:
        print(f"cannot reach {client.url}: {e}", file=sys.stderr)
        return 2
    print(json.dumps(h, indent=2))
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
