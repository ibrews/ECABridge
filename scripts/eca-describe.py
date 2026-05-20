#!/usr/bin/env python3
"""Pretty-print info for one ECABridge command.

Faster than scanning the full ~330 KB `tools/list` payload: fetch once,
filter to one entry, and render description + input/output schema +
example side by side. Optional fuzzy matching surfaces likely-intended
names when an exact match is missing.

Usage:
    python scripts/eca-describe.py create_actor
    python scripts/eca-describe.py --json dump_blueprint_graph
    python scripts/eca-describe.py --search blueprint   # name substring
    python scripts/eca-describe.py --from-file scripts/tools-list.json find_assets

Pure stdlib; uses scripts/eca_client.py for the HTTP layer if a server
is reachable, otherwise falls back to a cached --from-file dump.
"""
from __future__ import annotations

import argparse
import difflib
import json
import sys
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_URL = "http://127.0.0.1:3000/mcp"
DEFAULT_EXAMPLES = SCRIPT_DIR / "command-examples.json"

# Local import. Done lazily so --from-file works even without urllib.
sys.path.insert(0, str(SCRIPT_DIR))


def load_tools_from_file(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict) and "result" in data:
        return data["result"].get("tools", []) or []
    if isinstance(data, dict) and "tools" in data:
        return data["tools"] or []
    if isinstance(data, list):
        return data
    raise SystemExit(f"{path}: unexpected JSON shape")


def fetch_tools(url: str, timeout: float) -> list[dict[str, Any]]:
    from eca_client import ECAClient  # local import
    return ECAClient(url, timeout=timeout).tools()


def render_text(tool: dict[str, Any], example: dict[str, Any] | None,
                stream: Any = sys.stdout) -> None:
    name = tool.get("name", "?")
    print(f"\n  \033[36m{name}\033[0m", file=stream)
    desc = tool.get("description", "").strip()
    if desc:
        for line in desc.splitlines():
            print(f"    {line}", file=stream)
    schema = tool.get("inputSchema")
    if schema:
        print("\n  input schema:", file=stream)
        for line in json.dumps(schema, indent=2).splitlines():
            print(f"    {line}", file=stream)
    out_schema = tool.get("outputSchema")
    if out_schema:
        print("\n  output schema:", file=stream)
        for line in json.dumps(out_schema, indent=2).splitlines():
            print(f"    {line}", file=stream)
    if example:
        print("\n  example arguments:", file=stream)
        for line in json.dumps(example.get("arguments", {}), indent=2).splitlines():
            print(f"    {line}", file=stream)
        src = example.get("source")
        if src:
            print(f"    (source: {src})", file=stream)
    print(file=stream)


def find_command(tools: list[dict[str, Any]], name: str
                 ) -> tuple[dict[str, Any] | None, list[str]]:
    """Return (exact-match-tool-or-None, fuzzy-suggestions)."""
    by_name = {t.get("name", ""): t for t in tools}
    if name in by_name:
        return by_name[name], []
    candidates = list(by_name.keys())
    matches = difflib.get_close_matches(name, candidates, n=5, cutoff=0.55)
    if not matches:
        # Fall back to substring contains — useful for typos like "actor_get".
        lower = name.lower()
        matches = [n for n in candidates if lower in n.lower()][:5]
    return None, matches


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("name", nargs="?", help="command name to describe")
    p.add_argument("--url", default=DEFAULT_URL, help="MCP endpoint")
    p.add_argument("--from-file", type=Path,
                   help="read tools/list from a cached JSON dump instead of the server")
    p.add_argument("--examples", type=Path, default=DEFAULT_EXAMPLES,
                   help="examples JSON produced by gen-examples.py")
    p.add_argument("--search", action="store_true",
                   help="treat NAME as a substring filter; list every match")
    p.add_argument("--json", action="store_true",
                   help="emit the raw tool entry (+ example) as JSON")
    p.add_argument("--timeout", type=float, default=30.0)
    args = p.parse_args()

    if not args.name:
        p.error("name is required (or pass --search with a substring)")

    if args.from_file:
        tools = load_tools_from_file(args.from_file)
    else:
        try:
            tools = fetch_tools(args.url, args.timeout)
        except Exception as e:  # broad: covers transport + import errors
            print(f"[fail] could not fetch tools/list from {args.url}: {e}",
                  file=sys.stderr)
            print("  hint: pass --from-file to read from a cached dump",
                  file=sys.stderr)
            return 2

    examples: dict[str, dict[str, Any]] = {}
    if args.examples.exists():
        try:
            examples = json.loads(args.examples.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            print(f"[warn] {args.examples}: {e}", file=sys.stderr)

    if args.search:
        needle = args.name.lower()
        hits = [t for t in tools if needle in t.get("name", "").lower()
                                  or needle in t.get("description", "").lower()]
        if args.json:
            print(json.dumps([{**t, "_example": examples.get(t.get("name", ""))}
                              for t in hits], indent=2))
            return 0
        if not hits:
            print(f"no commands match: {args.name}", file=sys.stderr)
            return 1
        for t in hits:
            render_text(t, examples.get(t.get("name", "")))
        return 0

    tool, suggestions = find_command(tools, args.name)
    if tool is None:
        print(f"unknown command: {args.name}", file=sys.stderr)
        if suggestions:
            print(f"did you mean: {', '.join(suggestions)}?", file=sys.stderr)
        return 1

    if args.json:
        payload = {**tool}
        ex = examples.get(tool.get("name", ""))
        if ex:
            payload["_example"] = ex
        print(json.dumps(payload, indent=2))
        return 0

    render_text(tool, examples.get(tool.get("name", "")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
