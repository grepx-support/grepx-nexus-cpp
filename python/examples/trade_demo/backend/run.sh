#!/usr/bin/env bash
# Starts one (or all) of trade_demo's backend clients. Assumes
# nexus_server is already running and ../setup.sh has been run once
# (the venv is shared with frontend/, one directory up).
#
# Usage:
#   ./run.sh main        the primary worker (worker_client/main.py +
#                         worker_client/business_logic.py): claims/accepts
#                         every NEW trade, any symbol
#   ./run.sh reader       SELECT-only client, polls symbol=ABC every 2s
#   ./run.sh updater      subscribes live, claims new symbol=ABC trades
#   ./run.sh inserter     submits one symbol=ABC trade, then exits
#   ./run.sh all          all four together — main/reader/updater in the
#                         background, inserter once to give them
#                         something to react to, then waits
#
# main/reader/updater run until Ctrl+C. inserter is one-shot by design
# (see inserter_client.py) — running it alone just submits and exits
# immediately, nothing to wait on.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."   # trade_demo/, for the shared venv

if [ -f .venv/bin/activate ]; then
    source .venv/bin/activate
elif [ -f .venv/Scripts/activate ]; then
    source .venv/Scripts/activate
else
    echo "no venv found in trade_demo/ — run ./setup.sh there first" >&2
    exit 1
fi

cd backend

CLIENT="${1:-}"
case "$CLIENT" in
    main|reader|updater|inserter|all) ;;
    *)
        echo "usage: ./run.sh {main|reader|updater|inserter|all}" >&2
        exit 1
        ;;
esac

script_for() {
    case "$1" in
        main) echo "worker_client/main.py" ;;
        reader) echo "reader_client.py" ;;
        updater) echo "updater_client.py" ;;
        inserter) echo "inserter_client.py" ;;
    esac
}

if [ "$CLIENT" != "all" ]; then
    if [ "$CLIENT" = "inserter" ]; then
        # One-shot — no pidfile, nothing to wait on, nothing to trap.
        exec python "$(script_for inserter)"
    fi
    python "$(script_for "$CLIENT")" &
    PID=$!
    echo "$PID" > .run.pids
    trap 'echo; echo "stopping..."; kill "$PID" 2>/dev/null; rm -f .run.pids' EXIT
    echo "backend/$CLIENT running: pid=$PID  Ctrl+C to stop."
    wait
    exit 0
fi

# all: start the three long-running clients, then submit once via
# inserter to give reader/updater something real to observe.
python "$(script_for main)" &
MAIN_PID=$!
python "$(script_for reader)" &
READER_PID=$!
python "$(script_for updater)" &
UPDATER_PID=$!

echo "$MAIN_PID $READER_PID $UPDATER_PID" > .run.pids
trap 'echo; echo "stopping..."; kill "$MAIN_PID" "$READER_PID" "$UPDATER_PID" 2>/dev/null; rm -f .run.pids' EXIT

sleep 1   # let logins/subscriptions land before the insert, for readable log ordering
echo "backend/all running: main=$MAIN_PID reader=$READER_PID updater=$UPDATER_PID"
echo "submitting one ABC trade via inserter_client.py..."
python "$(script_for inserter)"

echo
echo "main/reader/updater still running — Ctrl+C to stop."
wait
