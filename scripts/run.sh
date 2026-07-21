#!/usr/bin/env bash
# Finds and starts the already-built nexus_server executable — saves
# hunting through cmake-build-debug/apps/nexus_server/ by hand every time.
#
# Doesn't build anything. If nothing is found, build the nexus_server
# CMake target first (CLion's Build button, or
# `cmake --build <build-dir> --target nexus_server`), then re-run this.
#
# All the detailed request/response logging (every REQUEST/RESPONSE line,
# nanosecond timestamps, login/logout, errors) lives inside nexus_server
# itself (see apps/nexus_server/src/main.cpp + libs/logging) — this
# script just launches whatever binary is already sitting on disk. If
# that binary predates a source change (this repo has had a lot of them:
# auth, sessions, logging), it won't have that code in it at all, and
# nothing here can fix that — only a rebuild can. The warning below is
# this script's only defense against silently launching a stale build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."   # repo root

EXE=$(find . \( -path "*/cmake-build-*/apps/nexus_server/nexus_server" \
                 -o -path "*/cmake-build-*/apps/nexus_server/nexus_server.exe" \) \
              -type f 2>/dev/null | head -n 1)

if [ -z "$EXE" ]; then
    echo "run.sh: no built nexus_server found under any cmake-build-*/ directory." >&2
    echo "Build the nexus_server CMake target first, then re-run this." >&2
    exit 1
fi

# Warn if the binary is older than its own source — a stale build is
# exactly what produces "it started but nothing happens" confusion, and
# rebuilding is the only fix; this script can't do that itself (that's a
# CLion/CMake step, not a launch step).
SRC_DIR="apps/nexus_server/src"
if [ -d "$SRC_DIR" ]; then
    NEWEST_SRC=$(find "$SRC_DIR" -name '*.cpp' -o -name '*.hpp' 2>/dev/null | xargs -r ls -t 2>/dev/null | head -n 1)
    if [ -n "$NEWEST_SRC" ] && [ "$NEWEST_SRC" -nt "$EXE" ]; then
        echo "run.sh: WARNING — $EXE is older than $NEWEST_SRC." >&2
        echo "This binary predates a source change and may be missing recent" >&2
        echo "features (logging, auth, sessions, ...). Rebuild nexus_server" >&2
        echo "(CLion's Build button, or cmake --build <dir> --target nexus_server)" >&2
        echo "before relying on its output for debugging." >&2
        echo >&2
    fi
fi

echo "starting $EXE"
exec "$EXE"
