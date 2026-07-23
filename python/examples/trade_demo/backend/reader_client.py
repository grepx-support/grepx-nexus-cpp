"""SELECT-only client — proves a client that never inserts or submits
anything can still see live data land, purely by polling SELECT. Uses
its own application_name ("trade-demo-reader") and access_mode
("SELECT") so it shows up as its own row in user_login, distinguishable
from the frontend/worker/inserter/updater in an audit query.

access_mode is metadata recorded for audit (see libs/session) — Nexus
doesn't currently enforce it as an ACL (a "SELECT" client can still call
insert()/submit() if the code asked it to); this script just never does,
by design, to demonstrate the read-only role cleanly.

Run:
    python reader_client.py            # polls every 2s until Ctrl+C
    python reader_client.py --once     # one SELECT, print, exit
"""
from __future__ import annotations

import logging
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # trade_demo/, for app_model

from app_model import SNAPSHOT_TABLE, create_trade_app  # noqa: E402
from nexus_api.logging_config import configure_logging  # noqa: E402

configure_logging()
logger = logging.getLogger(__name__)

SYMBOL = "ABC"
APP = create_trade_app("trade-demo-reader", access_mode="SELECT")


def poll_once() -> None:
    rows = APP.client.select(SNAPSHOT_TABLE, f"symbol = '{SYMBOL}'")
    if not rows:
        logger.info("reader: no %s trades yet", SYMBOL)
        return
    for row in rows:
        logger.info("reader: %s trade %s status=%s fields=%s",
                    SYMBOL, row.key, row.fields.get("status"), row.fields)


def main() -> None:
    if "--once" in sys.argv:
        poll_once()
        return
    logger.info("reader: polling every 2s for symbol=%s (Ctrl+C to stop)", SYMBOL)
    try:
        while True:
            poll_once()
            time.sleep(2)
    except KeyboardInterrupt:
        pass
    finally:
        APP.client.close()


if __name__ == "__main__":
    main()
