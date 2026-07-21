"""Flask 'frontend' for the trade demo — a new trade order becomes a NEW
entry in the NewRequest audit table and a NEW snapshot row in
transactions. backend_worker.py picks it up from there and moves it
through the state machine, writing its own audit entries to
BackEndServer along the way.

Setup:
    ./setup.sh   (creates a venv, installs deps, creates the DB schema)

Run (separate terminals):
    nexus_server   (working directory = repo root, or NEXUS_DB_PATH set —
                    see setup.sh's printed instructions)
    ./run.sh       (starts this + backend_worker.py together)

Then open http://localhost:5001/ in a browser — submit a trade and
watch its status move NEW -> CLAIMED -> ACCEPTED as backend_worker.py
processes it. Or via curl:
    curl -X POST localhost:5001/trades -H "Content-Type: application/json" \
        -d "{\"symbol\": \"AAPL\", \"side\": \"BUY\", \"qty\": 100, \"price\": 190.5}"
    curl localhost:5001/trades/<trade_id>
"""
import logging
import uuid

from app_model import NEW_REQUEST_TABLE, SNAPSHOT_TABLE, create_trade_app
from flask import Flask, jsonify, request
from nexus_api.logging_config import configure_logging

configure_logging()
logger = logging.getLogger(__name__)

app = Flask(__name__)
# Distinct application_name from backend_worker.py's — see app_model.py's
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


DASHBOARD_HTML = """<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>trade_demo</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 2rem; color: #1a1a1a; }
  h1 { font-size: 1.25rem; }
  form { display: flex; gap: 0.5rem; margin-bottom: 1.5rem; flex-wrap: wrap; }
  input { padding: 0.4rem; border: 1px solid #ccc; border-radius: 4px; }
  button { padding: 0.4rem 0.9rem; border: 0; border-radius: 4px; background: #1a56db; color: white; cursor: pointer; }
  table { border-collapse: collapse; width: 100%; }
  th, td { text-align: left; padding: 0.4rem 0.6rem; border-bottom: 1px solid #eee; font-size: 0.9rem; }
  th { color: #666; font-weight: 600; }
  .status { padding: 0.15rem 0.5rem; border-radius: 999px; font-size: 0.78rem; font-weight: 600; }
  .NEW { background: #fef3c7; color: #92400e; }
  .CLAIMED { background: #dbeafe; color: #1e40af; }
  .ACCEPTED { background: #d1fae5; color: #065f46; }
  .REJECTED, .FAILED { background: #fee2e2; color: #991b1b; }
  #msg { font-size: 0.85rem; color: #666; min-height: 1.2em; }
</style>
</head>
<body>
<h1>trade_demo — NewRequest &rarr; Nexus &rarr; BackEndServer</h1>

<form id="submit-form">
  <input name="symbol" placeholder="symbol (e.g. AAPL)" required>
  <input name="side" placeholder="side (BUY/SELL)" required>
  <input name="qty" type="number" placeholder="qty" required>
  <input name="price" type="number" step="0.01" placeholder="price" required>
  <button type="submit">Submit trade</button>
</form>
<div id="msg"></div>

<table>
  <thead>
    <tr><th>trade_id</th><th>status</th><th>application</th><th>username</th>
        <th>symbol</th><th>side</th><th>qty</th><th>price</th><th>occurred_at</th></tr>
  </thead>
  <tbody id="rows"></tbody>
</table>

<script>
async function refresh() {
  const res = await fetch('/trades');
  const trades = await res.json();
  document.getElementById('rows').innerHTML = trades.map(t => `
    <tr>
      <td>${t.trade_id.slice(0, 8)}&hellip;</td>
      <td><span class="status ${t.status}">${t.status}</span></td>
      <td>${t.application ?? ''}</td>
      <td>${t.username ?? ''}</td>
      <td>${t.symbol ?? ''}</td>
      <td>${t.side ?? ''}</td>
      <td>${t.qty ?? ''}</td>
      <td>${t.price ?? ''}</td>
      <td>${(t.occurred_at ?? '').replace('T', ' ').slice(0, 19)}</td>
    </tr>`).join('');
}

document.getElementById('submit-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const form = new FormData(e.target);
  const body = {
    symbol: form.get('symbol'),
    side: form.get('side'),
    qty: Number(form.get('qty')),
    price: Number(form.get('price')),
  };
  const res = await fetch('/trades', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body),
  });
  const data = await res.json();
  const msg = document.getElementById('msg');
  msg.textContent = res.ok
    ? `submitted ${data.trade_id} — watch it move NEW -> CLAIMED -> ACCEPTED below`
    : `error: ${data.error}`;
  e.target.reset();
  refresh();
});

refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
"""


@app.get("/")
def dashboard():
    return DASHBOARD_HTML


if __name__ == "__main__":
    app.run(port=5001)
