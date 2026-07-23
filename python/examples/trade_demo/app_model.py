"""Shared NexusApplication config for the trade demo — both
flask_frontend.py and main.py call create_trade_app() from
here so the table names and state machine live in exactly one place.

`transitions` must match the server's authoritative state machine — see
libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp.

A *factory function* rather than a module-level singleton on purpose:
flask_frontend.py and main.py run as separate processes but
both import this module, and each needs its own application_name —
nexus_server rejects a second concurrent login sharing the same
(application_name, access_mode) pair (see libs/session). A single
hardcoded NexusClient() here would give both processes the same
identity and the second one to connect would fail to log in.
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))  # repo/python/

from nexus_api import NexusApplication, NexusClient

# Stage names — recorded in each row's "application" field (see
# envelope.py) so "which stage produced this" is still answerable from
# the row itself. No longer separate SQL tables: the server's outbox
# table now durably records every submit() atomically with the snapshot
# write (see libs/persistence's SqliteSnapshotStore::submit()), so there's
# nothing left for a client-side per-stage insert to do.
NEW_REQUEST_STAGE = "NewRequest"
BACKEND_STAGE = "BackEndServer"
SNAPSHOT_TABLE = "transactions"

# No client-side TRANSITIONS dict — the server is the sole authority on
# what's a legal move (see
# libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp),
# validated inside the same atomic submit() that applies the write.

# Demo-only default credentials — seed with:
#   scripts/setup.sh   (or directly: python python/seed_user.py $NEXUS_USERNAME $NEXUS_PASSWORD)
# Override via env vars for anything beyond local demo use.
NEXUS_USERNAME = os.environ.get("NEXUS_USERNAME", "demo")
NEXUS_PASSWORD = os.environ.get("NEXUS_PASSWORD", "demo")


def create_trade_app(application_name: str, access_mode: str = "ALL") -> NexusApplication:
    """access_mode is recorded in user_login for audit (see libs/session)
    — not an enforced ACL, just a label for which role this connection
    is playing (see multi_client_demo/ for read-only/insert-only/
    subscribe-and-update clients all using the same server, distinguished
    this way in the audit trail)."""
    return NexusApplication(
        client=NexusClient(NEXUS_USERNAME, NEXUS_PASSWORD, application_name, access_mode),
        snapshot_table=SNAPSHOT_TABLE,
    )
