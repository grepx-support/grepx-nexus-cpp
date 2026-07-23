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
# NEXUS_DB_PATH is set by _common.sh to one fixed file (repo root's
# nexus_data.db, unless already overridden) — create_schema.py and
# seed_user.py both apply to exactly that one file, and start_nexus_server.sh exports
# the same variable before launching nexus_server, so the schema/seed
# step and the server always agree on the same database. Without
# NEXUS_DB_PATH set, create_schema.py instead applies to every DB it
# might plausibly need to (repo root AND next to any built
# nexus_server it finds) — a reasonable fallback on its own, but two
# real files where you only want one.
#
# Does NOT build nexus_server — that's a separate CMake/IDE step (see
# repo root CMakeLists.txt). Run this once after building, and again
# whenever models.py gains a new table.
#
# NEXUS_USERNAME/NEXUS_PASSWORD override the "demo"/"demo" default —
# python/examples/order_demo and python/examples/trade_demo fall back to that same default
# if those env vars aren't set, so leave them unset for local demo use.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
source ./_common.sh
echo "db: $NEXUS_DB_PATH"
cd ../python

python3 create_schema.py
echo
python3 seed_user.py "${NEXUS_USERNAME:-demo}" "${NEXUS_PASSWORD:-demo}"

echo
echo "Next, from this repo's scripts/ directory:"
echo "  ./start_nexus_server.sh                                to start nexus_server (if built)"
echo "  ./start_client.sh [order_demo|trade_demo]  to run a demo client"
