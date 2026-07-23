#!/usr/bin/env bash
# Starts order_demo: the Flask frontend (http://localhost:5000) and the
# Nexus worker, together. Assumes nexus_server is already running and
# ./setup.sh has been run once.
#
# Ctrl+C stops both cleanly.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if [ -f .venv/bin/activate ]; then
    source .venv/bin/activate
elif [ -f .venv/Scripts/activate ]; then
    source .venv/Scripts/activate
else
    echo "no venv found here — run ./setup.sh first" >&2
    exit 1
fi

python flask_frontend.py &
FRONTEND_PID=$!
python main.py &
WORKER_PID=$!

# Both demos' worker entrypoints are now named main.py (see
# business_logic.py/main.py split) — indistinguishable from each other
# by process name/command-line alone, unlike when they had unique names
# (nexus_worker.py vs backend_worker.py). A pidfile in this demo's own
# directory is what stop_client.sh (run from a different terminal) uses
# instead, so it stops exactly this demo's two processes and not the
# other one's.
echo "$FRONTEND_PID $WORKER_PID" > .run.pids
trap 'echo; echo "stopping..."; kill "$FRONTEND_PID" "$WORKER_PID" 2>/dev/null; rm -f .run.pids' EXIT

echo "order_demo running:"
echo "  frontend  pid=$FRONTEND_PID  http://localhost:5000/orders"
echo "  worker    pid=$WORKER_PID"
echo "Ctrl+C to stop."
wait
