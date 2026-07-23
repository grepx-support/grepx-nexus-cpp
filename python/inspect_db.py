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
import itertools
import sqlite3
import sys
from pathlib import Path

from create_schema import resolve_db_paths
from nexus_api.codec import decode_fields, decode_storage_fields

REPO_ROOT = Path(__file__).resolve().parent.parent


def safe_decode_fields(raw: str) -> dict:
    """nexus_rows.fields and outbox.fields are written straight to SQLite
    by nexus_server's C++ SqliteSnapshotStore, which uses control-
    character delimiters ('\\x1e'/'\\x1f') — see decode_storage_fields()'s
    docstring. That's tried first since it's what every real row on disk
    actually looks like. decode_fields() (the ';'/'=' wire-protocol
    format NexusClient speaks over TCP, never written straight to SQLite
    by anything in this repo) is tried second only in case some row was
    hand-inserted in that format for a test. If neither parses, one odd
    row still just shows up as a single _raw column instead of taking
    the whole inspector down with an unhandled exception.
    """
    if not raw:
        return {}
    decoded = decode_storage_fields(raw)
    if decoded:
        return decoded
    try:
        decoded = decode_fields(raw)
        if decoded:
            return decoded
    except (ValueError, IndexError):
        pass
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


def collect_outbox_rows(conn: sqlite3.Connection) -> tuple[list[str], list[list]]:
    """Same idea as collect_nexus_rows_groups but for outbox: every other
    column (id/table_name/transaction_id/from_status/to_status/
    occurred_at/published_at) stays as-is, and the fields blob column
    gets decoded and spread into one column per field key — instead of
    one long unreadable string, same reasoning as nexus_rows.fields."""
    raw_rows = conn.execute(
        "SELECT id, table_name, transaction_id, from_status, to_status, fields, occurred_at, published_at "
        "FROM outbox ORDER BY id"
    ).fetchall()

    decoded = [
        (id_, table_name, transaction_id, from_status, to_status, safe_decode_fields(fields), occurred_at, published_at)
        for id_, table_name, transaction_id, from_status, to_status, fields, occurred_at, published_at in raw_rows
    ]

    # outbox's own fixed columns (transaction_id, occurred_at) happen to
    # share names with keys the submitted business row's own fields dict
    # commonly carries too (every demo's app_model.py includes both in
    # its fields=... — see business_logic.py). Decoding would otherwise
    # produce two same-named columns showing the same value twice; the
    # fixed column already answers "what transaction/when", so skip
    # those specific keys from the decoded-field columns.
    fixed_columns = {"id", "table_name", "transaction_id", "from_status", "to_status", "occurred_at", "published_at"}
    field_keys: list[str] = []
    for row in decoded:
        for key in row[5]:
            if key not in field_keys and key not in fixed_columns:
                field_keys.append(key)

    headers = ["id", "table_name", "transaction_id", "from_status", "to_status"] + field_keys + \
              ["occurred_at", "published_at"]
    rows = [
        [id_, table_name, transaction_id, from_status, to_status] +
        [fields.get(key, "") for key in field_keys] +
        [occurred_at, published_at]
        for id_, table_name, transaction_id, from_status, to_status, fields, occurred_at, published_at in decoded
    ]
    return headers, rows


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

        # only_table might name a real SQL table (outbox, users, ...) or a
        # logical table_name that only exists inside nexus_rows
        # (transactions, NewRequest, ...) — those are two different
        # things filtered two different ways, so tell them apart by
        # whether only_table is actually one of the real tables that
        # exist in this DB. Without this, "--table outbox" used to still
        # print every nexus_rows group alongside it — noise when the
        # whole point was to look at outbox on its own.
        other_real_tables = set(tables) - {"nexus_rows"}
        show_nexus_rows = "nexus_rows" in tables and (not only_table or only_table not in other_real_tables)
        if show_nexus_rows:
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
            headers, rows = collect_outbox_rows(conn) if table == "outbox" else collect_plain_table(conn, table)
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
def _html_table(table_id: str, headers: list[str], rows: list[list]) -> str:
    if not rows:
        return "<p class='empty'>(empty)</p>"
    thead = "".join(f"<th>{html.escape(str(h))}</th>" for h in headers)
    tbody = "".join(
        "<tr>" + "".join(f"<td>{html.escape(str(c))}</td>" for c in row) + "</tr>" for row in rows
    )
    # One filter box per table, not one global search — grepping "which
    # table has a row matching X" is a different question from "narrow
    # down the table I'm already looking at", and a per-table box
    # answers the second one without also hiding every other table's
    # rows while you're typing. Plain substring match across every cell
    # in a row, case-insensitive — "basic filtering", not a query language.
    filter_box = (
        f'<input type="text" class="filter-box" data-target="{table_id}" '
        f'placeholder="filter rows…" oninput="filterTable(this)">'
    )
    return f"{filter_box}<table id='{table_id}'><thead><tr>{thead}</tr></thead><tbody>{tbody}</tbody></table>"


def build_html_report(db_paths: list[Path], only_table: str | None) -> str:
    sections = []
    table_ids = (f"tbl-{i}" for i in itertools.count(1))
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
                    section.append(_html_table(next(table_ids), headers, rows))
            for table in tables:
                if table == "nexus_rows" or (only_table and table != only_table):
                    continue
                headers, rows = collect_outbox_rows(conn) if table == "outbox" else collect_plain_table(conn, table)
                section.append(f"<h3>{html.escape(table)} ({len(rows)})</h3>")
                section.append(_html_table(next(table_ids), headers, rows))
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
  .filter-box {{
    display: block; margin: .5rem 0 .25rem; padding: 4px 8px; font-size: .85rem;
    border: 1px solid #ccc; border-radius: 4px; width: 22rem; max-width: 100%;
  }}
</style></head>
<body><h1>Nexus DB inspection</h1>{body}
<script>
function filterTable(input) {{
  const term = input.value.toLowerCase();
  const table = document.getElementById(input.dataset.target);
  table.querySelectorAll('tbody tr').forEach(function (tr) {{
    tr.style.display = tr.textContent.toLowerCase().includes(term) ? '' : 'none';
  }});
}}
</script>
</body></html>"""


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
