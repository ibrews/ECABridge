#!/usr/bin/env python3
"""Generate a single-file HTML command browser for ECABridge.

Pulls `tools/list` from a running ECABridge server (or a cached JSON file)
and renders `docs/commands.html` — a searchable, filterable page grouping
commands by category. Examples from scripts/command-examples.json are
embedded inline next to each command.

The result is one self-contained HTML file: no external CSS/JS, no
network dependencies after build. Drop it on gh-pages, open it locally,
or e-mail it to a teammate.

Usage:
    # Live server:
    python scripts/gen-docs.py
    # From a cached dump (`scripts/tools-list.json`):
    python scripts/gen-docs.py --from-file scripts/tools-list.json
    # Different output:
    python scripts/gen-docs.py --output docs/commands.html

Pure stdlib; no UE required.
"""
from __future__ import annotations

import argparse
import html
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_URL = "http://127.0.0.1:3000/mcp"
DEFAULT_OUTPUT = REPO_ROOT / "docs" / "commands.html"
DEFAULT_EXAMPLES = SCRIPT_DIR / "command-examples.json"


def fetch_tools_from_server(url: str, timeout: float = 30.0) -> list[dict[str, Any]]:
    body = json.dumps({"jsonrpc": "2.0", "method": "tools/list",
                       "id": 1, "params": {}}).encode("utf-8")
    req = urllib.request.Request(
        url, data=body,
        headers={"Content-Type": "application/json",
                 "Accept": "application/json, text/event-stream"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        payload = json.loads(resp.read().decode("utf-8"))
    return payload.get("result", {}).get("tools", []) or []


def load_examples(path: Path) -> dict[str, dict[str, Any]]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"[warn] {path}: {e}", file=sys.stderr)
        return {}


def _category_of(tool: dict[str, Any]) -> str:
    """tools/list doesn't expose category directly. ECABridge prefixes most
    names with a verb-noun pattern (`add_actor_tag`, `dump_blueprint_graph`).
    Group by the word after the leading verb when possible; otherwise by
    first underscore segment."""
    name = tool.get("name", "")
    if not name:
        return "uncategorized"
    parts = name.split("_")
    if len(parts) >= 2 and parts[0] in {
        "add", "remove", "create", "delete", "set", "get", "list", "dump",
        "find", "spawn", "open", "close", "start", "stop", "execute",
        "save", "load", "import", "export", "build", "compile", "snapshot",
        "validate", "capture", "describe", "render",
    }:
        return parts[1]
    return parts[0]


def build_html(tools: list[dict[str, Any]], examples: dict[str, dict[str, Any]]) -> str:
    """Render a single-file HTML page. Data is embedded as a JSON island that
    the inline script consumes — keeps filtering snappy even with 400+ tools."""
    categories: dict[str, list[dict[str, Any]]] = {}
    for t in tools:
        c = _category_of(t)
        ex = examples.get(t.get("name", ""))
        if ex:
            t = {**t, "_example": ex}
        categories.setdefault(c, []).append(t)
    for c, items in categories.items():
        items.sort(key=lambda x: x.get("name", ""))
    sorted_categories = sorted(categories.items(), key=lambda kv: kv[0])

    data_json = json.dumps(
        [{"category": c, "tools": items} for c, items in sorted_categories],
        indent=None, separators=(",", ":"),
    )
    total = sum(len(v) for v in categories.values())
    safe_data = html.escape(data_json, quote=False).replace("</", "<\\/")

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>ECABridge command reference</title>
<style>
  :root {{
    --bg: #0f1115; --fg: #e6e6e6; --muted: #9aa0a6;
    --accent: #61dafb; --card: #1a1d23; --border: #2a2d33;
    --code-bg: #0a0c10;
  }}
  * {{ box-sizing: border-box; }}
  body {{ background: var(--bg); color: var(--fg); margin: 0;
         font: 14px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI",
                Roboto, Helvetica, Arial, sans-serif; }}
  header {{ position: sticky; top: 0; background: var(--bg);
            border-bottom: 1px solid var(--border); padding: 12px 24px;
            z-index: 10; }}
  header h1 {{ margin: 0 0 8px; font-size: 18px; }}
  header .meta {{ color: var(--muted); font-size: 12px; }}
  .controls {{ display: flex; gap: 8px; margin-top: 8px;
               flex-wrap: wrap; align-items: center; }}
  input[type=search] {{ flex: 1; min-width: 200px; padding: 6px 10px;
                        background: var(--card); color: var(--fg);
                        border: 1px solid var(--border); border-radius: 4px;
                        font-size: 14px; }}
  select {{ background: var(--card); color: var(--fg);
            border: 1px solid var(--border); padding: 6px 10px;
            border-radius: 4px; font-size: 14px; }}
  main {{ padding: 16px 24px; max-width: 1100px; }}
  .category {{ margin: 24px 0 8px; padding-bottom: 4px;
               border-bottom: 1px solid var(--border);
               color: var(--accent); font-size: 13px; text-transform: uppercase;
               letter-spacing: 0.05em; }}
  details.tool {{ background: var(--card); border: 1px solid var(--border);
                  border-radius: 6px; margin: 8px 0; padding: 0 12px; }}
  details.tool > summary {{ padding: 10px 0; cursor: pointer;
                            list-style: none; display: flex;
                            justify-content: space-between; gap: 8px; }}
  details.tool > summary::-webkit-details-marker {{ display: none; }}
  details.tool .name {{ font-family: ui-monospace, "Cascadia Code",
                        Menlo, Consolas, monospace; color: var(--accent); }}
  details.tool .summary-desc {{ color: var(--muted); font-size: 12px;
                                 text-align: right; max-width: 60%;
                                 overflow: hidden; text-overflow: ellipsis;
                                 white-space: nowrap; }}
  details.tool[open] .summary-desc {{ display: none; }}
  .body {{ padding: 8px 0 14px; border-top: 1px solid var(--border); }}
  .body h3 {{ margin: 12px 0 4px; font-size: 12px; text-transform: uppercase;
              color: var(--muted); letter-spacing: 0.05em; }}
  pre {{ background: var(--code-bg); padding: 10px 12px;
         border-radius: 4px; overflow: auto; font-size: 12px;
         font-family: ui-monospace, "Cascadia Code", Menlo, Consolas, monospace; }}
  .footer {{ color: var(--muted); margin: 32px 24px; font-size: 12px; }}
  .empty {{ color: var(--muted); padding: 24px; text-align: center; }}
</style>
</head>
<body>
<header>
  <h1>ECABridge command reference</h1>
  <div class="meta">{total} commands. Generated from <code>tools/list</code>.</div>
  <div class="controls">
    <input id="q" type="search" placeholder="Filter by name, description, or category..." autofocus>
    <select id="cat"><option value="">All categories</option></select>
  </div>
</header>
<main id="out"></main>
<div class="footer">
  ECABridge — <code>github.com/ibrews/ECABridge</code>.
  Built via <code>scripts/gen-docs.py</code>.
</div>
<script id="data" type="application/json">{safe_data}</script>
<script>
(function() {{
  const raw = document.getElementById("data").textContent
    .replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"');
  const groups = JSON.parse(raw);
  const out = document.getElementById("out");
  const q = document.getElementById("q");
  const cat = document.getElementById("cat");

  const catSet = new Set(groups.map(g => g.category));
  Array.from(catSet).sort().forEach(c => {{
    const opt = document.createElement("option");
    opt.value = c; opt.textContent = c;
    cat.appendChild(opt);
  }});

  function escapeHtml(s) {{
    return String(s == null ? "" : s)
      .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }}

  function render() {{
    const term = q.value.trim().toLowerCase();
    const sel = cat.value;
    out.innerHTML = "";
    let shown = 0;
    for (const g of groups) {{
      if (sel && g.category !== sel) continue;
      const items = g.tools.filter(t => {{
        if (!term) return true;
        const blob = (t.name + " " + (t.description || "") + " " + g.category).toLowerCase();
        return blob.includes(term);
      }});
      if (!items.length) continue;
      const h = document.createElement("h2");
      h.className = "category";
      h.textContent = g.category + " (" + items.length + ")";
      out.appendChild(h);
      for (const t of items) {{
        shown++;
        const det = document.createElement("details");
        det.className = "tool";
        const desc = t.description || "";
        det.innerHTML =
          '<summary>' +
            '<span class="name">' + escapeHtml(t.name) + '</span>' +
            '<span class="summary-desc">' + escapeHtml(desc.slice(0, 140)) + '</span>' +
          '</summary>' +
          '<div class="body">' +
            (desc ? '<p>' + escapeHtml(desc) + '</p>' : '') +
            (t.inputSchema ? '<h3>Input schema</h3><pre>' +
              escapeHtml(JSON.stringify(t.inputSchema, null, 2)) + '</pre>' : '') +
            (t.outputSchema ? '<h3>Output schema</h3><pre>' +
              escapeHtml(JSON.stringify(t.outputSchema, null, 2)) + '</pre>' : '') +
            (t._example ? '<h3>Example</h3><pre>' +
              escapeHtml(JSON.stringify(t._example, null, 2)) + '</pre>' : '') +
          '</div>';
        out.appendChild(det);
      }}
    }}
    if (!shown) {{
      const e = document.createElement("div");
      e.className = "empty";
      e.textContent = "No commands match.";
      out.appendChild(e);
    }}
  }}

  q.addEventListener("input", render);
  cat.addEventListener("change", render);
  render();
}})();
</script>
</body>
</html>
"""


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--url", default=DEFAULT_URL,
                   help="ECABridge MCP endpoint to query for tools/list")
    p.add_argument("--from-file", type=Path,
                   help="read tools/list response JSON from a file instead of the server")
    p.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                   help=f"output HTML path (default: docs/{DEFAULT_OUTPUT.name})")
    p.add_argument("--examples", type=Path, default=DEFAULT_EXAMPLES,
                   help="examples JSON produced by gen-examples.py")
    p.add_argument("--quiet", action="store_true")
    args = p.parse_args()

    if args.from_file:
        try:
            data = json.loads(args.from_file.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            print(f"[fail] could not read {args.from_file}: {e}", file=sys.stderr)
            return 2
        if isinstance(data, dict) and "result" in data:
            tools = data["result"].get("tools", []) or []
        elif isinstance(data, dict) and "tools" in data:
            tools = data["tools"] or []
        elif isinstance(data, list):
            tools = data
        else:
            print(f"[fail] {args.from_file}: unexpected JSON shape", file=sys.stderr)
            return 2
    else:
        try:
            tools = fetch_tools_from_server(args.url)
        except (urllib.error.URLError, OSError) as e:
            print(f"[fail] cannot reach {args.url}: {e}", file=sys.stderr)
            print("  hint: pass --from-file to render from a cached tools/list JSON dump",
                  file=sys.stderr)
            return 2

    examples = load_examples(args.examples)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(build_html(tools, examples), encoding="utf-8")
    if not args.quiet:
        print(f"wrote {len(tools)} command(s) -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
