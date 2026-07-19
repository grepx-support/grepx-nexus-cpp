"""Shared NexusApplication config for the trade demo — both
flask_frontend.py and backend_worker.py import TRADE_APP from here so
the table names and state machine live in exactly one place.

`transitions` must match the server's authoritative state machine — see
libs/statemachine/include/nexus/statemachine/transaction_transitions.hpp.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # repo/python/

from nexus_api import NexusApplication, NexusClient

NEW_REQUEST_TABLE = "NewRequest"
BACKEND_TABLE = "BackEndServer"
SNAPSHOT_TABLE = "transactions"

TRADE_APP = NexusApplication(
    client=NexusClient(),
    snapshot_table=SNAPSHOT_TABLE,
    transitions={
        "NEW": {"CLAIMED", "REJECTED"},
        "CLAIMED": {"ACCEPTED", "REJECTED"},
    },
)
