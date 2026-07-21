"""Backend worker for order_demo — subscribes to the orders snapshot
table for NEW orders, claims and accepts each one, writing an audit
entry to NewOrderRequest for every step (claim and accept are separate
transitions, so two audit rows per order, not one). Mirrors
trade_demo/backend_worker.py exactly — same shape, order_demo-specific
names — so the two example demos read the same way side by side.

Run several of these at once to see the compare-and-swap in
TransitionService do its job: if two workers see the same NEW order,
only one wins the CLAIMED transition; the other gets a version conflict
and backs off.

Run:
    python python/examples/order_demo/nexus_worker.py
"""
import logging
import time

from app_model import NEW_ORDER_TABLE, SNAPSHOT_TABLE, create_order_app
from nexus_api import Row
from nexus_api.logging_config import configure_logging

configure_logging()
logger = logging.getLogger(__name__)

# Distinct application_name from flask_frontend.py's — see app_model.py's
# create_order_app() docstring for why this can't be a shared singleton.
ORDER_APP = create_order_app("order-demo-worker")


def handle_new_order(row: Row) -> None:
    # Nexus already filtered this to status='NEW' rows server-side
    # (DispatchService checks the predicate before sending the EVENT).
    logger.info("saw new order: key=%s fields=%s", row.key, row.fields)

    try:
        # expected_version=1 is safe here specifically because we only
        # ever claim rows straight off a submit() (which always lands at
        # version 1 — see SqliteSnapshotStore::upsert). A worker that
        # re-attaches mid-stream instead of reacting live can't safely
        # guess the current version; Nexus doesn't expose it via
        # SELECT/GET today.
        version = ORDER_APP.advance(
            NEW_ORDER_TABLE, row.key, from_status="NEW", to_status="CLAIMED",
            expected_version=1, application=NEW_ORDER_TABLE, username="ops-bot",
            fields=row.fields,
        )
        logger.info("claimed order: key=%s version=%d", row.key, version)
    except (ValueError, RuntimeError) as exc:
        logger.warning("lost the claim race: key=%s error=%s", row.key, exc)
        return

    time.sleep(0.5)  # pretend to do work

    version = ORDER_APP.advance(
        NEW_ORDER_TABLE, row.key, from_status="CLAIMED", to_status="ACCEPTED",
        expected_version=version, application=NEW_ORDER_TABLE, username="ops-bot",
        fields=row.fields,
    )
    logger.info("accepted order: key=%s version=%d", row.key, version)


def main() -> None:
    logger.info("subscribing to orders where status = 'NEW'...")
    subscription = ORDER_APP.client.subscribe(SNAPSHOT_TABLE, "status = 'NEW'", handle_new_order)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscription.close()


if __name__ == "__main__":
    main()
