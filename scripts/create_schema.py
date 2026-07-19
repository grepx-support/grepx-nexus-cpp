#!/usr/bin/env python3
"""Creates/migrates the Nexus SQLite schema, built from the table models
in models.py.

The C++ server (libs/persistence/sqlite_snapshot_store.hpp) never creates
or alters schema itself — it assumes nexus_rows already exists and fails
loud on first query if it doesn't. Run this script once before starting
nexus_server, and again whenever models.py gains a new table.

Path resolution — must land on the exact same file nexus_server opens,
or every insert fails with "table not found" and it looks like nothing
works:
    1. An explicit path argument, if given.
    2. The NEXUS_DB_PATH env var, if set (SqliteSnapshotStore checks the
       same variable — set it once, both sides agree).
    3. <repo_root>/nexus_data.db — anchored to this script's own
       location, NOT the caller's current directory, so `cd
       somewhere-else && python .../create_schema.py` still lands in
       the same place every time.

nexus_server itself defaults to "nexus_data.db" relative to its
process's CWD if NEXUS_DB_PATH isn't set — which is often NOT the repo
root (an IDE's default working directory for a build target is
frequently the build output folder). Simplest fix: set the
nexus_server run configuration's working directory to the repo root,
matching this script's default. nexus_server also prints the absolute
path it opened on startup, so you can always check which file is
actually in use.

Usage:
    python scripts/create_schema.py [path-to-db]
"""
import os
import sqlite3
import sys
from pathlib import Path

from models import ALL_TABLES

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DB_PATH = REPO_ROOT / "nexus_data.db"


def resolve_db_path() -> Path:
    if len(sys.argv) > 1:
        return Path(sys.argv[1])
    if env_path := os.environ.get("NEXUS_DB_PATH"):
        return Path(env_path)
    return DEFAULT_DB_PATH


def main() -> None:
    db_path = resolve_db_path()
    conn = sqlite3.connect(db_path)
    try:
        for table in ALL_TABLES:
            conn.executescript(table.to_sql())
        conn.commit()
        print(f"schema applied to {db_path.resolve()}: {', '.join(t.name for t in ALL_TABLES)}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
