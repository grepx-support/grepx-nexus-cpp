#!/usr/bin/env bash
# Starts a Python demo client against nexus_server: order_demo or
# trade_demo. Runs that demo's own setup.sh first — just venv creation +
# pip install now, schema/seeding already happened via ./setup.sh — then
# runs the demo.
#
# Always runs setup.sh, not just when .venv looks missing: pip install
# is idempotent and fast once packages are already present, and
# skipping it based solely on ".venv exists" previously masked a stale
# or incomplete venv (e.g. left over from before this repo's
# python/order_demo -> python/examples/order_demo move) with a
# confusing ModuleNotFoundError instead of just fixing itself.
#
# Usage:
#   ./start_client.sh              # order_demo (default)
#   ./start_client.sh order_demo
#   ./start_client.sh trade_demo
#
# Assumes nexus_server is already running (./run.sh) and ./setup.sh has
# been run at least once (schema + a seeded user — without that every
# login fails with "unauthorized").
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../python/examples"

DEMO="${1:-order_demo}"
if [ ! -d "$DEMO" ]; then
    echo "start_client.sh: unknown demo '$DEMO' (expected order_demo or trade_demo)" >&2
    exit 1
fi

cd "$DEMO"
./setup.sh
echo

# Wait for nexus_server to actually accept connections before launching
# the demo — otherwise a demo started moments after ./run.sh can lose
# the race against the server still opening its DB / binding the port,
# and fail with a raw ConnectionRefusedError instead of a clear message.
NEXUS_PORT="${NEXUS_PORT:-7878}"
echo -n "start_client.sh: waiting for nexus_server on 127.0.0.1:$NEXUS_PORT..."
server_up=false
for _ in $(seq 1 30); do
    if (exec 3<>"/dev/tcp/127.0.0.1/$NEXUS_PORT") 2>/dev/null; then
        exec 3>&-
        echo " up."
        server_up=true
        break
    fi
    echo -n "."
    sleep 0.5
done
if [ "$server_up" != true ]; then
    echo
    echo "start_client.sh: nexus_server isn't accepting connections on port $NEXUS_PORT after 15s." >&2
    echo "Check the terminal running ./run.sh — it should show \"listening on 127.0.0.1:$NEXUS_PORT\"." >&2
    echo "If it shows nothing past \"starting ...\", the server process exited; check for a crash there." >&2
    exit 1
fi

exec ./run.sh
