# Sourced (not run directly) by setup.sh, run.sh, inspect_db.sh — the
# single place that decides where nexus_data.db lives, so every script
# agrees on exactly one file instead of each one guessing independently.
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
# that touches the DB (including run.sh, so nexus_server itself opens
# the same file) collapses that back down to one.
NEXUS_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export NEXUS_DB_PATH="${NEXUS_DB_PATH:-$NEXUS_REPO_ROOT/nexus_data.db}"
