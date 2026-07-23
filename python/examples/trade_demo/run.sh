#!/usr/bin/env bash
# Starts trade_demo end to end: the frontend (frontend/run.sh), then
# prompts for which backend client to start (backend/run.sh), then
# shows a short status summary. Assumes nexus_server is already running
# and ./setup.sh has been run once.
#
# Run frontend/run.sh or backend/run.sh directly instead if you only
# want one side — this script is the "start everything" convenience
# wrapper, not a replacement for either.
#
# Ctrl+C stops both the frontend and whichever backend client(s) got
# started.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

./frontend/run.sh &
FRONTEND_SCRIPT_PID=$!
sleep 1   # let it finish logging in before the prompt/status below

echo
echo "Which backend client should run alongside it?"
echo "  main      - primary worker: claims/accepts every NEW trade, any symbol"
echo "  reader    - SELECT-only, polls symbol=ABC every 2s"
echo "  updater   - subscribes live, claims new symbol=ABC trades"
echo "  inserter  - submits one symbol=ABC trade, then exits"
echo "  all       - main + reader + updater in the background, inserter once"
read -r -p "[main/reader/updater/inserter/all] (default: all): " CHOICE
CHOICE="${CHOICE:-all}"

./backend/run.sh "$CHOICE" &
BACKEND_SCRIPT_PID=$!

trap 'echo; echo "stopping everything..."; kill "$FRONTEND_SCRIPT_PID" "$BACKEND_SCRIPT_PID" 2>/dev/null' EXIT

echo
echo "== trade_demo status =========================================="
echo "frontend : http://localhost:5001/trades  (script pid=$FRONTEND_SCRIPT_PID)"
echo "backend  : $CHOICE  (script pid=$BACKEND_SCRIPT_PID)"
echo
echo "Check what actually landed with, from the repo's scripts/ dir:"
echo "  ./inspect_db.sh --table transactions   the trade row(s), latest status"
echo "  ./inspect_db.sh --table outbox         one durable record per submit()"
echo "  ./inspect_db.sh --table user_login     one row per client that logged in"
echo "================================================================="
echo
echo "Ctrl+C to stop everything."
wait
