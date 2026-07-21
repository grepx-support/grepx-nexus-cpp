#!/usr/bin/env python3
"""Dumps every table in a Nexus SQLite DB, human-readable — a generic
debug tool for "is the data actually there" questions, so nobody has to
hand-write a one-off sqlite3 script or dig through a GUI plugin for it.

Schema-driven: reads sqlite_master to discover EVERY table that actually
exists (not a hardcoded list), so it stays correct as python/models.py
grows. nexus_rows gets special handling, two ways:
  - grouped and labelled by its table_name column, since that's Nexus's
    logical table (orders, transactions, NewRequest, BackEndServer, ...)
    — "everything's crammed into one SQL table" is exactly the thing
    people expect to see as separate tables and don't.
  - its `fields` column (the wire-encoded blob, e.g.
    "symbol=s:AAPL;qty=i:10;...") gets decoded via nexus_api.codec and
    spread into one column per field, instead of printed as one long
    unreadable string.

Path resolution is identical to create_schema.py/seed_user.py — same
NEXUS_DB_PATH-env-var-first, else-every-known-DB rule — so this always
looks at the same file(s) nexus_server actually reads/writes, not a
guess. Pass a path explicitly to just check one file.

Usage:
    python python/inspect_db.py                # every DB this repo knows about
    python python/inspect_db.py path/to/db      # just that one
    python python/inspect_db.py --table orders  # only that logical/SQL table
    python python/inspect_db.py --html          # also write an HTML report
"""
from __future__ import annotations

import html
import sqlite3
import sys
from pathlib import Path

from create_schema import resolve_db_paths
from nexus_api.codec import decode_fields

REPO_ROOT = Path(__file__).resolve().parent.parent


def safe_decode_fields(raw: str) -> dict:
    """decode_fields() assumes every row was written in the "k=type:v;..."
    wire format the Python SDK produces — but nexus_rows can hold rows
    written by other paths too (the C++ statemachine/audit-log writer, an
    older format, hand-inserted test data, ...) that don't necessarily
    match it. One row like that used to take the whole inspector down;
    now it just shows up as a single _raw column instead of an unhandled
    exception, so one odd row never hides everything else in the table.
    """
    try:
        return decode_fields(raw)
    except (ValueError, IndexError):
        return {"_raw": raw}


# --------------------------------------------------------------------------
# Console table rendering — no external deps (tabulate, rich, ...) on
# purpose, same reasoning as sha256.hpp: one small thing, not worth a
# dependency for.
# --------------------------------------------------------------------------
def render_table(headers: list[str], rows: list[list[str]]) -> str:
    cells = [[str(c) for c in row] for row in rows]
    widths = [len(h) for h in headers]
    for row in cells:
        for i, c in enumerate(row):
            widths[i] = max(widths[i], len(c))

    def line(ch: str) -> str:
        return "+" + "+".join(ch * (w + 2) for w in widths) + "+"

    def fmt_row(values: list[str]) -> str:
        return "| " + " | ".join(v.ljust(widths[i]) for i, v in enumerate(values)) + " |"

    out = [line("-"), fmt_row(headers), line("=")]
    for row in cells:
        out.append(fmt_row(row))
    out.append(line("-"))
    return "\n".join(out)


# --------------------------------------------------------------------------
# Data collection — pure functions returning (headers, rows), shared by
# both the console and HTML renderers so they never drift apart.
# --------------------------------------------------------------------------
def collect_plain_table(conn: sqlite3.Connection, table: str) -> tuple[list[str], list[list[str]]]:
    cols = [c[1] for c in conn.execute(f"PRAGMA table_info({table})")]
    rows = conn.execute(f"SELECT * FROM {table}").fetchall()
    return cols, [list(row) for row in rows]


def collect_nexus_rows_groups(
    conn: sqlite3.Connection, only_table: str | None
) -> list[tuple[str, list[str], list[list[str]]]]:
    """Returns one (logical_table_name, headers, rows) tuple per distinct
    table_name — headers are row_key/version plus every field key seen
    across that group's rows, decoded from the wire format."""
    query = "SELECT DISTINCT table_name FROM nexus_rows"
    params: tuple = ()
    if only_table:
        query += " WHERE table_name = ?"
        params = (only_table,)
    logical_tables = [r[0] for r in conn.execute(query + " ORDER BY table_name", params)]

    groups = []
    for name in logical_tables:
        raw_rows = conn.execute(
            "SELECT row_key, fields, version FROM nexus_rows WHERE table_name = ? ORDER BY row_key",
            (name,),
        ).fetchall()

        decoded = [(row_key, safe_decode_fields(fields), version) for row_key, fields, version in raw_rows]

        field_keys: list[str] = []
        for _, fields, _ in decoded:
            for key in fields:
                if key not in field_keys:
                    field_keys.append(key)

        headers = ["row_key", "version"] + field_keys
        rows = [
            [row_key, version] + [fields.get(key, "") for key in field_keys]
            for row_key, fields, version in decoded
        ]
        groups.append((name, headers, rows))
    return groups


