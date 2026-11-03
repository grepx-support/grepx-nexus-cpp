# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

# Sourced (not run directly) by setup.sh, start_nexus_server.sh, inspect_db.sh — the
# single place that decides where nexus_data.db and nexus_server's log
# file live, so every script agrees on exactly one location for each
# instead of each one guessing independently.
#
# Without this, create_schema.py/seed_user.py's own fallback logic
# (repo root, PLUS next to any already-built nexus_server it can find)
# applies schema/seeds to more than one file "just in case", since it
# has no way to know in advance where NEXUS_DB_PATH-less nexus_server
# will end up looking (next to its own executable, by default). That's
# a reasonable defensive default for create_schema.py to have on its
# own, but it means inspect_db.sh showing two files isn't a bug in
# inspect_db.sh — it's two real, separately-seeded databases existing.
# Setting NEXUS_DB_PATH once, here, and exporting it before every script
# that touches the DB (including start_nexus_server.sh, so nexus_server itself opens
# the same file) collapses that back down to one.
NEXUS_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export NEXUS_DB_PATH="${NEXUS_DB_PATH:-$NEXUS_REPO_ROOT/nexus_data.db}"

# Same idea for the log file. nexus_server (see apps/nexus_server/src/main.cpp's
# setup_logging()) defaults to <its own executable's directory>/logs/nexus_server.log
# when NEXUS_LOG_PATH isn't set — which buries it inside cmake-build-*/apps/nexus_server/,
# not obvious to find. Pinning it to repo-root/logs/ here, the same way NEXUS_DB_PATH
# is pinned, keeps it next to the DB and out of build output.
export NEXUS_LOG_PATH="${NEXUS_LOG_PATH:-$NEXUS_REPO_ROOT/logs/nexus_server.log}"
