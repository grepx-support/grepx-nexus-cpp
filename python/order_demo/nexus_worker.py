"""Backend worker — subscribes to Nexus for new orders and moves each one
through NEW -> CLAIMED -> ACCEPTED. Run several of these at once to see
Nexus fan work out across workers: TransitionService's compare-and-swap
means if two workers see the same NEW row, only one wins the CLAIMED
transition; the other gets a version-conflict error and backs off.

Run:
    python python/order_demo/nexus_worker.py
"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # repo/python/

from nexus_api import NexusClient, Row

TABLE = "orders"

nexus = NexusClient()


def handle_new_order(row: Row) -> None:
    # Nexus already filtered this: DispatchService only sends an EVENT
    # here if the row matched "status = 'NEW'" at the moment it changed,
    # so this callback only ever fires for freshly-inserted orders.
    print(f"[worker] saw new order {row.key}: {row.fields}")

    try:
        # expected_version=1 is safe here specifically because we only
        # ever claim rows straight off an INSERT (which always lands at
        # version 1 — see SqliteSnapshotStore::upsert). There's currently
        # no wire-level way to read a row's *current* version outside of
        # a TRANSITION response, so a worker that re-attaches mid-stream
        # can't safely guess it — a real deployment would need Nexus to
        # expose version via SELECT/GET.
        claimed_fields = {**row.fields, "status": "CLAIMED"}
        version = nexus.transition(TABLE, row.key, "NEW", "CLAIMED", 1, claimed_fields)
        print(f"[worker] claimed {row.key}, version={version}")
    except RuntimeError as exc:
        print(f"[worker] lost the claim race for {row.key}: {exc}")
        return

    time.sleep(0.5)  # pretend to do work

    accepted_fields = {**claimed_fields, "status": "ACCEPTED"}
    version = nexus.transition(TABLE, row.key, "CLAIMED", "ACCEPTED", version, accepted_fields)
    print(f"[worker] accepted {row.key}, version={version}")


def main() -> None:
    print("[worker] subscribing to orders where status = 'NEW'...")
    subscription = nexus.subscribe(TABLE, "status = 'NEW'", handle_new_order)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscription.close()


if __name__ == "__main__":
    main()
