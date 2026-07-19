#!/usr/bin/env python3
"""Creates/migrates the Nexus SQLite schema, built from the table models
in models.py.

The C++ server (libs/persistence/sqlite_snapshot_store.hpp) never creates
or alters schema itself — it assumes nexus_rows already exists and fails
loud on first query if it doesn't. Run this script once before starting
nexus_server, and again whenever models.py gains a new table.

Usage:
    python scripts/create_schema.py [path-to-db]

Defaults to ./nexus_data.db, matching SqliteSnapshotStore's default path.
"""
import sqlite3
import sys

from models import ALL_TABLES


def main() -> None:
    db_path = sys.argv[1] if len(sys.argv) > 1 else "nexus_data.db"
    conn = sqlite3.connect(db_path)
    try:
        for table in ALL_TABLES:
            conn.executescript(table.to_sql())
        conn.commit()
        print(f"schema applied to {db_path}: {', '.join(t.name for t in ALL_TABLES)}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
