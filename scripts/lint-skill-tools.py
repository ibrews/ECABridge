#!/usr/bin/env python3
"""Cross-check the ue5-mcp field manual against the actual command surface.

ECABridge registers commands with `REGISTER_ECA_COMMAND(FECACommand_X)` in
`Source/ECABridge/Private/Commands/*.cpp`. The wire-level command name
("get_actors_in_level", ...) lives on the same class as
`virtual FString GetName() const override { return TEXT("..."); }` in the
matching header. This script extracts the (class -> wire-name) mapping by
parsing the headers, then optionally compares it to backtick-quoted tool
names in a skill markdown file (e.g. docs/skill/SKILL.md).

The failure mode this catches: a skill rev that mentions a tool the plugin
no longer exposes (renamed, deleted, or never shipped). Runs in CI on
every push so a stale skill can't slip through.

Usage:
    # Just list every registered command, one per line, sorted:
    python scripts/lint-skill-tools.py --list

    # Compare a skill against the registry. Exits non-zero on mismatches.
    python scripts/lint-skill-tools.py --skill docs/skill/SKILL.md

    # Same, but treat mismatches as warnings (exit zero):
    python scripts/lint-skill-tools.py --skill docs/skill/SKILL.md --warn-only

Exit codes: 0 ok / 1 mismatches found / 2 invocation error.

Pure stdlib; no UE required.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = REPO_ROOT / "Source" / "ECABridge"

# REGISTER_ECA_COMMAND(FECACommand_Foo) — captures the class identifier.
REGISTER_RE = re.compile(r"REGISTER_ECA_COMMAND\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")

# In headers, the wire-level name lives on a one-liner like:
#   virtual FString GetName() const override { return TEXT("get_actors_in_level"); }
# (occasionally with extra whitespace or different quoting). We pin the
# class via a preceding `class FECACommand_X` line.
CLASS_RE = re.compile(r"^class\s+([A-Za-z_][A-Za-z0-9_]*)\b")
GETNAME_RE = re.compile(r"GetName\(\)\s*const(?:\s+override)?\s*\{\s*return\s+TEXT\(\"([a-z0-9_]+)\"\)")

# In skill markdown, treat backtick-quoted snake_case identifiers as tool
# references. Filter to ones that look like command names (lowercase +
# underscores + at least one underscore so we skip bare words like `true`).
SKILL_TOKEN_RE = re.compile(r"`([a-z][a-z0-9_]*_[a-z0-9_]+)`")


def find_register_calls() -> set[str]:
    """Return the set of class identifiers passed to REGISTER_ECA_COMMAND."""
    classes: set[str] = set()
    for cpp in SOURCE_DIR.rglob("*.cpp"):
        text = cpp.read_text(encoding="utf-8", errors="replace")
        for m in REGISTER_RE.finditer(text):
            classes.add(m.group(1))
    return classes


def build_class_to_name() -> dict[str, str]:
    """Walk headers and cpps, return {class_identifier: wire_name}.

    Most commands live in headers, but a few (e.g. event-queue commands)
    declare the class inline in the .cpp file, so both are scanned.
    """
    mapping: dict[str, str] = {}
    for path in (*SOURCE_DIR.rglob("*.h"), *SOURCE_DIR.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        current_class: str | None = None
        for line in text.splitlines():
            cm = CLASS_RE.match(line.strip())
            if cm:
                current_class = cm.group(1)
                continue
            if current_class is None:
                continue
            gm = GETNAME_RE.search(line)
            if gm:
                mapping[current_class] = gm.group(1)
                current_class = None
    return mapping


def registered_command_names() -> tuple[set[str], list[str]]:
    """Return (command_names, warnings)."""
    classes = find_register_calls()
    mapping = build_class_to_name()
    names: set[str] = set()
    warnings: list[str] = []
    for cls in sorted(classes):
        if cls in mapping:
            names.add(mapping[cls])
        else:
            warnings.append(f"REGISTER_ECA_COMMAND({cls}): no GetName() found in any header")
    return names, warnings


def tools_referenced_in_skill(skill_path: Path) -> set[str]:
    text = skill_path.read_text(encoding="utf-8", errors="replace")
    return set(SKILL_TOKEN_RE.findall(text))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="Print every registered command name, sorted, then exit.")
    parser.add_argument("--skill", type=Path, help="Path to a skill markdown file to cross-check.")
    parser.add_argument("--warn-only", action="store_true", help="Exit 0 even if mismatches are found.")
    parser.add_argument("--ignore", action="append", default=[], help="Token to ignore in skill cross-check (repeatable).")
    args = parser.parse_args(argv)

    if not args.list and not args.skill:
        parser.error("specify --list or --skill (or both)")

    names, warnings = registered_command_names()

    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)

    if args.list:
        for n in sorted(names):
            print(n)

    if args.skill:
        if not args.skill.exists():
            print(f"error: skill file not found: {args.skill}", file=sys.stderr)
            return 2
        referenced = tools_referenced_in_skill(args.skill)
        ignored = set(args.ignore)
        missing = sorted((referenced - names) - ignored)
        if missing:
            print(f"\n{len(missing)} skill-referenced tool(s) not registered in ECABridge:", file=sys.stderr)
            for m in missing:
                print(f"  - {m}", file=sys.stderr)
            print(
                "\nFix by renaming the skill reference, adding the command, or passing --ignore <name>.",
                file=sys.stderr,
            )
            if not args.warn_only:
                return 1
        else:
            print(f"\nskill {args.skill}: all {len(referenced)} referenced tools exist in the registry.", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
