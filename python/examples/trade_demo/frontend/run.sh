#!/usr/bin/env bash
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

# Starts trade_demo's Flask frontend (http://localhost:5001) on its
# own — standalone, no backend client started alongside it. Assumes
# nexus_server is already running and ../setup.sh has been run once
# (the venv is shared with backend/, one directory up — nothing
# frontend-specific to install).
#
# Ctrl+C stops it. Run trade_demo/run.sh instead if you also want a
# backend client started (it prompts which one).
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

cd frontend
python flask_frontend.py &
FRONTEND_PID=$!

echo "$FRONTEND_PID" > .run.pid
trap 'echo; echo "stopping..."; kill "$FRONTEND_PID" 2>/dev/null; rm -f .run.pid' EXIT

echo "trade_demo frontend running: pid=$FRONTEND_PID  http://localhost:5001/trades"
echo "Ctrl+C to stop."
wait
