# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Table models for the Nexus SQLite schema.

The schema lives here as data (Column/Table), not as a hand-written SQL
string — create_schema.py just turns ALL_TABLES into CREATE TABLE
statements. Add a new table (e.g. an idempotency table, an audit table)
by adding another Table() below; nothing else needs to change.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional


@dataclass(frozen=True)
class Column:
    name: str
    sql_type: str
    not_null: bool = True
    default: Optional[str] = None

    def to_sql(self) -> str:
        parts = [self.name, self.sql_type]
        if self.not_null:
            parts.append("NOT NULL")
        if self.default is not None:
            parts.append(f"DEFAULT {self.default}")
        return " ".join(parts)


@dataclass(frozen=True)
class Table:
    name: str
    columns: list[Column]
    primary_key: list[str] = field(default_factory=list)

    def to_sql(self) -> str:
        lines = [c.to_sql() for c in self.columns]
        if self.primary_key:
            lines.append(f"PRIMARY KEY ({', '.join(self.primary_key)})")
        body = ",\n    ".join(lines)
        return f"CREATE TABLE IF NOT EXISTS {self.name} (\n    {body}\n);"


# Mirrors libs/persistence/include/nexus/persistence/sqlite_snapshot_store.hpp
# exactly — table_name/row_key/fields/version, same names, same types.
# If you change one side, change the other.
NEXUS_ROWS = Table(
    name="nexus_rows",
    columns=[
        Column("table_name", "TEXT"),
        Column("row_key", "TEXT"),
        Column("fields", "TEXT"),
        Column("version", "INTEGER", default="1"),
    ],
    primary_key=["table_name", "row_key"],
)

USERS = Table(
    name="users",
    columns=[
        Column("username", "TEXT"),
        Column("password_hash", "TEXT"),
    ],
    primary_key=["username"],
)

# Audit trail for every login/logout — mirrors
# libs/session/include/nexus/session/sqlite_login_audit.hpp. One row per
# login attempt (status starts LOGGED_IN, logout_time/status get updated
# when the client disconnects). Deliberately has no password column —
# see sqlite_login_audit.hpp for why; auth_method records HOW someone
# authenticated, never WITH WHAT.
USER_LOGIN = Table(
    name="user_login",
    columns=[
        Column("username", "TEXT"),
        Column("application_name", "TEXT"),
        Column("access_mode", "TEXT"),
        Column("auth_method", "TEXT"),
        Column("login_time", "TEXT"),
        Column("logout_time", "TEXT", not_null=False),
        Column("status", "TEXT"),
    ],
)

ALL_TABLES: list[Table] = [NEXUS_ROWS, USERS, USER_LOGIN]

# Durable outbox — the single record of "a business transaction was
# submitted" for every snapshot-table write, in the same SQLite
# transaction as the snapshot upsert (see
# libs/persistence/include/nexus/persistence/sqlite_snapshot_store.hpp,
# SqliteSnapshotStore::submit()). This replaces the old pattern of one
# hand-named SQL "stage table" per lifecycle stage (NewRequest,
# BackEndServer, ...) — those were really just insert() calls against
# nexus_rows under a different table_name, not an actual audit/outbox
# mechanism, and writing them was a second, non-atomic network round
# trip the client had to make itself.
#
# published_at is set once the server has handed the event to its
# in-memory subscriber bus — NULL means "durably recorded, not yet
# delivered to any live subscriber" (e.g. server restarted between
# commit and publish). A row here is never lost even if delivery is;
# it's what makes catch-up/replay possible instead of best-effort-only.
#
# Plain SQL, not a Table() — Table/Column only model natural/composite
# primary keys (every other table here uses one); outbox needs an
# AUTOINCREMENT surrogate key instead, which isn't worth generalizing
# Column for one table.
OUTBOX_SQL = """\
CREATE TABLE IF NOT EXISTS outbox (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
    from_status TEXT,
    to_status TEXT NOT NULL,
    fields TEXT NOT NULL,
    occurred_at TEXT NOT NULL,
    published_at TEXT
);
CREATE INDEX IF NOT EXISTS idx_outbox_unpublished ON outbox(published_at)
    WHERE published_at IS NULL;
"""
