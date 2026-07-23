"""Worker entrypoint for order_demo — deliberately thin. Everything it
does: configure logging, build the app, wire up RecoveringSubscriber
with business_logic's handler, start it, wait for Ctrl+C. No
subscription/recovery mechanics and no domain logic live here — see
nexus_api.application.RecoveringSubscriber for the former,
business_logic.py for the latter. Mirrors trade_demo/main.py exactly.

Run:
    python python/examples/order_demo/main.py
"""
import logging
import time

from app_model import SNAPSHOT_TABLE, create_order_app
from business_logic import handle_new_order
from nexus_api import RecoveringSubscriber
from nexus_api.logging_config import configure_logging

configure_logging()
logger = logging.getLogger(__name__)

# Distinct application_name from flask_frontend.py's — see app_model.py's
# create_order_app() docstring for why this can't be a shared singleton.
ORDER_APP = create_order_app("order-demo-worker")


def main() -> None:
    subscriber = RecoveringSubscriber(
        client=ORDER_APP.client,
        table=SNAPSHOT_TABLE,
        recovery_criteria="status != 'ACCEPTED' AND status != 'REJECTED'",
        live_criteria="status = 'NEW'",
        on_event=lambda row: handle_new_order(ORDER_APP, row),
    ).start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        subscriber.close()


if __name__ == "__main__":
    main()
