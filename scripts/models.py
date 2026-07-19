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

ALL_TABLES: list[Table] = [NEXUS_ROWS]
