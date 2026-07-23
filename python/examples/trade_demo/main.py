"""Backend worker entrypoint for the trade demo — deliberately thin.
Everything it does: configure logging, build the app, wire up
RecoveringSubscriber with business_logic's handler, start it, wait for
Ctrl+C. No subscription/recovery mechanics and no domain logic live
here — see nexus_api.application.RecoveringSubscriber for the former,
business_logic.py for the latter.

Run several of these at once to see the server's atomic submit() do its
job: if two workers see the same NEW trade, only one wins the CLAIMED
move; the other gets rejected and backs off (see business_logic.py).

Run:
    python python/examples/trade_demo/main.py
"""
import logging
import time

from app_model import SNAPSHOT_TABLE, create_trade_app
from business_logic import handle_new_trade
from nexus_api import RecoveringSubscriber
from nexus_api.logging_config import configure_logging

configure_logging()
logger = logging.getLogger(__name__)

# Distinct application_name from flask_frontend.py's — see app_model.py's
# create_trade_app() docstring for why this can't be a shared singleton.
TRADE_APP = create_trade_app("trade-demo-backend-worker")


def main() -> None:
    # Recovery criteria: anything not already at a terminal status is
    # "pending work this worker needs to know about" — covers NEW (not
    # yet claimed) and CLAIMED (claimed by a worker that may have died
    # before accepting it).
    subscriber = RecoveringSubscriber(
        client=TRADE_APP.client,
        table=SNAPSHOT_TABLE,
        recovery_criteria="status != 'ACCEPTED' AND status != 'REJECTED'",
        live_criteria="status = 'NEW'",
        on_event=lambda row: handle_new_trade(TRADE_APP, row),
    ).start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscriber.close()


if __name__ == "__main__":
    main()
