#!/usr/bin/env bash
# Starts trade_demo: the Flask frontend (http://localhost:5001) and the
# backend worker, together. Assumes nexus_server is already running and
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
python backend_worker.py &
WORKER_PID=$!

trap 'echo; echo "stopping..."; kill "$FRONTEND_PID" "$WORKER_PID" 2>/dev/null' EXIT

echo "trade_demo running:"
echo "  frontend  pid=$FRONTEND_PID  http://localhost:5001/trades"
echo "  worker    pid=$WORKER_PID"
echo "Ctrl+C to stop."
wait
