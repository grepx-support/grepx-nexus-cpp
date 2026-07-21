#!/usr/bin/env bash
# One-time(ish) setup for the Nexus server itself: creates the SQLite
# schema AND seeds a login user — the two things that have to happen
# before nexus_server will accept any client at all.
#
# The actual Python (models.py, create_schema.py, seed_user.py) lives
# under python/, not here — this directory is bash-only, just the
# scripts that bring services up. Nothing else in this repo depends on
# that split (create_schema.py's own path resolution is unaffected by
# which directory it's run from), it's purely about keeping "orchestrate
# startup" (here) separate from "define/apply the schema" (python/).
#
# create_schema.py applies the schema everywhere nexus_server might
# actually look — the repo root, any NEXUS_DB_PATH override, and next to
# any already-built nexus_server executable it can find under
# cmake-build-*/ — so this works regardless of how nexus_server ends up
# being launched. seed_user.py uses that exact same path resolution, so
# whichever DB create_schema.py just touched, seed_user.py touches too.
#
# Does NOT build nexus_server — that's a separate CMake/IDE step (see
# repo root CMakeLists.txt). Run this once after building, and again
# whenever models.py gains a new table, or nexus_server was rebuilt into
# a fresh build directory that hasn't had schema applied yet.
#
# NEXUS_USERNAME/NEXUS_PASSWORD override the "demo"/"demo" default —
# python/examples/order_demo and python/examples/trade_demo fall back to that same default
# if those env vars aren't set, so leave them unset for local demo use.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../python"

python3 create_schema.py
echo
python3 seed_user.py "${NEXUS_USERNAME:-demo}" "${NEXUS_PASSWORD:-demo}"

echo
echo "Next, from this repo's scripts/ directory:"
echo "  ./run.sh                                to start nexus_server (if built)"
echo "  ./start_client.sh [order_demo|trade_demo]  to run a demo client"
