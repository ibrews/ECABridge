#!/usr/bin/env python3
"""ECABridge smoke test.

Exercises ~20 commands across the major categories against a running
ECABridge MCP server (default http://127.0.0.1:3000/mcp). Asserts response
shape - tools return content[0].text parseable as JSON with success=True
and the expected top-level fields. Not CI-wired; intended as a fast manual
sanity check after a deploy.

Usage:
    python scripts/smoke-test.py [--url URL] [--asset BLUEPRINT_PATH]

Exits non-zero if any check fails.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Any


DEFAULT_URL = "http://127.0.0.1:3000/mcp"
DEFAULT_BLUEPRINT = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"


@dataclass
class TestCase:
    name: str
    tool: str
    args: dict[str, Any]
    must_have_keys: tuple[str, ...] = ()
    expect_isError: bool = False
    expect_image: bool = False
    custom_check: Any = None


def call(url: str, tool: str, args: dict[str, Any], timeout: float = 60.0) -> dict[str, Any]:
    body = json.dumps({
        "jsonrpc": "2.0",
        "method": "tools/call",
        "id": 1,
        "params": {"name": tool, "arguments": args},
    }).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def health(url: str) -> dict[str, Any]:
    base = url.rsplit("/", 1)[0] + "/health"
    with urllib.request.urlopen(base, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


def list_tools(url: str) -> set[str]:
    """Return the set of registered tool names from tools/list.

    Used to skip-rather-than-fail TestCases whose tool isn't in the registry —
    e.g. optional-dep-gated commands like the MetaHuman category on a UE 5.7
    install that doesn't ship MetaHumanCharacter.
    """
    body = json.dumps({"jsonrpc": "2.0", "method": "tools/list", "id": 1, "params": {}}).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", "Accept": "application/json, text/event-stream"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        payload = json.loads(resp.read().decode("utf-8"))
    tools = payload.get("result", {}).get("tools", [])
    return {t.get("name") for t in tools if t.get("name")}


def build_cases(asset: str) -> list[TestCase]:
    return [
        # Actor introspection
        TestCase("actor.get_actors_in_level", "get_actors_in_level", {},
                 must_have_keys=("actors", "count")),
        TestCase("actor.find_actors", "find_actors", {"name_pattern": "*"},
                 must_have_keys=("actors", "count")),
        TestCase("editor.get_selected_actors", "get_selected_actors", {},
                 must_have_keys=("actors", "count")),
        TestCase("editor.get_level_info", "get_level_info", {},
                 must_have_keys=("total_actors", "actor_counts")),
        # Asset introspection
        TestCase("asset.find_assets",       "find_assets",
                 {"path_filter": "/Game/FirstPerson/", "max_results": 50},
                 must_have_keys=("assets", "total_found")),
        TestCase("asset.dump_asset",        "dump_asset", {"asset_path": asset},
                 must_have_keys=("asset_path", "asset_class")),
        TestCase("asset.get_asset_references", "get_asset_references", {"asset_path": asset},
                 must_have_keys=("dependencies", "referencers")),
        TestCase("asset.list_blueprints",   "list_blueprints", {"path": "/Game/FirstPerson"},
                 must_have_keys=("blueprints", "count")),
        # Blueprint dump (the rosetta stone)
        TestCase("blueprint.dump_blueprint_graph", "dump_blueprint_graph",
                 {"blueprint_path": asset},
                 must_have_keys=("blueprint_path", "graphs", "variables")),
        TestCase("blueprint.get_blueprint_info", "get_blueprint_info",
                 {"blueprint_path": asset},
                 must_have_keys=("name", "parent_class")),
        # Component
        TestCase("component.get_blueprint_components", "get_blueprint_components",
                 {"blueprint_path": asset},
                 must_have_keys=("components", "count")),
        # Material introspection (best-effort - find first material)
        TestCase("material.list_materials", "list_materials",
                 {"path": "/Game", "recursive": True},
                 must_have_keys=("materials", "count")),
        # Sequencer
        TestCase("sequencer.find_assets_levelseq", "find_assets",
                 {"class_filter": "LevelSequence", "max_results": 5}),
        # Niagara
        TestCase("niagara.get_niagara_systems", "get_niagara_systems", {},
                 must_have_keys=("systems", "count")),
        # MetaHuman (gracefully fails if plugin disabled)
        TestCase("metahuman.list_metahuman_presets", "list_metahuman_presets", {}),
        # Mutable / Customizable Object discovery
        TestCase("mutable.list_co_node_types", "list_co_node_types", {}),
        # Editor screenshot (inline base64 image)
        TestCase("screenshot.take_gameplay_screenshot", "take_gameplay_screenshot", {},
                 expect_image=True),
        # Python sandbox - run() -> dict
        TestCase(
            "sandbox.run_dict",
            "execute_script",
            {"script": (
                "def run():\n"
                "    actors = execute_tool('actor', 'get_actors_in_level', {})\n"
                "    return {'mode': 'run_dict', 'actor_count': actors['result']['count']}\n"
            )},
            custom_check=lambda r: (
                isinstance(r.get("command_result"), dict)
                and r["command_result"].get("mode") == "run_dict"
                and isinstance(r["command_result"].get("actor_count"), int)
            ),
        ),
        # Python sandbox - legacy print() (must still work)
        TestCase(
            "sandbox.print_legacy",
            "execute_script",
            {"script": (
                "import json\n"
                "actors = execute_tool('actor', 'get_actors_in_level', {})\n"
                "print(json.dumps({'mode': 'print_legacy', 'count': actors['result']['count']}))\n"
            )},
            custom_check=lambda r: (
                any("print_legacy" in (e.get("output") or "") for e in r.get("log_output", []))
                and r.get("success") is True
            ),
        ),
        # Programmatic toolkit metadata
        TestCase("sandbox.get_execution_environment", "get_execution_environment", {},
                 must_have_keys=("instructions", "preamble", "commands", "command_count")),
    ]


@dataclass
class Outcome:
    name: str
    ok: bool
    detail: str = ""
    elapsed_ms: int = 0
    skipped: bool = False


def run_case(url: str, c: TestCase) -> Outcome:
    started = time.time()
    try:
        resp = call(url, c.tool, c.args)
    except (urllib.error.URLError, OSError, json.JSONDecodeError) as e:
        return Outcome(c.name, False, f"transport: {e!r}", int((time.time() - started) * 1000))

    elapsed_ms = int((time.time() - started) * 1000)
    result = resp.get("result")
    if not isinstance(result, dict):
        return Outcome(c.name, False, f"no result object: {resp!r}", elapsed_ms)

    if result.get("isError") and not c.expect_isError:
        return Outcome(c.name, False, f"isError=True: {result.get('content')}", elapsed_ms)

    content = result.get("content", [])
    if not content:
        return Outcome(c.name, False, "empty content array", elapsed_ms)

    if c.expect_image:
        if not any(blk.get("type") == "image" for blk in content):
            return Outcome(c.name, False, f"no image content block: {[b.get('type') for b in content]}", elapsed_ms)
        return Outcome(c.name, True, "image content block present", elapsed_ms)

    text_block = next((b for b in content if b.get("type") == "text"), None)
    if not text_block:
        return Outcome(c.name, False, "no text block", elapsed_ms)

    try:
        parsed = json.loads(text_block["text"])
    except json.JSONDecodeError as e:
        return Outcome(c.name, False, f"text is not JSON: {e}", elapsed_ms)

    if parsed.get("success") is False:
        return Outcome(c.name, False, f"success=False: {parsed.get('error')}", elapsed_ms)

    payload = parsed.get("result", parsed)
    for k in c.must_have_keys:
        if k not in payload:
            return Outcome(c.name, False, f"missing key '{k}' in result", elapsed_ms)

    if c.custom_check is not None:
        try:
            if not c.custom_check(payload):
                return Outcome(c.name, False, "custom_check returned False", elapsed_ms)
        except Exception as e:
            return Outcome(c.name, False, f"custom_check raised: {e!r}", elapsed_ms)

    return Outcome(c.name, True, "", elapsed_ms)


def main() -> int:
    parser = argparse.ArgumentParser(description="ECABridge smoke test")
    parser.add_argument("--url", default=DEFAULT_URL, help="MCP endpoint")
    parser.add_argument("--asset", default=DEFAULT_BLUEPRINT,
                        help="Blueprint asset path used for introspection checks")
    parser.add_argument("--quiet", action="store_true", help="Only print failures + summary")
    args = parser.parse_args()

    try:
        h = health(args.url)
        if not h.get("bridge_ready"):
            print(f"[fail] /health says bridge_ready=False: {h}", file=sys.stderr)
            return 2
        if not args.quiet:
            print(f"server up: {h.get('commands')} commands, bridge_ready=True")
    except (urllib.error.URLError, OSError) as e:
        print(f"[fail] cannot reach {args.url}: {e}", file=sys.stderr)
        return 2

    try:
        registry = list_tools(args.url)
    except (urllib.error.URLError, OSError, json.JSONDecodeError) as e:
        print(f"[fail] tools/list failed: {e!r}", file=sys.stderr)
        return 2

    cases = build_cases(args.asset)
    outcomes: list[Outcome] = []
    for c in cases:
        if c.tool not in registry:
            o = Outcome(c.name, ok=True, skipped=True,
                        detail=f"tool '{c.tool}' not in registry (optional-dep-gated out)")
            outcomes.append(o)
            if not args.quiet:
                print(f"  skip {o.name}  - {o.detail}")
            continue
        o = run_case(args.url, c)
        outcomes.append(o)
        if o.ok:
            if not args.quiet:
                print(f"  ok   {o.name}  ({o.elapsed_ms} ms)")
        else:
            print(f"  FAIL {o.name}  ({o.elapsed_ms} ms) - {o.detail}", file=sys.stderr)

    failed  = [o for o in outcomes if not o.ok]
    skipped = [o for o in outcomes if o.skipped]
    passed  = [o for o in outcomes if o.ok and not o.skipped]
    summary = f"\n{len(passed)}/{len(outcomes)} passed"
    if skipped:
        summary += f", {len(skipped)} skipped"
    print(summary)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
