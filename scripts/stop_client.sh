#!/usr/bin/env bash
# Stops a running demo client (order_demo or trade_demo — both flask
# frontend and worker) started via ./start_client.sh in another
# terminal. Ctrl+C there does the same thing directly (see each demo's
# run.sh trap); this is for stopping it from a different terminal
# without hunting down that one.
#
# Usage:
#   ./stop_client.sh              # stop both demos, whichever is running
#   ./stop_client.sh order_demo   # stop only order_demo's processes
#   ./stop_client.sh trade_demo   # stop only trade_demo's processes
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../python/examples"

DEMO="${1:-}"
case "$DEMO" in
    order_demo|trade_demo) DEMOS="$DEMO" ;;
    "") DEMOS="order_demo trade_demo" ;;
    *)
        echo "stop_client.sh: unknown demo '$DEMO' (expected order_demo or trade_demo)" >&2
        exit 1
        ;;
esac

stopped_any=false

kill_pid() {
    local pid="$1" label="$2"
    if command -v taskkill >/dev/null 2>&1; then
        taskkill //F //PID "$pid" >/dev/null 2>&1 && {
            echo "stop_client.sh: stopped $label (pid $pid)"
            stopped_any=true
        }
    else
        kill "$pid" 2>/dev/null && {
            echo "stop_client.sh: stopped $label (pid $pid)"
            stopped_any=true
        }
    fi
}

for demo in $DEMOS; do
    pidfile="$demo/.run.pids"
    if [ -f "$pidfile" ]; then
        # Primary path: run.sh wrote its own frontend/worker PIDs here.
        # Reliable regardless of what the worker script happens to be
        # named — order_demo and trade_demo's workers are both main.py,
        # so matching by process name/command-line alone (the old
        # approach) could no longer tell the two demos' workers apart.
        read -r frontend_pid worker_pid < "$pidfile"
        kill_pid "$frontend_pid" "$demo flask_frontend.py"
        kill_pid "$worker_pid" "$demo main.py"
        rm -f "$pidfile"
    else
        # Fallback: no pidfile (started some other way, or a pre-pidfile
        # run.sh). Best-effort match by working directory in the command
        # line where available, else by port for the frontend at least.
        echo "stop_client.sh: no $pidfile found — falling back to best-effort process match" >&2
        if command -v taskkill >/dev/null 2>&1 && command -v wmic >/dev/null 2>&1; then
            pids=$(wmic process where "name='python.exe' and CommandLine like '%${demo}%'" get ProcessId 2>/dev/null \
                   | tr -d '\r' | grep -E '^[0-9]+$')
            for pid in $pids; do
                kill_pid "$pid" "$demo (matched by path in command line)"
            done
        else
            pids=$( (ps -ef 2>/dev/null || ps -A 2>/dev/null) | grep -E "$demo" | grep -v grep | awk '{print $1}')
            for pid in $pids; do
                kill_pid "$pid" "$demo (matched by path in command line)"
            done
        fi
    fi
done

if [ "$stopped_any" != true ]; then
    echo "stop_client.sh: no matching demo process found (${DEMO:-order_demo/trade_demo})." >&2
    exit 1
fi
