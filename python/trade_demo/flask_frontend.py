"""Flask 'frontend' for the trade demo — a new trade order becomes a NEW
entry in the NewRequest audit table and a NEW snapshot row in
transactions. backend_worker.py picks it up from there and moves it
through the state machine, writing its own audit entries to
BackEndServer along the way.

Setup:
    pip install -r python/trade_demo/requirements.txt
    python scripts/create_schema.py

Run (separate terminals):
    nexus_server
    python python/trade_demo/flask_frontend.py
    python python/trade_demo/backend_worker.py

Then:
    curl -X POST localhost:5001/trades -H "Content-Type: application/json" \
        -d "{\"symbol\": \"AAPL\", \"side\": \"BUY\", \"qty\": 100, \"price\": 190.5}"
    curl localhost:5001/trades/<trade_id>
"""
import uuid

from app_model import NEW_REQUEST_TABLE, SNAPSHOT_TABLE, TRADE_APP
from flask import Flask, jsonify, request

app = Flask(__name__)

REQUIRED_FIELDS = ("symbol", "side", "qty", "price")


@app.post("/trades")
def submit_trade():
    body = request.get_json(force=True, silent=True) or {}
    if not all(k in body for k in REQUIRED_FIELDS):
        return jsonify(error=f"required fields: {', '.join(REQUIRED_FIELDS)}"), 400

    trade_id = str(uuid.uuid4())
    TRADE_APP.submit(
        NEW_REQUEST_TABLE, trade_id,
        status="NEW",
        application=NEW_REQUEST_TABLE,
        username=body.get("username", "anonymous"),
        fields={
            "symbol": str(body["symbol"]),
            "side": str(body["side"]),
            "qty": int(body["qty"]),
            "price": float(body["price"]),
        },
    )
    # 202: accepted into Nexus, not yet processed — BackEndServer picks
    # this up asynchronously via SUBSCRIBE.
    return jsonify(trade_id=trade_id, status="NEW"), 202


@app.get("/trades/<trade_id>")
def get_trade(trade_id):
    # No SELECT-by-key on the wire — filter client-side, fine at demo scale.
    rows = TRADE_APP.client.select(SNAPSHOT_TABLE)
    match = next((r for r in rows if r.key == trade_id), None)
    if match is None:
        return jsonify(error="not found"), 404
    return jsonify(trade_id=match.key, **match.fields)


if __name__ == "__main__":
    app.run(port=5001)
