"""Flask 'frontend' service — the REST edge microservices normally call
each other through. Instead, every request becomes a row in Nexus with
status=NEW; any number of backend workers pick it up via SUBSCRIBE (see
nexus_worker.py). This is the "services talk to Nexus, not to each
other" pattern in miniature.

Setup (once):
    pip install -r python/order_demo/requirements.txt
    python scripts/create_schema.py

Run (three separate terminals):
    ./cmake-build-debug/apps/nexus_server/nexus_server   (or wherever you built it)
    python python/order_demo/flask_frontend.py
    python python/order_demo/nexus_worker.py

Then:
    curl -X POST localhost:5000/orders -H "Content-Type: application/json" \
        -d "{\"amount\": 150, \"currency\": \"USD\"}"
    curl localhost:5000/orders/<order_id>
"""
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # repo/python/

from flask import Flask, jsonify, request

from nexus_api import NexusClient

app = Flask(__name__)
nexus = NexusClient()

TABLE = "orders"


@app.post("/orders")
def create_order():
    body = request.get_json(force=True, silent=True) or {}
    amount = body.get("amount")
    currency = body.get("currency")
    if amount is None or currency is None:
        return jsonify(error="amount and currency are required"), 400

    order_id = str(uuid.uuid4())
    nexus.insert(TABLE, order_id, {
        "amount": int(amount),
        "currency": str(currency),
        "status": "NEW",
    })
    # 202: accepted for processing, not processed yet — a worker picks
    # this up asynchronously via SUBSCRIBE. This endpoint never talks to
    # a backend service directly.
    return jsonify(order_id=order_id, status="NEW"), 202


@app.get("/orders/<order_id>")
def get_order(order_id):
    # No SELECT-by-key on the wire (Predicate only matches on fields, not
    # row key) — fine at demo scale, filter client-side.
    rows = nexus.select(TABLE)
    match = next((r for r in rows if r.key == order_id), None)
    if match is None:
        return jsonify(error="not found"), 404
    return jsonify(order_id=match.key, **match.fields)


if __name__ == "__main__":
    app.run(port=5000)
