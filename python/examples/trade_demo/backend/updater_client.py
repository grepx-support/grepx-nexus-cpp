# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Subscribe-and-update client — the "one real time, then inserting
something" role: reacts live to new ABC trades and moves each one
NEW -> CLAIMED, via submit() (one call, server validates/records/
publishes atomically — see libs/application/submit_service.hpp).

Uses RecoveringSubscriber, same as trade_demo's own worker
(main.py/business_logic.py) — subscribe first (buffered), recover
whatever's already pending, drain, go live — so it never misses an ABC
trade regardless of whether it existed before this script started or
arrived while it was starting up. Own application_name
("trade-demo-updater") and access_mode ("ALL", since it both submits
and subscribes) — shows up as its own row(s) in user_login.

Run:
    python updater_client.py   # runs until Ctrl+C
"""
from __future__ import annotations

import logging
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # trade_demo/, for app_model

from app_model import BACKEND_STAGE, SNAPSHOT_TABLE, create_trade_app  # noqa: E402
from nexus_api import RecoveringSubscriber, Row  # noqa: E402
from nexus_api.logging_config import configure_logging  # noqa: E402

configure_logging()
logger = logging.getLogger(__name__)

SYMBOL = "ABC"
APP = create_trade_app("trade-demo-updater", access_mode="ALL")


def handle_new_abc_trade(row: Row) -> None:
    if row.fields.get("status") != "NEW":
        return
    logger.info("updater: saw new %s trade: key=%s fields=%s", SYMBOL, row.key, row.fields)
    try:
        version = APP.submit(row.key, status="CLAIMED", application=BACKEND_STAGE,
                              username="updater-script", fields=row.fields)
        logger.info("updater: claimed %s trade: key=%s version=%d", SYMBOL, row.key, version)
    except RuntimeError as exc:
        logger.info("updater: lost the claim race: key=%s error=%s", row.key, exc)


def main() -> None:
    criteria = f"symbol = '{SYMBOL}' AND status = 'NEW'"
    subscriber = RecoveringSubscriber(
        client=APP.client,
        table=SNAPSHOT_TABLE,
        recovery_criteria=criteria,
        live_criteria=criteria,
        on_event=handle_new_abc_trade,
    ).start()
    logger.info("updater: live, watching for symbol=%s status=NEW (Ctrl+C to stop)", SYMBOL)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscriber.close()


if __name__ == "__main__":
    main()
