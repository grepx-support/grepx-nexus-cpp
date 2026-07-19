"""Backend worker for the trade demo — subscribes to the transactions
snapshot table for NEW trades, claims and accepts each one, writing an
audit entry to BackEndServer for every step (claim and accept are
separate transitions, so two audit rows per trade, not one).

Run several of these at once to see the compare-and-swap in
TransitionService do its job: if two workers see the same NEW trade,
only one wins the CLAIMED transition; the other gets a version conflict
and backs off.

Run:
    python python/trade_demo/backend_worker.py
"""
import time

from app_model import BACKEND_TABLE, SNAPSHOT_TABLE, TRADE_APP
from nexus_api import Row


def handle_new_trade(row: Row) -> None:
    # Nexus already filtered this to status='NEW' rows server-side
    # (DispatchService checks the predicate before sending the EVENT).
    print(f"[backend] saw new trade {row.key}: {row.fields}")

    try:
        # expected_version=1 is safe here specifically because we only
        # ever claim rows straight off a submit() (which always lands at
        # version 1 — see SqliteSnapshotStore::upsert). A worker that
        # re-attaches mid-stream instead of reacting live can't safely
        # guess the current version; Nexus doesn't expose it via
        # SELECT/GET today.
        version = TRADE_APP.advance(
            BACKEND_TABLE, row.key, from_status="NEW", to_status="CLAIMED",
            expected_version=1, application=BACKEND_TABLE, username="ops-bot",
            fields=row.fields,
        )
        print(f"[backend] claimed {row.key}, version={version}")
    except (ValueError, RuntimeError) as exc:
        print(f"[backend] lost the claim race for {row.key}: {exc}")
        return

    time.sleep(0.5)  # pretend to run risk checks / settlement prep

    version = TRADE_APP.advance(
        BACKEND_TABLE, row.key, from_status="CLAIMED", to_status="ACCEPTED",
        expected_version=version, application=BACKEND_TABLE, username="ops-bot",
        fields=row.fields,
    )
    print(f"[backend] accepted {row.key}, version={version}")


def main() -> None:
    print("[backend] subscribing to transactions where status = 'NEW'...")
    subscription = TRADE_APP.client.subscribe(SNAPSHOT_TABLE, "status = 'NEW'", handle_new_trade)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscription.close()


if __name__ == "__main__":
    main()
