#!/usr/bin/env python3
"""Creates/migrates the Nexus SQLite schema.

The C++ server (libs/persistence/sqlite_snapshot_store.hpp) never creates
or alters schema itself — it assumes nexus_rows already exists and fails
loud on first query if it doesn't. Run this script once before starting
nexus_server, and again whenever SCHEMA below gains a migration.

Usage:
    python scripts/create_schema.py [path-to-db]

Defaults to ./nexus_data.db, matching SqliteSnapshotStore's default path.
"""
import sqlite3
import sys

SCHEMA = """
CREATE TABLE IF NOT EXISTS nexus_rows (
    table_name TEXT NOT NULL,
    row_key    TEXT NOT NULL,
    fields     TEXT NOT NULL,
    version    INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (table_name, row_key)
);
"""


def main() -> None:
    db_path = sys.argv[1] if len(sys.argv) > 1 else "nexus_data.db"
    conn = sqlite3.connect(db_path)
    try:
        conn.executescript(SCHEMA)
        conn.commit()
        print(f"schema applied to {db_path}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
