"""Flask 'frontend' for the trade demo — a new trade order becomes a NEW
snapshot row in transactions. backend/main.py (or any of the
backend/*_client.py scripts) picks it up from there and moves it through
the state machine.

Setup (from trade_demo/, one directory up):
    ./setup.sh   (creates a venv, installs deps, creates the DB schema)

Run:
    ./run.sh                    (this alone, standalone)
    ../run.sh                   (this + prompts for which backend/ client(s), from trade_demo/)

Then open http://localhost:5001/ in a browser — submit a trade and
watch its status move NEW -> CLAIMED -> ACCEPTED as whichever backend/
client is running processes it. Or via curl:
    curl -X POST localhost:5001/trades -H "Content-Type: application/json" \
        -d "{\"symbol\": \"AAPL\", \"side\": \"BUY\", \"qty\": 100, \"price\": 190.5}"
    curl localhost:5001/trades/<trade_id>
"""
import logging
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # trade_demo/, for app_model

from app_model import NEW_REQUEST_STAGE, SNAPSHOT_TABLE, create_trade_app  # noqa: E402
from flask import Flask, jsonify, render_template, request  # noqa: E402
from nexus_api.logging_config import configure_logging  # noqa: E402

configure_logging()
logger = logging.getLogger(__name__)

app = Flask(__name__)
# Distinct application_name from main.py's — see app_model.py's
# create_trade_app() docstring for why this can't be a shared singleton.
TRADE_APP = create_trade_app("trade-demo-frontend")

REQUIRED_FIELDS = ("symbol", "side", "qty", "price")


@app.post("/trades")
def submit_trade():
    body = request.get_json(force=True, silent=True) or {}
    if not all(k in body for k in REQUIRED_FIELDS):
        return jsonify(error=f"required fields: {', '.join(REQUIRED_FIELDS)}"), 400

    trade_id = str(uuid.uuid4())
    TRADE_APP.submit(
        trade_id,
        status="NEW",
        application=NEW_REQUEST_STAGE,
        username=body.get("username", "anonymous"),
        fields={
            "symbol": str(body["symbol"]),
            "side": str(body["side"]),
            "qty": int(body["qty"]),
            "price": float(body["price"]),
        },
    )
    logger.info("trade submitted: trade_id=%s symbol=%s side=%s qty=%s price=%s",
                trade_id, body["symbol"], body["side"], body["qty"], body["price"])
    # 202: accepted into Nexus, not yet processed — BackEndServer picks
    # this up asynchronously via SUBSCRIBE.
    return jsonify(trade_id=trade_id, status="NEW"), 202


@app.get("/trades")
def list_trades():
    rows = TRADE_APP.client.select(SNAPSHOT_TABLE)
    trades = sorted(
        ({"trade_id": r.key, **r.fields} for r in rows),
        key=lambda t: t.get("occurred_at", ""),
        reverse=True,
    )
    return jsonify(trades)


@app.get("/trades/<trade_id>")
def get_trade(trade_id):
    # No SELECT-by-key on the wire — filter client-side, fine at demo scale.
    rows = TRADE_APP.client.select(SNAPSHOT_TABLE)
    match = next((r for r in rows if r.key == trade_id), None)
    if match is None:
        return jsonify(error="not found"), 404
    return jsonify(trade_id=match.key, **match.fields)


@app.get("/")
def dashboard():
    # Markup lives in templates/dashboard.html, styling/behavior in
    # static/dashboard.css and static/dashboard.js — Flask finds both
    # automatically relative to this file's directory.
    return render_template("dashboard.html")


if __name__ == "__main__":
    app.run(port=5001)
