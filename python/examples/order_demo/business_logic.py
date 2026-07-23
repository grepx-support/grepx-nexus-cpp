"""The order worker's actual domain logic — what happens when an order
needs claiming/accepting. Nothing in here knows about sockets,
subscriptions, threads, or startup ordering; that's main.py's job. This
module only knows: given an order app and a row, what should happen.
Mirrors trade_demo/business_logic.py exactly — same shape,
order_demo-specific names.
"""
from __future__ import annotations

import logging
import time

from app_model import NEW_ORDER_STAGE
from nexus_api import NexusApplication, Row

logger = logging.getLogger(__name__)


def handle_new_order(app: NexusApplication, row: Row) -> None:
    """Moves one order NEW -> CLAIMED -> ACCEPTED. Called for every row
    RecoveringSubscriber hands it — live event or startup catch-up scan,
    same call either way (submit() is safe to call again for an order
    that's already past NEW — the server just rejects it as an illegal
    transition, meaning another worker instance already has it)."""
    if row.fields.get("status") != "NEW":
        return

    logger.info("saw new order: key=%s fields=%s", row.key, row.fields)

    try:
        version = app.submit(row.key, status="CLAIMED", application=NEW_ORDER_STAGE,
                              username="ops-bot", fields=row.fields)
        logger.info("claimed order: key=%s version=%d", row.key, version)
    except RuntimeError as exc:
        logger.info("lost the claim race: key=%s error=%s", row.key, exc)
        return

    time.sleep(0.5)  # pretend to do work

    version = app.submit(row.key, status="ACCEPTED", application=NEW_ORDER_STAGE,
                          username="ops-bot", fields=row.fields)
    logger.info("accepted order: key=%s version=%d", row.key, version)
