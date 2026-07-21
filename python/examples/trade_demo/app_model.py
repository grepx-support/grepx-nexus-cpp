"""Shared NexusApplication config for the trade demo — both
flask_frontend.py and backend_worker.py call create_trade_app() from
here so the table names and state machine live in exactly one place.

`transitions` must match the server's authoritative state machine — see
libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp.

A *factory function* rather than a module-level singleton on purpose:
flask_frontend.py and backend_worker.py run as separate processes but
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

NEW_REQUEST_TABLE = "NewRequest"
BACKEND_TABLE = "BackEndServer"
SNAPSHOT_TABLE = "transactions"

TRANSITIONS = {
    "NEW": {"CLAIMED", "REJECTED"},
    "CLAIMED": {"ACCEPTED", "REJECTED"},
}

# Demo-only default credentials — seed with:
#   scripts/setup.sh   (or directly: python python/seed_user.py $NEXUS_USERNAME $NEXUS_PASSWORD)
# Override via env vars for anything beyond local demo use.
NEXUS_USERNAME = os.environ.get("NEXUS_USERNAME", "demo")
NEXUS_PASSWORD = os.environ.get("NEXUS_PASSWORD", "demo")


def create_trade_app(application_name: str) -> NexusApplication:
    return NexusApplication(
        client=NexusClient(NEXUS_USERNAME, NEXUS_PASSWORD, application_name),
        snapshot_table=SNAPSHOT_TABLE,
        transitions=TRANSITIONS,
    )
