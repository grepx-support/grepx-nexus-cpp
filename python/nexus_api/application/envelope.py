"""Standard envelope every Nexus row carries on top of its business
fields: which application produced the entry, its status at that point,
the acting user, and when. Used by both the append-only per-stage tables
(NewRequest, BackEndServer, ...) and the single latest-snapshot table
(transactions) — see nexus_application.py.
"""
from __future__ import annotations

from datetime import datetime, timezone


def make_envelope(*, status: str, application: str, username: str, fields: dict) -> dict:
    """Merges the standard envelope on top of the caller's business
    fields. Envelope keys are applied last so a request can't spoof its
    own status/application/user by putting those keys in `fields`."""
    return {
        **fields,
        "status": status,
        "application": application,
        "username": username,
        "occurred_at": datetime.now(timezone.utc).isoformat(),
    }
