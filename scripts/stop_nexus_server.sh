#!/usr/bin/env bash
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

# Stops a running nexus_server process (started via ./start_nexus_server.sh in another
# terminal). Ctrl+C in that terminal does the same thing directly — this
# is for stopping it from a *different* terminal/window without having
# to go find the one running it.
#
# Tries taskkill first (this repo mainly targets Windows/Git-Bash, and
# taskkill matching by image name is far more reliable there than PID
# games across the MSYS/Windows PID boundary — note the "//" instead of
# "/" on its flags, which stops Git-Bash from mangling them into a path).
# Falls back to `ps` + kill for Linux/macOS.
set -uo pipefail

if command -v taskkill >/dev/null 2>&1; then
    if taskkill //F //IM nexus_server.exe >/dev/null 2>&1; then
        echo "stop_nexus_server.sh: stopped nexus_server.exe"
        exit 0
    fi
    echo "stop_nexus_server.sh: no running nexus_server.exe found." >&2
    exit 1
fi

PIDS=$( (ps -ef 2>/dev/null || ps -A 2>/dev/null) | grep '[n]exus_server' | awk '{print $1}')
if [ -z "$PIDS" ]; then
    echo "stop_nexus_server.sh: no running nexus_server process found." >&2
    exit 1
fi

for pid in $PIDS; do
    echo "stop_nexus_server.sh: stopping nexus_server (pid $pid)"
    kill "$pid" 2>/dev/null || true
done
