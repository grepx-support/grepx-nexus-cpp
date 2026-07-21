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

DEMO="${1:-}"
case "$DEMO" in
    order_demo) PATTERNS="flask_frontend.py|nexus_worker.py" ;;
    trade_demo) PATTERNS="flask_frontend.py|backend_worker.py" ;;
    "") PATTERNS="flask_frontend.py|nexus_worker.py|backend_worker.py" ;;
    *)
        echo "stop_client.sh: unknown demo '$DEMO' (expected order_demo or trade_demo)" >&2
        exit 1
        ;;
esac

stopped_any=false

if command -v taskkill >/dev/null 2>&1 && command -v wmic >/dev/null 2>&1; then
    # Windows: python.exe processes all share one image name, so filter
    # by command line (the script path) via wmic instead of taskkill's
    # //IM, which would kill every python.exe on the machine.
    IFS='|' read -ra pats <<< "$PATTERNS"
    for pat in "${pats[@]}"; do
        pids=$(wmic process where "name='python.exe' and CommandLine like '%${pat}%'" get ProcessId 2>/dev/null \
               | tr -d '\r' | grep -E '^[0-9]+$')
        for pid in $pids; do
            taskkill //F //PID "$pid" >/dev/null 2>&1 && {
                echo "stop_client.sh: stopped $pat (pid $pid)"
                stopped_any=true
            }
        done
    done
else
    PIDS=$( (ps -ef 2>/dev/null || ps -A 2>/dev/null) | grep -E "$PATTERNS" | grep -v grep | awk '{print $1}')
    for pid in $PIDS; do
        echo "stop_client.sh: stopping pid $pid"
        kill "$pid" 2>/dev/null && stopped_any=true
    done
fi

if [ "$stopped_any" != true ]; then
    echo "stop_client.sh: no matching demo process found (${DEMO:-order_demo/trade_demo})." >&2
    exit 1
fi
