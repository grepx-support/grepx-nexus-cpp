"""The backend worker's actual domain logic — what happens when a trade
needs claiming/accepting. Nothing in here knows about sockets,
subscriptions, threads, or startup ordering; that's main.py's job. This
module only knows: given a trade app and a row, what should happen.

Kept separate from main.py specifically so the "how do we find out about
a row" plumbing (RecoveringSubscriber) can change — or be unit tested
with a fake NexusApplication — without touching this at all.
"""
from __future__ import annotations

import logging
import time

from app_model import BACKEND_STAGE
from nexus_api import NexusApplication, Row

logger = logging.getLogger(__name__)


def handle_new_trade(app: NexusApplication, row: Row) -> None:
    """Moves one trade NEW -> CLAIMED -> ACCEPTED. Called for every row
    RecoveringSubscriber hands it — whether that row is a live event or
    one recovered from a startup catch-up scan makes no difference here;
    same call, same effect either way (submit() is safe to call again
    for a trade that's already past NEW — the server just rejects it as
    an illegal transition, logged below and otherwise ignored, since
    another worker instance already has it).
    """
    if row.fields.get("status") != "NEW":
        # Already claimed (by this worker on a prior recovery pass, or
        # by a different worker instance) — nothing to do.
        return

    logger.info("saw new trade: key=%s fields=%s", row.key, row.fields)

    try:
        version = app.submit(row.key, status="CLAIMED", application=BACKEND_STAGE,
                              username="ops-bot", fields=row.fields)
        logger.info("claimed trade: key=%s version=%d", row.key, version)
    except RuntimeError as exc:
        # Another worker (or another recovery pass in this same worker)
        # already claimed it first — the server's atomic submit() only
        # lets one caller win the NEW -> CLAIMED move.
        logger.info("lost the claim race: key=%s error=%s", row.key, exc)
        return

    # Pretend to run risk checks / settlement prep.
    time.sleep(0.5)

    version = app.submit(row.key, status="ACCEPTED", application=BACKEND_STAGE,
                          username="ops-bot", fields=row.fields)
    logger.info("accepted trade: key=%s version=%d", row.key, version)
