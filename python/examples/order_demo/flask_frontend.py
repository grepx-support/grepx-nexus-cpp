"""Flask 'frontend' for order_demo — a new order becomes a NEW entry in
the NewOrderRequest audit table and a NEW snapshot row in orders.
main.py picks it up from there and moves it through the state
machine. Mirrors trade_demo/flask_frontend.py exactly — same shape,
order_demo-specific fields — so the two example demos read the same way
side by side.

Setup:
    scripts/setup.sh   (creates schema + seeds a login user — see
                        python/create_schema.py, python/seed_user.py)

Run (separate terminals):
    scripts/run.sh     (starts nexus_server)
    ./run.sh           (starts this + main.py together)

Then open http://localhost:5000/ in a browser — submit an order and
watch its status move NEW -> CLAIMED -> ACCEPTED as main.py
processes it. Or via curl:
    curl -X POST localhost:5000/orders -H "Content-Type: application/json" \
        -d "{\"amount\": 150, \"currency\": \"USD\"}"
    curl localhost:5000/orders/<order_id>
"""
import logging
import uuid

from app_model import NEW_ORDER_STAGE, SNAPSHOT_TABLE, create_order_app
from flask import Flask, jsonify, request
from nexus_api.logging_config import configure_logging

configure_logging()
logger = logging.getLogger(__name__)

app = Flask(__name__)
# Distinct application_name from main.py's — see app_model.py's
# create_order_app() docstring for why this can't be a shared singleton.
ORDER_APP = create_order_app("order-demo-frontend")

REQUIRED_FIELDS = ("amount", "currency")


@app.post("/orders")
def submit_order():
    body = request.get_json(force=True, silent=True) or {}
    if not all(k in body for k in REQUIRED_FIELDS):
        return jsonify(error=f"required fields: {', '.join(REQUIRED_FIELDS)}"), 400

    order_id = str(uuid.uuid4())
    ORDER_APP.submit(
        order_id,
        status="NEW",
        application=NEW_ORDER_STAGE,
        username=body.get("username", "anonymous"),
        fields={
            "amount": int(body["amount"]),
            "currency": str(body["currency"]),
        },
    )
    logger.info("order submitted: order_id=%s amount=%s currency=%s",
                order_id, body["amount"], body["currency"])
    # 202: accepted into Nexus, not yet processed — main.py picks
    # this up asynchronously via SUBSCRIBE.
    return jsonify(order_id=order_id, status="NEW"), 202


@app.get("/orders")
def list_orders():
    rows = ORDER_APP.client.select(SNAPSHOT_TABLE)
    orders = sorted(
        ({"order_id": r.key, **r.fields} for r in rows),
        key=lambda o: o.get("occurred_at", ""),
        reverse=True,
    )
    return jsonify(orders)


@app.get("/orders/<order_id>")
def get_order(order_id):
    # No SELECT-by-key on the wire — filter client-side, fine at demo scale.
    rows = ORDER_APP.client.select(SNAPSHOT_TABLE)
    match = next((r for r in rows if r.key == order_id), None)
    if match is None:
        return jsonify(error="not found"), 404
    return jsonify(order_id=match.key, **match.fields)


DASHBOARD_HTML = """<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>order_demo</title>
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
<h1>order_demo — NewOrderRequest &rarr; Nexus &rarr; nexus_worker</h1>

<form id="submit-form">
  <input name="amount" type="number" placeholder="amount" required>
  <input name="currency" placeholder="currency (e.g. USD)" required>
  <button type="submit">Submit order</button>
  <button type="button" id="refresh-btn">Refresh</button>
</form>
<div id="msg"></div>
<div id="poll-status" style="font-size: 0.8rem; color: #999;"></div>

<table>
  <thead>
    <tr><th>order_id</th><th>status</th><th>application</th><th>username</th>
        <th>amount</th><th>currency</th><th>occurred_at</th></tr>
  </thead>
  <tbody id="rows"></tbody>
</table>

<script>
// Polls only while something's actually still moving — a fixed
// setInterval would hit the server forever even after every order sits
// at ACCEPTED/REJECTED with nothing left to change, which doesn't scale
// with more tabs/orders open. Each refresh() decides for itself whether
// to schedule the next one, based on what it just fetched, so idle
// dashboards go quiet.
const TERMINAL = new Set(['ACCEPTED', 'REJECTED']);
let pollTimer = null;

async function refresh() {
  if (pollTimer !== null) { clearTimeout(pollTimer); pollTimer = null; }

  const res = await fetch('/orders');
  const orders = await res.json();
  document.getElementById('rows').innerHTML = orders.map(o => `
    <tr>
      <td>${o.order_id.slice(0, 8)}&hellip;</td>
      <td><span class="status ${o.status}">${o.status}</span></td>
      <td>${o.application ?? ''}</td>
      <td>${o.username ?? ''}</td>
      <td>${o.amount ?? ''}</td>
      <td>${o.currency ?? ''}</td>
      <td>${(o.occurred_at ?? '').replace('T', ' ').slice(0, 19)}</td>
    </tr>`).join('');

  const pending = orders.some(o => !TERMINAL.has(o.status));
  document.getElementById('poll-status').textContent =
    pending ? 'watching for updates…' : 'idle — nothing pending';
  if (pending) pollTimer = setTimeout(refresh, 1500);
}

document.getElementById('submit-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const form = new FormData(e.target);
  const body = {
    amount: Number(form.get('amount')),
    currency: form.get('currency'),
  };
  const res = await fetch('/orders', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body),
  });
  const data = await res.json();
  const msg = document.getElementById('msg');
  msg.textContent = res.ok
    ? `submitted ${data.order_id} — watch it move NEW -> CLAIMED -> ACCEPTED below`
    : `error: ${data.error}`;
  e.target.reset();
  refresh();
});

document.getElementById('refresh-btn').addEventListener('click', refresh);

refresh();
</script>
</body>
</html>
"""


@app.get("/")
def dashboard():
    return DASHBOARD_HTML


if __name__ == "__main__":
    app.run(port=5000)
