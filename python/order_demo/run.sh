#!/usr/bin/env bash
# Starts order_demo: the Flask frontend (http://localhost:5000) and the
# Nexus worker, together. Assumes nexus_server is already running and
# ./setup.sh has been run once.
#
# Ctrl+C stops both cleanly.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if [ ! -d .venv ]; then
    echo "no .venv found here — run ./setup.sh first" >&2
    exit 1
fi
source .venv/bin/activate

python flask_frontend.py &
FRONTEND_PID=$!
python nexus_worker.py &
WORKER_PID=$!

trap 'echo; echo "stopping..."; kill "$FRONTEND_PID" "$WORKER_PID" 2>/dev/null' EXIT INT TERM

echo "order_demo running:"
echo "  frontend  pid=$FRONTEND_PID  http://localhost:5000/orders"
echo "  worker    pid=$WORKER_PID"
echo "Ctrl+C to stop."
wait
