"""One row as returned by SELECT or delivered by an EVENT."""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class Row:
    key: str
    fields: dict