# --------------------------------------------------------------------------
# Console output
# --------------------------------------------------------------------------
def print_db_console(db_path: Path, only_table: str | None) -> None:
    print(f"=== {db_path.resolve()} ===")
    if not db_path.exists():
        print("  (file does not exist -- has scripts/setup.sh been run for this path?)\n")
        return

    conn = sqlite3.connect(db_path)
    try:
        tables = [
            r[0]
            for r in conn.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
            )
        ]
        if not tables:
            print("  (no tables at all -- schema not applied yet, run scripts/setup.sh)\n")
            return

        if "nexus_rows" in tables and (not only_table or only_table != "nexus_rows"):
            groups = collect_nexus_rows_groups(conn, None if not only_table or only_table == "nexus_rows" else only_table)
            if not groups:
                print("nexus_rows: (empty -- nothing submitted yet, or wrong DB file/wrong table_name)\n")
            for name, headers, rows in groups:
                print(f"nexus_rows [{name}] ({len(rows)} row{'s' if len(rows) != 1 else ''})")
                print(render_table(headers, rows) if rows else "  (empty)")
                print()

        for table in tables:
            if table == "nexus_rows":
                continue
            if only_table and table != only_table:
                continue
            headers, rows = collect_plain_table(conn, table)
            print(f"{table} ({len(rows)} row{'s' if len(rows) != 1 else ''})")
            print(render_table(headers, rows) if rows else "  (empty)")
            print()
    finally:
        conn.close()


# --------------------------------------------------------------------------
# HTML output — one self-contained file covering every DB dumped in this
# run, so "what's actually in the database" is a double-click away
# instead of a terminal squint.
# --------------------------------------------------------------------------
def _html_table(headers: list[str], rows: list[list]) -> str:
    if not rows:
        return "<p class='empty'>(empty)</p>"
    thead = "".join(f"<th>{html.escape(str(h))}</th>" for h in headers)
    tbody = "".join(
        "<tr>" + "".join(f"<td>{html.escape(str(c))}</td>" for c in row) + "</tr>" for row in rows
    )
    return f"<table><thead><tr>{thead}</tr></thead><tbody>{tbody}</tbody></table>"


def build_html_report(db_paths: list[Path], only_table: str | None) -> str:
    sections = []
    for db_path in db_paths:
        section = [f"<h2>{html.escape(str(db_path.resolve()))}</h2>"]
        if not db_path.exists():
            section.append("<p class='empty'>(file does not exist)</p>")
            sections.append("".join(section))
            continue

        conn = sqlite3.connect(db_path)
        try:
            tables = [
                r[0]
                for r in conn.execute(
                    "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
                )
            ]
            if "nexus_rows" in tables:
                for name, headers, rows in collect_nexus_rows_groups(
                    conn, None if not only_table or only_table == "nexus_rows" else only_table
                ):
                    section.append(f"<h3>nexus_rows &rarr; {html.escape(name)} ({len(rows)})</h3>")
                    section.append(_html_table(headers, rows))
            for table in tables:
                if table == "nexus_rows" or (only_table and table != only_table):
                    continue
                headers, rows = collect_plain_table(conn, table)
                section.append(f"<h3>{html.escape(table)} ({len(rows)})</h3>")
                section.append(_html_table(headers, rows))
        finally:
            conn.close()
        sections.append("".join(section))

    body = "".join(sections)
    return f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Nexus DB inspection</title>
<style>
  body {{ font-family: -apple-system, Segoe UI, sans-serif; margin: 2rem; color: #222; }}
  h2 {{ margin-top: 2.5rem; border-bottom: 2px solid #333; padding-bottom: .25rem; }}
  h3 {{ margin-top: 1.5rem; color: #444; }}
  table {{ border-collapse: collapse; margin: .5rem 0 1.5rem; font-size: .9rem; }}
  th, td {{ border: 1px solid #ccc; padding: 4px 10px; text-align: left; white-space: nowrap; }}
  th {{ background: #f0f0f0; }}
  tr:nth-child(even) {{ background: #fafafa; }}
  .empty {{ color: #888; font-style: italic; }}
</style></head>
<body><h1>Nexus DB inspection</h1>{body}</body></html>"""


# --------------------------------------------------------------------------
def main() -> None:
    args = sys.argv[1:]

    only_table = None
    if "--table" in args:
        idx = args.index("--table")
        only_table = args[idx + 1]
        del args[idx:idx + 2]

    want_html = "--html" in args
    if want_html:
        args.remove("--html")

    explicit_path = args[0] if args else None
    db_paths = resolve_db_paths(explicit_path)

    for db_path in db_paths:
        print_db_console(db_path, only_table)

    if want_html:
        out_path = REPO_ROOT / "db_inspect.html"
        out_path.write_text(build_html_report(db_paths, only_table), encoding="utf-8")
        print(f"HTML report written to {out_path.resolve()}")


if __name__ == "__main__":
    main()
