#!/usr/bin/env bash
# Dumps every table in the Nexus SQLite DB(s) this repo knows about —
# read-only, safe to run anytime nexus_server is up or down.
#
# Usage:
#   ./inspect_db.sh                  # every known DB
#   ./inspect_db.sh path/to/db       # just that one
#   ./inspect_db.sh --table orders   # only that logical/SQL table
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
source ./_common.sh   # same NEXUS_DB_PATH setup.sh/run.sh use — one file, not two
cd ../python
python inspect_db.py "$@"
