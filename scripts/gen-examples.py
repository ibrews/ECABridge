#!/usr/bin/env python3
"""Extract example payloads per ECABridge command from scripts/smoke-test.py.

Scans the smoke test's TestCase(...) literals via the `ast` module and writes
`scripts/command-examples.json`, a map of `tool_name -> {arguments: {...},
source: "smoke-test"}`. Future call sites (tutorials, recorded sessions) can
contribute additional examples by writing JSON files into
`scripts/example-sources/` — those are merged on top of the smoke-test pass.

Output schema (per command):
    {
        "arguments": {...},        # most recent example seen for this tool
        "source": "smoke-test"     # which extractor produced it
    }

Intended usage:
    python scripts/gen-examples.py
    cat scripts/command-examples.json | jq '.create_actor'

Pure stdlib; no Unreal Editor required. Run from anywhere — the script
resolves paths relative to its own location.
"""
from __future__ import annotations

import argparse
import ast
import json
import sys
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = SCRIPT_DIR.parent
SMOKE_TEST = SCRIPT_DIR / "smoke-test.py"
EXTRA_DIR = SCRIPT_DIR / "example-sources"
# Default to the plugin's Resources/ folder so the runtime example-injection
# code (FECAMCPServer) can find a tracked, repo-committed copy at startup.
DEFAULT_OUTPUT = PLUGIN_ROOT / "Resources" / "command-examples.json"


def _literal(node: ast.AST) -> Any:
    """Best-effort literal evaluation. Returns None when the node isn't a pure
    literal (e.g. references a name, calls a function). Callers use that as
    "skip this case" because the payload wasn't statically determinable."""
    try:
        return ast.literal_eval(node)
    except (ValueError, SyntaxError):
        return None


def _extract_testcase_kwargs(call: ast.Call) -> dict[str, Any] | None:
    """Pull `tool=<str>` and `args=<dict>` out of a TestCase(...) call.

    smoke-test.py uses positional args: TestCase(name, tool, args, ...). Handle
    that and the keyword form. Returns None when the tool or args aren't pure
    literals (e.g. include `asset` variable references — those test cases get
    a synthetic placeholder example below)."""
    tool: str | None = None
    arguments: Any = None

    if len(call.args) >= 2:
        v = _literal(call.args[1])
        if isinstance(v, str):
            tool = v
    if len(call.args) >= 3:
        arguments = _literal(call.args[2])
        if arguments is None and isinstance(call.args[2], ast.Dict):
            arguments = _dict_with_placeholders(call.args[2])

    for kw in call.keywords:
        if kw.arg == "tool":
            v = _literal(kw.value)
            if isinstance(v, str):
                tool = v
        elif kw.arg == "args":
            arguments = _literal(kw.value)
            if arguments is None and isinstance(kw.value, ast.Dict):
                arguments = _dict_with_placeholders(kw.value)

    if tool is None:
        return None
    if arguments is None:
        arguments = {}
    if not isinstance(arguments, dict):
        return None
    return {"tool": tool, "arguments": arguments}


def _dict_with_placeholders(node: ast.Dict) -> dict[str, Any] | None:
    """Convert a Dict AST that contains non-literal values (e.g. variable refs)
    into a dict using "<placeholder>" for the non-literal entries. Preserves
    the shape so examples remain useful even when smoke-test.py parameterizes
    arguments by a function-arg."""
    out: dict[str, Any] = {}
    for k, v in zip(node.keys, node.values):
        if not isinstance(k, ast.Constant) or not isinstance(k.value, str):
            return None
        val = _literal(v)
        if val is None and not isinstance(v, ast.Constant):
            val = "<placeholder>"
        elif isinstance(v, ast.Constant) and v.value is None:
            val = None
        out[k.value] = val
    return out


def parse_smoke_test(path: Path) -> dict[str, dict[str, Any]]:
    """Walk smoke-test.py looking for TestCase(...) calls; emit one example
    per unique tool name (last wins, mirroring author intent)."""
    if not path.exists():
        print(f"[warn] {path} not found — no smoke-test examples extracted", file=sys.stderr)
        return {}
    tree = ast.parse(path.read_text(encoding="utf-8"))
    examples: dict[str, dict[str, Any]] = {}
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        func = node.func
        name = func.id if isinstance(func, ast.Name) else (
            func.attr if isinstance(func, ast.Attribute) else None
        )
        if name != "TestCase":
            continue
        kw = _extract_testcase_kwargs(node)
        if kw is None:
            continue
        examples[kw["tool"]] = {"arguments": kw["arguments"], "source": "smoke-test"}
    return examples


def load_extra_sources(directory: Path) -> dict[str, dict[str, Any]]:
    """Merge any *.json files under scripts/example-sources/ — each is a map
    of tool_name -> {arguments: {...}, source?: "..."}. Later files override
    earlier alphabetically; smoke-test examples are the floor."""
    if not directory.exists():
        return {}
    merged: dict[str, dict[str, Any]] = {}
    for path in sorted(directory.glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            print(f"[warn] skipping {path.name}: {e}", file=sys.stderr)
            continue
        if not isinstance(data, dict):
            continue
        for tool, entry in data.items():
            if not isinstance(entry, dict) or "arguments" not in entry:
                continue
            entry.setdefault("source", path.stem)
            merged[tool] = entry
    return merged


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                   help=f"output JSON path (default: {DEFAULT_OUTPUT.name})")
    p.add_argument("--smoke-test", type=Path, default=SMOKE_TEST,
                   help="smoke-test.py to extract from")
    p.add_argument("--extra-dir", type=Path, default=EXTRA_DIR,
                   help="optional dir of *.json files to merge on top")
    p.add_argument("--quiet", action="store_true")
    args = p.parse_args()

    examples = parse_smoke_test(args.smoke_test)
    extras = load_extra_sources(args.extra_dir)
    examples.update(extras)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(examples, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    if not args.quiet:
        print(f"wrote {len(examples)} example(s) -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
