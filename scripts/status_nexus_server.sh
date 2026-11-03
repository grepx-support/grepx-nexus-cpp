#!/usr/bin/env bash
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

# One-shot health check: is nexus_server up, and what do its and any
# running demo clients' logs say right now. Doesn't start or stop
# anything — read-only, safe to run anytime.
#
# Usage:
#   ./status_nexus_server.sh              # last 20 lines of each log (default)
#   ./status_nexus_server.sh 50           # last 50 lines instead
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
source ./_common.sh   # same NEXUS_DB_PATH/NEXUS_LOG_PATH setup.sh/start_nexus_server.sh use
cd ..   # repo root

LINES="${1:-20}"
NEXUS_PORT="${NEXUS_PORT:-7878}"

echo "== nexus_server =========================================="
echo "db: $NEXUS_DB_PATH"
EXE=$(find . \( -path "*/cmake-build-*/apps/nexus_server/nexus_server" \
                 -o -path "*/cmake-build-*/apps/nexus_server/nexus_server.exe" \) \
              -type f 2>/dev/null | head -n 1)

if [ -z "$EXE" ]; then
    echo "not built — no nexus_server executable found under any cmake-build-*/ directory"
else
    echo "binary: $EXE"
    SRC_DIR="apps/nexus_server/src"
    NEWEST_SRC=$(find "$SRC_DIR" -name '*.cpp' -o -name '*.hpp' 2>/dev/null | xargs -r ls -t 2>/dev/null | head -n 1)
    if [ -n "$NEWEST_SRC" ] && [ "$NEWEST_SRC" -nt "$EXE" ]; then
        echo "  ⚠ STALE — older than $NEWEST_SRC, rebuild before trusting its output"
    fi

    if (exec 3<>"/dev/tcp/127.0.0.1/$NEXUS_PORT") 2>/dev/null; then
        exec 3>&-
        echo "port $NEXUS_PORT: UP (accepting connections)"
    else
        echo "port $NEXUS_PORT: DOWN (not accepting connections — is it running? ./start_nexus_server.sh)"
    fi

    LOG="$NEXUS_LOG_PATH"
    if [ -f "$LOG" ]; then
        echo "log: $LOG (last $LINES lines)"
        echo "---"
        tail -n "$LINES" "$LOG"
    else
        echo "log: not found at $LOG yet — nothing logged since last (re)build/start,"
        echo "  or this binary predates NEXUS_LOG_PATH support (see staleness warning above)"
    fi
fi

echo
echo "== demo clients (python/logs/) ============================"
if [ -d logs ] && [ -n "$(ls -A logs/*.log 2>/dev/null)" ]; then
    for f in logs/*.log; do
        echo "--- $f (last $LINES lines) ---"
        tail -n "$LINES" "$f"
        echo
    done
else
    echo "no client logs yet — logs/*.log is created the first time a demo runs"
    echo "(see python/nexus_api/logging_config.py)"
fi
