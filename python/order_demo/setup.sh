#!/usr/bin/env bash
# One-time setup for order_demo: creates a virtualenv here, installs
# requirements.txt into it, and creates the Nexus SQLite schema (the
# nexus_rows table nexus_server assumes already exists).
#
# You still need, separately (not done by this script):
#   nexus_server built and running (see repo root README / CMake). Point
#   it at the same DB this script just created: either run it with its
#   working directory set to the repo root, or set NEXUS_DB_PATH to the
#   absolute path printed below. nexus_server also prints the absolute
#   path it actually opened on startup — check that matches.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

python3 -m venv .venv

# POSIX venvs put activate under bin/; Windows (incl. Git Bash/MSYS) puts
# it under Scripts/ instead — support both.
if [ -f .venv/bin/activate ]; then
    source .venv/bin/activate
elif [ -f .venv/Scripts/activate ]; then
    source .venv/Scripts/activate
else
    echo "setup.sh: could not find venv activate script under .venv/" >&2
    exit 1
fi

python -m pip install --upgrade pip
python -m pip install -r requirements.txt

echo
echo "order_demo: venv ready in $(pwd)/.venv"
echo
python ../../scripts/create_schema.py
echo
echo "Next: start nexus_server (working directory = repo root, or set"
echo "NEXUS_DB_PATH to the path printed above), then ./run.sh"
