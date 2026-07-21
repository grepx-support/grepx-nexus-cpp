#!/usr/bin/env python3
"""Dumps every table in a Nexus SQLite DB, human-readable — a generic
debug tool for "is the data actually there" questions, so nobody has to
hand-write a one-off sqlite3 script or dig through a GUI plugin for it.

Schema-driven: reads sqlite_master to discover EVERY table that actually
exists (not a hardcoded list), so it stays correct as python/models.py
grows. nexus_rows gets special handling: it's the one physical table that
backs every logical Nexus table (orders, transactions, NewRequest,
BackEndServer, ...) via its table_name column (see python/models.py) —
"everything's crammed into one SQL table" is exactly the thing people
expect to see as separate tables and don't, so rows are grouped and
labelled by that logical name instead of dumped flat.

Path resolution is identical to create_schema.py/seed_user.py — same
NEXUS_DB_PATH-env-var-first, else-every-known-DB rule — so this always
looks at the same file(s) nexus_server actually reads/writes, not a
guess. Pass a path explicitly to just check one file.

Usage:
    python python/inspect_db.py                # every DB this repo knows about
    python python/inspect_db.py path/to/db      # just that one
    python python/inspect_db.py --table orders  # only that logical/SQL table
"""
from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

from create_schema import resolve_db_paths


def _dump_plain_table(conn: sqlite3.Connection, table: str) -> None:
    cols = [c[1] for c in conn.execute(f"PRAGMA table_info({table})")]
    rows = conn.execute(f"SELECT * FROM {table}").fetchall()
    print(f"-- {table} ({len(rows)} row{'s' if len(rows) != 1 else ''}) --")
    if not rows:
        print("  (empty)")
        return
    for row in rows:
        print("  " + ", ".join(f"{c}={v!r}" for c, v in zip(cols, row)))


def _dump_nexus_rows(conn: sqlite3.Connection, only_table: str | None) -> None:
    query = "SELECT DISTINCT table_name FROM nexus_rows"
    params: tuple = ()
    if only_table:
        query += " WHERE table_name = ?"
        params = (only_table,)
    logical_tables = [r[0] for r in conn.execute(query + " ORDER BY table_name", params)]

    total = sum(
        conn.execute("SELECT COUNT(*) FROM nexus_rows WHERE table_name = ?", (name,)).fetchone()[0]
        for name in logical_tables
    )
    print(f"-- nexus_rows ({total} row{'s' if total != 1 else ''} across "
          f"{len(logical_tables)} logical table{'s' if len(logical_tables) != 1 else ''}) --")
    if not logical_tables:
        print("  (empty — nothing submitted yet, or wrong DB file/wrong table_name)")
        return

    for name in logical_tables:
        rows = conn.execute(
            "SELECT row_key, fields, version FROM nexus_rows WHERE table_name = ? ORDER BY row_key",
            (name,),
        ).fetchall()
        print(f"  [{name}] ({len(rows)} row{'s' if len(rows) != 1 else ''})")
        for row_key, fields, version in rows:
            print(f"    row_key={row_key!r} version={version} fields={fields}")


def dump_db(db_path: Path, only_table: str | None) -> None:
    print(f"=== {db_path.resolve()} ===")
    if not db_path.exists():
        print("  (file does not exist — has scripts/setup.sh been run for this path?)")
        print()
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
            print("  (no tables at all — schema not applied yet, run scripts/setup.sh)")

        for table in tables:
            if only_table and table not in ("nexus_rows", only_table):
                continue
            if table == "nexus_rows":
                _dump_nexus_rows(conn, only_table if only_table != "nexus_rows" else None)
            else:
                _dump_plain_table(conn, table)
    finally:
        conn.close()
    print()


def main() -> None:
    args = sys.argv[1:]
    only_table = None
    if "--table" in args:
        idx = args.index("--table")
        only_table = args[idx + 1]
        del args[idx:idx + 2]

    explicit_path = args[0] if args else None
    for db_path in resolve_db_paths(explicit_path):
        dump_db(db_path, only_table)


if __name__ == "__main__":
    main()
