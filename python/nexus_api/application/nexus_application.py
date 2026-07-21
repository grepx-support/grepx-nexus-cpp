"""NexusApplication — the reusable model for "services talk to Nexus
instead of each other" flows, so a new application can be stood up later
without re-deriving this pattern.

Shape:
  - Every stage a request passes through (e.g. "NewRequest",
    "BackEndServer") writes its own append-only entry — one row per
    attempt, never overwritten. That's the audit trail.
  - One shared snapshot table (e.g. "transactions") always holds exactly
    one row per transaction_id: the latest status only, version-guarded.
  - Every row, in every table, carries the same envelope on top of the
    caller's business fields: status, application, username, occurred_at
    (see envelope.py) — so "which application set this status, and
    when" is always answerable straight from the row.

No new SQL table is needed to add a stage: NewRequest/BackEndServer/
transactions are all just different table_name values inside the one
physical nexus_rows table (see python/models.py) — Nexus was already
built generic enough for that.

Depends on ClientInterface, not the concrete NexusClient — this class
only ever calls insert()/transition(), so it shouldn't need to know
(or force a caller to construct) anything about sockets, auth, or
connection pooling underneath.

IMPORTANT: `transitions` here is the client-side mirror of whatever the
server's StateMachine actually enforces (see
libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp).
Checking it client-side just avoids a wasted round trip on an obviously
illegal transition — the server is still the authority; a mismatch
between this dict and the C++ catalog means the server has the last
word regardless of what is_allowed() says here.

Usage:
    app = NexusApplication(
        client=NexusClient("alice", "secret", "my-application"),
        snapshot_table="transactions",
        transitions={
            "NEW": {"CLAIMED", "REJECTED"},
            "CLAIMED": {"ACCEPTED", "REJECTED"},
        },
    )

    app.submit("NewRequest", trade_id, status="NEW", application="NewRequest",
               username="trader1", fields={"symbol": "AAPL", "qty": 100, "side": "BUY"})

    version = app.advance("BackEndServer", trade_id, from_status="NEW", to_status="CLAIMED",
                           expected_version=1, application="BackEndServer", username="ops1",
                           fields={"symbol": "AAPL", "qty": 100, "side": "BUY"})
"""
from __future__ import annotations

import uuid
from dataclasses import dataclass
from typing import Dict, Set

from ..interfaces.client_interface import ClientInterface
from .envelope import make_envelope


@dataclass
class NexusApplication:
    client: ClientInterface
    snapshot_table: str
    transitions: Dict[str, Set[str]]

    def is_allowed(self, from_status: str, to_status: str) -> bool:
        return to_status in self.transitions.get(from_status, set())

    def submit(self, stage_table: str, transaction_id: str, *, status: str,
               application: str, username: str, fields: dict) -> None:
        """First entry for a transaction: writes the stage's audit row
        and creates the snapshot row (always lands at version 1)."""
        row = make_envelope(status=status, application=application, username=username,
                             fields={**fields, "transaction_id": transaction_id})
        self.client.insert(stage_table, self._entry_key(transaction_id), row)
        self.client.insert(self.snapshot_table, transaction_id, row)

    def advance(self, stage_table: str, transaction_id: str, *, from_status: str,
                to_status: str, expected_version: int, application: str,
                username: str, fields: dict) -> int:
        """Every later stage: checks the transition client-side, writes
        that stage's audit row (one row per attempt, so a two-step
        NEW->CLAIMED->ACCEPTED backend produces two audit rows, not one),
        then CAS-updates the snapshot. Returns the snapshot's new version."""
        if not self.is_allowed(from_status, to_status):
            raise ValueError(f"illegal transition {from_status} -> {to_status}")

        row = make_envelope(status=to_status, application=application, username=username,
                             fields={**fields, "transaction_id": transaction_id})
        self.client.insert(stage_table, self._entry_key(transaction_id), row)
        return self.client.transition(self.snapshot_table, transaction_id, from_status,
                                       to_status, expected_version, row)

    @staticmethod
    def _entry_key(transaction_id: str) -> str:
        # Unique per attempt — a stage table is append-only, so reusing
        # transaction_id as the row key would let a retry silently
        # overwrite the previous entry instead of adding a new one.
        return f"{transaction_id}:{uuid.uuid4().hex[:8]}"
