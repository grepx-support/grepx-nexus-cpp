# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""NexusApplication — the reusable model for "services talk to Nexus
instead of each other" flows, so a new application can be stood up later
without re-deriving this pattern.

One shared snapshot table (e.g. "transactions") always holds exactly one
row per transaction_id: the latest status only, version-guarded. Every
row carries the same envelope on top of the caller's business fields:
status, application, username, occurred_at (see envelope.py) — so
"which application set this status, and when" is always answerable
straight from the row.

This used to also write to a separate, hand-named "stage table" per
lifecycle step (NewRequest, BackEndServer, ...) as a second, non-atomic
insert() call — that's gone. The server's outbox table (see
libs/persistence's SqliteSnapshotStore::submit()) now durably records
every submitted transaction, atomically with the snapshot write, as
part of the single submit() call below. There's no client-side audit
trail to maintain because there's no client-side audit write anymore.

This also no longer carries a client-side `transitions` dict. Whether
NEW -> CLAIMED is legal is the server's call alone (see
libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp)
— the server already validates every submit() against it, inside the
same atomic operation that applies the write, so a client-side copy of
the same rules was just a second place for that logic to go stale.

Depends on ClientInterface, not the concrete NexusClient — this class
only ever calls client.submit(), so it shouldn't need to know (or force
a caller to construct) anything about sockets, auth, or connection
pooling underneath.

Usage:
    app = NexusApplication(
        client=NexusClient("alice", "secret", "my-application"),
        snapshot_table="transactions",
    )

    app.submit(trade_id, status="NEW", application="NewRequest",
               username="trader1", fields={"symbol": "AAPL", "qty": 100, "side": "BUY"})

    version = app.submit(trade_id, status="CLAIMED", application="BackEndServer",
                          username="ops1", fields={"symbol": "AAPL", "qty": 100, "side": "BUY"})
"""
from __future__ import annotations

from dataclasses import dataclass

from ..interfaces.client_interface import ClientInterface
from .envelope import make_envelope


@dataclass
class NexusApplication:
    client: ClientInterface
    snapshot_table: str

    def submit(self, transaction_id: str, *, status: str, application: str,
               username: str, fields: dict) -> int:
        """The only write path a caller needs, first entry or later one
        alike — the server tells the difference itself (no prior row vs.
        a prior row) and validates/records/publishes atomically. Returns
        the snapshot's new version; raises if the server rejects the
        transition (e.g. status isn't reachable from wherever this
        transaction_id currently is)."""
        row = make_envelope(status=status, application=application, username=username,
                             fields={**fields, "transaction_id": transaction_id})
        return self.client.submit(self.snapshot_table, transaction_id, row)
