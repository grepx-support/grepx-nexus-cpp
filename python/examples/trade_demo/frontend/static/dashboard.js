// Polls only while something's actually still moving — a fixed
// setInterval would hit the server forever even after every trade sits
// at ACCEPTED/REJECTED with nothing left to change, which doesn't scale
// with more tabs/trades open. Each refresh() decides for itself whether
// to schedule the next one, based on what it just fetched, so idle
// dashboards go quiet.
const TERMINAL = new Set(['ACCEPTED', 'REJECTED']);
let pollTimer = null;

async function refresh() {
  if (pollTimer !== null) { clearTimeout(pollTimer); pollTimer = null; }

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

  const pending = trades.some(t => !TERMINAL.has(t.status));
  document.getElementById('poll-status').textContent =
    pending ? 'watching for updates…' : 'idle — nothing pending';
  if (pending) pollTimer = setTimeout(refresh, 1500);
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

document.getElementById('refresh-btn').addEventListener('click', refresh);

refresh();
