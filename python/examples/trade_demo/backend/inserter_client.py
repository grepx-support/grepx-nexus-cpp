"""INSERT-only client — submits new trades for a fixed test symbol
(ABC) so there's always a known, easy-to-find row to check in
nexus_rows and outbox after running. Own application_name
("trade-demo-inserter") and access_mode ("INSERT") — shows up as its
own row in user_login.

Run:
    python inserter_client.py            # submit one ABC trade, exit
    python inserter_client.py --count 5  # submit 5, 1s apart
"""
from __future__ import annotations

import logging
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # trade_demo/, for app_model

from app_model import NEW_REQUEST_STAGE, create_trade_app  # noqa: E402
from nexus_api.logging_config import configure_logging  # noqa: E402

configure_logging()
logger = logging.getLogger(__name__)

SYMBOL = "ABC"
APP = create_trade_app("trade-demo-inserter", access_mode="INSERT")


def submit_one() -> str:
    trade_id = str(uuid.uuid4())
    APP.submit(
        trade_id,
        status="NEW",
        application=NEW_REQUEST_STAGE,
        username="inserter-script",
        fields={"symbol": SYMBOL, "side": "BUY", "qty": 10, "price": 11.0},
    )
    logger.info("inserter: submitted %s trade_id=%s status=NEW", SYMBOL, trade_id)
    return trade_id


def main() -> None:
    count = 1
    if "--count" in sys.argv:
        count = int(sys.argv[sys.argv.index("--count") + 1])

    for i in range(count):
        submit_one()
        if i < count - 1:
            time.sleep(1)

    APP.client.close()


if __name__ == "__main__":
    main()
