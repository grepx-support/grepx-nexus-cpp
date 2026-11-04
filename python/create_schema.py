#!/usr/bin/env python3
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Creates/migrates the Nexus SQLite schema, built from the table models
in models.py.

The C++ server (libs/persistence/sqlite_snapshot_store.hpp) never creates
or alters schema itself — it assumes nexus_rows already exists and fails
loud on first query if it doesn't. Run this script once before starting
nexus_server, and again whenever models.py gains a new table.

Path resolution — must land on the exact same file nexus_server opens,
or every insert fails with "table not found" and it looks like nothing
works:
    1. An explicit path argument, if given — schema applied there only.
    2. Otherwise:
       a. The NEXUS_DB_PATH env var, if set (SqliteSnapshotStore checks
          the same variable — set it once, both sides agree).
       b. <repo_root>/nexus_data.db (nexus_server's default when
          NEXUS_DB_PATH isn't set and it's launched with the repo root
          as its working directory).
       c. Every already-built nexus_server(.exe) under the repo (any
          cmake-build-*/apps/nexus_server/ directory) — nexus_server's
          default DB, when NEXUS_DB_PATH isn't set, is "nexus_data.db"
          next to its own executable. Schema gets applied there too, so
          setup still works even if you haven't set NEXUS_DB_PATH or
          pointed the IDE's working directory at the repo root.

nexus_server prints the absolute path it actually opened on startup —
that's always the source of truth if something still doesn't match.

Normally you don't run this directly — scripts/setup.sh calls it (along
with seed_user.py) as step one of bringing the whole stack up.

Usage:
    python python/create_schema.py [path-to-db]
"""
import os
import sqlite3
import sys
from pathlib import Path

from models import ALL_TABLES, OUTBOX_SQL

REPO_ROOT = Path(__file__).resolve().parent.parent


def discover_build_dirs() -> list[Path]:
    """Every directory holding an already-built nexus_server(.exe)."""
    return sorted({
        exe.parent
        for pattern in ("cmake-build-*/apps/nexus_server/nexus_server.exe",
                         "cmake-build-*/apps/nexus_server/nexus_server")
        for exe in REPO_ROOT.glob(pattern)
    })


def resolve_db_paths(explicit_path: str | None = None) -> list[Path]:
    """Takes the explicit path (if any) as a parameter rather than
    reading sys.argv directly — sys.argv belongs to whichever script's
    __main__ is actually running, and this function is also called from
    seed_user.py, which has its own, differently-shaped argv (username/
    password positional args, not a db path). Reading sys.argv here
    would silently mistake seed_user.py's username for a db path — this
    is not a hypothetical, it's exactly what happened before this was a
    parameter."""
    if explicit_path is not None:
        return [Path(explicit_path)]

    if env_path := os.environ.get("NEXUS_DB_PATH"):
        return [Path(env_path)]

    paths = [REPO_ROOT / "nexus_data.db"]
    paths.extend(build_dir / "nexus_data.db" for build_dir in discover_build_dirs())
    return paths


def apply_schema(db_path: Path) -> None:
    conn = sqlite3.connect(db_path)
    try:
        for table in ALL_TABLES:
            conn.executescript(table.to_sql())
        conn.executescript(OUTBOX_SQL)
        conn.commit()
        names = [t.name for t in ALL_TABLES] + ["outbox"]
        print(f"schema applied to {db_path.resolve()}: {', '.join(names)}")
    finally:
        conn.close()


def main() -> None:
    explicit_path = sys.argv[1] if len(sys.argv) > 1 else None
    for db_path in resolve_db_paths(explicit_path):
        apply_schema(db_path)


if __name__ == "__main__":
    main()
